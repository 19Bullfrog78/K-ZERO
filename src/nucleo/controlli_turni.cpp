// K-Zero - controlli sui turni, giorno per giorno (K-CTR-054 ... K-CTR-087, docs/controlli.md).
// Principio: un dato che contraddice un altro e' un errore; un piano incompleto e' un avviso. Un fatto con le date
// (assenza, disponibilita', scadenza, fermo, distrazione scaduta) che colpisce un turno ricorrente (rotazione,
// servizio fisso, mezzo scritto nel turno) e' un avviso: quel giorno il turno e' scoperto o senza mezzo, e il rimedio
// e' un'assegnazione del singolo giorno. La stessa cosa su un'assegnazione del singolo giorno e' un errore.
// Sovrapposizioni e intervalli si controllano anche a cavallo della mezzanotte, tra la fine e l'inizio di un ciclo e
// tra una rotazione e la successiva: si guardano le date vere, una per una. I problemi uguali di molti giorni si
// raggruppano (group_by_days).
#include <algorithm>
#include <map>
#include <set>
#include <tuple>

#include "controlli.hpp"
#include "orari.hpp"
#include "turni.hpp"

namespace kzero {

namespace {

// Massimo di giorni del periodo di un pacchetto con i turni (tre anni): oltre e' certamente un errore di scrittura,
// e i controlli giorno per giorno diventerebbero lentissimi.
constexpr int kMaxShiftPeriodDays = 1096;

struct Interval {
    long long a = 0, b = 0;   // minuti assoluti, [a, b)
    Day day = 0;
    const Shift* shift = nullptr;
    const Source* where = nullptr;   // punto dei dati dell'assegnazione (resta valido: e' nel pacchetto)
    bool unverified = false;         // K-017: la persona o il mezzo del turno non sono verificati
};

// Pezzi delle corse di un giorno: per ogni corsa del pacchetto (stessa posizione), i turni da autista che la fanno.
using Pieces = std::vector<std::vector<std::pair<const PlannedItem*, const ShiftDay*>>>;

class ShiftChecker {
public:
    ShiftChecker(const Package& p, std::vector<Problem>& out, PlanCollector* collector)
        : p_(p), out_(out), collector_(collector), plan_(p), idx_(plan_.index()), pieces_(p.trips.size()), escorts_(p.trips.size()) {
        // Cose che non cambiano da un giorno all'altro, preparate una volta sola.
        for (const Trip& t : p.trips) {
            trip_days_.push_back(plan_.days().days_of(t.calendar.code));
            bool aboard = false;
            for (const Passage& ps : t.passages) aboard = aboard || ps.assistant != AssistantMove::None;
            assistant_aboard_.push_back(aboard ? 1 : 0);
        }
    }

    void run() {
        if (p_.shifts.empty() && p_.rotations.empty() && p_.fixed.empty() && p_.day_assignments.empty() && p_.blocks.empty())
            return;
        const DateRange period = p_.manifest.period;
        statics();
        if (period.to < period.from) return;
        if (!p_.shifts.empty() && period.to - period.from + 1 > kMaxShiftPeriodDays) {
            add(Check::PeriodTooLongForShifts, p_.manifest.src,
                {number_param("giorni", period.to - period.from + 1), number_param("massimo", kMaxShiftPeriodDays)});
            return;
        }
        day_assignments_outside();
        for (Day d = period.from; d <= period.to; ++d) one_day(d);
        overlaps(persons_, Check::PersonOverlap, Element::Person);
        overlaps(vehicles_, Check::VehicleOverlap, Element::Vehicle);
        if (!p_.shifts.empty()) never_used();
    }

private:
    // by_kind: il problema nasce da un requisito che dipende dal tipo di servizio (K-014, KindRequirements): senza i dati
    // particolari si conta soltanto. unverified: dipende da un risultato del piano non verificato (K-017, ShiftDay): chi non puo'
    // leggere il tipo lo conta soltanto, tra i non verificati.
    void add(Check c, const Source& where, std::vector<Param> params, int variant = 0, bool by_kind = false, bool unverified = false) {
        out_.push_back(Problem{c, variant, where, std::move(params), by_kind, unverified});
    }

    static Param personal(Param p, const std::string& hidden = "riservato") {
        p.sensitivity = Param::Sensitivity::Personal;
        p.hidden = hidden;
        return p;
    }
    static Param particular(Param p) {
        p.sensitivity = Param::Sensitivity::Particular;
        p.hidden = "riservato";
        return p;
    }
    static Param shift_param(const Shift& s) { return ref_param("turno", Element::Shift, s.code); }
    static Param person_param(const Person& x, const char* name = "persona") { return ref_param(name, Element::Person, x.code); }
    static Param vehicle_param(const Vehicle& v, const char* name = "mezzo") { return ref_param(name, Element::Vehicle, v.code); }

    // --- Controlli senza date ---------------------------------------------------------------------------------

    void statics() {
        // Pezzi di corsa, ordine degli elementi, mezzo e ruolo, ricovero vicino a casa.
        for (const Shift& s : p_.shifts) {
            std::optional<Minutes> previous;
            bool trips = false;
            for (const ShiftItem& it : s.items) {
                std::optional<Minutes> start;
                if (it.is_trip) {
                    trips = true;
                    const Trip* t = idx_.trip(it.trip.trip.code);
                    if (!t) continue;   // codice inesistente: K-CTR-010
                    const PartResolution pr = resolve_part(it.trip, *t);
                    if (pr.problem != PartProblem::None) {
                        if (pr.bad && !idx_.place(pr.bad->place.code)) continue;   // luogo inesistente: K-CTR-010
                        const int variant = pr.problem == PartProblem::PlaceNotOnTrip ? 0 : pr.problem == PartProblem::Ambiguous ? 1 : 2;
                        add(Check::ShiftPartInvalid, pr.bad ? pr.bad->src : it.src,
                            {shift_param(s), ref_param("corsa", Element::Trip, t->code), ref_param("luogo", Element::Place, pr.bad ? pr.bad->place.code : "")},
                            variant);
                        continue;
                    }
                    start = t->passages[pr.from].time;
                } else {
                    start = it.activity.start;
                }
                if (previous && start && *start < *previous)
                    add(Check::ShiftItemsOrder, it.src, {shift_param(s), time_param("orario", *start), time_param("precedente", *previous)}, 0);
                if (start) previous = start;
            }
            if (s.role == ShiftRole::Driver && trips && s.vehicle_mode == ShiftVehicleMode::None)
                add(Check::ShiftVehicleInconsistent, s.vehicle_src, {shift_param(s)}, 0);
            if (s.role != ShiftRole::Driver && s.vehicle_mode != ShiftVehicleMode::None)
                add(Check::ShiftVehicleInconsistent, s.vehicle_src, {shift_param(s)}, 1);
            const CompanySettings* settings = idx_.settings(s.company.code);
            if (settings && settings->shelter == ShelterNearHome::Never)
                for (const Ref* place : {&s.start_place, &s.end_place}) {
                    const Place* pl = idx_.place(place->code);
                    if (pl && pl->type == PlaceType::ExternalShelter)
                        add(Check::ShiftAtShelterNever, place->src,
                            {shift_param(s), ref_param("luogo", Element::Place, pl->code), ref_param("azienda", Element::Company, s.company.code)});
                }
        }
        // Rotazioni scritte in modo incoerente.
        for (const Rotation& r : p_.rotations) {
            std::map<Code, std::size_t> rows;
            for (std::size_t i = 0; i < r.rows.size(); ++i) {
                const RotationRow& row = r.rows[i];
                if (static_cast<int>(row.steps.size()) != r.length)
                    add(Check::RotationMalformed, row.src,
                        {ref_param("rotazione", Element::Rotation, r.code), ref_param("persona", Element::Person, row.person.code),
                         number_param("passi", static_cast<long long>(row.steps.size())), number_param("lunghezza", r.length)},
                        0);
                if (!rows.emplace(row.person.code, i).second)
                    add(Check::RotationMalformed, row.src,
                        {ref_param("rotazione", Element::Rotation, r.code), ref_param("persona", Element::Person, row.person.code)}, 1);
            }
            int y = 0, m = 0, dd = 0;
            ymd_from_day(r.start, y, m, dd);
            if (r.unit == RotationUnit::Month && dd != 1)
                add(Check::RotationMalformed, r.src, {ref_param("rotazione", Element::Rotation, r.code), date_param("partenza", r.start)}, 2);
        }
        // Blocchi incompatibili: la stessa corsa su mezzi diversi in periodi che si toccano.
        for (std::size_t i = 0; i < p_.blocks.size(); ++i)
            for (std::size_t j = 0; j < i; ++j) {
                const Block& a = p_.blocks[i];
                const Block& b = p_.blocks[j];
                if (a.vehicle.code == b.vehicle.code) continue;
                const Day from = std::max(a.from.value_or(p_.manifest.period.from), b.from.value_or(p_.manifest.period.from));
                const Day to = std::min(a.to.value_or(p_.manifest.period.to), b.to.value_or(p_.manifest.period.to));
                if (to < from) continue;
                for (const Ref& ta : a.trips)
                    for (const Ref& tb : b.trips)
                        if (ta.code == tb.code)
                            add(Check::IncompatibleBlocks, ta.src,
                                {ref_param("corsa", Element::Trip, ta.code), ref_param("blocco", Element::Block, a.code),
                                 vehicle_param_code(a.vehicle.code), ref_param("altro", Element::Block, b.code),
                                 ref_param("altro_mezzo", Element::Vehicle, b.vehicle.code)});
            }
    }

    static Param vehicle_param_code(const Code& c) { return ref_param("mezzo", Element::Vehicle, c); }

    // Assegnazioni del singolo giorno fuori dal periodo del pacchetto.
    void day_assignments_outside() {
        for (const DayAssignment& a : p_.day_assignments)
            if (!plan_.days().in_period(a.date) && idx_.shift(a.shift.code))
                add(Check::AssignmentOnDayWithoutShift, a.src,
                    {ref_param("turno", Element::Shift, a.shift.code), date_param("data", a.date)}, 0);
    }

    // --- Un giorno ---------------------------------------------------------------------------------------------

    void one_day(Day d) {
        const std::vector<ShiftDay> plan = plan_.day(d);
        if (collector_) collector_->add(d, plan);
        std::set<Code> present;
        for (const ShiftDay& sd : plan) present.insert(sd.shift->code);
        // Assegnazioni del giorno su un turno che quel giorno non c'e'.
        for (const DayAssignment& a : p_.day_assignments)
            if (a.date == d && idx_.shift(a.shift.code) && !present.count(a.shift.code))
                add(Check::AssignmentOnDayWithoutShift, a.src, {ref_param("turno", Element::Shift, a.shift.code), date_param("data", d)}, 1);

        // Turni da autista e da assistente di ogni corsa, quel giorno (i vettori si riusano da un giorno all'altro).
        for (const std::size_t i : touched_) {
            pieces_[i].clear();
            escorts_[i].clear();
        }
        touched_.clear();
        for (const ShiftDay& sd : plan) {
            items(sd);
            person(sd);
            vehicle(sd);
            for (const PlannedItem& it : sd.items)
                if (it.trip) {
                    const std::size_t i = trip_index(it.trip);
                    if (sd.shift->role == ShiftRole::Driver) pieces_[i].push_back({&it, &sd});
                    if (sd.shift->role == ShiftRole::Escort) escorts_[i].push_back(&sd);
                    touched_.push_back(i);
                }
        }
        trips(d);
        escort_checks(plan);
        blocks(d);
    }

    // Elementi del turno che si sovrappongono quel giorno.
    void items(const ShiftDay& sd) {
        const auto days = [&] { return days_param("giorni", sd.day); };   // solo se serve
        for (std::size_t i = 1; i < sd.items.size(); ++i)
            if (sd.items[i].start < sd.items[i - 1].end && sd.items[i].start >= sd.items[i - 1].start)
                add(Check::ShiftItemsOrder, sd.items[i].item->src,
                    {shift_param(*sd.shift), time_param("orario", sd.items[i].start), time_param("fine", sd.items[i - 1].end), days()}, 1);
    }

    // Chi fa il turno: candidati, ruolo, prestito, competenze, abilitazioni, fatti, preferenze, abbinamento.
    void person(const ShiftDay& sd) {
        const Shift& s = *sd.shift;
        const auto days = [&] { return days_param("giorni", sd.day); };   // solo se serve
        // Due persone della rotazione nello stesso turno.
        for (std::size_t i = 1; i < sd.rotation_candidates.size(); ++i) {
            const RotationCandidate& c = sd.rotation_candidates[i];
            const RotationCandidate& first = sd.rotation_candidates.front();
            if (!c.person || !first.person || c.person == first.person) continue;
            add(Check::RotationTwoPersons, c.rotation->rows[c.row].src,
                {shift_param(s), person_param(*c.person), person_param(*first.person, "altra"), days()});
        }
        for (std::size_t i = 1; i < sd.fixed_candidates.size(); ++i)
            add(Check::TwoAssignmentsSameShift, sd.fixed_candidates[i]->src,
                {shift_param(s), ref_param("altro", Element::FixedAssignment, sd.fixed_candidates.front()->code), days()}, 0);
        for (std::size_t i = 1; i < sd.day_candidates.size(); ++i)
            add(Check::TwoAssignmentsSameShift, sd.day_candidates[i]->src, {shift_param(s), date_param("data", sd.day)}, 1);

        if (!sd.planned) {
            if (sd.person_source == PersonSource::None) add(Check::ShiftUncovered, s.src, {shift_param(s), days()});
            return;
        }
        const Person& p = *sd.planned;
        // Fatti con le date: avviso sul turno ricorrente, errore sull'assegnazione del giorno.
        if (sd.person_impediment != PersonImpediment::None) {
            const auto [variant, where, detail] = person_fact(sd);
            const bool nv = !sd.person_fact_verified;   // K-017
            // L'autorizzazione del committente serve solo per alcuni tipi di servizio (K-014).
            if (sd.person_source == PersonSource::Day)
                add(Check::DayAssignmentAgainstFact, sd.person_assignment->src, {shift_param(s), person_param(p), detail, date_param("data", sd.day)},
                    variant, sd.person_impediment == PersonImpediment::AuthorizationExpired, nv);
            else
                add(Check::RecurringShiftPersonFact, where, {shift_param(s), person_param(p), detail, days()}, variant, false, nv);
        }
        if (!sd.person) return;
        // K-017: se non e' verificato che la persona faccia il turno, non lo sono nemmeno i problemi che nascono da lei.
        const bool pu = !sd.person_verified;
        persons_[&p].push_back({absolute(sd.day, sd.start), absolute(sd.day, sd.end), sd.day, &s, &sd.person_position(), pu});
        (pu ? maybe_persons_ : used_persons_).insert(p.code);
        const Source& where = sd.person_position();

        const bool driver = s.role == ShiftRole::Driver || s.role == ShiftRole::SecondDriver;
        if ((driver && !p.has_role(PersonRole::Driver)) || (s.role == ShiftRole::Escort && !p.has_role(PersonRole::Escort)))
            add(Check::PersonRoleMismatch, where, {shift_param(s), person_param(p), days()}, s.role == ShiftRole::Escort ? 1 : 0, false, pu);
        if (p.company.code != s.company.code &&
            std::none_of(p.loans.begin(), p.loans.end(), [&s](const Loan& l) { return l.company.code == s.company.code; }))
            add(Check::OtherCompanyWithoutLoan, where,
                {shift_param(s), person_param(p), ref_param("azienda", Element::Company, s.company.code), days()}, 0, false, pu);
        // Competenze richieste dal turno e (per l'autista) dai servizi delle sue corse.
        std::set<Code> required;
        for (const Ref& k : s.competences) required.insert(k.code);
        if (s.role == ShiftRole::Driver)
            for (const PlannedItem& it : sd.items)
                if (it.trip)
                    for (const Ref& sr : it.trip->services)
                        if (const Service* sv = idx_.service(sr.code))
                            for (const Ref& k : sv->competences) required.insert(k.code);
        for (const Code& k : required)
            if (!p.has_competence(k))
                add(Check::PersonWithoutCompetence, where, {shift_param(s), person_param(p), particular(text_param("competenza", k)), days()}, 0,
                    false, pu);
        // Abilitazioni e autorizzazioni del committente.
        const Vehicle* v = s.role == ShiftRole::Driver ? sd.vehicle : s.role == ShiftRole::SecondDriver ? sd.aboard : nullptr;
        const bool vu = s.role == ShiftRole::Driver ? !sd.vehicle_verified : s.role == ShiftRole::SecondDriver && !sd.aboard_verified;
        if (v)
            if (const Norms* n = idx_.norms_of(s.company.code)) {
                const LicenceResult r = check_licences(p, *v, *n, sd.day);
                if (r.status == LicenceStatus::Missing)
                    add(Check::DriverWithoutLicence, where,
                        {shift_param(s), person_param(p), vehicle_param(*v), personal(text_param("alternative", alternatives(*r.requirement, *v))), days()},
                        0, false, pu || vu);
            }
        if (s.role == ShiftRole::Driver)
            for (const RequiredAuthorization& ra : required_authorizations(sd, idx_)) {
                const bool any = std::any_of(p.authorizations.begin(), p.authorizations.end(),
                                             [&ra](const ClientAuthorization& a) { return a.client.code == ra.contract->client.code; });
                if (!any)
                    add(Check::DriverWithoutLicence, where,
                        {shift_param(s), person_param(p), ref_param("committente", Element::Client, ra.contract->client.code),
                         ref_param("servizio", Element::Service, ra.service->code), days()},
                        1, true, pu || ra.required == Outcome::Unverified);   // K-014: la chiede il regolamento per tipo di servizio
            }
        preferences(sd, p, pu);
        pairing(sd, p, pu || !sd.vehicle_verified);
    }

    // Variante, punto dei dati e dettaglio (dato personale) di un fatto con le date che colpisce la persona.
    std::tuple<int, Source, Param> person_fact(const ShiftDay& sd) const {
        const Person& p = *sd.planned;
        switch (sd.person_impediment) {
            case PersonImpediment::Absent:
                return {0, sd.absence->src, personal(text_param("tipo", sd.absence->type.code))};
            case PersonImpediment::OutsideAvailability:
                return {1, Source{p.src.file, p.src.pointer + "/disponibilita", 0, 0},
                        personal(text_param("orari", text_from_minutes(sd.start) + "-" + text_from_minutes(sd.end)))};
            case PersonImpediment::LoanNotValid:
                return {2, sd.person_loan->src, ref_param("azienda", Element::Company, sd.shift->company.code)};
            case PersonImpediment::LicenceExpired:
                return {3, sd.licence->src, personal(text_param("tipo", sd.licence->type.code))};
            case PersonImpediment::AuthorizationExpired:
                return {4, sd.authorization->src, personal(text_param("tipo", sd.authorization->client.code))};
            case PersonImpediment::None: break;
        }
        return {0, p.src, text_param("tipo", "")};
    }

    // Le alternative di un requisito di guida, scritte con i codici: "D / D1".
    static std::string alternatives(const DrivingRequirement& r, const Vehicle&) {
        std::string s;
        for (const LicenceAlternative& a : r.alternatives) {
            std::string one;
            for (const Ref& l : a.licences) one += (one.empty() ? "" : "+") + l.code;
            s += (s.empty() ? "" : " / ") + one;
        }
        return s;
    }

    void preferences(const ShiftDay& sd, const Person& p, bool unverified) {
        const auto days = [&] { return days_param("giorni", sd.day); };   // solo se serve
        const int variant = sd.person_source == PersonSource::Day ? 1 : 0;
        for (const Preference& f : p.preferences) {
            if (!f.covers_date(sd.day)) continue;
            bool violated = false;
            switch (f.kind) {
                case PreferenceKind::FreeDays: violated = f.days.has(weekday_of(sd.day)); break;
                case PreferenceKind::TimeWindow: violated = sd.start < f.start || sd.end > f.end; break;
                case PreferenceKind::AvoidShifts:
                    violated = std::any_of(f.shifts.begin(), f.shifts.end(), [&sd](const Ref& r) { return r.code == sd.shift->code; });
                    break;
                case PreferenceKind::MaxBreak:
                    for (std::size_t i = 1; i < sd.items.size(); ++i)
                        violated = violated || sd.items[i].start - sd.items[i - 1].end > f.max_break_min;
                    break;
                case PreferenceKind::Other: break;
            }
            if (violated) add(Check::PreferenceNotRespected, f.src, {person_param(p), shift_param(*sd.shift), days()}, variant, false, unverified);
        }
    }

    // Abbinamento autista-mezzo dell'azienda del turno.
    void pairing(const ShiftDay& sd, const Person& p, bool unverified) {
        if (sd.shift->role != ShiftRole::Driver || !sd.vehicle) return;
        const CompanySettings* settings = idx_.settings(sd.shift->company.code);
        if (!settings || settings->pairing == DriverVehiclePairing::None) return;
        const Vehicle& v = *sd.vehicle;
        if (p.usual_vehicle && p.usual_vehicle->code == v.code) return;
        const auto days = [&] { return days_param("giorni", sd.day); };   // solo se serve
        std::vector<Param> params = {shift_param(*sd.shift), person_param(p), vehicle_param(v), days()};
        const int with_usual = p.usual_vehicle ? 0 : 1;
        if (settings->pairing == DriverVehiclePairing::Fixed) {
            const bool day = sd.person_source == PersonSource::Day || sd.vehicle_source == VehicleSource::Day;
            if (day) add(Check::PairingNotRespected, day_source(sd), params, 2 + with_usual, false, unverified);
            else add(Check::PairingFixedViolated, sd.person_position(), params, with_usual, false, unverified);
        } else if (p.usual_vehicle) {
            add(Check::PairingNotRespected, sd.person_position(), params, 0, false, unverified);
        }
    }

    static Source day_source(const ShiftDay& sd) {
        if (sd.person_source == PersonSource::Day) return sd.person_assignment->src;
        return sd.vehicle_assignment->src;
    }

    // Il mezzo del turno: da assegnare, fatti con le date, prestito, uso, vincolo di destinazione.
    void vehicle(const ShiftDay& sd) {
        const Shift& s = *sd.shift;
        if (s.role != ShiftRole::Driver) return;
        const auto days = [&] { return days_param("giorni", sd.day); };   // solo se serve
        if (s.vehicle_mode == ShiftVehicleMode::ToAssign && sd.vehicle_source == VehicleSource::None)
            add(Check::ShiftVehicleToAssign, s.vehicle_src, {shift_param(s), days()});
        if (s.vehicle_mode == ShiftVehicleMode::Usual && sd.vehicle_source == VehicleSource::Usual && sd.person && !sd.person->usual_vehicle)
            add(Check::ShiftVehicleInconsistent, sd.person_position(), {shift_param(s), person_param(*sd.person), days()}, 2, false,
                !sd.person_verified);
        if (!sd.planned_vehicle) return;
        const Vehicle& v = *sd.planned_vehicle;
        if (sd.vehicle_impediment != VehicleImpediment::None) {
            int variant = 0;
            Source where = v.src;
            Param detail = text_param("tipo", "");
            switch (sd.vehicle_impediment) {
                case VehicleImpediment::Stopped:
                    variant = 0;
                    where = sd.stop->src;
                    detail = text_param("tipo", sd.stop->type.code);
                    break;
                case VehicleImpediment::UseNotAuthorized:
                    variant = 1;
                    where = sd.use_authorization->src;
                    detail = text_param("tipo", sd.use_authorization->use.code);
                    break;
                case VehicleImpediment::LoanNotValid:
                    variant = 2;
                    where = sd.vehicle_loan->src;
                    detail = text_param("tipo", sd.shift->company.code);
                    break;
                case VehicleImpediment::None: break;
            }
            const bool nv = !sd.vehicle_fact_verified;   // K-017
            // Gli usi richiesti dipendono dal tipo di servizio (K-014).
            if (sd.vehicle_source == VehicleSource::Day)
                add(Check::DayAssignmentAgainstFact, sd.vehicle_assignment->src, {shift_param(s), vehicle_param(v), detail, date_param("data", sd.day)},
                    5 + variant, sd.vehicle_impediment == VehicleImpediment::UseNotAuthorized, nv);
            else
                add(Check::RecurringShiftVehicleFact, where, {shift_param(s), vehicle_param(v), detail, days()}, variant, false, nv);
        }
        if (!sd.vehicle) return;
        // K-017: se non e' verificato che il turno abbia il mezzo, non lo sono nemmeno i problemi che nascono dal mezzo.
        const bool vu = !sd.vehicle_verified;
        (vu ? maybe_vehicles_ : used_vehicles_).insert(v.code);
        const Source& where = sd.vehicle_position();
        vehicles_[&v].push_back({absolute(sd.day, sd.vehicle_start.value_or(sd.start)), absolute(sd.day, sd.vehicle_end.value_or(sd.end)),
                                 sd.day, &s, &where, vu});
        if (v.company.code != s.company.code &&
            std::none_of(v.loans.begin(), v.loans.end(), [&s](const Loan& l) { return l.company.code == s.company.code; }))
            add(Check::OtherCompanyWithoutLoan, where, {shift_param(s), vehicle_param(v), ref_param("azienda", Element::Company, s.company.code), days()},
                1, false, vu);
        const bool constrained = v.constraint && (!v.constraint->until || sd.day <= *v.constraint->until);
        std::set<Code> reported;
        for (const PlannedItem& it : sd.items) {
            if (!it.trip) continue;
            for (const Ref& sr : it.trip->services) {
                const Service* sv = idx_.service(sr.code);
                if (!sv || !reported.insert(sv->code).second) continue;
                // Uso dei mezzi: senza nessuna autorizzazione a un uso ammesso e' sempre sbagliato. Gli usi richiesti dipendono dal
                // tipo di servizio (K-014): per ogni tipo possibile (K-017).
                if (v.use) {
                    bool yes = false, no = false;
                    std::string uses;
                    for (const std::optional<std::vector<Code>>& allowed : plan_.uses_of(*sv)) {
                        if (!allowed) {
                            no = true;
                            continue;
                        }
                        const auto ok = [&allowed](const Code& u) { return std::find(allowed->begin(), allowed->end(), u) != allowed->end(); };
                        const bool authorized = std::any_of(v.use_authorizations.begin(), v.use_authorizations.end(),
                                                            [&ok](const UseAuthorization& a) { return ok(a.use.code); });
                        if (ok(v.use->code) || authorized) {
                            no = true;
                            continue;
                        }
                        if (!yes)
                            for (const Code& u : *allowed) uses += (uses.empty() ? "" : ", ") + u;
                        yes = true;
                    }
                    const Outcome o = outcome_of(yes, no);
                    if (o != Outcome::No)
                        add(Check::VehicleWithoutUseForService, where,
                            {shift_param(s), vehicle_param(v), text_param("uso", v.use->code), ref_param("servizio", Element::Service, sv->code),
                             text_param("usi", uses), days()},
                            0, true, vu || o == Outcome::Unverified);
                }
                if (constrained) {
                    const DestinationConstraint& c = *v.constraint;
                    const bool contract_ok = std::any_of(c.contracts.begin(), c.contracts.end(),
                                                         [sv](const Ref& r) { return r.code == sv->contract.code; });
                    // K-014: se il vincolo nomina tipi di servizio, l'esito dipende dal tipo: per ogni tipo possibile (K-017).
                    bool against = false, admitted = false;
                    for (const ServiceKind k : possible_kinds(sv->kind))
                        (std::find(c.kinds.begin(), c.kinds.end(), k) != c.kinds.end() ? admitted : against) = true;
                    const Outcome o = contract_ok ? Outcome::No : outcome_of(against, admitted);
                    if (o != Outcome::No)
                        add(Check::VehicleAgainstConstraint, where, {shift_param(s), vehicle_param(v), ref_param("servizio", Element::Service, sv->code), days()},
                            0, !c.kinds.empty(), vu || o == Outcome::Unverified);
                }
            }
        }
    }

    // Corse del giorno: senza turno, divise in pezzi con un buco, nello stesso pezzo di due turni.
    void trips(Day d) {
        if (p_.shifts.empty()) return;
        for (std::size_t i = 0; i < p_.trips.size(); ++i) {
            const Trip& t = p_.trips[i];
            if (!plan_.days().active(trip_days_[i], d) || t.passages.empty()) continue;
            const auto days = [&] { return days_param("giorni", d); };   // solo se serve
            if (pieces_[i].empty()) {
                add(Check::TripWithoutShift, t.src, {ref_param("corsa", Element::Trip, t.code), days()});
                continue;
            }
            auto v = pieces_[i];
            std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
                return std::tie(a.first->from, a.first->to) < std::tie(b.first->from, b.first->to);
            });
            const std::size_t last = t.passages.size() - 1;
            if (v.front().first->from > 0)
                add(Check::TripPartsGap, v.front().first->item->src,
                    {ref_param("corsa", Element::Trip, t.code), ref_param("da", Element::Place, t.passages[0].place.code),
                     ref_param("a", Element::Place, t.passages[v.front().first->from].place.code), days()});
            std::size_t covered = v.front().first->to;
            for (std::size_t i = 1; i < v.size(); ++i) {
                const PlannedItem& cur = *v[i].first;
                if (cur.from < covered)
                    add(Check::TripInTwoShifts, cur.item->src,
                        {ref_param("corsa", Element::Trip, t.code), shift_param(*v[i].second->shift),
                         ref_param("altro", Element::Shift, previous_owner(v, i, cur.from)->shift->code), days()});
                else if (cur.from > covered)
                    add(Check::TripPartsGap, cur.item->src,
                        {ref_param("corsa", Element::Trip, t.code), ref_param("da", Element::Place, t.passages[covered].place.code),
                         ref_param("a", Element::Place, t.passages[cur.from].place.code), days()});
                covered = std::max(covered, cur.to);
            }
            if (covered < last)
                add(Check::TripPartsGap, v.back().first->item->src,
                    {ref_param("corsa", Element::Trip, t.code), ref_param("da", Element::Place, t.passages[covered].place.code),
                     ref_param("a", Element::Place, t.passages[last].place.code), days()});
        }
    }

    // Il turno che copriva gia' la parte della corsa da cui parte il pezzo i.
    static const ShiftDay* previous_owner(const std::vector<std::pair<const PlannedItem*, const ShiftDay*>>& v, std::size_t i,
                                          std::size_t from) {
        for (std::size_t k = 0; k < i; ++k)
            if (v[k].first->from <= from && from < v[k].first->to) return v[k].second;
        return v.front().second;
    }

    // Assistente a bordo: chi lo fornisce secondo il turno e il contratto; persona dell'azienda assegnata.
    void escort_checks(const std::vector<ShiftDay>& plan) {
        for (const ShiftDay& sd : plan) {
            if (sd.shift->role != ShiftRole::Driver) continue;
            const auto days = [&] { return days_param("giorni", sd.day); };   // solo se serve
            for (const PlannedItem& it : sd.items) {
                if (!it.trip || it.trip->services.empty()) continue;
                const Service* sv = idx_.service(it.trip->services.front().code);
                const Contract* c = sv ? idx_.contract(sv->contract.code) : nullptr;
                if (!c) continue;
                const bool aboard = it.item->trip.escort.has_value() || assistant_aboard_[trip_index(it.trip)] != 0;
                const EscortProvider provider = it.item->trip.escort.value_or(c->escort_provider);
                const Param trip = ref_param("corsa", Element::Trip, it.trip->code);
                const Param contract = ref_param("contratto", Element::Contract, c->code);
                if (it.item->trip.escort && *it.item->trip.escort != c->escort_provider)
                    add(Check::EscortProviderDiffers, it.item->src, {shift_param(*sd.shift), trip, contract, days()},
                        *it.item->trip.escort == EscortProvider::Company ? 0 : 1);
                const std::vector<const ShiftDay*>& e = escorts_[trip_index(it.trip)];
                // K-017: una persona non verificata puo' non esserci.
                const bool company_escort =
                    std::any_of(e.begin(), e.end(), [](const ShiftDay* x) { return x->person != nullptr && x->person_verified; });
                const bool maybe_escort = std::any_of(e.begin(), e.end(), [](const ShiftDay* x) { return x->person != nullptr; });
                if (aboard && provider == EscortProvider::Company && !company_escort)
                    add(Check::EscortWithoutPerson, it.item->src, {shift_param(*sd.shift), trip, days()}, 0, false, maybe_escort);
                if (!e.empty() && provider == EscortProvider::Client && !it.item->trip.escort)
                    add(Check::EscortProviderDiffers, e.front()->shift->src,
                        {ref_param("turno", Element::Shift, e.front()->shift->code), trip, contract, days()}, 2);
            }
        }
    }

    // Corse bloccate su un mezzo e fatte con un altro.
    void blocks(Day d) {
        const auto days = [&] { return days_param("giorni", d); };   // solo se serve
        for (const Block& b : p_.blocks) {
            if (!b.valid_on(d)) continue;
            for (const Ref& tr : b.trips) {
                const Trip* t = idx_.trip(tr.code);
                if (!t) continue;
                for (const auto& [item, sd] : pieces_[trip_index(t)])
                    if (sd->vehicle && sd->vehicle->code != b.vehicle.code)
                        add(Check::BlockedTripOtherVehicle, sd->vehicle_position(),
                            {ref_param("corsa", Element::Trip, t->code), ref_param("blocco", Element::Block, b.code),
                             vehicle_param_code(b.vehicle.code), ref_param("altro", Element::Vehicle, sd->vehicle->code),
                             shift_param(*sd->shift), days()},
                            0, false, !sd->vehicle_verified);
            }
        }
    }

    // Persone o mezzi in due turni che si sovrappongono, anche a cavallo della mezzanotte (un orario arriva al massimo a
    // 47:59: il turno sovrapposto e' dello stesso giorno o del giorno prima). Il messaggio non scrive la data dell'altro
    // turno: e' il giorno prima di ogni giorno elencato, e cosi' lo stesso problema di molti giorni si raggruppa.
    // L'elenco e' per persona o per mezzo (in ordine di posizione nel pacchetto).
    // K-017: un turno non verificato puo' non esserci e cambiare il turno "piu' lungo" con cui si confrontano i successivi. Un
    // problema e' certo solo se viene uguale con tutti i turni e con i soli turni certi (allora e' uguale con qualunque parte di
    // quelli non verificati); gli altri non sono verificati.
    template <class T>
    void overlaps(std::map<const T*, std::vector<Interval>>& all, Check c, Element e) {
        using Pair = std::pair<const Interval*, const Interval*>;
        for (auto& entry : all) {
            const Code& code = entry.first->code;
            std::vector<Interval>& v = entry.second;
            std::sort(v.begin(), v.end(), [](const Interval& a, const Interval& b) { return std::tie(a.a, a.b) < std::tie(b.a, b.b); });
            const auto pairs = [&v](bool only_certain) {
                std::vector<Pair> out;
                const Interval* longest = nullptr;
                for (const Interval& x : v) {
                    if (only_certain && x.unverified) continue;
                    if (longest && x.a < longest->b) out.push_back({&x, longest});
                    if (!longest || x.b > longest->b) longest = &x;
                }
                return out;
            };
            const std::vector<Pair> every = pairs(false);
            const bool some_unverified = std::any_of(v.begin(), v.end(), [](const Interval& x) { return x.unverified; });
            const std::vector<Pair> certain = some_unverified ? pairs(true) : every;
            const auto in = [](const std::vector<Pair>& w, const Pair& p) { return std::find(w.begin(), w.end(), p) != w.end(); };
            const auto report = [&](const Pair& p, bool unverified) {
                add(c, *p.first->where,
                    {ref_param(e == Element::Person ? "persona" : "mezzo", e, code), shift_param(*p.first->shift),
                     ref_param("altro", Element::Shift, p.second->shift->code), days_param("giorni", p.first->day)},
                    p.second->day == p.first->day ? 0 : 1, false, unverified);
            };
            for (const Pair& p : every) report(p, !in(certain, p));
            for (const Pair& p : certain)
                if (!in(every, p)) report(p, true);
        }
    }

    // Persone senza turni e mezzi mai usati (in un pacchetto che ha turni). Le persone e i mezzi ipotetici e i mezzi
    // di scorta non contano.
    // K-017: con un turno non verificato e nessuno certo, non e' verificato nemmeno che la persona o il mezzo non lavorino.
    void never_used() {
        for (const Person& x : p_.persons)
            if (!x.hypothetical && !used_persons_.count(x.code))
                add(Check::PersonWithoutShifts, x.src, {person_param(x)}, 0, false, maybe_persons_.count(x.code) > 0);
        for (const Vehicle& v : p_.vehicles)
            if (!v.hypothetical && !v.spare && !used_vehicles_.count(v.code))
                add(Check::VehicleNeverUsed, v.src, {vehicle_param(v)}, 0, false, maybe_vehicles_.count(v.code) > 0);
    }

    std::size_t trip_index(const Trip* t) const { return static_cast<std::size_t>(t - p_.trips.data()); }

    const Package& p_;
    std::vector<Problem>& out_;
    PlanCollector* collector_ = nullptr;
    Planner plan_;
    const Index& idx_;
    Pieces pieces_;
    std::vector<std::vector<const ShiftDay*>> escorts_;
    std::vector<std::size_t> touched_;
    std::vector<const std::vector<char>*> trip_days_;
    std::vector<char> assistant_aboard_;
    std::map<const Person*, std::vector<Interval>> persons_;
    std::map<const Vehicle*, std::vector<Interval>> vehicles_;
    std::set<Code> used_persons_, used_vehicles_;     // in un turno certo
    std::set<Code> maybe_persons_, maybe_vehicles_;   // K-017: in un turno non verificato
};

}  // namespace

void check_shifts(const Package& p, std::vector<Problem>& out, PlanCollector* collector) { ShiftChecker(p, out, collector).run(); }

}  // namespace kzero
