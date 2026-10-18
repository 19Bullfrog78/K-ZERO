// K-Zero - controlli sul contenuto del pacchetto (vedi controlli.hpp e docs/controlli.md).
#include "controlli.hpp"

#include <map>
#include <set>
#include <tuple>

#include "pacchetto.hpp"
#include "ricerca.hpp"

namespace kzero {

Param text_param(std::string name, std::string value) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Text;
    p.text = std::move(value);
    return p;
}

Param number_param(std::string name, long long value) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Number;
    p.number = value;
    return p;
}

Param decimal_param(std::string name, double value) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Decimal;
    p.decimal = value;
    return p;
}

Param time_param(std::string name, Minutes value) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Time;
    p.number = value;
    return p;
}

Param date_param(std::string name, Day value) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Date;
    p.number = value;
    return p;
}

Param ref_param(std::string name, Element element, std::string code) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Ref;
    p.element = element;
    p.text = std::move(code);
    return p;
}

Param elements_param(std::string name, Element element) {
    Param p;
    p.name = std::move(name);
    p.kind = Param::Kind::Elements;
    p.element = element;
    return p;
}

namespace {

std::string position_text(const Source& s) { return s.pointer.empty() ? s.file : s.file + " " + s.pointer; }

class Checker {
public:
    explicit Checker(const Package& p) : p_(p), idx_(p) {}

    std::vector<Problem> run() {
        duplicates();
        references();
        place_types();
        periods();
        calendars();
        trip_times();
        ambiguous_stops();
        numbers_only();
        negative_on_board();
        waits();
        vehicle_rules();
        transfers();
        connections();
        passenger_counts();
        assistant_requests();
        lots();
        groups_on_trips();
        coordinates();
        dwell_times();
        max_ride();
        unused_stops();
        services_without_trips();
        trips_without_service();
        empty_groups();
        demand_against_trips();
        verbal_transfers();
        vehicle_costs();
        companies_without_vehicles();
        contract_regulations();
        return std::move(out_);
    }

private:
    void add(Check c, const Source& where, std::vector<Param> params, int variant = 0) {
        out_.push_back(Problem{c, variant, where, std::move(params)});
    }

    // --- Codici doppi ------------------------------------------------------------------

    template <class T>
    void duplicates_in(const std::vector<const T*>& items, Element e) {
        std::map<Code, const Source*> seen;
        for (const T* x : items) {
            const auto [it, inserted] = seen.emplace(x->code, &x->src);
            if (!inserted)
                add(Check::DuplicateCode, x->src,
                    {text_param("codice", x->code), elements_param("elementi", e),
                     text_param("prima", position_text(*it->second))});
        }
    }

    template <class T>
    static std::vector<const T*> pointers(const std::vector<T>& v) {
        std::vector<const T*> out;
        for (const T& x : v) out.push_back(&x);
        return out;
    }

    void duplicates() {
        duplicates_in(pointers(p_.manifest.companies), Element::Company);
        duplicates_in(pointers(p_.places), Element::Place);
        duplicates_in(pointers(p_.segments), Element::Segment);
        duplicates_in(pointers(p_.regulations), Element::Regulation);
        duplicates_in(pointers(p_.holidays), Element::Holidays);
        duplicates_in(pointers(p_.calendars), Element::Calendar);
        duplicates_in(pointers(p_.schools), Element::School);
        duplicates_in(pointers(p_.clients), Element::Client);
        duplicates_in(pointers(p_.contracts), Element::Contract);
        duplicates_in(pointers(p_.lots), Element::Lot);
        duplicates_in(pointers(p_.services), Element::Service);
        duplicates_in(pointers(p_.trips), Element::Trip);
        duplicates_in(pointers(p_.vehicles), Element::Vehicle);
        duplicates_in(pointers(p_.depots), Element::Depot);
        std::vector<const PassengerGroup*> groups;
        std::vector<const Passenger*> passengers;
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups) {
                groups.push_back(&g);
                for (const Passenger& x : g.passengers) passengers.push_back(&x);
            }
        duplicates_in(groups, Element::Group);
        duplicates_in(passengers, Element::Passenger);
    }

    // --- Riferimenti -----------------------------------------------------------------------

    bool exists(Element e, const Code& c) const {
        switch (e) {
            case Element::Company: return idx_.company(c) != nullptr;
            case Element::Place: return idx_.place(c) != nullptr;
            case Element::Segment: return idx_.segment(c) != nullptr;
            case Element::Regulation: return idx_.regulation(c) != nullptr;
            case Element::Calendar: return idx_.calendar(c) != nullptr;
            case Element::School: return idx_.school(c) != nullptr;
            case Element::Client: return idx_.client(c) != nullptr;
            case Element::Contract: return idx_.contract(c) != nullptr;
            case Element::Lot: return idx_.lot(c) != nullptr;
            case Element::Service: return idx_.service(c) != nullptr;
            case Element::Group: return idx_.group(c) != nullptr;
            case Element::Passenger: return idx_.passenger(c) != nullptr;
            case Element::Trip: return idx_.trip(c) != nullptr;
            case Element::Vehicle: return idx_.vehicle(c) != nullptr;
            case Element::Depot: return idx_.depot(c) != nullptr;
            case Element::Transfer: return true;
            case Element::Holidays: return idx_.holidays(c) != nullptr;
        }
        return false;
    }

    void ref(const Ref& r, Element e) {
        if (!exists(e, r.code))
            add(Check::UnknownReference, r.src, {text_param("codice", r.code), elements_param("elementi", e)});
    }

    void references() {
        for (const Place& pl : p_.places)
            if (pl.limits)
                for (const AccessBan& b : pl.limits->bans)
                    if (b.vehicle) ref(*b.vehicle, Element::Vehicle);
        for (const RoadSegment& s : p_.segments) {
            ref(s.from, Element::Place);
            ref(s.to, Element::Place);
            for (const AccessBan& b : s.limits.bans)
                if (b.vehicle) ref(*b.vehicle, Element::Vehicle);
        }
        for (const Calendar& c : p_.calendars) {
            if (c.base) ref(*c.base, Element::Calendar);
            for (const auto* links : {&c.adds, &c.removes})
                for (const CalendarLink& l : *links)
                    ref(l.ref, l.kind == CalendarLinkKind::Holidays ? Element::Holidays : Element::Calendar);
        }
        for (const School& s : p_.schools) ref(s.place, Element::Place);
        for (const Contract& c : p_.contracts) {
            ref(c.client, Element::Client);
            ref(c.company, Element::Company);
            ref(c.regulation, Element::Regulation);
        }
        for (const ServiceLot& l : p_.lots) {
            ref(l.contract, Element::Contract);
            for (const Ref& s : l.services) ref(s, Element::Service);
        }
        for (const Service& s : p_.services) {
            ref(s.contract, Element::Contract);
            ref(s.calendar, Element::Calendar);
            for (const PassengerGroup& g : s.groups) {
                if (g.school) ref(*g.school, Element::School);
                for (const DemandStop& d : g.stops) ref(d.place, Element::Place);
                for (const Passenger& x : g.passengers) {
                    if (x.boarding) ref(x.boarding->place, Element::Place);
                    if (x.alighting) ref(x.alighting->place, Element::Place);
                }
            }
        }
        for (const Trip& t : p_.trips) {
            for (const Ref& s : t.services) ref(s, Element::Service);
            ref(t.calendar, Element::Calendar);
            for (const Passage& p : t.passages) {
                ref(p.place, Element::Place);
                for (const PassageCount& c : p.persons) ref(c.group, Element::Group);
                for (const TransferHalf& h : p.transfers) {
                    ref(h.group, Element::Group);
                    if (h.passenger) ref(*h.passenger, Element::Passenger);
                }
            }
            for (const Connection& c : t.connections) ref(c.place, Element::Place);
        }
        for (const Vehicle& v : p_.vehicles) ref(v.company, Element::Company);
        for (const Depot& d : p_.depots) {
            ref(d.place, Element::Place);
            ref(d.company, Element::Company);
        }
    }

    // --- Luoghi del tipo giusto ----------------------------------------------------------------

    void place_types() {
        for (const School& s : p_.schools)
            if (const Place* pl = idx_.place(s.place.code))
                if (pl->type != PlaceType::School && pl->type != PlaceType::Center)
                    add(Check::WrongPlaceType, s.place.src, {ref_param("luogo", Element::Place, pl->code)}, 0);
        for (const Depot& d : p_.depots)
            if (const Place* pl = idx_.place(d.place.code))
                if (pl->type != PlaceType::Depot && pl->type != PlaceType::ExternalShelter)
                    add(Check::WrongPlaceType, d.place.src, {ref_param("luogo", Element::Place, pl->code)}, 1);
    }

    // --- Periodi rovesciati ----------------------------------------------------------------

    void dates(const Source& where, Day from, Day to) {
        if (to < from) add(Check::InvertedPeriod, where, {date_param("dal", from), date_param("al", to)}, 0);
    }

    void times(const Source& where, Minutes from, Minutes to) {
        if (to < from) add(Check::InvertedPeriod, where, {time_param("dalle", from), time_param("alle", to)}, 1);
    }

    void periods() {
        dates(p_.manifest.src, p_.manifest.period.from, p_.manifest.period.to);
        for (const Calendar& c : p_.calendars) {
            dates(c.src, c.range.from, c.range.to);
            for (const Suspension& s : c.suspensions) dates(s.src, s.range.from, s.range.to);
        }
        for (const School& s : p_.schools)
            for (const Timetable& t : s.timetables) {
                dates(t.src, t.period.from, t.period.to);
                times(t.src, t.start, t.end);
            }
        for (const Regulation& r : p_.regulations)
            if (r.valid_from && r.valid_to) dates(r.src, *r.valid_from, *r.valid_to);
        for (const Contract& c : p_.contracts)
            if (c.valid_from && c.valid_to) dates(c.src, *c.valid_from, *c.valid_to);
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups)
                if (g.window) times(g.src, g.window->open, g.window->close);
    }

    // --- Calendari -----------------------------------------------------------------------------

    bool in_cycle(const Calendar& c) const {
        std::set<Code> seen{c.code};
        const Calendar* cur = &c;
        while (cur->base) {
            if (cur->base->code == c.code) return true;
            if (!seen.insert(cur->base->code).second) return false;   // ciclo che non passa da c
            cur = idx_.calendar(cur->base->code);
            if (!cur) return false;
        }
        return false;
    }

    // Dal calendario from si arriva al calendario target, seguendo le basi e i calendari aggiunti o tolti?
    bool leads_to(const Code& from, const Code& target) const {
        std::set<Code> seen;
        std::vector<Code> todo{from};
        while (!todo.empty()) {
            const Code cur = todo.back();
            todo.pop_back();
            if (cur == target) return true;
            if (!seen.insert(cur).second) continue;
            const Calendar* c = idx_.calendar(cur);
            if (!c) continue;
            if (c->base) todo.push_back(c->base->code);
            for (const auto* links : {&c->adds, &c->removes})
                for (const CalendarLink& l : *links)
                    if (l.kind == CalendarLinkKind::Calendar) todo.push_back(l.ref.code);
        }
        return false;
    }

    // Il calendario si combina con se stesso (giro che passa da almeno un calendario aggiunto o tolto)?
    // Restituisce il punto dei dati da cui parte il giro, oppure nullptr.
    const Source* combination_cycle(const Calendar& c) const {
        for (const auto* links : {&c.adds, &c.removes})
            for (const CalendarLink& l : *links)
                if (l.kind == CalendarLinkKind::Calendar && leads_to(l.ref.code, c.code)) return &l.ref.src;
        if (c.base && leads_to(c.base->code, c.code)) return &c.base->src;
        return nullptr;
    }

    void calendars() {
        const DateRange period = p_.manifest.period;
        for (const Calendar& c : p_.calendars) {
            const bool inverted = c.range.to < c.range.from;
            const auto outside = [&c](Day d) { return d < c.range.from || d > c.range.to; };
            const Param cal = ref_param("calendario", Element::Calendar, c.code);
            if (!inverted) {
                for (const DayEntry& e : c.added)
                    if (outside(e.day))
                        add(Check::DateOutsideCalendar, e.src,
                            {cal, date_param("data", e.day), date_param("dal", c.range.from), date_param("al", c.range.to)}, 0);
                for (const DayEntry& e : c.removed)
                    if (outside(e.day))
                        add(Check::DateOutsideCalendar, e.src,
                            {cal, date_param("data", e.day), date_param("dal", c.range.from), date_param("al", c.range.to)}, 1);
                for (const Suspension& s : c.suspensions)
                    if (s.range.from <= s.range.to && (outside(s.range.from) || outside(s.range.to)))
                        add(Check::DateOutsideCalendar, s.src,
                            {cal, date_param("inizio", s.range.from), date_param("fine", s.range.to),
                             date_param("dal", c.range.from), date_param("al", c.range.to)}, 2);
            }
            for (const DayEntry& e : c.added) {
                bool removed = false;
                for (const DayEntry& r : c.removed) removed = removed || r.day == e.day;
                if (removed) {
                    add(Check::ContradictoryDate, e.src, {cal, date_param("data", e.day)}, 0);
                    continue;
                }
                for (const Suspension& s : c.suspensions)
                    if (e.day >= s.range.from && e.day <= s.range.to) {
                        add(Check::ContradictoryDate, e.src, {cal, date_param("data", e.day)}, 1);
                        break;
                    }
            }
            const bool cycle = in_cycle(c);
            if (cycle) add(Check::CalendarCycle, c.base->src, {cal});
            const Source* combined = cycle ? nullptr : combination_cycle(c);
            if (combined) add(Check::CalendarCombinationCycle, *combined, {cal});
            if (!inverted && !cycle && !combined && period.from <= period.to && !first_active_day(c, period, idx_))
                add(Check::CalendarWithoutDays, c.src, {cal, date_param("dal", period.from), date_param("al", period.to)});
        }
    }
    // --- Orari delle corse ------------------------------------------------------------------------

    void trip_times() {
        for (const Trip& t : p_.trips) {
            const Param trip = ref_param("corsa", Element::Trip, t.code);
            const auto& ps = t.passages;
            for (std::size_t i = 0; i < ps.size(); ++i) {
                if (ps[i].departure && *ps[i].departure < ps[i].time)
                    add(Check::TimeGoesBack, ps[i].src,
                        {trip, time_param("partenza", *ps[i].departure), time_param("arrivo", ps[i].time)}, 2);
                if (i == 0 || ps[i].time >= ps[i - 1].leaves()) continue;
                if (i >= 2 && ps[i].time >= ps[i - 2].leaves())
                    // L'orario fuori posto e' il precedente: es. "16:08" tra 12:05 e 12:09.
                    add(Check::TimeGoesBack, ps[i - 1].src,
                        {trip, time_param("orario", ps[i - 1].leaves()), time_param("precedente", ps[i - 2].leaves()),
                         time_param("successivo", ps[i].time)},
                        0);
                else
                    add(Check::TimeGoesBack, ps[i].src,
                        {trip, time_param("orario", ps[i].time), time_param("precedente", ps[i - 1].leaves())}, 1);
            }
        }
    }

    // --- Fermate con lo stesso nome o codice esterno ---------------------------------------------

    void ambiguous_stops() {
        const PlaceSearch search(p_.places, {PlaceType::Stop});
        const auto report = [this](const std::vector<std::pair<std::string, std::vector<Code>>>& keys, int variant) {
            for (const auto& [key, codes] : keys) {
                std::string shown = key;
                for (char& ch : shown)
                    if (ch == '\x1F') ch = '/';
                for (std::size_t k = 1; k < codes.size(); ++k)
                    if (const Place* pl = idx_.place(codes[k]))
                        add(Check::AmbiguousStop, pl->src,
                            {text_param("nome", shown), ref_param("luogo", Element::Place, codes[k]),
                             ref_param("altro", Element::Place, codes[0])},
                            variant);
            }
        };
        report(search.ambiguous_names(), 0);
        report(search.ambiguous_external_codes(), 1);
    }

    // --- Pacchetto "solo numeri" ---------------------------------------------------------------

    void numbers_only() {
        if (p_.manifest.level != Level::NumbersOnly) return;
        for (const Place& pl : p_.places)
            if (pl.is_private) add(Check::NumbersOnlyPersonalData, pl.src, {ref_param("luogo", Element::Place, pl.code)}, 0);
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups)
                if (!g.passengers.empty())
                    add(Check::NumbersOnlyPersonalData, g.passengers.front().src,
                        {ref_param("gruppo", Element::Group, g.code),
                         number_param("passeggeri", static_cast<long long>(g.passengers.size()))},
                        1);
    }

    // --- Persone a bordo ----------------------------------------------------------------------

    bool counted(const Code& group) const {
        const PassengerGroup* g = idx_.group(group);
        return g && g->counts != CountKind::Unknown;
    }

    void negative_on_board() {
        for (const Trip& t : p_.trips) {
            std::map<Code, int> aboard;
            std::set<Code> reported;
            for (const Passage& p : t.passages) {
                std::map<Code, std::pair<int, int>> moves;   // gruppo -> (scendono, salgono)
                for (const PassageCount& c : p.persons) {
                    moves[c.group.code].first += c.alighting;
                    moves[c.group.code].second += c.boarding;
                }
                for (const TransferHalf& h : p.transfers)
                    (h.action == TransferAction::Deliver ? moves[h.group.code].first : moves[h.group.code].second) +=
                        h.persons;
                for (const auto& [group, move] : moves) {
                    if (!counted(group)) continue;
                    const int before = aboard[group];
                    const int after = before - move.first;
                    if (after < 0 && reported.insert(group).second)
                        add(Check::NegativeOnBoard, p.src,
                            {ref_param("corsa", Element::Trip, t.code), ref_param("gruppo", Element::Group, group),
                             number_param("scendono", move.first), number_param("a_bordo", before)});
                    aboard[group] = std::max(after, 0) + move.second;
                }
            }
        }
    }

    // --- Attesa concordata e regolamento ----------------------------------------------------------

    void waits() {
        std::set<std::tuple<std::string, std::string, Code, int>> done;
        for (const Service& s : p_.services) {
            const Contract* c = idx_.contract(s.contract.code);
            const Regulation* r = c ? idx_.regulation(c->regulation.code) : nullptr;
            if (!r) continue;
            for (const PassengerGroup& g : s.groups) {
                const School* sc = g.school ? idx_.school(g.school->code) : nullptr;
                if (!sc) continue;
                for (const Timetable& t : sc->timetables) {
                    if (!t.wait) continue;
                    const auto check = [&](const std::optional<ValueRule>& limit, int minutes, int variant) {
                        if (!limit || minutes <= limit->value) return;
                        if (!done.emplace(t.wait->src.file, t.wait->src.pointer, r->code, variant).second) return;
                        add(Check::WaitOutsideRegulation, t.wait->src,
                            {ref_param("scuola", Element::School, sc->code), number_param("minuti", minutes),
                             number_param("massimo", limit->value), ref_param("regolamento", Element::Regulation, r->code),
                             ref_param("contratto", Element::Contract, c->code)},
                            variant);
                    };
                    check(r->max_wait_before_min, t.wait->before_min, 0);
                    check(r->max_wait_after_min, t.wait->after_min, 1);
                }
            }
        }
    }

    // --- Mezzo minimo e massimo ---------------------------------------------------------------------

    void vehicle_rules() {
        const auto check = [this](const VehicleRules& r) {
            if (!r.min_seats || !r.max_seats || *r.max_seats >= *r.min_seats) return false;
            add(Check::MaxSeatsBelowMin, r.src, {number_param("massimo", *r.max_seats), number_param("minimo", *r.min_seats)}, 0);
            return true;
        };
        for (const Contract& c : p_.contracts) check(c.rules);
        for (const Service& s : p_.services) {
            if (check(s.rules) || (!s.rules.min_seats && !s.rules.max_seats)) continue;
            const Contract* c = idx_.contract(s.contract.code);
            if (!c) continue;
            // Il servizio cambia solo una delle due regole: vale insieme all'altra, quella del contratto.
            const auto min = s.rules.min_seats ? s.rules.min_seats : c->rules.min_seats;
            const auto max = s.rules.max_seats ? s.rules.max_seats : c->rules.max_seats;
            if (min && max && *max < *min)
                add(Check::MaxSeatsBelowMin, s.rules.src,
                    {number_param("massimo", *max), number_param("minimo", *min), ref_param("contratto", Element::Contract, c->code)},
                    1);
        }
    }

    // --- Trasbordi -------------------------------------------------------------------------------

    struct HalfAt {
        const Trip* trip;
        const Passage* passage;
        const TransferHalf* half;
    };

    void transfers() {
        std::map<Code, std::vector<HalfAt>> delivers, receives;
        for (const Trip& t : p_.trips)
            for (const Passage& p : t.passages)
                for (const TransferHalf& h : p.transfers) {
                    (h.action == TransferAction::Deliver ? delivers : receives)[h.code].push_back({&t, &p, &h});
                    const Place* pl = idx_.place(p.place.code);
                    if (pl && (pl->is_private || !pl->transfer))
                        add(Check::TransferNotAuthorized, h.src,
                            {text_param("trasbordo", h.code), ref_param("luogo", Element::Place, pl->code)});
                }
        std::set<Code> codes;
        for (const auto& [code, v] : delivers) codes.insert(code);
        for (const auto& [code, v] : receives) codes.insert(code);
        const int margin = p_.manifest.transfer_margin_min;
        for (const Code& code : codes) {
            const auto& d = delivers[code];
            const auto& r = receives[code];
            const Param transfer = text_param("trasbordo", code);
            if (d.empty() || r.empty()) {
                const HalfAt& only = d.empty() ? r.front() : d.front();
                add(Check::TransferHalfMissing, only.half->src, {transfer, ref_param("corsa", Element::Trip, only.trip->code)},
                    d.empty() ? 1 : 0);
                continue;
            }
            if (d.size() > 1) add(Check::TransferHalvesDiffer, d[1].half->src, {transfer}, 0);
            if (r.size() > 1) add(Check::TransferHalvesDiffer, r[1].half->src, {transfer}, 1);
            const HalfAt& give = d.front();
            const HalfAt& take = r.front();
            if (give.trip == take.trip) {
                add(Check::TransferHalvesDiffer, take.half->src, {transfer, ref_param("corsa", Element::Trip, take.trip->code)}, 2);
                continue;
            }
            if (give.passage->place.code != take.passage->place.code)
                add(Check::TransferHalvesDiffer, take.half->src,
                    {transfer, ref_param("luogo", Element::Place, take.passage->place.code),
                     ref_param("altro", Element::Place, give.passage->place.code)},
                    3);
            const bool same_passenger = (give.half->passenger.has_value() == take.half->passenger.has_value()) &&
                                        (!give.half->passenger || give.half->passenger->code == take.half->passenger->code);
            if (give.half->group.code != take.half->group.code || give.half->persons != take.half->persons || !same_passenger)
                add(Check::TransferHalvesDiffer, take.half->src, {transfer}, 4);
            const Param receiver = ref_param("corsa", Element::Trip, take.trip->code);
            const Param giver = ref_param("corsa_consegna", Element::Trip, give.trip->code);
            if (take.passage->time + margin > give.passage->time)
                add(Check::TransferLateReceiver, take.half->src,
                    {transfer, receiver, ref_param("luogo", Element::Place, take.passage->place.code),
                     time_param("arrivo", take.passage->time), number_param("margine", margin),
                     time_param("limite", give.passage->time - margin), giver, time_param("consegna", give.passage->time)});
            if (take.passage->leaves() < give.passage->time)
                add(Check::TransferReceiverLeaves, take.half->src,
                    {transfer, receiver, time_param("partenza", take.passage->leaves()), giver,
                     time_param("consegna", give.passage->time)});
        }
    }

    // --- Coincidenze ------------------------------------------------------------------------------

    void connections() {
        for (const Trip& t : p_.trips)
            for (const Connection& c : t.connections) {
                const Passage* first = nullptr;
                const Passage* last = nullptr;
                for (const Passage& p : t.passages)
                    if (p.place.code == c.place.code) {
                        if (!first) first = &p;
                        last = &p;
                    }
                const Param trip = ref_param("corsa", Element::Trip, t.code);
                const Param place = ref_param("luogo", Element::Place, c.place.code);
                if (!first) {
                    add(Check::ConnectionNotOnTrip, c.src, {trip, place, text_param("servizio", c.external_service)});
                    continue;
                }
                if (c.kind == ConnectionKind::WaitArrival && first->leaves() < c.time)
                    add(Check::ConnectionWaitArrival, c.src,
                        {trip, place, time_param("partenza", first->leaves()), text_param("servizio", c.external_service),
                         time_param("arrivo", c.time)});
                if (c.kind == ConnectionKind::GuaranteeDeparture && last->time > c.time)
                    add(Check::ConnectionGuaranteeDeparture, c.src,
                        {trip, place, time_param("arrivo", last->time), text_param("servizio", c.external_service),
                         time_param("partenza", c.time)});
            }
    }

    // --- Conteggi del gruppo e passeggeri uno per uno --------------------------------------------

    void passenger_counts() {
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups) {
                if (g.passengers.empty() || g.counts == CountKind::Unknown) continue;
                struct Expected {
                    int boarding = 0, alighting = 0, wheelchairs = 0, folded_aids = 0;
                };
                std::map<Code, Expected> expected;
                std::set<Code> stops;
                for (const DemandStop& d : g.stops) stops.insert(d.place.code);
                const Param group = ref_param("gruppo", Element::Group, g.code);
                for (const Passenger& x : g.passengers) {
                    if (!x.travels()) continue;
                    for (const auto* a : {&x.boarding, &x.alighting}) {
                        if (!*a) continue;
                        const Code& place = (*a)->place.code;
                        if (!stops.count(place))
                            add(Check::CountsDifferFromPassengers, x.src,
                                {ref_param("passeggero", Element::Passenger, x.code), ref_param("luogo", Element::Place, place), group},
                                3);
                        Expected& e = expected[place];
                        (a == &x.boarding ? e.boarding : e.alighting) += 1;
                        if (a == &x.boarding) {   // carrozzine e ausili piegati: si contano dove la persona sale
                            if (x.needs.wheelchair == WheelchairUse::Seated) e.wheelchairs += 1;
                            e.folded_aids += x.needs.folded_items();
                        }
                    }
                }
                for (const DemandStop& d : g.stops) {
                    const Expected e = expected[d.place.code];
                    const Param place = ref_param("luogo", Element::Place, d.place.code);
                    if (d.boarding != e.boarding)
                        add(Check::CountsDifferFromPassengers, d.src,
                            {group, place, number_param("conteggio", d.boarding), number_param("passeggeri", e.boarding)}, 0);
                    if (d.alighting != e.alighting)
                        add(Check::CountsDifferFromPassengers, d.src,
                            {group, place, number_param("conteggio", d.alighting), number_param("passeggeri", e.alighting)}, 1);
                    if (d.wheelchairs != e.wheelchairs) add(Check::CountsDifferFromPassengers, d.src, {group, place}, 2);
                    if (d.folded_aids != e.folded_aids) add(Check::CountsDifferFromPassengers, d.src, {group, place}, 4);
                }
            }
    }

    // --- Assistente: gruppo e passeggeri coerenti --------------------------------------------------

    void assistant_requests() {
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups) {
                if (g.assistant_required) continue;
                for (const Passenger& x : g.passengers)
                    if (x.travels() && x.needs.assistant)
                        add(Check::AssistantNotRequested, x.src,
                            {ref_param("passeggero", Element::Passenger, x.code), ref_param("gruppo", Element::Group, g.code)});
            }
    }

    // --- Gruppi di servizi ------------------------------------------------------------------------

    void lots() {
        std::map<Code, const ServiceLot*> lot_of;
        for (const ServiceLot& l : p_.lots)
            for (const Ref& r : l.services) {
                const Service* s = idx_.service(r.code);
                if (s && s->contract.code != l.contract.code)
                    add(Check::LotServiceOtherContract, r.src,
                        {ref_param("servizio", Element::Service, s->code), ref_param("contratto", Element::Contract, s->contract.code),
                         ref_param("gruppo_servizi", Element::Lot, l.code),
                         ref_param("contratto_gruppo", Element::Contract, l.contract.code)});
                const auto [it, inserted] = lot_of.emplace(r.code, &l);
                if (!inserted && it->second != &l)
                    add(Check::ServiceInTwoLots, r.src,
                        {ref_param("servizio", Element::Service, r.code), ref_param("gruppo_servizi", Element::Lot, l.code),
                         ref_param("altro", Element::Lot, it->second->code)});
            }
    }

    // --- Gruppi di servizi che la corsa non soddisfa ------------------------------------------------

    void groups_on_trips() {
        for (const Trip& t : p_.trips) {
            std::set<Code> services;
            for (const Ref& r : t.services) services.insert(r.code);
            const auto check = [&](const Ref& group, const Source& where) {
                const Service* s = idx_.service_of_group(group.code);
                if (s && !services.count(s->code))
                    add(Check::GroupNotOnTripServices, where,
                        {ref_param("corsa", Element::Trip, t.code), ref_param("gruppo", Element::Group, group.code),
                         ref_param("servizio", Element::Service, s->code)});
            };
            for (const Passage& p : t.passages) {
                for (const PassageCount& c : p.persons) check(c.group, c.src);
                for (const TransferHalf& h : p.transfers) check(h.group, h.src);
            }
        }
    }

    // --- Avvisi --------------------------------------------------------------------------------------

    void coordinates() {
        for (const Place& pl : p_.places)
            if (!pl.coord) add(Check::PlaceWithoutCoordinates, pl.src, {ref_param("luogo", Element::Place, pl.code)});
    }

    void dwell_times() {
        if (!p_.dwell) {
            add(Check::DwellTimesUndefined, Source{}, {}, 0);
            return;
        }
        const DwellTimes& d = *p_.dwell;
        bool complete = d.per_stop_s && d.school_unload_s;
        for (DwellKind k : {DwellKind::Student, DwellKind::Kindergarten, DwellKind::Disabled}) {
            bool found = false;
            for (const auto& x : d.per_passenger) found = found || x.kind == k;
            complete = complete && found;
        }
        if (!complete) add(Check::DwellTimesUndefined, d.src, {}, 1);
    }

    void max_ride() {
        for (const Service& s : p_.services) {
            if (s.freedom != TimeFreedom::Free) continue;
            const Contract* c = idx_.contract(s.contract.code);
            const Regulation* r = idx_.regulation_of(s);
            for (const PassengerGroup& g : s.groups) {
                const School* sc = g.school ? idx_.school(g.school->code) : nullptr;
                bool defined = s.rules.max_ride_min || (c && c->rules.max_ride_min) || (sc && sc->max_ride_min);
                if (!defined && r && r->max_ride_min && sc && sc->grade) defined = r->max_ride_min->for_grade(*sc->grade).has_value();
                if (!defined)
                    add(Check::MaxRideUndefined, g.src,
                        {ref_param("gruppo", Element::Group, g.code), ref_param("servizio", Element::Service, s.code)});
            }
        }
    }

    void unused_stops() {
        std::set<Code> used;
        for (const Trip& t : p_.trips) {
            for (const Passage& p : t.passages) used.insert(p.place.code);
            for (const Connection& c : t.connections) used.insert(c.place.code);
        }
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups) {
                for (const DemandStop& d : g.stops) used.insert(d.place.code);
                for (const Passenger& x : g.passengers) {
                    if (x.boarding) used.insert(x.boarding->place.code);
                    if (x.alighting) used.insert(x.alighting->place.code);
                }
            }
        for (const Place& pl : p_.places)
            if (pl.type == PlaceType::Stop && !pl.is_private && !used.count(pl.code))
                add(Check::StopNeverUsed, pl.src, {ref_param("luogo", Element::Place, pl.code)});
    }

    void services_without_trips() {
        std::set<Code> served;
        for (const Trip& t : p_.trips)
            for (const Ref& r : t.services) served.insert(r.code);
        for (const Service& s : p_.services)
            if (!served.count(s.code)) add(Check::ServiceWithoutTrips, s.src, {ref_param("servizio", Element::Service, s.code)});
    }

    void trips_without_service() {
        for (const Trip& t : p_.trips)
            if (t.services.empty()) add(Check::TripWithoutService, t.src, {ref_param("corsa", Element::Trip, t.code)});
    }

    void empty_groups() {
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups)
                if (g.counts != CountKind::Unknown && g.passengers.empty() && g.total_boarding() + g.total_alighting() == 0)
                    add(Check::GroupWithoutPassengers, g.src,
                        {ref_param("gruppo", Element::Group, g.code), ref_param("servizio", Element::Service, s.code)});
    }

    // Domanda di ogni gruppo con conteggi esatti contro le persone delle corse, calendario per calendario:
    // corse con lo stesso calendario si sommano; corse con calendari diversi portano la domanda in giorni diversi.
    void demand_against_trips() {
        struct Load {
            int boarding = 0, alighting = 0;
            const Source* first = nullptr;
        };
        // gruppo -> calendario -> luogo -> persone delle corse
        std::map<Code, std::map<Code, std::map<Code, Load>>> loads;
        for (const Trip& t : p_.trips)
            for (const Passage& p : t.passages)
                for (const PassageCount& c : p.persons) {
                    Load& l = loads[c.group.code][t.calendar.code][p.place.code];
                    l.boarding += c.boarding;
                    l.alighting += c.alighting;
                    if (!l.first) l.first = &c.src;
                }
        for (const Service& s : p_.services)
            for (const PassengerGroup& g : s.groups) {
                if (g.counts != CountKind::Exact) continue;
                const auto by_calendar = loads.find(g.code);
                if (by_calendar == loads.end()) continue;
                const Param group = ref_param("gruppo", Element::Group, g.code);
                for (const auto& [calendar, by_place] : by_calendar->second) {
                    const Param cal = ref_param("calendario", Element::Calendar, calendar);
                    std::set<Code> in_demand;
                    for (const DemandStop& d : g.stops) {
                        in_demand.insert(d.place.code);
                        const auto it = by_place.find(d.place.code);
                        const Load l = it == by_place.end() ? Load{} : it->second;
                        const Param place = ref_param("luogo", Element::Place, d.place.code);
                        if (l.boarding != d.boarding)
                            add(Check::DemandDiffersFromTrips, d.src,
                                {group, place, number_param("domanda", d.boarding), number_param("corse", l.boarding), cal}, 0);
                        if (l.alighting != d.alighting)
                            add(Check::DemandDiffersFromTrips, d.src,
                                {group, place, number_param("domanda", d.alighting), number_param("corse", l.alighting), cal}, 1);
                    }
                    for (const auto& [place, l] : by_place)
                        if (!in_demand.count(place) && (l.boarding > 0 || l.alighting > 0))
                            add(Check::DemandDiffersFromTrips, *l.first,
                                {group, ref_param("luogo", Element::Place, place), number_param("corse", l.boarding + l.alighting), cal},
                                2);
                }
            }
    }

    void verbal_transfers() {
        for (const Place& pl : p_.places)
            if (pl.transfer && pl.transfer->verbal && pl.transfer->document.empty())
                add(Check::TransferVerbalOnly, pl.transfer->src, {ref_param("luogo", Element::Place, pl.code)});
    }

    void vehicle_costs() {
        for (const Vehicle& v : p_.vehicles) {
            const int variant = !v.daily_fixed_cost && !v.cost_per_km ? 2 : !v.daily_fixed_cost ? 0 : !v.cost_per_km ? 1 : -1;
            if (variant >= 0) add(Check::VehicleWithoutCosts, v.src, {ref_param("mezzo", Element::Vehicle, v.code)}, variant);
        }
    }

    void companies_without_vehicles() {
        std::set<Code> with;
        for (const Vehicle& v : p_.vehicles) with.insert(v.company.code);
        for (const Company& c : p_.manifest.companies)
            if (!with.count(c.code)) add(Check::CompanyWithoutVehicles, c.src, {ref_param("azienda", Element::Company, c.code)});
    }

    void contract_regulations() {
        const DateRange package = p_.manifest.period;
        for (const Contract& c : p_.contracts) {
            const Regulation* r = idx_.regulation(c.regulation.code);
            if (!r) continue;
            DateRange period{std::max(package.from, c.valid_from.value_or(package.from)),
                             std::min(package.to, c.valid_to.value_or(package.to))};
            if (period.to < period.from) continue;
            const auto gap = [&](Day from, Day to) {
                add(Check::ContractWithoutRegulation, c.regulation.src,
                    {ref_param("contratto", Element::Contract, c.code), ref_param("regolamento", Element::Regulation, r->code),
                     date_param("dal", from), date_param("al", to)});
            };
            if (r->valid_from && *r->valid_from > period.from) gap(period.from, std::min(period.to, *r->valid_from - 1));
            if (r->valid_to && *r->valid_to < period.to) gap(std::max(period.from, *r->valid_to + 1), period.to);
        }
    }

    const Package& p_;
    Index idx_;
    std::vector<Problem> out_;
};

}  // namespace

std::vector<Problem> check_package(const Package& p) { return Checker(p).run(); }

}  // namespace kzero
