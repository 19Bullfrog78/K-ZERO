// K-Zero - punti degli utenti dell'elenco (K-027): vedi punti.hpp.
#include "punti.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <tuple>

#include "elenco.hpp"
#include "ricerca.hpp"
#include "schede.hpp"

namespace kzero {

// --- Le schede nei giorni ------------------------------------------------------------------------------------------------

const ServiceCard* card_by_id(const Package& p, const std::string& id) {
    for (const ServiceCard& s : p.service_cards)
        if (s.id == id) return &s;
    return nullptr;
}

const CardVersion* card_version_on(const ServiceCard& s, Day d) {
    const CardVersion* out = nullptr;
    for (const CardVersion& v : s.versions)
        if (v.valid.from <= d && d <= v.valid.to) out = &v;
    return out;
}

const CardVersion* card_version_number(const ServiceCard& s, int number) {
    for (const CardVersion& v : s.versions)
        if (v.number == number) return &v;
    return nullptr;
}

int card_place_count(const Card& k, const Code& place) {
    int n = 0;
    for (const CardStop& f : k.stops) n += f.place == place ? 1 : 0;
    return n;
}

std::optional<std::size_t> card_passage(const Card& k, const Code& place, int occurrence) {
    int n = 0;
    for (std::size_t i = 0; i < k.stops.size(); ++i)
        if (k.stops[i].place == place && ++n == occurrence) return i;
    return std::nullopt;
}

int card_occurrence(const Card& k, std::size_t row) {
    int n = 0;
    for (std::size_t i = 0; i <= row && i < k.stops.size(); ++i) n += k.stops[i].place == k.stops[row].place ? 1 : 0;
    return n;
}

int card_boarding(const CardStop& s) { return s.boarding - (s.escort == AssistantMove::Boards ? 1 : 0); }
int card_alighting(const CardStop& s) { return s.alighting - (s.escort == AssistantMove::Alights ? 1 : 0); }

PointsCalendar::PointsCalendar(const Package& p, const Index& idx) : idx_(idx) {
    // D4: i giorni di scuola sono i giorni dei calendari di base delle schede della scuola (la parola che il committente scrive sulla
    // scheda, es. SCOLASTICO, MATERNA), per il committente.
    for (const ServiceCard& s : p.service_cards)
        for (const CardVersion& v : s.versions)
            if (!v.card.base_calendar.empty())
                for (const Code& sc : v.card.schools) basi_[{s.client.code, sc}].insert(v.card.base_calendar);
}

bool PointsCalendar::runs(const CardVersion& v, Day d) const {
    if (d < v.valid.from || d > v.valid.to) return false;
    const auto k = std::make_pair(&v, d);
    const auto it = gira_.find(k);
    if (it != gira_.end()) return it->second;
    auto c = calendari_.find(&v);
    if (c == calendari_.end()) c = calendari_.emplace(&v, card_calendar(v.card, v.calendar, v.valid)).first;
    const bool g = calendar_active(c->second, d, idx_);
    gira_.emplace(k, g);
    return g;
}

std::optional<bool> PointsCalendar::school_day(const Code& client, const Code& school, Day d) const {
    const auto it = basi_.find({client, school});
    if (it == basi_.end() || it->second.empty()) return std::nullopt;
    bool almeno_uno = false;
    for (const Code& b : it->second) {
        const Calendar* c = idx_.calendar(b);
        if (!c) continue;
        almeno_uno = true;
        const auto k = std::make_pair(b, d);
        auto a = attivo_.find(k);
        if (a == attivo_.end()) a = attivo_.emplace(k, calendar_active(*c, d, idx_)).first;
        if (a->second) return true;
    }
    if (!almeno_uno) return std::nullopt;
    return false;
}

// --- La storia dei punti -------------------------------------------------------------------------------------------------

const PointEntry* point_entry_on(const UserPoints& up, Day d) {
    const PointEntry* out = nullptr;
    for (const PointEntry& e : up.history)
        if (!e.replaced && e.from <= d && (!out || e.from >= out->from)) out = &e;
    return out;
}

std::vector<const PointRule*> point_rules_on(const UserPoints& up, Day d) {
    std::vector<const PointRule*> out;
    const PointEntry* e = point_entry_on(up, d);
    if (!e) return out;
    const Weekday w = weekday_of(d);
    for (const PointRule& r : e->rules)
        if (r.covers(w)) out.push_back(&r);
    return out;
}

bool point_entry_double(const PointEntry& e) {
    std::set<Code> luoghi;
    for (const PointRule& r : e.rules) luoghi.insert(r.place.code);
    return luoghi.size() > 1;
}

std::vector<Day> add_point_entry(UserPoints& up, const PointEntry& e) {
    std::vector<Day> sostituite;
    for (PointEntry& x : up.history)
        if (!x.replaced && x.from >= e.from) {
            x.replaced = e.mark;
            sostituite.push_back(x.from);
        }
    // In ordine di dal; a pari dal la voce nuova dopo quelle sostituite.
    auto it = up.history.begin();
    while (it != up.history.end() && it->from <= e.from) ++it;
    up.history.insert(it, e);
    return sostituite;
}

const NotAttending* not_attending_on(const ListUser& u, Day d, const std::string& leg) {
    for (const NotAttending& n : u.not_attending)
        if (n.covers(d, leg)) return &n;
    return nullptr;
}

bool point_right(const ListUser& u, const std::string& leg, Day d) {
    if (!u.in_plan()) return false;
    if (u.active) return true;
    for (const ListUserUnlock& s : u.unlocks)
        if (s.leg == leg && s.covers(d)) return true;
    return false;
}

Travel point_travel(const ListUser& u, const Code& client, const std::string& leg, Day d, const PointsCalendar& cal) {
    bool orari = false, giorno = false;
    const Weekday w = weekday_of(d);
    for (const ListUserTime& t : u.times) {
        if (t.leg != leg) continue;
        orari = true;
        if (t.day == w && t.dates.from <= d && d <= t.dates.to) giorno = true;
    }
    if (!orari) return Travel::Unknown;
    const std::optional<bool> scuola = cal.school_day(client, u.school.code, d);
    if (!scuola) return Travel::Unknown;
    return giorno && *scuola ? Travel::Yes : Travel::No;
}

std::optional<Minutes> point_user_time(const ListUser& u, const std::string& leg, Day d) {
    const Weekday w = weekday_of(d);
    for (const ListUserTime& t : u.times)
        if (t.leg == leg && t.day == w && t.dates.from <= d && d <= t.dates.to) return t.time;
    return std::nullopt;
}

namespace {

// La riga della scuola dell'utente per un passaggio: all'andata la prima riga dopo nel luogo della scuola, al ritorno l'ultima prima.
// Nella coincidenza (K-023: la scheda finisce, all'andata, o comincia, al ritorno, in un punto di trasbordo per la sua scuola) la riga
// del punto di trasbordo, con coincidence vero.
using SchoolRow = PointSchoolRow;
SchoolRow school_row(const Card& k, std::size_t row, const Code& school, const Index& idx) {
    SchoolRow out;
    const School* sc = idx.school(school);
    if (!sc || k.stops.empty()) return out;
    const Code& luogo = sc->place.code;
    if (k.direction == Direction::Outbound) {
        for (std::size_t j = row + 1; j < k.stops.size(); ++j)
            if (k.stops[j].place == luogo) return {j, false};
    } else {
        for (std::size_t j = row; j-- > 0;)
            if (k.stops[j].place == luogo) return {j, false};
    }
    const std::optional<CardTransferEnd> e = card_transfer_end(k, idx);
    if (!e || e->school != school) return out;
    if (k.direction == Direction::Outbound && !e->at_start && row + 1 < k.stops.size()) return {k.stops.size() - 1, true};
    if (k.direction == Direction::Return && e->at_start && row > 0) return {std::size_t{0}, true};
    return out;
}

// Le attese massime per l'orario della proposta (D6): quella concordata con la scuola quel giorno, se no quella del regolamento del
// contratto. Nessun valore nel programma.
struct Waits {
    std::optional<int> before, after;
};
Waits waits_for(const Index& idx, const School* sc, const Contract* ct, Day d) {
    Waits w;
    if (sc)
        for (const Timetable& t : sc->timetables)
            if (t.period.from <= d && d <= t.period.to && t.days.has(weekday_of(d)) && t.wait) {
                w.before = t.wait->before_min;
                w.after = t.wait->after_min;
                return w;
            }
    if (ct)
        if (const Regulation* r = idx.regulation(ct->regulation.code)) {
            if (r->max_wait_before_min) w.before = r->max_wait_before_min->value;
            if (r->max_wait_after_min) w.after = r->max_wait_after_min->value;
        }
    return w;
}

Day first_weekday_from(Day from, Weekday w) {
    Day d = from;
    while (weekday_of(d) != w) ++d;
    return d;
}

std::optional<int> civic_number(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size() && text[i] == ' ') ++i;
    long long n = 0;
    std::size_t cifre = 0;
    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])) && cifre < 6) {
        n = n * 10 + (text[i] - '0');
        ++i;
        ++cifre;
    }
    if (cifre == 0) return std::nullopt;
    return static_cast<int>(n);
}

}  // namespace

PointSchoolRow point_school_row(const Card& k, std::size_t row, const Code& school, const Index& idx) { return school_row(k, row, school, idx); }

// --- La proposta -----------------------------------------------------------------------------------------------------------

std::vector<Code> street_points(const StreetTable& t, const ListUserHome& home, const School* school) {
    const std::string comune = normalize_name(home.town), frazione = normalize_name(home.hamlet), via = normalize_name(home.street);
    const std::optional<int> civico = civic_number(home.number);
    if (comune.empty() || via.empty()) return {};
    std::vector<std::pair<int, Code>> trovati;
    for (const StreetRow& r : t.rows) {
        if (normalize_name(r.town) != comune || normalize_name(r.street) != via) continue;
        if (!r.hamlet.empty() && !frazione.empty() && normalize_name(r.hamlet) != frazione) continue;
        if (r.number_from) {
            if (!civico || *civico < *r.number_from || *civico > *r.number_to) continue;
            if (r.side == StreetSide::Even && *civico % 2 != 0) continue;
            if (r.side == StreetSide::Odd && *civico % 2 == 0) continue;
        }
        int livello = 0;
        if (r.school) {
            if (!school || r.school->code != school->code) continue;
            livello = 0;
        } else if (r.grade) {
            if (!school || !school->grade || *school->grade != *r.grade) continue;
            livello = 2;
        } else {
            livello = 4;
        }
        if (!r.number_from) ++livello;   // a pari livello, con i civici prima che senza
        trovati.push_back({livello, r.point.code});
    }
    std::vector<Code> out;
    if (trovati.empty()) return out;
    int migliore = trovati.front().first;
    for (const auto& [l, c] : trovati) migliore = std::min(migliore, l);
    for (const auto& [l, c] : trovati)
        if (l == migliore && std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
    return out;
}

std::vector<PointCandidate> point_candidates(const Package& p, const Index& idx, const PointsCalendar& cal, const ClientList& list,
                                             const ListUser& u, const std::string& leg, Direction direction, const Code& company,
                                             const Code& place, Weekday weekday, Day from, std::optional<Minutes> user_time) {
    (void)leg;
    std::vector<PointCandidate> out;
    const School* sc = idx.school(u.school.code);
    for (const ServiceCard& s : p.service_cards) {
        if (s.client.code != list.client.code) continue;
        const Contract* ct = idx.contract(s.contract.code);
        if (!ct || ct->company.code != company) continue;
        const CardVersion* v = card_version_on(s, from);
        if (!v || v->card.direction != direction) continue;
        // Gira in quel giorno della settimana, dal giorno della proposta alla fine della versione.
        bool gira = false;
        Day primo = 0;
        for (Day d = first_weekday_from(std::max(from, v->valid.from), weekday); d <= v->valid.to; d += 7)
            if (cal.runs(*v, d)) {
                gira = true;
                primo = d;
                break;
            }
        if (!gira) continue;
        const Card& k = v->card;
        for (std::size_t i = 0; i < k.stops.size(); ++i) {
            if (k.stops[i].place != place) continue;
            const SchoolRow sr = school_row(k, i, u.school.code, idx);
            if (!sr.row) continue;
            if (user_time && !sr.coincidence) {
                // D6: l'ora dell'utente e' l'ora delle lezioni; la scheda arriva alla scuola entro l'ora (andata) o ne parte dopo (ritorno),
                // con le attese massime che il pacchetto ha. Nella coincidenza l'orario della scuola non si sa: nessun filtro.
                const Waits w = waits_for(idx, sc, ct, primo);
                const Minutes t = k.stops[*sr.row].time;
                const bool ok = direction == Direction::Outbound ? t <= *user_time && (!w.before || t >= *user_time - *w.before)
                                                                 : t >= *user_time && (!w.after || t <= *user_time + *w.after);
                if (!ok) continue;
            }
            out.push_back(PointCandidate{&s, v, place, card_occurrence(k, i), i, *sr.row});
        }
    }
    return out;
}

std::vector<PointProposal> propose_points(const Package& p, const Index& idx, Day from, const Code& user_filter, const std::string& leg_filter,
                                          bool all) {
    std::vector<PointProposal> out;
    const PointsCalendar cal(p, idx);
    for (const ClientList& list : p.client_lists) {
        const ReadingProfile* profile = idx.reading_profile(list.profile.code);
        if (!profile) continue;
        for (const ListUser& u : list.users) {
            if (!u.in_plan()) continue;
            if (!user_filter.empty() && u.code != user_filter) continue;
            for (const ListUserLeg& lg : u.legs) {
                if (!lg.company) continue;
                if (!leg_filter.empty() && lg.leg != leg_filter) continue;
                const ProfileLeg* pl = profile->leg(lg.leg);
                if (!pl) continue;
                // Ha diritto al servizio su quella tratta: attivo, o sospeso con uno sblocco della tratta.
                const bool diritto = u.active || std::any_of(u.unlocks.begin(), u.unlocks.end(), [&](const ListUserUnlock& s) { return s.leg == lg.leg; });
                if (!diritto) continue;
                PointProposal x;
                x.list = &list;
                x.user = &u;
                x.leg = lg.leg;
                x.direction = pl->direction;
                x.company = lg.company->code;
                const UserPoints* up = u.points_of(lg.leg);
                const PointEntry* e = up ? point_entry_on(*up, from) : nullptr;
                x.has_point = e && !e->rules.empty();
                if (x.has_point && !all) continue;
                // I giorni della settimana in cui viaggia la tratta dal giorno della proposta (dai suoi orari); senza orari non si sanno.
                std::vector<Weekday> giorni;
                bool orari = false;
                for (const ListUserTime& t : u.times) {
                    if (t.leg != lg.leg) continue;
                    orari = true;
                    if (t.dates.to >= from && std::find(giorni.begin(), giorni.end(), t.day) == giorni.end()) giorni.push_back(t.day);
                }
                x.days_known = orari;
                if (!orari)
                    for (int i = 0; i < 7; ++i) giorni.push_back(static_cast<Weekday>(i));
                std::sort(giorni.begin(), giorni.end());
                // Le fonti, in ordine: la prima che da' almeno un candidato.
                std::vector<std::pair<PointSource, std::vector<Code>>> fonti;
                if (u.stop) fonti.push_back({PointSource::IndicatedStop, {u.stop->code}});
                if (u.home)
                    for (const StreetTable& t : p.street_tables)
                        if (t.company.code == lg.company->code) {
                            const std::vector<Code> punti = street_points(t, *u.home, idx.school(u.school.code));
                            if (!punti.empty()) fonti.push_back({PointSource::StreetTable, punti});
                        }
                for (const auto& [fonte, luoghi] : fonti) {
                    std::vector<PointDayProposal> per_giorno;
                    bool qualcuno = false;
                    for (const Weekday w : giorni) {
                        PointDayProposal g;
                        g.weekday = w;
                        g.user_time = orari ? point_user_time(u, lg.leg, first_weekday_from(from, w)) : std::nullopt;
                        for (const Code& l : luoghi)
                            for (const PointCandidate& c :
                                 point_candidates(p, idx, cal, list, u, lg.leg, pl->direction, lg.company->code, l, w, from, g.user_time))
                                if (std::find(g.candidates.begin(), g.candidates.end(), c) == g.candidates.end()) g.candidates.push_back(c);
                        qualcuno = qualcuno || !g.candidates.empty();
                        if (orari || !g.candidates.empty()) per_giorno.push_back(std::move(g));
                    }
                    if (!qualcuno) continue;
                    x.source = fonte;
                    x.places = luoghi;
                    x.days = std::move(per_giorno);
                    break;
                }
                // Le regole pronte: i giorni con un candidato solo. Lo stesso candidato in tutti i giorni (e nessun giorno senza o con
                // piu' candidati): una regola "sempre".
                std::vector<std::pair<PointCandidate, Weekdays>> gruppi;
                bool tutti_uguali = !x.days.empty();
                for (const PointDayProposal& g : x.days) {
                    if (g.candidates.size() != 1) {
                        tutti_uguali = false;
                        continue;
                    }
                    const PointCandidate& c = g.candidates.front();
                    auto it = std::find_if(gruppi.begin(), gruppi.end(), [&](const auto& y) { return y.first == c; });
                    if (it == gruppi.end()) {
                        gruppi.push_back({c, Weekdays{}});
                        it = gruppi.end() - 1;
                    }
                    it->second.add(g.weekday);
                }
                if (gruppi.size() != 1) tutti_uguali = false;
                for (const auto& [c, w] : gruppi) {
                    PointRule r;
                    r.all_days = tutti_uguali;
                    if (!tutti_uguali) r.days = w;
                    r.card_id = c.card->id;
                    r.place = Ref{c.place, {}};
                    r.occurrence = c.occurrence;
                    r.version = c.version->number;
                    x.ready.push_back(r);
                }
                out.push_back(std::move(x));
            }
        }
    }
    return out;
}

// --- Il confronto con le schede ----------------------------------------------------------------------------------------------

namespace {

Param user_param(const ListUser& u) { return ref_param("utente", Element::ListUser, u.code); }
Param card_param(const ServiceCard& s) { return ref_param("scheda", Element::ServiceCard, s.code); }

struct DayCounts {
    std::vector<ComparedPassage> passages;
    std::vector<Code> not_attending, not_counted;
};

std::string signature(const DayCounts& c) {
    std::string s;
    const auto lista = [&](const std::vector<Code>& v) {
        for (const Code& x : v) s += x + ",";
        s += ";";
    };
    for (const ComparedPassage& p : c.passages) {
        lista(p.boarding);
        lista(p.alighting);
        lista(p.doubles);
        lista(p.doubles_same_card);
    }
    lista(c.not_attending);
    lista(c.not_counted);
    return s;
}

void ordina(std::vector<Code>& v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

class Confronto {
public:
    Confronto(const Package& p, const Index& idx, DateRange range, const std::string& only_card)
        : p_(p), idx_(idx), range_(range), only_(only_card), cal_(p, idx) {
        for (const ServiceCard& s : p.service_cards) per_id_[s.id] = &s;
    }

    PointsComparison run() {
        // I giorni in cui gira ogni versione delle schede scelte, dentro il periodo e l'anno scolastico dell'elenco del committente.
        for (const ServiceCard& s : p_.service_cards) {
            if (!scelta(s)) continue;
            const ClientList* l = idx_.client_list(s.client.code);
            if (!l) continue;
            const DateRange anno = anno_di(*l);
            for (const CardVersion& v : s.versions) {
                const Day da = std::max({range_.from, anno.from, v.valid.from}), a = std::min({range_.to, anno.to, v.valid.to});
                for (Day d = da; d <= a; ++d)
                    if (cal_.runs(v, d)) {
                        DayCounts& c = giorni_[&v][d];
                        c.passages.resize(v.card.stops.size());
                        for (std::size_t i = 0; i < v.card.stops.size(); ++i) {
                            c.passages[i].card_boarding = card_boarding(v.card.stops[i]);
                            c.passages[i].card_alighting = card_alighting(v.card.stops[i]);
                        }
                    }
            }
        }
        for (const ClientList& l : p_.client_lists) elenco(l);
        PointsComparison out;
        for (const ServiceCard& s : p_.service_cards) {
            if (!scelta(s)) continue;
            for (const CardVersion& v : s.versions) {
                const auto it = giorni_.find(&v);
                if (it == giorni_.end()) continue;
                out.versions.push_back(versione(s, v, it->second));
                differenze(s, v, out.versions.back());
            }
        }
        out.problems = std::move(problemi_);
        return out;
    }

private:
    bool scelta(const ServiceCard& s) const { return only_.empty() || s.code == only_ || s.id == only_; }
    DateRange anno_di(const ClientList& l) const {
        const ReadingProfile* pr = idx_.reading_profile(l.profile.code);
        return school_year(l.first_year, pr ? pr->start_month : 9);
    }
    void add(Check c, int variant, const Source& where, std::vector<Param> params) {
        problemi_.push_back(Problem{c, variant, where, std::move(params)});
    }

    void elenco(const ClientList& l) {
        const ReadingProfile* profile = idx_.reading_profile(l.profile.code);
        if (!profile) return;
        const DateRange anno = anno_di(l);
        const Day da = std::max(range_.from, anno.from), a = std::min(range_.to, anno.to);
        for (const ListUser& u : l.users) {
            if (!u.in_plan()) continue;
            for (const ListUserLeg& lg : u.legs) {
                if (!lg.company) continue;
                const ProfileLeg* pl = profile->leg(lg.leg);
                if (!pl) continue;
                tratta(l, u, lg, *pl, da, a);
            }
        }
    }

    void tratta(const ClientList& l, const ListUser& u, const ListUserLeg& lg, const ProfileLeg& pl, Day da, Day a) {
        const UserPoints* up = u.points_of(lg.leg);
        const bool diritto_mai =
            !u.active && std::none_of(u.unlocks.begin(), u.unlocks.end(), [&](const ListUserUnlock& s) { return s.leg == lg.leg; });
        const bool orari = std::any_of(u.times.begin(), u.times.end(), [&](const ListUserTime& t) { return t.leg == lg.leg; });
        const Param tr = text_param("tratta", lg.leg);
        if (!diritto_mai && only_.empty()) {
            // D4: senza orari i giorni non si sanno (variante 0); con gli orari ma senza i giorni di scuola (nessuna scheda del
            // committente passa dalla sua scuola) nemmeno (variante 1). Mai giorni inventati.
            if (!orari) add(Check::PointsDaysUnknown, 0, u.src, {user_param(u), tr});
            else if (!scuola_nota(l, u)) add(Check::PointsDaysUnknown, 1, u.src, {user_param(u), tr, ref_param("scuola", Element::School, u.school.code)});
        }
        // Senza orari e senza nessun punto per la tratta: "senza punto" una volta sola (D4), non giorno per giorno.
        if (!orari && !diritto_mai && only_.empty()) {
            bool qualcuno = false;
            if (up)
                for (const PointEntry& e : up->history) qualcuno = qualcuno || (!e.replaced && !e.rules.empty());
            if (!qualcuno) add(Check::PointsMissing, 2, u.src, {user_param(u), tr});
        }
        for (Day d = da; d <= a; ++d) {
            const std::vector<const PointRule*> regole = up ? point_rules_on(*up, d) : std::vector<const PointRule*>{};
            const Travel viaggia = point_travel(u, l.client.code, lg.leg, d, cal_);
            const bool diritto = point_right(u, lg.leg, d);
            const NotAttending* nv = not_attending_on(u, d, lg.leg);
            if (regole.empty()) {
                if (viaggia == Travel::Yes && diritto && !nv && only_.empty())
                    add(Check::PointsMissing, u.active ? 0 : 1, u.src, {user_param(u), tr, days_param("giorni", d)});
                continue;
            }
            if (viaggia == Travel::No) continue;
            std::vector<std::pair<const CardVersion*, std::size_t>> contati;
            for (const PointRule* r : regole) {
                const auto it = per_id_.find(r->card_id);
                if (it == per_id_.end()) continue;   // scheda che non c'e' nel registro: K-CTR-184 variante 3, una volta (controlli dei dati)
                const ServiceCard& s = *it->second;
                if (!scelta(s)) continue;
                const Param sp = card_param(s);
                const CardVersion* v = card_version_on(s, d);
                if (!v) {
                    if (viaggia == Travel::Yes) add(Check::PointsNotValid, 2, r->src, {user_param(u), tr, sp, days_param("giorni", d)});
                    continue;
                }
                if (!cal_.runs(*v, d)) {
                    if (viaggia == Travel::Yes) add(Check::PointsNotValid, 1, r->src, {user_param(u), tr, sp, number_param("versione", v->number), days_param("giorni", d)});
                    continue;
                }
                const auto g = giorni_.find(v);
                DayCounts* dc = nullptr;
                if (g != giorni_.end()) {
                    const auto h = g->second.find(d);
                    if (h != g->second.end()) dc = &h->second;
                }
                const Param vp = number_param("versione", v->number);
                // Il punto deve essere coerente con l'utente e con la tratta (altrimenti non si conta).
                if (s.client.code != l.client.code) {
                    add(Check::PointsInconsistent, 0, r->src, {user_param(u), tr, sp, ref_param("committente", Element::Client, s.client.code), days_param("giorni", d)});
                    continue;
                }
                const Contract* ct = idx_.contract(s.contract.code);
                if (!ct || ct->company.code != lg.company->code) {
                    add(Check::PointsInconsistent, 1, r->src, {user_param(u), tr, sp, ref_param("azienda", Element::Company, lg.company->code), days_param("giorni", d)});
                    if (dc) dc->not_counted.push_back(u.code);
                    continue;
                }
                if (v->card.direction != pl.direction) {
                    add(Check::PointsInconsistent, 2, r->src, {user_param(u), tr, sp, vp, days_param("giorni", d)});
                    if (dc) dc->not_counted.push_back(u.code);
                    continue;
                }
                // Il passaggio (D2): lo stesso solo se il luogo c'e' lo stesso numero di volte della versione confermata; mai riagganciato.
                const CardVersion* confermata = card_version_number(s, r->version);
                const Param lp = ref_param("luogo", Element::Place, r->place.code);
                if (!confermata) {
                    add(Check::PointsPassageChanged, 1, r->src, {user_param(u), tr, sp, lp, number_param("volta", r->occurrence), number_param("confermata", r->version), vp, days_param("giorni", d)});
                    if (dc) dc->not_counted.push_back(u.code);
                    continue;
                }
                const int prima = card_place_count(confermata->card, r->place.code), ora = card_place_count(v->card, r->place.code);
                if (prima != ora) {
                    add(Check::PointsPassageChanged, 0, r->src, {user_param(u), tr, sp, lp, number_param("volta", r->occurrence), number_param("confermata", r->version),
                                                                  number_param("volte_prima", prima), vp, number_param("volte", ora), days_param("giorni", d)});
                    if (dc) dc->not_counted.push_back(u.code);
                    continue;
                }
                const std::optional<std::size_t> riga = card_passage(v->card, r->place.code, r->occurrence);
                if (!riga) {
                    add(Check::PointsNotValid, 0, r->src, {user_param(u), tr, sp, vp, lp, number_param("volta", r->occurrence), number_param("volte", ora), days_param("giorni", d)});
                    if (dc) dc->not_counted.push_back(u.code);
                    continue;
                }
                if (!diritto) {
                    add(Check::PointsSuspendedNoUnlock, 0, r->src, {user_param(u), tr, sp, days_param("giorni", d)});
                    if (dc) dc->not_counted.push_back(u.code);
                    continue;
                }
                if (nv) {
                    if (dc) dc->not_attending.push_back(u.code);
                    continue;
                }
                if (!dc) continue;   // fuori dal periodo del confronto
                const SchoolRow sr = school_row(v->card, *riga, u.school.code, idx_);
                const bool andata = v->card.direction == Direction::Outbound;
                std::vector<Code>& qui = andata ? dc->passages[*riga].boarding : dc->passages[*riga].alighting;
                if (std::find(qui.begin(), qui.end(), u.code) == qui.end()) qui.push_back(u.code);
                if (sr.row) {
                    // Alla scuola una volta per ogni passaggio contato (D7): la doppia tratta scende due volte, come la conta il committente.
                    (andata ? dc->passages[*sr.row].alighting : dc->passages[*sr.row].boarding).push_back(u.code);
                    dc->passages[*sr.row].school = true;
                    if (sr.coincidence) add(Check::PointsInconsistent, 4, r->src, {user_param(u), tr, sp, vp, days_param("giorni", d)});
                } else {
                    add(Check::PointsInconsistent, 3, r->src, {user_param(u), tr, sp, vp, ref_param("scuola", Element::School, u.school.code), days_param("giorni", d)});
                }
                contati.push_back({v, *riga});
            }
            // D7: contato su piu' passaggi lo stesso giorno = "doppio"; sulla stessa versione della scheda, "doppio sulla stessa scheda".
            if (contati.size() > 1)
                for (std::size_t i = 0; i < contati.size(); ++i) {
                    DayCounts& c = giorni_[contati[i].first][d];
                    c.passages[contati[i].second].doubles.push_back(u.code);
                    for (std::size_t j = 0; j < contati.size(); ++j)
                        if (j != i && contati[j].first == contati[i].first && contati[j].second != contati[i].second)
                            c.passages[contati[i].second].doubles_same_card.push_back(u.code);
                }
        }
    }

    bool scuola_nota(const ClientList& l, const ListUser& u) const {
        // Basta un giorno qualsiasi: nessun valore = nessuna scheda del committente passa dalla scuola.
        return cal_.school_day(l.client.code, u.school.code, range_.from).has_value();
    }

    ComparedVersion versione(const ServiceCard& s, const CardVersion& v, std::map<Day, DayCounts>& giorni) {
        ComparedVersion out;
        out.card = &s;
        out.version = &v;
        for (auto& [d, c] : giorni) {
            for (ComparedPassage& x : c.passages) {
                std::sort(x.boarding.begin(), x.boarding.end());   // senza togliere i doppi della scuola (D7)
                std::sort(x.alighting.begin(), x.alighting.end());
                ordina(x.doubles);
                ordina(x.doubles_same_card);
            }
            ordina(c.not_attending);
            ordina(c.not_counted);
        }
        // Per giorno della settimana; dentro, i giorni di fila (di quel giorno della settimana) con lo stesso risultato sono un periodo.
        for (int w = 0; w < 7; ++w) {
            ComparedPeriod* corrente = nullptr;
            std::string firma_corrente;
            for (const auto& [d, c] : giorni) {
                if (static_cast<int>(weekday_of(d)) != w) continue;
                const std::string firma = signature(c);
                if (!corrente || firma != firma_corrente) {
                    ComparedPeriod x;
                    x.weekday = static_cast<Weekday>(w);
                    x.passages = c.passages;
                    x.not_attending = c.not_attending;
                    x.not_counted = c.not_counted;
                    for (const ComparedPassage& q : x.passages)
                        x.differs = x.differs || static_cast<int>(q.boarding.size()) != q.card_boarding ||
                                    static_cast<int>(q.alighting.size()) != q.card_alighting;
                    out.periods.push_back(std::move(x));
                    corrente = &out.periods.back();
                    firma_corrente = firma;
                }
                corrente->days.push_back(d);
            }
        }
        return out;
    }

    // K-CTR-181 (una per versione: i passaggi, variante 0; la scuola di scarico, variante 1) e K-CTR-182 (per passaggio).
    void differenze(const ServiceCard& s, const CardVersion& v, const ComparedVersion& cv) {
        const bool andata = v.card.direction == Direction::Outbound;
        for (int variante = 0; variante < 2; ++variante) {
            std::set<std::size_t> passaggi;
            std::vector<Day> giorni;
            std::optional<std::tuple<std::size_t, Day, const ComparedPassage*>> primo;
            for (const ComparedPeriod& pe : cv.periods)
                for (std::size_t i = 0; i < pe.passages.size(); ++i) {
                    const ComparedPassage& q = pe.passages[i];
                    if (q.school != (variante == 1)) continue;
                    if (static_cast<int>(q.boarding.size()) == q.card_boarding && static_cast<int>(q.alighting.size()) == q.card_alighting) continue;
                    passaggi.insert(i);
                    giorni.insert(giorni.end(), pe.days.begin(), pe.days.end());
                    const Day d = pe.days.front();
                    if (!primo || std::make_pair(i, d) < std::make_pair(std::get<0>(*primo), std::get<1>(*primo))) primo = std::make_tuple(i, d, &q);
                }
            if (!primo) continue;
            const auto& [i, d, q] = *primo;
            (void)d;
            const CardStop& f = v.card.stops[i];
            Param g = days_param("giorni", giorni.front());
            g.days = giorni;
            std::sort(g.days.begin(), g.days.end());
            g.days.erase(std::unique(g.days.begin(), g.days.end()), g.days.end());
            add(Check::PointsCountsDiffer, variante, v.src,
                {card_param(s), number_param("versione", v.number), number_param("passaggi", static_cast<long long>(passaggi.size())),
                 ref_param("luogo", Element::Place, f.place), number_param("volta", card_occurrence(v.card, i)), time_param("orario", f.time),
                 number_param("scheda_salgono", q->card_boarding), number_param("salgono", static_cast<long long>(q->boarding.size())),
                 number_param("scheda_scendono", q->card_alighting), number_param("scendono", static_cast<long long>(q->alighting.size())), g});
        }
        for (const ComparedPeriod& pe : cv.periods)
            for (std::size_t i = 0; i < pe.passages.size(); ++i) {
                const ComparedPassage& q = pe.passages[i];
                if (q.school) continue;
                const int sulla_scheda = andata ? q.card_boarding : q.card_alighting;
                const std::size_t assegnati = andata ? q.boarding.size() : q.alighting.size();
                if (sulla_scheda <= 0 || assegnati > 0) continue;
                const CardStop& f = v.card.stops[i];
                for (const Day d : pe.days)
                    add(Check::PointsPassageEmpty, andata ? 0 : 1, v.src,
                        {card_param(s), number_param("versione", v.number), ref_param("luogo", Element::Place, f.place),
                         number_param("volta", card_occurrence(v.card, i)), time_param("orario", f.time), number_param("numero", sulla_scheda),
                         days_param("giorni", d)});
            }
    }

    const Package& p_;
    const Index& idx_;
    DateRange range_;
    std::string only_;
    PointsCalendar cal_;
    std::map<std::string, const ServiceCard*> per_id_;
    std::map<const CardVersion*, std::map<Day, DayCounts>> giorni_;
    std::vector<Problem> problemi_;
};

}  // namespace

PointsComparison compare_points(const Package& p, const Index& idx, DateRange range, const std::string& only_card) {
    return Confronto(p, idx, range, only_card).run();
}

// --- I valori dall'ingresso standard ----------------------------------------------------------------------------------------

namespace {

std::string pulito(const std::string& t) {
    std::size_t a = 0, b = t.size();
    while (a < b && (t[a] == ' ' || t[a] == '\t')) ++a;
    while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t')) --b;
    return t.substr(a, b - a);
}

std::vector<std::string> dividi(const std::string& t, char sep) {
    std::vector<std::string> out;
    std::size_t a = 0;
    for (;;) {
        const std::size_t b = t.find(sep, a);
        out.push_back(pulito(t.substr(a, b == std::string::npos ? std::string::npos : b - a)));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

bool codice_valido(const std::string& c) {
    if (c.empty() || c.size() > 64 || !std::isalnum(static_cast<unsigned char>(c[0]))) return false;
    for (char x : c)
        if (!std::isalnum(static_cast<unsigned char>(x)) && x != '-' && x != '_' && x != '.') return false;
    return true;
}

// "<scheda> / <luogo> [/ <volta>] [/ <giorni>]"
std::optional<PointLinesFault> leggi_punto(const std::string& v, PointLine& out) {
    const std::vector<std::string> parti = dividi(v, '/');
    if (parti.size() < 2 || parti.size() > 4 || parti[0].empty() || parti[1].empty() || parti[0].size() > 300) return PointLinesFault::Syntax;
    if (!codice_valido(parti[1])) return PointLinesFault::Syntax;
    out.card = parti[0];
    out.place = parti[1];
    std::size_t i = 2;
    if (i < parti.size() && !parti[i].empty() && std::isdigit(static_cast<unsigned char>(parti[i][0]))) {
        if (parti[i].size() > 2 || !std::all_of(parti[i].begin(), parti[i].end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
            return PointLinesFault::Number;
        const int n = std::stoi(parti[i]);
        if (n < 1 || n > 99) return PointLinesFault::Number;
        out.occurrence = n;
        ++i;
    }
    if (i < parti.size()) {
        if (i + 1 != parti.size()) return PointLinesFault::Syntax;
        const std::optional<Weekdays> g = parse_weekdays(parti[i]);
        if (!g) return PointLinesFault::Day;
        out.all_days = false;
        out.days = *g;
    }
    return std::nullopt;
}

}  // namespace

std::optional<Weekdays> parse_weekdays(const std::string& text) {
    static const char* nomi[] = {"LUNEDI", "MARTEDI", "MERCOLEDI", "GIOVEDI", "VENERDI", "SABATO", "DOMENICA"};   // come normalize_name
    Weekdays out;
    bool qualcuno = false;
    std::string t = text;
    std::replace(t.begin(), t.end(), ',', ' ');
    std::string parola;
    std::vector<std::string> parole;
    for (char c : t + " ") {
        if (c == ' ' || c == '\t') {
            if (!parola.empty()) parole.push_back(parola);
            parola.clear();
        } else {
            parola += c;
        }
    }
    for (const std::string& p : parole) {
        const std::string n = normalize_name(p);
        int trovato = -1;
        for (int i = 0; i < 7; ++i)
            if (n == nomi[i]) trovato = i;
        if (trovato < 0) return std::nullopt;
        out.add(static_cast<Weekday>(trovato));
        qualcuno = true;
    }
    if (!qualcuno) return std::nullopt;
    return out;
}

PointLines parse_point_lines(const std::vector<std::string>& lines, PointLinesMode mode) {
    PointLines out;
    const auto ferma = [&](PointLinesFault f, int riga) {
        out.fault = f;
        out.line = riga;
        return out;
    };
    std::set<std::string> visti;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const int riga = static_cast<int>(i) + 1;
        const std::string r = pulito(lines[i]);
        if (r.empty()) continue;
        const std::size_t due = r.find(':');
        if (mode == PointLinesMode::Assign && normalize_name(r) == "NESSUNO") {
            if (out.nobody) return ferma(PointLinesFault::RepeatedField, riga);
            out.nobody = true;
            continue;
        }
        if (due == std::string::npos) return ferma(PointLinesFault::UnknownField, riga);
        const std::string campo = pulito(r.substr(0, due)), valore = pulito(r.substr(due + 1));
        if (mode == PointLinesMode::Confirm) {
            // "<utente> <tratta>: <punto>"
            const std::vector<std::string> chi = dividi(campo, ' ');
            std::vector<std::string> pezzi;
            for (const std::string& x : chi)
                if (!x.empty()) pezzi.push_back(x);
            if (pezzi.size() != 2 || !codice_valido(pezzi[0]) || pezzi[1].empty() || pezzi[1].size() > 100) return ferma(PointLinesFault::Syntax, riga);
            PointLine x;
            x.user = pezzi[0];
            x.leg = pezzi[1];
            x.line = riga;
            if (const auto f = leggi_punto(valore, x)) return ferma(*f, riga);
            if (!x.occurrence) return ferma(PointLinesFault::Syntax, riga);   // le righe pronte scrivono sempre la volta
            out.points.push_back(x);
            continue;
        }
        const std::string nome = normalize_name(campo);
        const auto una_volta = [&](const std::string& c) { return visti.insert(c).second; };
        if (mode == PointLinesMode::Assign && nome == "PUNTO") {
            PointLine x;
            x.line = riga;
            if (const auto f = leggi_punto(valore, x)) return ferma(*f, riga);
            out.points.push_back(x);
        } else if (mode == PointLinesMode::Assign && nome == "CONSENSO") {
            if (!una_volta(nome)) return ferma(PointLinesFault::RepeatedField, riga);
            if (valore.empty() || valore.size() > 300) return ferma(PointLinesFault::Missing, riga);
            out.consent = valore;
        } else if (mode == PointLinesMode::NotAttending && nome == "MOTIVO") {
            if (!una_volta(nome)) return ferma(PointLinesFault::RepeatedField, riga);
            static const std::vector<std::pair<const char*, NotAttendingReason>> motivi = {
                {"FAMIGLIA", NotAttendingReason::Family}, {"ALTRO MEZZO", NotAttendingReason::OtherVehicle},
                {"ASSENTE", NotAttendingReason::Absent},
                {"ALTRO", NotAttendingReason::Other}};
            const std::string m = normalize_name(valore);
            for (const auto& [testo, motivo] : motivi)
                if (m == testo) out.reason = motivo;
            if (!out.reason) return ferma(PointLinesFault::Reason, riga);
        } else if (mode == PointLinesMode::NotAttending && nome == "NOTA") {
            if (!una_volta(nome)) return ferma(PointLinesFault::RepeatedField, riga);
            if (valore.size() > 2000) return ferma(PointLinesFault::Syntax, riga);
            out.note = valore;
        } else if (mode == PointLinesMode::Voice && nome == "CHI") {
            if (!una_volta(nome)) return ferma(PointLinesFault::RepeatedField, riga);
            if (valore.empty() || valore.size() > 300) return ferma(PointLinesFault::Missing, riga);
            out.who = valore;
        } else {
            return ferma(PointLinesFault::UnknownField, riga);
        }
    }
    if (mode == PointLinesMode::Assign) {
        if (out.nobody && (!out.points.empty() || !out.consent.empty())) return ferma(PointLinesFault::NobodyWithPoints, 0);
        if (!out.nobody && out.points.empty()) return ferma(PointLinesFault::Missing, 0);
    }
    if (mode == PointLinesMode::Confirm && out.points.empty()) return ferma(PointLinesFault::Missing, 0);
    if (mode == PointLinesMode::NotAttending && !out.reason) return ferma(PointLinesFault::Missing, 0);
    if (mode == PointLinesMode::Voice && out.who.empty()) return ferma(PointLinesFault::Missing, 0);
    return out;
}

// --- Controlli (kzero controlla) -------------------------------------------------------------------------------------------

namespace {

bool giorni_in_comune(bool tutti_a, const Weekdays& a, bool tutti_b, const Weekdays& b) {
    for (int i = 0; i < 7; ++i) {
        const Weekday w = static_cast<Weekday>(i);
        if ((tutti_a || a.has(w)) && (tutti_b || b.has(w))) return true;
    }
    return false;
}
bool periodi_in_comune(std::optional<Day> da1, std::optional<Day> a1, std::optional<Day> da2, std::optional<Day> a2) {
    const Day inizio1 = da1 ? *da1 : -1000000, fine1 = a1 ? *a1 : 1000000000;
    const Day inizio2 = da2 ? *da2 : -1000000, fine2 = a2 ? *a2 : 1000000000;
    return inizio1 <= fine2 && inizio2 <= fine1;
}
bool tratte_in_comune(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    if (a.empty() || b.empty()) return true;
    for (const std::string& x : a)
        if (std::find(b.begin(), b.end(), x) != b.end()) return true;
    return false;
}

class ControlliPunti {
public:
    ControlliPunti(const Package& p, const Index& idx, std::vector<Problem>& out) : p_(p), idx_(idx), out_(out) {}

    void run() {
        std::set<std::string> schede;
        for (const ServiceCard& s : p_.service_cards) schede.insert(s.id);
        for (const ClientList& l : p_.client_lists) {
            const ReadingProfile* profile = idx_.reading_profile(l.profile.code);
            for (const ListUser& u : l.users) utente(u, profile, schede);
        }
        stradari();
        const PointsComparison c = compare_points(p_, idx_, p_.manifest.period);
        out_.insert(out_.end(), c.problems.begin(), c.problems.end());
    }

private:
    void add(Check c, const Source& where, std::vector<Param> params, int variant = 0) {
        out_.push_back(Problem{c, variant, where, std::move(params)});
    }

    void utente(const ListUser& u, const ReadingProfile* profile, const std::set<std::string>& schede) {
        const Param up = ref_param("utente", Element::ListUser, u.code);
        // Tratte che il profilo non ha (K-CTR-140 variante 5), punti della stessa tratta due volte (6).
        std::set<std::string> viste;
        for (const UserPoints& p : u.points) {
            if (profile && !profile->leg(p.leg)) add(Check::ListInvalid, p.src, {up, text_param("tratta", p.leg)}, 5);
            if (!viste.insert(p.leg).second) add(Check::ListInvalid, p.src, {up, text_param("tratta", p.leg)}, 6);
            // La storia in ordine, con i dal tutti diversi tra le voci che contano (7).
            std::optional<Day> prima;
            for (const PointEntry& e : p.history) {
                if (e.replaced) continue;
                if (prima && e.from <= *prima) add(Check::ListInvalid, e.src, {up, text_param("tratta", p.leg), date_param("dal", e.from)}, 7);
                prima = e.from;
                // Regole della stessa voce con giorni in comune e lo stesso luogo (10): il punto sarebbe contato due volte.
                for (std::size_t i = 0; i < e.rules.size(); ++i)
                    for (std::size_t j = i + 1; j < e.rules.size(); ++j)
                        if (e.rules[i].place.code == e.rules[j].place.code && e.rules[i].card_id == e.rules[j].card_id &&
                            giorni_in_comune(e.rules[i].all_days, e.rules[i].days, e.rules[j].all_days, e.rules[j].days))
                            add(Check::ListInvalid, e.rules[j].src, {up, text_param("tratta", p.leg), date_param("dal", e.from)}, 10);
                if (point_entry_double(e) && e.consent.empty())
                    add(Check::PointsDoubleNoConsent, e.src, {up, text_param("tratta", p.leg), date_param("dal", e.from)});
                for (const PointRule& r : e.rules) {
                    if (!idx_.place(r.place.code)) add(Check::UnknownReference, r.place.src, {text_param("codice", r.place.code), elements_param("elementi", Element::Place)});
                    if (!schede.count(r.card_id))
                        add(Check::PointsNotValid, r.src, {up, text_param("tratta", p.leg), text_param("id", r.card_id)}, 3);
                }
            }
        }
        for (const NotAttending& n : u.not_attending)
            for (const std::string& l : n.legs)
                if (profile && !profile->leg(l)) add(Check::ListInvalid, n.src, {up, text_param("tratta", l)}, 5);
        // Periodi rovesciati (K-CTR-012) e che si sovrappongono (K-CTR-140 varianti 8 e 9).
        for (std::size_t i = 0; i < u.unlocks.size(); ++i) {
            const ListUserUnlock& a = u.unlocks[i];
            if (a.from && a.to && *a.to < *a.from) add(Check::InvertedPeriod, a.src, {date_param("dal", *a.from), date_param("al", *a.to)});
            for (std::size_t j = 0; j < i; ++j) {
                const ListUserUnlock& b = u.unlocks[j];
                if (a.leg == b.leg && giorni_in_comune(a.all_days, a.days, b.all_days, b.days) && periodi_in_comune(a.from, a.to, b.from, b.to)) {
                    add(Check::ListInvalid, a.src, {up, text_param("tratta", a.leg)}, 8);
                    break;
                }
            }
        }
        for (std::size_t i = 0; i < u.not_attending.size(); ++i) {
            const NotAttending& a = u.not_attending[i];
            if (a.to && *a.to < a.from) add(Check::InvertedPeriod, a.src, {date_param("dal", a.from), date_param("al", *a.to)});
            for (std::size_t j = 0; j < i; ++j) {
                const NotAttending& b = u.not_attending[j];
                if (tratte_in_comune(a.legs, b.legs) && periodi_in_comune(a.from, a.to, b.from, b.to)) {
                    add(Check::ListInvalid, a.src, {up}, 9);
                    break;
                }
            }
        }
    }

    void stradari() {
        std::map<Code, const Source*> aziende;
        for (const StreetTable& t : p_.street_tables) {
            if (!idx_.company(t.company.code))
                add(Check::UnknownReference, t.company.src, {text_param("codice", t.company.code), elements_param("elementi", Element::Company)});
            const auto [it, nuovo] = aziende.emplace(t.company.code, &t.src);
            if (!nuovo) add(Check::StreetTableInvalid, t.src, {ref_param("azienda", Element::Company, t.company.code)}, 2);
            std::set<std::string> righe;
            for (const StreetRow& r : t.rows) {
                if (r.school && !idx_.school(r.school->code))
                    add(Check::UnknownReference, r.school->src, {text_param("codice", r.school->code), elements_param("elementi", Element::School)});
                const Place* l = idx_.place(r.point.code);
                if (!l) add(Check::UnknownReference, r.point.src, {text_param("codice", r.point.code), elements_param("elementi", Element::Place)});
                else if (l->is_private) add(Check::StreetTableInvalid, r.src, {ref_param("azienda", Element::Company, t.company.code), text_param("punto", r.point.code)}, 3);
                if (r.school && r.grade) add(Check::StreetTableInvalid, r.src, {ref_param("azienda", Element::Company, t.company.code)}, 1);
                if (r.number_from && *r.number_to < *r.number_from)
                    add(Check::StreetTableInvalid, r.src, {ref_param("azienda", Element::Company, t.company.code), number_param("dal", *r.number_from), number_param("al", *r.number_to)}, 4);
                const std::string chiave = normalize_name(r.town) + "|" + normalize_name(r.hamlet) + "|" + normalize_name(r.street) + "|" +
                                           (r.number_from ? std::to_string(*r.number_from) + "-" + std::to_string(*r.number_to) : "") + "|" +
                                           std::to_string(static_cast<int>(r.side)) + "|" + (r.school ? r.school->code : "") + "|" +
                                           (r.grade ? std::to_string(static_cast<int>(*r.grade)) : "") + "|" + r.point.code;
                if (!righe.insert(chiave).second) add(Check::StreetTableInvalid, r.src, {ref_param("azienda", Element::Company, t.company.code)}, 0);
            }
        }
    }

    const Package& p_;
    const Index& idx_;
    std::vector<Problem>& out_;
};

}  // namespace

void check_points(const Package& p, const Index& idx, std::vector<Problem>& out) { ControlliPunti(p, idx, out).run(); }

}  // namespace kzero
