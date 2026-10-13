// K-Zero - indice del pacchetto, calendari, persone a bordo (vedi pacchetto.hpp).
#include "pacchetto.hpp"

namespace kzero {

Index::Index(const Package& p) : package_(&p) {
    fill(companies_, p.manifest.companies);
    fill(places_, p.places);
    fill(segments_, p.segments);
    fill(regulations_, p.regulations);
    fill(calendars_, p.calendars);
    fill(schools_, p.schools);
    fill(clients_, p.clients);
    fill(contracts_, p.contracts);
    fill(lots_, p.lots);
    fill(services_, p.services);
    fill(trips_, p.trips);
    fill(vehicles_, p.vehicles);
    fill(depots_, p.depots);
    for (const Service& s : p.services)
        for (const PassengerGroup& g : s.groups) {
            groups_.emplace(g.code, &g);
            group_service_.emplace(g.code, &s);
            for (const Passenger& x : g.passengers) {
                passengers_.emplace(x.code, &x);
                passenger_group_.emplace(x.code, &g);
            }
        }
}

const Regulation* Index::regulation_of(const Service& s) const {
    const Contract* c = contract(s.contract.code);
    return c ? regulation(c->regulation.code) : nullptr;
}

std::optional<ServiceType> Index::category(const Trip& t) const {
    return derive_service(
        t, [this](const Code& c) { return service(c); }, [this](const Code& c) { return group(c); },
        [this](const Code& c) { return school(c); });
}

namespace {

bool contains_day(const std::vector<DayEntry>& v, Day d) {
    for (const auto& e : v)
        if (e.day == d) return true;
    return false;
}

bool active(const Calendar& c, Day d, const Index& index, std::size_t depth) {
    if (depth > index.package().calendars.size()) return false;   // catena circolare di basi
    if (contains_day(c.removed, d)) return false;
    for (const auto& s : c.suspensions)
        if (d >= s.range.from && d <= s.range.to) return false;
    if (contains_day(c.added, d)) return true;
    if (d < c.range.from || d > c.range.to || !c.days.has(weekday_of(d))) return false;
    if (!c.base) return true;
    const Calendar* base = index.calendar(c.base->code);
    return base && active(*base, d, index, depth + 1);
}

}  // namespace

bool calendar_active(const Calendar& c, Day d, const Index& index) { return active(c, d, index, 0); }

std::optional<Day> first_active_day(const Calendar& c, DateRange within, const Index& index) {
    Day from = within.from, to = within.to;
    // Fuori dal periodo del calendario valgono solo le date aggiunte (che devono stare dentro: lo controllano
    // i controlli), quindi basta guardare il periodo del calendario.
    from = std::max(from, c.range.from);
    to = std::min(to, c.range.to);
    for (Day d = from; d <= to; ++d)
        if (calendar_active(c, d, index)) return d;
    return std::nullopt;
}

std::vector<OnBoardAfter> on_board(const Trip& t, const Index& index) {
    std::vector<OnBoardAfter> out;
    std::map<Code, int> by_group;
    const auto counted = [&index](const Code& group) {
        const PassengerGroup* g = index.group(group);
        return g && g->counts != CountKind::Unknown;
    };
    for (const Passage& p : t.passages) {
        for (const auto& c : p.persons)
            if (counted(c.group.code)) by_group[c.group.code] += c.boarding - c.alighting;
        for (const auto& h : p.transfers)
            if (counted(h.group.code))
                by_group[h.group.code] += h.action == TransferAction::Receive ? h.persons : -h.persons;
        OnBoardAfter a;
        a.by_group = by_group;
        for (const auto& [code, n] : by_group) a.total += n;
        out.push_back(a);
    }
    return out;
}

}  // namespace kzero
