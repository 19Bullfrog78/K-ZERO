// K-Zero - turni del giorno: chi fa ogni turno in una data (vedi turni.hpp).
#include "turni.hpp"

#include <algorithm>
#include <set>
#include <tuple>

namespace kzero {

const Source& ShiftDay::person_position() const {
    switch (person_source) {
        case PersonSource::Day: return person_assignment->src;
        case PersonSource::Fixed: return fixed->src;
        case PersonSource::Rotation: return rotation.rotation->rows[rotation.row].src;   // la riga della persona
        case PersonSource::None: break;
    }
    return shift->src;
}

const Source& ShiftDay::vehicle_position() const {
    switch (vehicle_source) {
        case VehicleSource::Day: return vehicle_assignment->src;
        case VehicleSource::Shift:
        case VehicleSource::Usual: return shift->vehicle_src;
        case VehicleSource::None: break;
    }
    return shift->src;
}

double loaded_km(const ShiftDay& sd) {
    double t = 0;
    for (const PlannedItem& it : sd.items) t += it.loaded_km;
    return t;
}

double empty_km(const ShiftDay& sd) {
    double t = 0;
    for (const PlannedItem& it : sd.items) t += it.empty_km;
    return t;
}

Minutes idle_minutes(const ShiftDay& sd) {
    Minutes t = 0;
    for (std::size_t i = 1; i < sd.items.size(); ++i) t += std::max(0, sd.items[i].start - sd.items[i - 1].end);
    return t;
}

const Code& end_place(const PlannedItem& it) {
    if (it.trip) return it.trip->passages[it.to].place.code;
    const ShiftActivity& a = it.item->activity;
    return a.to_place ? a.to_place->code : a.place.code;
}

const Code& start_place(const PlannedItem& it) {
    if (it.trip) return it.trip->passages[it.from].place.code;
    return it.item->activity.place.code;
}

// --- Pezzi di corsa ----------------------------------------------------------------------------------------------

namespace {

// Passaggio della corsa che corrisponde al punto: indice, oppure il problema.
PartProblem find_point(const TripPoint& p, const Trip& t, std::size_t& index) {
    std::size_t found = 0;
    for (std::size_t i = 0; i < t.passages.size(); ++i) {
        const Passage& ps = t.passages[i];
        if (ps.place.code != p.place.code) continue;
        if (p.time && ps.time != *p.time) continue;
        if (found++ == 0) index = i;
    }
    if (found == 0) return PartProblem::PlaceNotOnTrip;
    if (found > 1) return PartProblem::Ambiguous;
    return PartProblem::None;
}

}  // namespace

PartResolution resolve_part(const ShiftTrip& part, const Trip& trip) {
    PartResolution r;
    if (trip.passages.empty()) {
        r.problem = PartProblem::PlaceNotOnTrip;
        r.bad = part.from ? &*part.from : part.to ? &*part.to : nullptr;
        return r;
    }
    r.from = 0;
    r.to = trip.passages.size() - 1;
    if (part.from) {
        r.problem = find_point(*part.from, trip, r.from);
        if (r.problem != PartProblem::None) {
            r.bad = &*part.from;
            return r;
        }
    }
    if (part.to) {
        r.problem = find_point(*part.to, trip, r.to);
        if (r.problem != PartProblem::None) {
            r.bad = &*part.to;
            return r;
        }
    }
    if ((part.from || part.to) && r.from >= r.to) {
        r.problem = PartProblem::Reversed;
        r.bad = part.to ? &*part.to : &*part.from;
    }
    return r;
}

// --- Usi dei mezzi, requisiti di guida, autorizzazioni ----------------------------------------------------------

std::optional<std::vector<Code>> allowed_uses(const Service& s, ServiceKind kind, const Index& idx) {
    const auto codes = [](const std::vector<Ref>& refs) {
        std::vector<Code> v;
        for (const Ref& r : refs) v.push_back(r.code);
        return v;
    };
    const auto has_kind = [kind](const std::vector<ServiceKind>& kinds) {
        return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
    };
    if (const Regulation* r = idx.regulation_of(s); r && r->vehicle_uses)
        for (const ServiceUses& u : r->vehicle_uses->values)
            if (has_kind(u.kinds)) return codes(u.uses);
    const Contract* c = idx.contract(s.contract.code);
    if (const Norms* n = c ? idx.norms_of(c->company.code) : nullptr)
        for (const ServiceUsesNorm& u : n->service_uses)
            if (has_kind(u.value.kinds)) return codes(u.value.uses);
    return std::nullopt;
}

UsesByKind allowed_uses_by_kind(const Service& s, const Index& idx) {
    UsesByKind v;
    for (const ServiceKind k : possible_kinds(s.kind)) v.push_back(allowed_uses(s, k, idx));
    return v;
}

KindRequirements kind_requirements(const Package& p) {
    KindRequirements k;
    const auto with_kinds = [](const std::optional<ConditionsRule>& r) {
        return r && std::any_of(r->conditions.begin(), r->conditions.end(),
                                [](const PassengerCondition& c) { return !c.service_kinds.empty(); });
    };
    for (const Regulation& r : p.regulations) {
        if (r.vehicle_uses && !r.vehicle_uses->values.empty()) k.vehicle_uses.push_back(&r);
        if (r.client_authorization && !r.client_authorization->kinds.empty()) k.client_authorization.push_back(&r);
        if (with_kinds(r.assistant) || with_kinds(r.adult_at_dropoff)) k.conditions.push_back(&r);
    }
    for (const Norms& n : p.norms)
        if (!n.service_uses.empty()) k.service_uses.push_back(&n);
    for (const Vehicle& v : p.vehicles)
        if (v.constraint && !v.constraint->kinds.empty()) k.constraints.push_back(&v);
    return k;
}

namespace {

bool has_licence(const Person& p, const Code& type) {
    for (const Licence& l : p.licences)
        if (l.type.code == type) return true;
    return false;
}

bool valid_licence(const Person& p, const Code& type, Day d) {
    for (const Licence& l : p.licences)
        if (l.type.code == type && l.valid_on(d)) return true;
    return false;
}

const Licence* licence_of(const Person& p, const Code& type) {
    for (const Licence& l : p.licences)
        if (l.type.code == type) return &l;
    return nullptr;
}

bool requirement_matches(const DrivingRequirement& r, const Vehicle& v) {
    if (std::find(r.categories.begin(), r.categories.end(), v.category) == r.categories.end()) return false;
    if (r.min_seats && v.seats < *r.min_seats) return false;
    if (r.max_seats && v.seats > *r.max_seats) return false;
    if (r.trailer && *r.trailer != v.trailer) return false;
    if (!r.uses.empty()) {
        // L'uso della carta di circolazione o un uso autorizzato (distrazione), con qualunque data: prudenza.
        bool any = false;
        for (const Ref& u : r.uses) {
            any = any || (v.use && v.use->code == u.code);
            for (const UseAuthorization& a : v.use_authorizations) any = any || a.use.code == u.code;
        }
        if (!any) return false;
    }
    return true;
}

bool alternative_applies(const LicenceAlternative& a, const Vehicle& v) {
    if (a.max_seats && v.seats > *a.max_seats) return false;
    if (a.max_length_m && (!v.dims.length_m || *v.dims.length_m > *a.max_length_m)) return false;
    return true;
}

}  // namespace

LicenceResult check_licences(const Person& p, const Vehicle& v, const Norms& n, Day d) {
    LicenceResult expired;
    for (const DrivingRequirement& r : n.requirements) {
        if (!requirement_matches(r, v)) continue;
        const LicenceAlternative* static_ok = nullptr;
        bool day_ok = false;
        for (const LicenceAlternative& a : r.alternatives) {
            if (!alternative_applies(a, v)) continue;
            bool all = !a.licences.empty(), all_valid = !a.licences.empty();
            for (const Ref& l : a.licences) {
                all = all && has_licence(p, l.code);
                all_valid = all_valid && valid_licence(p, l.code, d);
            }
            if (all && !static_ok) static_ok = &a;
            day_ok = day_ok || all_valid;
        }
        if (!static_ok) return {LicenceStatus::Missing, &r, nullptr};
        if (!day_ok && expired.status == LicenceStatus::Ok) {
            expired.status = LicenceStatus::Expired;
            expired.requirement = &r;
            for (const Ref& l : static_ok->licences)
                if (!valid_licence(p, l.code, d)) {
                    expired.expired = licence_of(p, l.code);
                    break;
                }
        }
    }
    return expired;
}

std::vector<RequiredAuthorization> required_authorizations(const ShiftDay& sd, const Index& idx) {
    std::vector<RequiredAuthorization> out;
    std::map<Code, std::size_t> clients;
    for (const PlannedItem& it : sd.items) {
        if (!it.trip) continue;
        for (const Ref& sr : it.trip->services) {
            const Service* s = idx.service(sr.code);
            const Contract* c = s ? idx.contract(s->contract.code) : nullptr;
            const Regulation* r = c ? idx.regulation(c->regulation.code) : nullptr;
            if (!r || !r->client_authorization) continue;
            // K-017: per ogni tipo possibile del servizio (uno solo se noto).
            const auto& kinds = r->client_authorization->kinds;
            bool yes = false, no = false;
            for (const ServiceKind k : possible_kinds(s->kind)) (std::find(kinds.begin(), kinds.end(), k) != kinds.end() ? yes : no) = true;
            const Outcome required = outcome_of(yes, no);
            if (required == Outcome::No) continue;
            const auto [pos, inserted] = clients.emplace(c->client.code, out.size());
            if (inserted) out.push_back({c, s, required});
            else if (out[pos->second].required == Outcome::Unverified && required == Outcome::Yes) out[pos->second] = {c, s, required};
        }
    }
    return out;
}

// --- Il piano di un giorno ---------------------------------------------------------------------------------------

Planner::Planner(const Package& p) : p_(p), idx_(p), days_(idx_, p.manifest.period) {
    for (const Service& s : p.services) service_uses_.emplace(&s, allowed_uses_by_kind(s, idx_));
    // Rotazioni: turni di ogni passo e persona di ogni riga cercati una volta sola (un turno doppio vale il primo, come
    // nell'indice; un turno che non esiste si salta: K-CTR-010).
    std::unordered_map<Code, std::size_t> shift_pos;
    for (std::size_t i = 0; i < p.shifts.size(); ++i) shift_pos.emplace(p.shifts[i].code, i);
    for (const Rotation& r : p.rotations) {
        std::vector<RotationRowPlan> rows;
        for (const RotationRow& row : r.rows) {
            RotationRowPlan rp;
            rp.person = idx_.person(row.person.code);
            for (const RotationStep& st : row.steps) {
                std::vector<std::size_t> shifts;
                if (!st.rest)
                    for (const Ref& sh : st.shifts) {
                        const auto it = shift_pos.find(sh.code);
                        if (it != shift_pos.end()) shifts.push_back(it->second);
                    }
                rp.steps.push_back(std::move(shifts));
            }
            rows.push_back(std::move(rp));
        }
        rotation_plan_.push_back(std::move(rows));
    }
    for (const Shift& s : p.shifts)
        for (const ShiftItem& item : s.items) {
            if (!item.is_trip) continue;
            TripItem ti;
            ti.trip = idx_.trip(item.trip.trip.code);
            if (ti.trip) {
                ti.days = days_.days_of(ti.trip->calendar.code);
                ti.part = resolve_part(item.trip, *ti.trip);
            }
            trip_items_.emplace(&item, ti);
        }
    for (const DayAssignment& a : p.day_assignments) day_assignments_[{a.date, a.shift.code}].push_back(&a);
    for (const FixedAssignment& f : p.fixed) fixed_[f.shift.code].push_back(&f);
    for (const Absence& a : p.absences) absences_[a.person.code].push_back(&a);
    for (const VehicleStop& s : p.stops) stops_[s.vehicle.code].push_back(&s);
}

namespace {

// Un fatto che vale nei giorni [from, to], ogni giorno dalle start alle end (senza orari: tutto il giorno), si
// sovrappone all'intervallo assoluto [a, b) del turno? Si guardano anche il giorno prima e quello dopo (orari oltre
// le 24:00 e turni a cavallo della mezzanotte).
bool overlaps(DateRange days, const std::optional<Minutes>& start, const std::optional<Minutes>& end, Day d, long long a,
              long long b) {
    for (Day k = d - 1; k <= d + 1; ++k) {
        if (k < days.from || k > days.to) continue;
        const long long s = absolute(k, start.value_or(0));
        const long long e = absolute(k, end.value_or(1440));
        if (s < b && a < e) return true;
    }
    return false;
}

// La persona e' disponibile per tutto il turno? Se per quel giorno vale almeno una riga di disponibilita', il turno
// deve stare dentro una fascia di quel giorno della settimana; senza righe per quel giorno, nessun limite.
bool available(const Person& p, Day d, Minutes start, Minutes end) {
    bool declared = false;
    for (const Availability& a : p.availability) {
        if (!a.covers_date(d)) continue;
        declared = true;
        if (a.days.has(weekday_of(d)) && start >= a.start && end <= a.end) return true;
    }
    return !declared;
}

}  // namespace

void Planner::resolve_person(ShiftDay& sd) const {
    const Code& code = sd.shift->code;
    const auto da = day_assignments_.find({sd.day, code});
    if (da != day_assignments_.end()) sd.day_candidates = da->second;
    for (const DayAssignment* a : sd.day_candidates)
        if (a->person) {
            sd.planned = idx_.person(a->person->code);
            sd.person_source = PersonSource::Day;
            sd.person_assignment = a;
            break;
        }
    const auto fx = fixed_.find(code);
    if (fx != fixed_.end())
        for (const FixedAssignment* f : fx->second)
            if (days_.active(f->calendar.code, sd.day)) sd.fixed_candidates.push_back(f);
    if (sd.person_source == PersonSource::None && !sd.fixed_candidates.empty()) {
        sd.planned = idx_.person(sd.fixed_candidates.front()->person.code);
        sd.person_source = PersonSource::Fixed;
        sd.fixed = sd.fixed_candidates.front();
    }
    if (sd.person_source == PersonSource::None && !sd.rotation_candidates.empty()) {
        sd.rotation = sd.rotation_candidates.front();
        sd.planned = sd.rotation.person;
        sd.person_source = PersonSource::Rotation;
    }
    sd.person = sd.planned;
}

// Fatti con le date sulla persona prevista che non dipendono dal mezzo ne' dal tipo di servizio: assenza, disponibilita',
// prestito. Sempre certi.
void Planner::person_facts(ShiftDay& sd) const {
    const Person* p = sd.planned;
    if (!p) return;
    const long long a = absolute(sd.day, sd.start), b = absolute(sd.day, sd.end);
    const auto ab = absences_.find(p->code);
    if (ab != absences_.end())
        for (const Absence* x : ab->second)
            if (overlaps(x->days, x->start, x->end, sd.day, a, b)) {
                sd.person_impediment = PersonImpediment::Absent;
                sd.absence = x;
                break;
            }
    if (sd.person_impediment == PersonImpediment::None && !available(*p, sd.day, sd.start, sd.end))
        sd.person_impediment = PersonImpediment::OutsideAvailability;
    if (sd.person_impediment == PersonImpediment::None && p->company.code != sd.shift->company.code) {
        const Loan* any = nullptr;
        bool covered = false;
        for (const Loan& l : p->loans)
            if (l.company.code == sd.shift->company.code) {
                if (!any) any = &l;
                covered = covered || l.covers(sd.day);
            }
        if (any && !covered) {   // senza nessun prestito a quell'azienda e' un errore (K-CTR-073), non un fatto
            sd.person_impediment = PersonImpediment::LoanNotValid;
            sd.person_loan = any;
        }
    }
    // Un fatto con le date colpisce un turno ricorrente: quel giorno il turno e' scoperto (avviso). Su
    // un'assegnazione del singolo giorno la persona resta: e' un errore (K-CTR-067).
    if (sd.person_impediment != PersonImpediment::None && sd.person_source != PersonSource::Day) sd.person = nullptr;
}

// Fatti con le date sull'autista che dipendono dal mezzo del turno: abilitazione scaduta per quel mezzo, autorizzazione del
// committente scaduta (chiesta per tipo di servizio). Ci sono solo se il mezzo c'e': con il mezzo non verificato (K-017) anche
// il fatto non e' verificato; l'autorizzazione chiesta solo per alcuni tipi possibili non e' verificata.
void Planner::person_vehicle_facts(ShiftDay& sd) const {
    const Person* p = sd.planned;
    if (!p || sd.person_impediment != PersonImpediment::None || sd.shift->role != ShiftRole::Driver || !sd.vehicle) return;
    bool certain = true;
    if (const Norms* n = idx_.norms_of(sd.shift->company.code)) {
        const LicenceResult r = check_licences(*p, *sd.vehicle, *n, sd.day);
        if (r.status == LicenceStatus::Expired) {
            sd.person_impediment = PersonImpediment::LicenceExpired;
            sd.licence = r.expired;
        }
    }
    if (sd.person_impediment == PersonImpediment::None) {
        const ClientAuthorization* possible = nullptr;   // scaduta, per un'autorizzazione chiesta solo per alcuni tipi
        for (const RequiredAuthorization& ra : required_authorizations(sd, idx_)) {
            const ClientAuthorization* any = nullptr;
            bool valid = false;
            for (const ClientAuthorization& x : p->authorizations)
                if (x.client.code == ra.contract->client.code) {
                    if (!any) any = &x;
                    valid = valid || x.valid_on(sd.day);
                }
            if (!any || valid) continue;   // senza nessuna autorizzazione e' un errore (K-CTR-068), non un fatto
            if (ra.required == Outcome::Yes) {
                sd.person_impediment = PersonImpediment::AuthorizationExpired;
                sd.authorization = any;
                break;
            }
            if (!possible) possible = any;
        }
        if (sd.person_impediment == PersonImpediment::None && possible) {
            sd.person_impediment = PersonImpediment::AuthorizationExpired;
            sd.authorization = possible;
            certain = false;
        }
    }
    if (sd.person_impediment == PersonImpediment::None) return;
    certain = certain && sd.vehicle_verified;
    sd.person_fact_verified = certain;
    if (sd.person_source == PersonSource::Day) return;   // la persona resta: e' un errore (K-CTR-067)
    if (certain) sd.person = nullptr;
    else sd.person_verified = false;   // tiene la persona prevista
}

void Planner::resolve_vehicle(ShiftDay& sd) const {
    if (sd.shift->role != ShiftRole::Driver) return;
    for (const DayAssignment* a : sd.day_candidates)
        if (a->vehicle) {
            sd.planned_vehicle = idx_.vehicle(a->vehicle->code);
            sd.vehicle_source = VehicleSource::Day;
            sd.vehicle_assignment = a;
            break;
        }
    if (sd.vehicle_source == VehicleSource::None) {
        switch (sd.shift->vehicle_mode) {
            case ShiftVehicleMode::Specific:
                sd.vehicle_source = VehicleSource::Shift;
                if (sd.shift->vehicle) sd.planned_vehicle = idx_.vehicle(sd.shift->vehicle->code);
                break;
            case ShiftVehicleMode::Usual:
                sd.vehicle_source = VehicleSource::Usual;
                if (sd.person && sd.person->usual_vehicle) sd.planned_vehicle = idx_.vehicle(sd.person->usual_vehicle->code);
                break;
            case ShiftVehicleMode::ToAssign:
            case ShiftVehicleMode::None: break;
        }
    }
    sd.vehicle = sd.planned_vehicle;
}

void Planner::vehicle_facts(ShiftDay& sd) const {
    const Vehicle* v = sd.planned_vehicle;
    if (!v) return;
    const long long a = absolute(sd.day, sd.vehicle_start.value_or(sd.start));
    const long long b = absolute(sd.day, sd.vehicle_end.value_or(sd.end));
    const auto st = stops_.find(v->code);
    if (st != stops_.end())
        for (const VehicleStop* s : st->second)
            if (overlaps(s->days, s->start, s->end, sd.day, a, b)) {
                sd.vehicle_impediment = VehicleImpediment::Stopped;
                sd.stop = s;
                break;
            }
    if (sd.vehicle_impediment == VehicleImpediment::None && v->company.code != sd.shift->company.code) {
        const Loan* any = nullptr;
        bool covered = false;
        for (const Loan& l : v->loans)
            if (l.company.code == sd.shift->company.code) {
                if (!any) any = &l;
                covered = covered || l.covers(sd.day);
            }
        if (any && !covered) {
            sd.vehicle_impediment = VehicleImpediment::LoanNotValid;
            sd.vehicle_loan = any;
        }
    }
    // L'uso del mezzo che chiede il servizio dipende dal tipo di servizio (K-014): per ogni tipo possibile (K-017). Il primo
    // servizio per cui il mezzo non va con certezza decide; se nessuno e' certo, il primo per cui non va con alcuni tipi lo rende
    // non verificato.
    const UseAuthorization* possible = nullptr;
    if (sd.vehicle_impediment == VehicleImpediment::None && v->use) {
        for (const PlannedItem& it : sd.items) {
            if (!it.trip || sd.vehicle_impediment != VehicleImpediment::None) continue;
            for (const Ref& sr : it.trip->services) {
                const Service* s = idx_.service(sr.code);
                if (!s) continue;
                bool yes = false, no = false;
                const UseAuthorization* failing = nullptr;
                for (const std::optional<std::vector<Code>>& allowed : uses_of(*s)) {
                    const auto ok = [&allowed](const Code& u) { return std::find(allowed->begin(), allowed->end(), u) != allowed->end(); };
                    const UseAuthorization* any = nullptr;
                    bool valid = false;
                    if (allowed && !ok(v->use->code))
                        for (const UseAuthorization& x : v->use_authorizations)
                            if (ok(x.use.code)) {
                                if (!any) any = &x;
                                valid = valid || (sd.day >= x.period.from && sd.day <= x.period.to);
                            }
                    // Senza nessuna autorizzazione e' un errore (K-CTR-069), non un fatto.
                    if (any && !valid) {
                        yes = true;
                        if (!failing) failing = any;
                    } else {
                        no = true;
                    }
                }
                const Outcome o = outcome_of(yes, no);
                if (o == Outcome::Yes) {
                    sd.vehicle_impediment = VehicleImpediment::UseNotAuthorized;
                    sd.use_authorization = failing;
                    break;
                }
                if (o == Outcome::Unverified && !possible) possible = failing;
            }
        }
        if (sd.vehicle_impediment == VehicleImpediment::None && possible) {
            sd.vehicle_impediment = VehicleImpediment::UseNotAuthorized;
            sd.use_authorization = possible;
            sd.vehicle_fact_verified = false;
        }
    }
    if (sd.vehicle_impediment == VehicleImpediment::None || sd.vehicle_source == VehicleSource::Day) return;
    // Su un turno ricorrente il mezzo non c'e' quel giorno; con il fatto non verificato il turno tiene il mezzo previsto.
    if (sd.vehicle_fact_verified) sd.vehicle = nullptr;
    else sd.vehicle_verified = false;
}

void Planner::aboard(ShiftDay& sd, const std::vector<ShiftDay>& day) const {
    // Il mezzo e' quello del primo turno da autista (nell'ordine dei turni) che fa un pezzo delle stesse corse, per la prima corsa
    // del turno che ne ha uno. K-017: un mezzo non verificato puo' non esserci, e allora sarebbe quello dopo: si raccolgono i
    // mezzi possibili fino al primo certo; tutti uguali = certo.
    std::vector<const Vehicle*> possible;
    bool certain = false;
    for (const PlannedItem& it : sd.items) {
        if (!it.trip || certain) continue;
        for (const ShiftDay& other : day) {
            if (other.shift->role != ShiftRole::Driver || !other.vehicle) continue;
            const bool shares = std::any_of(other.items.begin(), other.items.end(),
                                            [&it](const PlannedItem& o) { return o.trip == it.trip && o.from < it.to && it.from < o.to; });
            if (!shares) continue;
            possible.push_back(other.vehicle);
            if (other.vehicle_verified) {
                certain = true;
                break;
            }
        }
    }
    if (!certain) possible.push_back(nullptr);   // nessun mezzo, se quelli non verificati non ci sono
    sd.aboard = possible.front();
    sd.aboard_verified = std::all_of(possible.begin(), possible.end(), [&possible](const Vehicle* v) { return v == possible.front(); });
    // Il secondo autista deve avere le abilitazioni per quel mezzo (per ognuno dei mezzi possibili).
    if (sd.shift->role != ShiftRole::SecondDriver || !sd.person || sd.person_impediment != PersonImpediment::None) return;
    const Norms* n = idx_.norms_of(sd.shift->company.code);
    if (!n) return;
    bool yes = false, no = false;
    const Licence* expired = nullptr;
    for (const Vehicle* v : possible) {
        const LicenceResult r = v ? check_licences(*sd.person, *v, *n, sd.day) : LicenceResult{};
        if (r.status == LicenceStatus::Expired) {
            yes = true;
            if (!expired) expired = r.expired;
        } else {
            no = true;
        }
    }
    const Outcome o = outcome_of(yes, no);
    if (o == Outcome::No) return;
    sd.person_impediment = PersonImpediment::LicenceExpired;
    sd.licence = expired;
    sd.person_fact_verified = o == Outcome::Yes;
    if (sd.person_source == PersonSource::Day) return;
    if (o == Outcome::Yes) sd.person = nullptr;
    else sd.person_verified = false;
}

bool Planner::item_on(const ShiftItem& item, Day d) const {
    if (!item.is_trip) return !item.activity.calendar || days_.active(item.activity.calendar->code, d);
    const auto it = trip_items_.find(&item);
    if (it == trip_items_.end()) {   // elemento che non viene dai turni del pacchetto
        const Trip* t = idx_.trip(item.trip.trip.code);
        return t && days_.active(t->calendar.code, d) && resolve_part(item.trip, *t).problem == PartProblem::None;
    }
    const TripItem& ti = it->second;
    return ti.trip && days_.active(ti.days, d) && ti.part.problem == PartProblem::None;
}

const UsesByKind& Planner::uses_of(const Service& s) const {
    const auto it = service_uses_.find(&s);
    if (it != service_uses_.end()) return it->second;
    static const UsesByKind nessuna_regola;   // servizio che non e' nel pacchetto
    return nessuna_regola;
}

int Planner::shift_days(const Shift& s) const {
    int n = 0;
    const DateRange& period = days_.period();
    for (Day d = period.from; d <= period.to; ++d)
        if (days_.active(s.calendar.code, d) &&
            std::any_of(s.items.begin(), s.items.end(), [&](const ShiftItem& it) { return item_on(it, d); }))
            ++n;
    return n;
}

std::vector<ShiftDay> Planner::day(Day d) const {
    std::vector<ShiftDay> out;
    if (!days_.in_period(d)) return out;

    // Candidati delle rotazioni valide quel giorno, turno per turno (una sola passata sulle righe).
    std::vector<std::vector<RotationCandidate>> rotation_candidates(p_.shifts.size());
    for (std::size_t ri = 0; ri < p_.rotations.size(); ++ri) {
        const Rotation& r = p_.rotations[ri];
        if (!r.valid_on(d, p_.manifest.period.to)) continue;
        const int step = r.step_index(d);
        for (std::size_t i = 0; i < r.rows.size(); ++i) {
            const RotationRowPlan& row = rotation_plan_[ri][i];
            if (static_cast<std::size_t>(step) >= row.steps.size()) continue;   // riga piu' corta del ciclo (K-CTR-057)
            for (const std::size_t s : row.steps[static_cast<std::size_t>(step)]) rotation_candidates[s].push_back({&r, i, step, row.person});
        }
    }

    for (std::size_t si = 0; si < p_.shifts.size(); ++si) {
        const Shift& s = p_.shifts[si];
        if (!days_.active(s.calendar.code, d)) continue;
        ShiftDay sd;
        sd.shift = &s;
        sd.day = d;
        const CompanySettings* settings = idx_.settings(s.company.code);
        for (const ShiftItem& item : s.items) {
            if (!item_on(item, d)) continue;   // corsa o attivita' che quel giorno non c'e' (o pezzo sbagliato: K-CTR-054)
            PlannedItem pi;
            pi.item = &item;
            if (item.is_trip) {
                const TripItem& ti = trip_items_.at(&item);
                const Trip* t = ti.trip;
                const PartResolution& pr = ti.part;
                pi.trip = t;
                pi.from = pr.from;
                pi.to = pr.to;
                pi.whole = pr.from == 0 && pr.to + 1 == t->passages.size();
                pi.start = t->passages[pr.from].time;
                pi.end = t->passages[pr.to].time;
                if (pi.whole) {
                    pi.loaded_km = t->loaded_km();
                    pi.empty_km = t->empty_km();
                } else {
                    for (std::size_t k = pr.from + 1; k <= pr.to; ++k) {
                        pi.loaded_km += t->passages[k].loaded_km.value_or(0.0);
                        pi.empty_km += t->passages[k].empty_km.value_or(0.0);
                    }
                }
            } else {
                const ShiftActivity& a = item.activity;
                pi.start = a.start;
                pi.end = a.end;
                pi.empty_km = a.empty_km.value_or(0.0);
                const ActivityType* type = settings ? CompanySettings::find_in(settings->activities, a.type.code) : nullptr;
                pi.uses_vehicle = !type || type->uses_vehicle;
            }
            sd.items.push_back(pi);
        }
        if (sd.items.empty()) continue;   // quel giorno il turno non ha niente da fare: non c'e'
        sd.start = sd.items.front().start;
        sd.end = sd.items.front().end;
        for (const PlannedItem& pi : sd.items) {
            sd.start = std::min(sd.start, pi.start);
            sd.end = std::max(sd.end, pi.end);
            if (!pi.uses_vehicle) continue;
            sd.vehicle_start = sd.vehicle_start ? std::min(*sd.vehicle_start, pi.start) : pi.start;
            sd.vehicle_end = sd.vehicle_end ? std::max(*sd.vehicle_end, pi.end) : pi.end;
        }
        sd.rotation_candidates = std::move(rotation_candidates[si]);
        resolve_person(sd);
        person_facts(sd);
        resolve_vehicle(sd);
        vehicle_facts(sd);
        person_vehicle_facts(sd);
        // Il mezzo abituale e' di chi fa il turno: se il turno e' scoperto non c'e'; se chi lo fa non e' verificato, nemmeno il
        // mezzo (K-017).
        if (sd.vehicle_source == VehicleSource::Usual && sd.vehicle) {
            if (!sd.person) {
                sd.vehicle = nullptr;
                sd.vehicle_verified = sd.person_verified;
            } else if (!sd.person_verified) {
                sd.vehicle_verified = false;
            }
        }
        out.push_back(std::move(sd));
    }

    // Assistenti e secondi autisti: il mezzo e' quello del turno dell'autista che fa le stesse corse. Il secondo
    // autista deve avere le abilitazioni per quel mezzo.
    for (ShiftDay& sd : out)
        if (sd.shift->role != ShiftRole::Driver) aboard(sd, out);

    std::stable_sort(out.begin(), out.end(), [](const ShiftDay& a, const ShiftDay& b) {
        return std::tie(a.start, a.shift->code) < std::tie(b.start, b.shift->code);
    });
    return out;
}

}  // namespace kzero
