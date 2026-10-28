// K-Zero - orari di lavoro (K-011): giornate delle persone, linea del tempo, regimi che valgono, misure del lavoro,
// contratto della persona, scelte dell'azienda, rientri, problemi e resoconto (vedi orari.hpp). I tipi di regola delle leggi
// stanno in orari_regole.cpp.
#include "orari.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>

#include "orari_interno.hpp"

namespace kzero {

using namespace orari;

Day monday_of(Day d) { return d - static_cast<Day>(weekday_of(d)); }

RouteKm route_km(const Trip& t) {
    RouteKm r;
    if (t.total_loaded_km || t.total_empty_km) {
        r.known = true;
        r.km = t.total_loaded_km.value_or(0.0) + t.total_empty_km.value_or(0.0);
        return r;
    }
    r.known = t.passages.size() >= 2;
    for (std::size_t i = 1; i < t.passages.size(); ++i) {
        const Passage& p = t.passages[i];
        if (!p.loaded_km && !p.empty_km) r.known = false;
        r.km += p.loaded_km.value_or(0.0) + p.empty_km.value_or(0.0);
    }
    return r;
}

void judge_return(ReturnJudgment& r) {
    using O = ReturnJudgment::Outcome;
    using W = ReturnJudgment::Why;
    r.outcome = O::NotJudged;
    r.why = W::None;
    // Il fermo minimo non c'e': non giustificato, comunque si contino i trasferimenti.
    if (r.stop < r.min_stop) {
        r.outcome = O::NotJustified;
        r.why = W::StopBelowMin;
        return;
    }
    // Contando tutti i trasferimenti come "in piu'" (il diretto non e' mai negativo): il limite piu' alto.
    const bool factor_ok = static_cast<double>(r.stop) + 1e-9 >= r.factor * r.transfers_driving;
    const bool km_judged = !r.max_km || r.km_known;
    const bool km_ok = !r.max_km || (r.km_known && r.transfers_km <= *r.max_km + 1e-9);
    if (r.same_place) {
        // Il diretto e' zero: i trasferimenti sono proprio la guida e i km in piu'.
        if (!factor_ok) {
            r.outcome = O::NotJustified;
            r.why = W::StopBelowFactor;
        } else if (!km_judged) {
            r.why = W::NeedKm;
        } else if (!km_ok) {
            r.outcome = O::NotJustified;
            r.why = W::KmOverMax;
        } else {
            r.outcome = O::Justified;
        }
        return;
    }
    if (factor_ok && km_ok) {
        r.outcome = O::Justified;
        return;
    }
    r.why = factor_ok && !km_judged ? W::NeedKm : W::NeedDistances;
}

// --- Le giornate delle persone ------------------------------------------------------------------------------------------

PlanCollector::PlanCollector(const Package& p) {
    for (const Norms& n : p.norms)
        for (const ClockChange& c : n.clock_changes) {
            clock_days_.insert(c.day);
            clock_days_.insert(c.day - 1);   // un turno oltre la mezzanotte resta del giorno in cui comincia
        }
}

void PlanCollector::add(Day d, const std::vector<ShiftDay>& plan) {
    if (clock_days_.count(d))
        for (const ShiftDay& sd : plan) {
            if (sd.items.empty()) continue;
            ShiftSpan s{sd.shift, d, sd.items.front().start, sd.items.front().end};
            for (const PlannedItem& it : sd.items) {
                s.start = std::min(s.start, it.start);
                s.end = std::max(s.end, it.end);
            }
            near_clock_.push_back(s);
        }
    for (const ShiftDay& sd : plan) {
        if (!sd.person) continue;
        std::vector<PersonDay>& v = days_[sd.person];
        if (v.empty() || v.back().day != d) {
            v.emplace_back();
            v.back().day = d;
        }
        PersonDay& pd = v.back();
        pd.shifts.push_back(sd.shift);
        pd.shift_sources.push_back(sd.person_source);
        pd.day_assignment = pd.day_assignment || sd.person_source == PersonSource::Day;
        for (const PlannedItem& it : sd.items) {
            WorkItem w;
            w.shift = sd.shift;
            w.item = it.item;
            w.trip = it.trip;
            w.from = it.from;
            w.to = it.to;
            w.start = it.start;
            w.end = it.end;
            w.role = sd.shift->role;
            w.vehicle = sd.shift->role == ShiftRole::Driver ? sd.vehicle : sd.aboard;
            w.source = sd.person_source;
            w.uses_vehicle = it.uses_vehicle;
            pd.items.push_back(w);
        }
    }
}

PlanData PlanCollector::take(const Package& p) {
    PlanData data;
    data.near_clock_change = std::move(near_clock_);
    near_clock_.clear();
    std::vector<PersonPlan>& out = data.persons;
    for (const Person& x : p.persons) {
        const auto it = days_.find(&x);
        if (it == days_.end()) continue;
        PersonPlan pp;
        pp.person = &x;
        pp.days = std::move(it->second);
        for (PersonDay& pd : pp.days) {
            std::stable_sort(pd.items.begin(), pd.items.end(),
                             [](const WorkItem& a, const WorkItem& b) { return std::tie(a.start, a.end) < std::tie(b.start, b.end); });
            pd.start = pd.items.empty() ? 0 : pd.items.front().start;
            pd.end = pd.start;
            for (const WorkItem& w : pd.items) {
                pd.start = std::min(pd.start, w.start);
                pd.end = std::max(pd.end, w.end);
            }
        }
        out.push_back(std::move(pp));
    }
    days_.clear();
    return data;
}

// --- Strumenti comuni (orari_interno.hpp) -------------------------------------------------------------------------------

namespace orari {

long long sum_in(const std::vector<Seg>& segs, const std::vector<long long>& prefix, const std::vector<int>& weight, long long a,
                 long long b) {
    if (b <= a || segs.empty()) return 0;
    const auto i0 = std::partition_point(segs.begin(), segs.end(), [a](const Seg& s) { return s.b <= a; }) - segs.begin();
    const auto j0 = std::partition_point(segs.begin(), segs.end(), [b](const Seg& s) { return s.a < b; }) - segs.begin();
    if (i0 >= j0) return 0;
    const auto i = static_cast<std::size_t>(i0), j = static_cast<std::size_t>(j0);
    long long total = prefix[j] - prefix[i];
    if (segs[i].a < a) total -= weight[i] * (a - segs[i].a);
    if (segs[j - 1].b > b) total -= weight[j - 1] * (segs[j - 1].b - b);
    return total;
}

std::vector<Free> free_periods(const PersonCtx& c, int world, bool on_call_free) {
    std::vector<Free> out;
    long long start = c.h0;
    bool open_left = true;
    for (const Seg& s : c.segs) {
        const TimeCategory k = s.cat[static_cast<std::size_t>(world)];
        if (k == TimeCategory::Break || (on_call_free && k == TimeCategory::OnCall)) continue;
        if (s.a > start) out.push_back({start, s.a, open_left, false});
        open_left = false;
        start = std::max(start, s.b);
    }
    out.push_back({start, std::max(start, c.h1), open_left, true});
    return out;
}

bool applies(const RegimeData& rd, const PersonCtx& c, RuleFamily f, Day d, int world) {
    if (!c.in_period(d)) return false;
    const Certainty x = rd.fam[static_cast<std::size_t>(d - c.d0)][static_cast<std::size_t>(fam(f))];
    return world == 0 ? x == Certainty::Certain : x != Certainty::No;
}

bool applies_any(const RegimeData& rd, const PersonCtx& c, RuleFamily f, Day from, Day to, int world) {
    from = std::max(from, c.d0);
    to = std::min(to, c.d1);
    if (to < from) return false;
    const std::vector<int>& n = rd.valid_days[static_cast<std::size_t>(world)][static_cast<std::size_t>(fam(f))];
    return n[static_cast<std::size_t>(to - c.d0) + 1] - n[static_cast<std::size_t>(from - c.d0)] > 0;
}

bool has_day_assignment(const PersonCtx& c, Day from, Day to) {
    for (Day d = std::max(from, c.d0); d <= std::min(to, c.d1); ++d)
        if (const PersonDay* p = c.pd(d); p && p->day_assignment) return true;
    return false;
}

std::string source_text(const RuleSource& s, const RuleSource& regime_source) {
    const std::string doc = s.document.empty() ? regime_source.document : s.document;
    std::string t = doc;
    if (!s.point.empty()) t += (t.empty() ? "" : ", ") + s.point;
    return t;
}

Param personal(Param p, std::string hidden) {
    p.sensitivity = Param::Sensitivity::Personal;
    p.hidden = std::move(hidden);
    return p;
}

std::vector<Param> rule_params(const Regime& r, const WorkRule& w) {
    return {personal(text_param("regola", r.code + " " + w.code), "una regola sugli orari"),
            personal(text_param("fonte", source_text(w.source, r.source)), "riservata")};
}

}  // namespace orari

// --- Il controllo degli orari ---------------------------------------------------------------------------------------------

namespace {

// Senza un regime (mandato K-011, punto 4): guida, altre mansioni e disponibilita' sul posto, come la regola generale
// sull'orario di lavoro (essere sul posto a disposizione e' lavoro).
const std::vector<TimeCategory> kWithoutRegime = {TimeCategory::Driving, TimeCategory::OtherWork, TimeCategory::Availability};

template <class T>
Certainty level_of(const Applies<T>& a, const T& v) {
    if (!a.present) return Certainty::Certain;
    if (std::find(a.certain.begin(), a.certain.end(), v) != a.certain.end()) return Certainty::Certain;
    if (std::find(a.to_confirm.begin(), a.to_confirm.end(), v) != a.to_confirm.end()) return Certainty::ToConfirm;
    return Certainty::No;
}

// Una legge sugli orari: un regime di legge con regole sul lavoro, sulla guida o sui riposi (non basta, es., il limite delle
// giornate del lavoro a chiamata).
bool is_hours_law(const Regime& r) {
    return r.kind == RegimeKind::Law && std::any_of(r.rules.begin(), r.rules.end(), [](const WorkRule& w) {
               return w.family == RuleFamily::Work || w.family == RuleFamily::Driving || w.family == RuleFamily::Rest;
           });
}

bool has_trip_conditions(const Regime& r) {
    return r.service_kinds.present || r.vehicle || r.route || !r.exclusions.empty();
}

class HoursChecker {
public:
    HoursChecker(const Package& p, const Index& idx, std::vector<Problem>& out, const HoursRequest& req, HoursReport* rep)
        : p_(p), idx_(idx), out_(out), req_(req), rep_(rep) {
        for (const Depot& d : p.depots) depot_places_.insert(d.place.code);
    }

    void run(const PlanData& plans) {
        if (p_.manifest.period.to < p_.manifest.period.from) return;
        if (rep_) {
            rep_->from = req_.from;
            rep_->to = req_.to;
        }
        clock_changes(plans.near_clock_change);
        for (const PersonPlan& pp : plans.persons) person(pp);
        partial_problems();
        clock_problems();
    }

    // Cambio dell'ora: un turno che lo attraversa si controlla a mano (le durate sono differenze tra orari scritti). Vale per
    // i cambi delle norme dell'azienda del turno, anche senza persona.
    void clock_changes(const std::vector<ShiftSpan>& spans) {
        for (const ShiftSpan& s : spans) {
            const Norms* n = idx_.norms_of(s.shift->company.code);
            if (!n) continue;
            for (const ClockChange& ch : n->clock_changes) {
                const long long sa = abs_min(s.day, s.start), sb = abs_min(s.day, s.end);
                const long long ca = abs_min(ch.day, std::min(ch.from, ch.to)), cb = abs_min(ch.day, std::max(ch.from, ch.to));
                if (sa < cb && ca < sb) clock_.insert({s.shift, s.day});
            }
        }
    }

private:
    // --- Preparazione di una persona -------------------------------------------------------------------------------

    const CompanySettings* settings_of(const Shift* s) const { return s ? idx_.settings(s->company.code) : nullptr; }

    TimeCategory trip_category(ShiftRole r) const {
        switch (r) {
            case ShiftRole::Driver: return TimeCategory::Driving;
            case ShiftRole::SecondDriver: return TimeCategory::Availability;   // D.Lgs. 234/2007 art. 3 lett. b n. 2
            case ShiftRole::Escort: return TimeCategory::OtherWork;
        }
        return TimeCategory::OtherWork;
    }

    void build_segments(PersonCtx& c) const {
        long long cursor = c.h0;
        for (std::size_t di = 0; di < c.plan->days.size(); ++di) {
            const PersonDay& pd = c.plan->days[di];
            long long prev_end = -1;
            const WorkItem* prev = nullptr;
            for (std::size_t ii = 0; ii < pd.items.size(); ++ii) {
                const WorkItem& it = pd.items[ii];
                long long a = abs_min(pd.day, it.start), b = abs_min(pd.day, it.end);
                a = std::max(a, cursor);
                if (b <= a) continue;
                if (prev && a > prev_end) {   // sosta non scritta tra due elementi della giornata
                    Seg g;
                    g.a = prev_end;
                    g.b = a;
                    g.unwritten = true;
                    g.day = static_cast<int>(di);
                    g.place = prev->trip ? &prev->trip->passages[prev->to].place.code
                                         : (prev->item->activity.to_place ? &prev->item->activity.to_place->code
                                                                           : &prev->item->activity.place.code);
                    const CompanySettings* st = settings_of(prev->shift);
                    if (st && st->unwritten) {
                        const UnwrittenStops& u = *st->unwritten;
                        const TimeCategory k = u.over_minutes && g.b - g.a > *u.over_minutes ? u.category_over : u.category;
                        g.cat = cats(k, k);
                    } else {   // versione 1: per prudenza disponibilita'; nel mondo certo un'interruzione
                        g.cat = cats(TimeCategory::Break, TimeCategory::Availability);
                        g.assumed = true;
                    }
                    c.segs.push_back(g);
                }
                Seg s;
                s.a = a;
                s.b = b;
                s.day = static_cast<int>(di);
                s.item = static_cast<int>(ii);
                if (it.trip) {
                    const TimeCategory k = trip_category(it.role);
                    s.cat = cats(k, k);
                    s.place = &it.trip->passages[it.to].place.code;
                } else {
                    const ShiftActivity& act = it.item->activity;
                    s.place = &act.place.code;
                    const CompanySettings* st = settings_of(it.shift);
                    const ActivityType* t = st ? CompanySettings::find_in(st->activities, act.type.code) : nullptr;
                    if (t && t->time) {
                        // Un'attivita' di guida (es. trasferimento a vuoto) e' guida solo per chi guida: per il secondo autista e
                        // per l'assistente a bordo vale come la corsa.
                        const TimeCategory k = *t->time == TimeCategory::Driving ? trip_category(it.role) : *t->time;
                        s.cat = cats(k, k);
                    } else {   // tipo senza categoria: per prudenza altre mansioni (mai interruzione o riposo)
                        s.cat = cats(TimeCategory::Break, TimeCategory::OtherWork);
                        s.assumed = true;
                    }
                }
                c.segs.push_back(s);
                cursor = b;
                prev_end = b;
                prev = &it;
            }
        }
    }

    // Quanto vale per la persona il regime sulla corsa di un elemento: ruolo, tipo di servizio, mezzo, percorso, esclusioni.
    Certainty trip_level(const Regime& r, const Regime& tc, const WorkItem& it, ApplyReason& why) const {
        why.what = ApplyReason::What::Trip;
        why.trip = it.trip;
        why.shift = it.shift;
        why.role = it.role;
        why.role_level = level_of(r.roles, it.role);
        std::set<ServiceKind> kinds;
        for (const Ref& sr : it.trip->services)
            if (const Service* s = idx_.service(sr.code)) kinds.insert(s->kind);
        why.kinds.assign(kinds.begin(), kinds.end());
        // Inclusione per tipo di servizio: basta un tipo.
        Certainty inc = Certainty::Certain;
        if (tc.service_kinds.present) {
            inc = Certainty::No;
            for (ServiceKind k : kinds) inc = max_c(inc, level_of(tc.service_kinds, k));
        }
        const RouteKm km = route_km(*it.trip);
        why.km_known = km.known;
        why.km = km.km;
        // Esclusioni: ogni tipo della corsa deve essere escluso da una voce; la certezza e' la piu' bassa tra i tipi. Si
        // calcola due volte: con i km come sono e come se il percorso fosse noto e dentro il limite, per sapere se l'incertezza
        // viene dal tipo di servizio o dai km che mancano.
        const auto excluded = [&](bool km_as_known) {
            if (tc.exclusions.empty() || kinds.empty()) return Certainty::No;
            Certainty all = Certainty::Certain;
            for (ServiceKind k : kinds) {
                Certainty best = Certainty::No;
                for (const RegimeExclusion& e : tc.exclusions) {
                    if (std::find(e.kinds.begin(), e.kinds.end(), k) == e.kinds.end()) continue;
                    Certainty lv = e.to_confirm ? Certainty::ToConfirm : Certainty::Certain;
                    if (e.route_up_to_km) {
                        if (km.km > *e.route_up_to_km + 1e-9) lv = Certainty::No;   // i km noti superano gia' il limite
                        else if (!km.known && !km_as_known) lv = min_c(lv, Certainty::ToConfirm);
                    }
                    best = max_c(best, lv);
                }
                all = min_c(all, best);
            }
            return all;
        };
        const auto applies_after = [](Certainty ex) {
            return ex == Certainty::Certain ? Certainty::No : ex == Certainty::ToConfirm ? Certainty::ToConfirm : Certainty::Certain;
        };
        const Certainty by_kinds = applies_after(excluded(true)), by_all = applies_after(excluded(false));
        // Se il risultato cambia con i km, l'incertezza e' nel percorso (i km mancano), non nel tipo di servizio.
        why.kinds_level = by_all == by_kinds ? min_c(inc, by_kinds) : inc;
        if (by_all != by_kinds) why.route_level = Certainty::ToConfirm;   // i km mancanti decidono
        // Mezzo: persone trasportabili compreso il conducente.
        why.vehicle = it.vehicle;
        why.persons = it.vehicle ? persons_with_driver(*it.vehicle) : 0;
        if (tc.vehicle) {
            // Persone trasportabili compreso il conducente: con i soli posti a sedere il mezzo supera il limite con certezza;
            // solo contando anche i posti in piedi, da confermare (prudenza).
            const Certainty data_level = tc.vehicle->to_confirm ? Certainty::ToConfirm : Certainty::Certain;
            if (!it.vehicle) why.vehicle_level = Certainty::ToConfirm;   // mezzo non noto: non determinabile
            else if (it.vehicle->seats + 1 > tc.vehicle->persons_over) why.vehicle_level = data_level;
            else if (why.persons > tc.vehicle->persons_over) why.vehicle_level = Certainty::ToConfirm;
            else why.vehicle_level = Certainty::No;
        }
        // Percorso.
        if (tc.route) {
            const RouteCondition& rc = *tc.route;
            Certainty lv = rc.to_confirm ? Certainty::ToConfirm : Certainty::Certain;
            if (rc.up_to_km) {
                if (km.km > *rc.up_to_km + 1e-9) lv = Certainty::No;
                else if (!km.known) lv = min_c(lv, Certainty::ToConfirm);
            }
            if (rc.over_km) {
                if (km.known && km.km <= *rc.over_km + 1e-9) lv = Certainty::No;
                else if (!km.known && km.km <= *rc.over_km + 1e-9) lv = min_c(lv, Certainty::ToConfirm);
            }
            why.route_level = min_c(why.route_level, lv);
        }
        return min_c(min_c(why.role_level, min_c(inc, by_all)), min_c(why.vehicle_level, why.route_level));
    }

    void applicability(PersonCtx& c) const {
        if (!c.norms) return;
        const Person& x = *c.person;
        for (const Regime& r : c.norms->regimes) {
            RegimeData rd;
            rd.regime = &r;
            rd.trip_conditions = &r;
            if (r.like)
                for (const Regime& o : c.norms->regimes)
                    if (o.code == r.like->code && !o.like) rd.trip_conditions = &o;
            for (const WorkRule& w : r.rules) {
                if (w.type == RuleType::WorkCount && !rd.count) rd.count = &w;
                if (w.type == RuleType::DailyRest && w.measure == RestMeasure::From24hEndRest && !rd.daily_rest) rd.daily_rest = &w;
                if (w.type == RuleType::WeeklyRest && !rd.weekly_rest) rd.weekly_rest = &w;
            }
            rd.fam.assign(static_cast<std::size_t>(c.days()), {});
            rd.why.assign(static_cast<std::size_t>(c.days()), {});
            rd.day_level.assign(static_cast<std::size_t>(c.days()), Certainty::No);
            rd.trigger_level.assign(static_cast<std::size_t>(c.days()), Certainty::No);
            const Certainty employment = level_of(r.employments, x.employment);
            const Certainty agreement = r.agreements.present ? (x.agreement_code.empty() ? Certainty::No : level_of(r.agreements, x.agreement_code))
                                                             : Certainty::Certain;
            const Certainty person_level = min_c(employment, agreement);
            const bool trips = has_trip_conditions(*rd.trip_conditions);
            for (const PersonDay& pd : c.plan->days) {
                if (!c.in_period(pd.day)) continue;
                if ((r.valid_from && pd.day < *r.valid_from) || (r.valid_to && pd.day > *r.valid_to)) continue;
                const auto di = static_cast<std::size_t>(pd.day - c.d0);
                Certainty best = Certainty::No;
                std::vector<ApplyReason> reasons;
                reasons.reserve(pd.items.size() + 1);
                const bool day_with_trips = std::any_of(pd.items.begin(), pd.items.end(), [](const WorkItem& it) { return it.trip != nullptr; });
                if (trips && !day_with_trips) {
                    // Una giornata senza corse (es. riserva o reperibilita'): le condizioni sulla corsa non si possono
                    // valutare. Se la persona e il ruolo vanno bene, il regime vale da confermare: mai "nessuna legge", mai certo.
                    for (const Shift* s : pd.shifts) {
                        ApplyReason why;
                        why.what = ApplyReason::What::NoTrips;
                        why.shift = s;
                        why.role = s->role;
                        why.role_level = level_of(r.roles, s->role);
                        why.employment_level = employment;
                        why.agreement_level = agreement;
                        why.level = min_c(min_c(why.role_level, person_level), Certainty::ToConfirm);
                        why.trigger_day = pd.day;
                        best = max_c(best, why.level);
                        reasons.push_back(std::move(why));
                    }
                } else if (trips) {
                    for (const WorkItem& it : pd.items) {
                        if (!it.trip) continue;
                        ApplyReason why;
                        why.employment_level = employment;
                        why.agreement_level = agreement;
                        const Certainty t = min_c(trip_level(r, *rd.trip_conditions, it, why), person_level);
                        why.level = t;
                        why.trigger_day = pd.day;
                        best = max_c(best, t);
                        reasons.push_back(std::move(why));
                    }
                } else {
                    for (const Shift* s : pd.shifts) {
                        ApplyReason why;
                        why.what = ApplyReason::What::Shift;
                        why.shift = s;
                        why.role = s->role;
                        why.role_level = level_of(r.roles, s->role);
                        why.employment_level = employment;
                        why.agreement_level = agreement;
                        why.level = min_c(why.role_level, person_level);
                        why.trigger_day = pd.day;
                        best = max_c(best, why.level);
                        reasons.push_back(std::move(why));
                    }
                }
                if (r.to_confirm && best == Certainty::Certain) {
                    best = Certainty::ToConfirm;
                    ApplyReason why;
                    why.what = ApplyReason::What::RegimeToConfirm;
                    why.level = Certainty::ToConfirm;
                    reasons.push_back(std::move(why));
                }
                rd.day_level[di] = best;
                // Una giornata senza corse non fa scattare l'estensione alla settimana: la fa scattare solo una corsa (o, per
                // un regime senza condizioni sulla corsa, il turno).
                rd.trigger_level[di] = trips && !day_with_trips ? Certainty::No : best;
                rd.fam[di].fill(best);
                rd.why[di] = std::move(reasons);
            }
            // Estensione: una corsa che fa scattare il regime vale per tutta la settimana, per le famiglie indicate.
            if (r.week_extension)
                for (Day m = monday_of(c.d0); m <= c.d1; m += 7) {
                    Certainty best = Certainty::No;
                    Day trigger = 0;
                    for (Day d = std::max(m, c.d0); d <= std::min(m + 6, c.d1); ++d) {
                        const Certainty lv = rd.trigger_level[static_cast<std::size_t>(d - c.d0)];
                        if (lv > best) {
                            best = lv;
                            trigger = d;
                        }
                    }
                    if (best == Certainty::No) continue;
                    for (Day d = std::max(m, c.d0); d <= std::min(m + 6, c.d1); ++d) {
                        const auto di = static_cast<std::size_t>(d - c.d0);
                        bool raised = false;
                        for (RuleFamily f : r.extension_families) {
                            Certainty& v = rd.fam[di][static_cast<std::size_t>(fam(f))];
                            if (best > v) {
                                v = best;
                                raised = true;
                            }
                        }
                        if (raised) {
                            ApplyReason why;
                            why.what = ApplyReason::What::Week;
                            why.level = best;
                            why.week_from = m;
                            why.week_to = m + 6;
                            why.trigger_day = trigger;
                            why.families = r.extension_families;
                            rd.why[di].push_back(std::move(why));
                        }
                    }
                }
            c.regimes.push_back(std::move(rd));
        }
        like_extension(c);
        applicability_overrides(c);
        for (RegimeData& rd : c.regimes)
            for (int w = 0; w < kWorlds; ++w)
                for (int f = 0; f < 5; ++f) {
                    std::vector<int>& n = rd.valid_days[static_cast<std::size_t>(w)][static_cast<std::size_t>(f)];
                    n.assign(static_cast<std::size_t>(c.days()) + 1, 0);
                    for (int d = 0; d < c.days(); ++d) {
                        const Certainty x = rd.fam[static_cast<std::size_t>(d)][static_cast<std::size_t>(f)];
                        const bool yes = w == 0 ? x == Certainty::Certain : x != Certainty::No;
                        n[static_cast<std::size_t>(d) + 1] = n[static_cast<std::size_t>(d)] + (yes ? 1 : 0);
                    }
                }
    }

    // Un regime che prende le condizioni sulla corsa da un altro (come_regime, es. il D.Lgs. 234 dal Reg. 561) quando l'altro
    // vale per tutta la settimana (estensione, nota INL 61/2021): nei giorni della settimana senza le corse che lo fanno
    // valere, il regime vale da confermare (di base: che segua l'estensione non e' scritto, K-011 aggiunta 1) o con
    // certezza se i dati lo dicono (estensione_come_regime "vale"), per tutte le sue famiglie. Le corse che lo fanno
    // scattare sono quelle che fanno valere l'altro regime, con le condizioni sulla persona di questo.
    void like_extension(PersonCtx& c) const {
        for (RegimeData& rd : c.regimes) {
            const Regime& r = *rd.regime;
            const Regime& tc = *rd.trip_conditions;
            if (&tc == &r || !tc.week_extension) continue;
            const Certainty follow = r.like_extension_to_confirm ? Certainty::ToConfirm : Certainty::Certain;
            std::vector<RuleFamily> families;
            for (const WorkRule& w : r.rules)
                if (std::find(families.begin(), families.end(), w.family) == families.end()) families.push_back(w.family);
            std::sort(families.begin(), families.end());
            for (Day m = monday_of(c.d0); m <= c.d1; m += 7) {
                Certainty best = Certainty::No;
                Day trigger = 0;
                for (Day d = std::max(m, c.d0); d <= std::min(m + 6, c.d1); ++d) {
                    const Certainty lv = rd.trigger_level[static_cast<std::size_t>(d - c.d0)];
                    if (lv > best) {
                        best = lv;
                        trigger = d;
                    }
                }
                if (best == Certainty::No) continue;
                const Certainty level = min_c(best, follow);
                for (Day d = std::max(m, c.d0); d <= std::min(m + 6, c.d1); ++d) {
                    const auto di = static_cast<std::size_t>(d - c.d0);
                    bool raised = false;
                    for (Certainty& v : rd.fam[di])
                        if (level > v) {
                            v = level;
                            raised = true;
                        }
                    if (!raised) continue;
                    ApplyReason why;
                    why.what = ApplyReason::What::Week;
                    why.level = level;
                    why.week_from = m;
                    why.week_to = m + 6;
                    why.trigger_day = trigger;
                    why.families = families;
                    why.like = &tc;
                    why.follow = follow;
                    rd.why[di].push_back(std::move(why));
                }
            }
        }
    }

    void applicability_overrides(PersonCtx& c) const {
        // Esclusioni tra regimi: nei giorni (o nelle settimane, con l'estensione) in cui un regime vale con certezza, un altro
        // non vale per alcune famiglie, oppure vale solo da confermare.
        for (const RegimeData& rd : c.regimes) {
            const Regime& r = *rd.regime;
            for (const RegimeOverride& o : r.overrides) {
                RegimeData* target = nullptr;
                for (RegimeData& t : c.regimes)
                    if (t.regime->code == o.regime.code && &t != &rd) target = &t;
                if (!target) continue;
                for (Day d = c.d0; d <= c.d1; ++d) {
                    bool in_scope = false;
                    if (r.week_extension) {
                        const Day m = monday_of(d);
                        for (Day k = std::max(m, c.d0); k <= std::min(m + 6, c.d1); ++k)
                            in_scope = in_scope || rd.day_level[static_cast<std::size_t>(k - c.d0)] == Certainty::Certain;
                    } else {
                        in_scope = rd.day_level[static_cast<std::size_t>(d - c.d0)] == Certainty::Certain;
                    }
                    if (!in_scope) continue;
                    const auto di = static_cast<std::size_t>(d - c.d0);
                    bool changed = false;
                    for (RuleFamily f : o.families) {
                        Certainty& v = target->fam[di][static_cast<std::size_t>(fam(f))];
                        const Certainty nv = o.effect == ExclusionEffect::Exclude ? Certainty::No : min_c(v, Certainty::ToConfirm);
                        if (nv != v) {
                            v = nv;
                            changed = true;
                        }
                    }
                    if (changed) {
                        ApplyReason why;
                        why.what = ApplyReason::What::OverriddenBy;
                        why.other = &r;
                        why.families = o.families;
                        why.effect = o.effect;
                        why.level = o.effect == ExclusionEffect::Exclude ? Certainty::No : Certainty::ToConfirm;
                        target->why[di].push_back(std::move(why));
                    }
                }
            }
        }
    }

    // Misure del lavoro: la prima senza regime, poi una per regime con il conteggio.
    void measures(PersonCtx& c) const {
        const auto make = [&](const Regime* r, const WorkRule* count) {
            Measure m;
            m.regime = r;
            for (int w = 0; w < kWorlds; ++w) {
                const RuleParams* par = nullptr;
                if (count) par = w == 1 && count->prudent ? &*count->prudent : &count->certain;
                const std::vector<TimeCategory>& cats = par ? par->categories : kWithoutRegime;
                std::vector<int>& weight = m.weight[static_cast<std::size_t>(w)];
                std::vector<char>& full = m.stop_full[static_cast<std::size_t>(w)];
                weight.assign(c.segs.size(), 0);
                full.assign(c.segs.size(), 0);
                const auto counted = [&](std::size_t i) {
                    return std::find(cats.begin(), cats.end(), c.segs[i].cat[static_cast<std::size_t>(w)]) != cats.end();
                };
                std::size_t i = 0;
                while (i < c.segs.size()) {
                    if (counted(i)) {
                        weight[i] = 100;
                        ++i;
                        continue;
                    }
                    // Una sosta: i pezzi non contati di seguito nella stessa giornata.
                    std::size_t j = i;
                    long long len = 0;
                    while (j < c.segs.size() && !counted(j) && c.segs[j].day == c.segs[i].day &&
                           (j == i || c.segs[j].a == c.segs[j - 1].b)) {
                        len += c.segs[j].b - c.segs[j].a;
                        ++j;
                    }
                    const bool inside = i > 0 && j < c.segs.size() && c.segs[i - 1].day == c.segs[i].day && c.segs[j].day == c.segs[i].day;
                    for (std::size_t k = i; k < j; ++k) {
                        if (par && par->stops_up_to && inside && len <= *par->stops_up_to) {
                            weight[k] = 100;
                            full[k] = 1;
                            continue;
                        }
                        if (!par) continue;
                        const TimeCategory cat = c.segs[k].cat[static_cast<std::size_t>(w)];
                        const Place* pl = c.segs[k].place ? idx_.place(*c.segs[k].place) : nullptr;
                        const bool away = pl && !c.base_municipality.empty() && !pl->municipality.empty() &&
                                          pl->municipality != c.base_municipality;
                        const std::optional<IdleShare>& share = away ? par->idle_away : par->idle_home;
                        if (share && std::find(share->categories.begin(), share->categories.end(), cat) != share->categories.end())
                            weight[k] = share->percent;
                    }
                    i = j;
                }
                std::vector<long long>& pre = m.prefix[static_cast<std::size_t>(w)];
                pre.assign(c.segs.size() + 1, 0);
                for (std::size_t k = 0; k < c.segs.size(); ++k) pre[k + 1] = pre[k] + weight[k] * (c.segs[k].b - c.segs[k].a);
            }
            return m;
        };
        c.measures.push_back(make(nullptr, nullptr));
        for (RegimeData& rd : c.regimes)
            if (rd.count) {
                rd.measure = static_cast<int>(c.measures.size());
                c.measures.push_back(make(rd.regime, rd.count));
            }
        for (int w = 0; w < kWorlds; ++w) {
            const auto ws = static_cast<std::size_t>(w);
            std::vector<int>& dw = c.drive_weight[ws];
            std::vector<long long>& pre = c.drive_prefix[ws];
            dw.assign(c.segs.size(), 0);
            pre.assign(c.segs.size() + 1, 0);
            for (std::size_t k = 0; k < c.segs.size(); ++k) {
                dw[k] = c.segs[k].cat[ws] == TimeCategory::Driving ? 1 : 0;
                pre[k + 1] = pre[k] + dw[k] * (c.segs[k].b - c.segs[k].a);
            }
        }
    }

    // I motivi di un regime in un giorno per il resoconto: le corse con lo stesso esito e le stesse condizioni in un motivo
    // solo (con l'elenco delle corse), gli altri motivi ripetuti una volta sola; i dati di ogni corsa una volta sola nella
    // giornata. L'ordine e' quello della prima volta.
    static void report_reasons(const std::vector<ApplyReason>& why, std::vector<ApplyReason>& out, std::vector<TripOnDay>& trips) {
        for (const ApplyReason& w : why) {
            const bool trip = w.what == ApplyReason::What::Trip;
            if (trip && w.trip &&
                std::none_of(trips.begin(), trips.end(), [&](const TripOnDay& t) { return t.trip == w.trip && t.shift == w.shift; })) {
                TripOnDay t;
                t.trip = w.trip;
                t.shift = w.shift;
                t.role = w.role;
                t.kinds = w.kinds;
                t.vehicle = w.vehicle;
                t.persons = w.persons;
                t.km_known = w.km_known;
                t.km = w.km;
                trips.push_back(std::move(t));
            }
            const auto same = [&](const ApplyReason& v) {
                if (v.what != w.what || v.level != w.level) return false;
                if (trip)
                    return v.role_level == w.role_level && v.kinds_level == w.kinds_level && v.vehicle_level == w.vehicle_level &&
                           (v.vehicle != nullptr) == (w.vehicle != nullptr) && v.route_level == w.route_level &&
                           v.employment_level == w.employment_level && v.agreement_level == w.agreement_level;
                return v.trip == w.trip && v.shift == w.shift && v.other == w.other && v.week_from == w.week_from &&
                       v.families == w.families && v.effect == w.effect && v.like == w.like && v.follow == w.follow;
            };
            auto it = std::find_if(out.begin(), out.end(), same);
            if (it == out.end()) {
                ApplyReason r = w;
                r.kinds.clear();   // i dati della corsa stanno in DayHours::trips
                r.trips.clear();
                it = out.insert(out.end(), std::move(r));
            }
            if (trip && w.trip && std::find(it->trips.begin(), it->trips.end(), w.trip) == it->trips.end()) it->trips.push_back(w.trip);
        }
    }

    // --- Una persona ---------------------------------------------------------------------------------------------------

    void person(const PersonPlan& pp) {
        PersonCtx c;
        c.index = &idx_;
        c.person = pp.person;
        c.plan = &pp;
        c.norms = idx_.norms_of(pp.person->company.code);
        c.d0 = p_.manifest.period.from;
        c.d1 = p_.manifest.period.to;
        c.h0 = abs_min(c.d0, 0);
        c.h1 = abs_min(c.d1 + 1, 0);
        if (pp.person->base_place)
            if (const Place* pl = idx_.place(pp.person->base_place->code)) c.base_municipality = pl->municipality;
        c.day_of_index.assign(static_cast<std::size_t>(c.days()), -1);
        for (std::size_t i = 0; i < pp.days.size(); ++i)
            if (c.in_period(pp.days[i].day)) c.day_of_index[static_cast<std::size_t>(pp.days[i].day - c.d0)] = static_cast<int>(i);
        build_segments(c);
        c.day_segs.assign(pp.days.size(), {0, 0});
        for (std::size_t k = 0; k < c.segs.size(); ++k) {
            auto& r = c.day_segs[static_cast<std::size_t>(c.segs[k].day)];
            if (r.first == r.second) r.first = k;
            r.second = k + 1;
        }
        applicability(c);
        measures(c);

        const CompanySettings* settings = idx_.settings(pp.person->company.code);
        std::vector<Breach> found[kWorlds];
        std::vector<Breach> notices[kWorlds];
        std::vector<Breach> near;
        for (const RegimeData& rd : c.regimes)
            for (const WorkRule& w : rd.regime->rules) {
                if (w.type == RuleType::WorkCount) continue;
                // Il mondo 2 serve solo se una lettura prudente puo' cambiare il risultato.
                const bool readings = w.prudent || (rd.count && rd.count->prudent) || (rd.daily_rest && rd.daily_rest->prudent) ||
                                      (rd.weekly_rest && rd.weekly_rest->prudent);
                for (int world = 0; world < kWorlds; ++world) {
                    if ((world == 0 && (w.to_confirm || rd.regime->to_confirm)) || (world == kMiddle && !readings)) {
                        // Senza lettura certa: nel mondo certo la regola non si valuta (solo avvisi).
                    } else {
                        RuleOut o;
                        evaluate_rule(c, rd, w, world, settings, o);
                        for (Breach& b : o.breaches) {
                            b.rule = &w;
                            b.regime = rd.regime;
                            found[world].push_back(std::move(b));
                        }
                        for (Breach& b : o.notices) {
                            b.rule = &w;
                            b.regime = rd.regime;
                            notices[world].push_back(std::move(b));
                        }
                        if (world == 1)
                            for (Breach& b : o.near) {
                                b.rule = &w;
                                b.regime = rd.regime;
                                near.push_back(std::move(b));
                            }
                        if (world == 1)
                            for (Day d : o.partial) partial_[&w].push_back({c.person->code, d});
                        if (world == 1 && rep_) partial_days_[&w].insert(o.partial.begin(), o.partial.end());
                    }
                }
            }
        contract(c, found);
        choices(c, settings, found);
        // Le violazioni che restano con le letture certe (mondo 2; dove non si e' valutato, uguale al prudente).
        std::set<std::string> middle;
        for (const Breach& b : found[kMiddle]) middle.insert(b.key);
        for (const Breach& b : notices[kMiddle]) middle.insert(b.key);
        for (const Breach& b : notices[1])
            if (!((b.rule && b.rule->prudent) || has_prudent_readings(c, b.regime))) middle.insert(b.key);
        for (const Breach& b : found[1]) {
            const bool readings = (b.rule && b.rule->prudent) || has_prudent_readings(c, b.regime);
            if (!readings) middle.insert(b.key);
        }
        emit(c, found, notices, middle);
        for (Breach& b : near) emit_near(c, b);
        returns_and_more(c);
        if (rep_) report(c);
        partial_days_.clear();
    }

    // Il regime (o, senza regime, uno dei regimi che contano il lavoro) ha letture prudenti del conteggio o dei riposi.
    static bool has_prudent_readings(const PersonCtx& c, const Regime* regime) {
        for (const RegimeData& rd : c.regimes) {
            if (regime ? rd.regime != regime : rd.measure < 0) continue;
            if ((rd.count && rd.count->prudent) || (rd.daily_rest && rd.daily_rest->prudent) || (rd.weekly_rest && rd.weekly_rest->prudent))
                return true;
        }
        return false;
    }

    // Motivi di un avviso "da confermare": cosa e' incerto nell'unita' (giorni da from a to). by_readings = la violazione
    // sparisce con le letture certe (mondo 2): viene da una lettura prudente.
    std::vector<std::string> reasons(const PersonCtx& c, const Breach& b, bool by_readings) const {
        std::vector<std::string> out = b.extra_reasons;
        if (b.regime && b.regime->to_confirm) out.push_back("regime_da_confermare");
        if (b.rule && b.rule->to_confirm) out.push_back("regola_da_confermare");
        if (by_readings) out.push_back("lettura_prudente");
        const Day d_from = day_of(b.from), d_to = day_of(std::max(b.from, b.to - 1));
        // Una regola di un regime: l'incertezza di quel regime. Il contratto e le scelte sul lavoro (senza regime):
        // l'incertezza dei regimi da confermare che contano il lavoro in quei giorni. Le altre scelte (guida, ampiezza,
        // riposo, giorni di fila) non dipendono da nessun regime.
        if (b.regime || b.work_measure) {
            const RuleFamily f = b.rule ? b.rule->family : RuleFamily::Work;
            for (const RegimeData& rd : c.regimes) {
                if (b.regime ? rd.regime != b.regime : rd.measure < 0) continue;
                for (Day d = std::max(d_from, c.d0); d <= std::min(d_to, c.d1); ++d) {
                    const auto di = static_cast<std::size_t>(d - c.d0);
                    if (rd.fam[di][static_cast<std::size_t>(fam(f))] != Certainty::ToConfirm) continue;
                    const auto explain = [&out](const ApplyReason& why) {
                        if (why.what == ApplyReason::What::OverriddenBy && why.effect == ExclusionEffect::ToConfirm)
                            out.push_back("insieme_ad_altro_regime");
                        if (why.what == ApplyReason::What::RegimeToConfirm) out.push_back("regime_da_confermare");
                        if (why.what == ApplyReason::What::NoTrips) out.push_back("giornata_senza_corse");
                        if (why.what == ApplyReason::What::Week && why.like && why.follow == Certainty::ToConfirm)
                            out.push_back("estensione_come_regime");
                        if (why.level != Certainty::ToConfirm) return;
                        if (why.employment_level == Certainty::ToConfirm) out.push_back("rapporto");
                        if (why.agreement_level == Certainty::ToConfirm) out.push_back("contratto_collettivo");
                        if (why.role_level == Certainty::ToConfirm) out.push_back("ruolo");
                        if (why.kinds_level == Certainty::ToConfirm) out.push_back("tipo_di_servizio");
                        if (why.vehicle_level == Certainty::ToConfirm) out.push_back(why.vehicle ? "mezzo" : "mezzo_non_noto");
                        if (why.route_level == Certainty::ToConfirm) out.push_back("percorso_non_noto");
                    };
                    for (const ApplyReason& why : rd.why[di]) {
                        explain(why);
                        // Estensione alla settimana: l'incertezza viene dal giorno della corsa che la fa scattare.
                        if (why.what == ApplyReason::What::Week && why.level == Certainty::ToConfirm && c.in_period(why.trigger_day))
                            for (const ApplyReason& t : rd.why[static_cast<std::size_t>(why.trigger_day - c.d0)])
                                if (t.what != ApplyReason::What::Week) explain(t);
                    }
                }
            }
        }
        for (const Seg& s : c.segs)
            if (s.assumed && s.b > b.from && s.a < b.to) {
                out.push_back("categorie_presunte");
                break;
            }
        if (out.empty()) out.push_back("dati_incerti");   // non dovrebbe succedere: un avviso dice sempre perche'
        return out;
    }

    // Le violazioni diventano problemi: nel mondo certo errori, solo nel prudente avvisi con i motivi.
    void emit(const PersonCtx& c, std::vector<Breach>* found, std::vector<Breach>* notices, const std::set<std::string>& middle) {
        std::set<std::string> certain_keys;
        for (const Breach& b : found[0]) certain_keys.insert(b.key);
        for (const Breach& b : found[0]) add(c, b, b.certain_check, {});
        std::set<std::string> prudent_keys;
        for (const Breach& b : found[1])
            if (!certain_keys.count(b.key) && prudent_keys.insert(b.key).second)
                add(c, b, b.prudent_check, reasons(c, b, !middle.count(b.key)));
        // Avvisi (straordinario, riposo cumulato): solo dove non c'e' gia' una violazione della stessa unita'.
        std::set<std::string> done;
        for (int w = 0; w < 2; ++w)   // il mondo 2 non da' problemi
            for (const Breach& b : notices[w]) {
                if (prudent_keys.count(b.key) || certain_keys.count(b.key) || !done.insert("avviso|" + b.key).second) continue;
                add(c, b, b.certain_check, w == 0 ? b.extra_reasons : reasons(c, b, !middle.count(b.key)));
            }
    }

    // Quale legge vale per la persona e' un dato personale quando il regime dipende dal suo rapporto o dal suo contratto
    // collettivo: allora la regola, la fonte e i valori del problema sono personali; altrimenti comuni.
    static bool depends_on_person(const Regime* r) { return !r || r->employments.present || r->agreements.present; }

    static void common_if_law_of_all(std::vector<Param>& params, const Breach& b) {
        if (!b.regime || depends_on_person(b.regime)) return;
        for (Param& x : params) {
            x.sensitivity = Param::Sensitivity::Common;
            x.hidden.clear();
        }
    }

    void add(const PersonCtx& c, const Breach& b, Check check, std::vector<std::string> motives) {
        std::vector<Param> params;
        params.push_back(ref_param("persona", Element::Person, c.person->code));
        for (const Param& x : b.params) params.push_back(x);
        common_if_law_of_all(params, b);
        Param days;
        days.name = "giorni";
        days.kind = Param::Kind::Days;
        days.days = b.days.empty() ? std::vector<Day>{b.day} : b.days;
        params.push_back(days);
        params.push_back(reasons_param("motivi", motives));   // vuoto per gli errori: i loro testi non lo usano
        out_.push_back(Problem{check, b.variant, c.person->src, std::move(params)});
    }

    void emit_near(const PersonCtx& c, const Breach& b) {
        std::vector<Param> params;
        params.push_back(ref_param("persona", Element::Person, c.person->code));
        for (const Param& x : b.params) params.push_back(x);
        common_if_law_of_all(params, b);
        Param days;
        days.name = "giorni";
        days.kind = Param::Kind::Days;
        days.days = b.days.empty() ? std::vector<Day>{b.day} : b.days;
        params.push_back(days);
        out_.push_back(Problem{Check::DaysNearLimit, 0, c.person->src, std::move(params)});
    }

    // --- Contratto della persona (livello 2) e scelte dell'azienda e della persona (livello 3) -----------------------------

    // Lavoro di una giornata secondo il contratto (e per le scelte sul lavoro): i conteggi possibili sono quelli dei regimi
    // che valgono quel giorno, con certezza o da confermare, e, se nessun regime vale con certezza, anche quello senza regime
    // (guida, altre mansioni e disponibilita' sul posto). Mondo certo: il piu' basso (un errore solo se tutti i conteggi
    // possibili superano il limite, K-011 aggiunta 1); mondo prudente: il piu' alto.
    long long day_work(const PersonCtx& c, const PersonDay& pd, int world) const {
        const long long a = c.start_abs(pd), b = c.end_abs(pd);
        const auto ws = static_cast<std::size_t>(world);
        std::optional<long long> lo, hi;
        bool any_certain = false;
        for (const RegimeData& rd : c.regimes) {
            if (rd.measure < 0 || !c.in_period(pd.day)) continue;
            const Certainty lv = rd.fam[static_cast<std::size_t>(pd.day - c.d0)][static_cast<std::size_t>(fam(RuleFamily::Work))];
            if (lv == Certainty::No) continue;
            const Measure& m = c.measures[static_cast<std::size_t>(rd.measure)];
            const long long v = sum_in(c.segs, m.prefix[ws], m.weight[ws], a, b);
            any_certain = any_certain || lv == Certainty::Certain;
            lo = lo ? std::min(*lo, v) : v;
            hi = hi ? std::max(*hi, v) : v;
        }
        if (!any_certain) {
            const Measure& base = c.measures.front();
            const long long none = sum_in(c.segs, base.prefix[ws], base.weight[ws], a, b);
            lo = lo ? std::min(*lo, none) : none;
            hi = hi ? std::max(*hi, none) : none;
        }
        return world == 0 ? *lo : *hi;
    }

    struct Unit {
        Day from, to;   // giorni
        long long x100 = 0;
    };

    std::vector<Unit> units(const PersonCtx& c, RulePeriod period, int world) const {
        std::vector<Unit> out;
        if (period == RulePeriod::Workday) {
            for (const PersonDay& pd : c.plan->days)
                if (c.in_period(pd.day)) out.push_back({pd.day, pd.day, day_work(c, pd, world)});
            return out;
        }
        const auto sum = [&](Day from, Day to) {
            long long t = 0;
            for (Day d = std::max(from, c.d0); d <= std::min(to, c.d1); ++d)
                if (const PersonDay* pd = c.pd(d)) t += day_work(c, *pd, world);
            return t;
        };
        if (period == RulePeriod::Week) {
            for (Day m = monday_of(c.d0); m <= c.d1; m += 7) out.push_back({m, m + 6, sum(m, m + 6)});
        } else if (period == RulePeriod::Month) {
            int y = 0, mo = 0, dd = 0;
            ymd_from_day(c.d0, y, mo, dd);
            for (Day first = day_from_ymd(y, mo, 1); first <= c.d1;) {
                ymd_from_day(first, y, mo, dd);
                const Day next = mo == 12 ? day_from_ymd(y + 1, 1, 1) : day_from_ymd(y, mo + 1, 1);
                out.push_back({first, next - 1, sum(first, next - 1)});
                first = next;
            }
        }
        return out;
    }

    void contract(const PersonCtx& c, std::vector<Breach>* found) const {
        const Person& x = *c.person;
        for (const DatedHours& h : x.hours) {
            if (!h.max) continue;
            const std::pair<RulePeriod, std::optional<double>> limits[] = {
                {RulePeriod::Workday, h.max->day}, {RulePeriod::Week, h.max->week}, {RulePeriod::Month, h.max->month}};
            for (const auto& [period, hours] : limits) {
                if (!hours) continue;
                const long long limit = std::llround(*hours * 60.0);
                for (int w = 0; w < kWorlds; ++w)
                    for (const Unit& u : units(c, period, w)) {
                        if ((h.from && u.from < *h.from) || (h.to && u.from > *h.to)) continue;
                        if (u.x100 <= limit * 100) continue;
                        // Fuori dal periodo del pacchetto (settimane e mesi a cavallo) si conta solo cio' che c'e': il lavoro
                        // noto supera gia' il limite.
                        Breach b;
                        b.work_measure = true;
                        b.certain_check = Check::ContractHoursExceeded;
                        b.prudent_check = Check::ContractHoursToConfirm;
                        b.variant = period == RulePeriod::Workday ? 0 : period == RulePeriod::Week ? 1 : 2;
                        b.day = u.from;
                        b.from = abs_min(u.from, 0);
                        b.to = abs_min(u.to + 1, 0);
                        b.key = "contratto|" + std::to_string(b.variant) + "|" + std::to_string(u.from);
                        b.params = {personal(duration_param("misurato", (u.x100 + 99) / 100), "riservato"),
                                    personal(duration_param("limite", limit), "riservato")};
                        found[w].push_back(b);
                    }
            }
        }
    }

    // Le scelte che valgono per la persona: quelle dell'azienda, con quelle della persona al posto delle stesse voci.
    static std::vector<std::pair<const WorkChoice*, bool>> effective_choices(const CompanySettings* s, const Person& x) {
        std::vector<std::pair<const WorkChoice*, bool>> out;
        const auto same = [](const WorkChoice& a, const WorkChoice& b) { return a.type == b.type && a.period == b.period; };
        for (const WorkChoice& w : x.choices) out.push_back({&w, true});
        if (s)
            for (const WorkChoice& w : s->choices)
                if (std::none_of(x.choices.begin(), x.choices.end(), [&](const WorkChoice& y) { return same(y, w); }))
                    out.push_back({&w, false});
        return out;
    }

    void choices(const PersonCtx& c, const CompanySettings* settings, std::vector<Breach>* found) const {
        for (const auto& [w, of_person] : effective_choices(settings, *c.person)) {
            const auto push = [&, w = w, of_person = of_person](int world, int variant, Day from, Day to, long long measured,
                                                                long long limit) {
                Breach b;
                b.certain_check = Check::ChoiceLimitViolated;
                b.prudent_check = Check::ChoiceNotRespected;
                b.variant = variant;
                b.day = from;
                b.from = abs_min(from, 0);
                b.to = abs_min(to + 1, 0);
                b.day_assignment = has_day_assignment(c, from, to);
                b.work_measure = variant <= 2;   // lavoro massimo della giornata, della settimana, del mese
                // Varianti 0-7 per le scelte dell'azienda, 8-15 per quelle della persona (dati personali).
                b.variant = variant + (of_person ? 8 : 0);
                b.key = "scelta|" + std::to_string(b.variant) + "|" + std::to_string(from);
                b.params = {duration_param("misurato", measured), duration_param("limite", limit)};
                if (variant == 7) b.params = {number_param("misurato", measured), number_param("limite", limit)};
                if (of_person)
                    for (Param& p : b.params) p = personal(p, "riservato");
                // Una preferenza, o un limite su un'assegnazione del singolo giorno, e' sempre un avviso.
                const bool error = world == 0 && w->strength == ChoiceStrength::Limit && !b.day_assignment;
                b.extra_reasons = {w->strength == ChoiceStrength::Preference ? "preferenza"
                                   : b.day_assignment                        ? "assegnazione_del_giorno"
                                                                             : ""};
                if (b.extra_reasons.front().empty()) b.extra_reasons.clear();
                if (world == kMiddle) found[kMiddle].push_back(b);
                else if (error) found[0].push_back(b);
                else {
                    b.certain_check = Check::ChoiceNotRespected;   // mai un errore
                    found[1].push_back(b);
                }
            };
            for (int world = 0; world < kWorlds; ++world) {
                const auto ws = static_cast<std::size_t>(world);
                switch (w->type) {
                    case RuleType::MaxWork: {
                        const int variant = w->period == RulePeriod::Week ? 1 : w->period == RulePeriod::Month ? 2 : 0;
                        for (const Unit& u : units(c, w->period, world))
                            if (u.x100 > static_cast<long long>(w->value) * 100) push(world, variant, u.from, u.to, (u.x100 + 99) / 100, w->value);
                        break;
                    }
                    case RuleType::MaxDriving: {
                        if (w->period == RulePeriod::Week) {
                            for (Day m = monday_of(c.d0); m <= c.d1; m += 7) {
                                const long long d = sum_in(c.segs, c.drive_prefix[ws], c.drive_weight[ws], abs_min(m, 0), abs_min(m + 7, 0));
                                if (d > w->value) push(world, 4, m, m + 6, d, w->value);
                            }
                        } else {
                            for (const PersonDay& pd : c.plan->days) {
                                const long long d =
                                    sum_in(c.segs, c.drive_prefix[ws], c.drive_weight[ws], c.start_abs(pd), c.end_abs(pd));
                                if (d > w->value && c.in_period(pd.day)) push(world, 3, pd.day, pd.day, d, w->value);
                            }
                        }
                        break;
                    }
                    case RuleType::MaxSpan:
                        for (const PersonDay& pd : c.plan->days)
                            if (c.in_period(pd.day) && pd.end - pd.start > w->value) push(world, 5, pd.day, pd.day, pd.end - pd.start, w->value);
                        break;
                    case RuleType::DailyRest:
                        for (std::size_t i = 1; i < c.plan->days.size(); ++i) {
                            const PersonDay& a = c.plan->days[i - 1];
                            const PersonDay& b = c.plan->days[i];
                            const long long gap = c.start_abs(b) - c.end_abs(a);
                            if (c.in_period(b.day) && gap < w->value) push(world, 6, b.day, b.day, std::max(0LL, gap), w->value);
                        }
                        break;
                    case RuleType::ConsecutiveDays: {
                        int run = 0;
                        Day last = 0;
                        for (const PersonDay& pd : c.plan->days) {
                            run = run > 0 && pd.day == last + 1 ? run + 1 : 1;
                            last = pd.day;
                            if (run > w->value && c.in_period(pd.day)) push(world, 7, pd.day, pd.day, run, w->value);
                        }
                        break;
                    }
                    default: break;
                }
            }
        }
    }

    // --- Rientri, giornate senza legge, casi non trattati, cambio dell'ora --------------------------------------------

    bool is_return_place(const PersonCtx& c, const Code& place) const {
        if (depot_places_.count(place)) return true;
        return c.person->base_place && c.person->base_place->code == place;
    }

    void returns_and_more(PersonCtx& c) {
        const Person& x = *c.person;
        std::vector<Day> no_law;
        for (const PersonDay& pd : c.plan->days) {
            if (!c.in_period(pd.day)) continue;
            const auto di = static_cast<std::size_t>(pd.day - c.d0);
            // Nessuna legge sugli orari nei dati per la persona quel giorno.
            bool any = false;
            for (const RegimeData& rd : c.regimes)
                if (is_hours_law(*rd.regime))
                    for (Certainty lv : rd.fam[di]) any = any || lv != Certainty::No;
            if (!any) no_law.push_back(pd.day);
            // Casi non trattati: piu' autisti sulla stessa corsa (multipresenza) dove vale un regime che lo dice.
            for (const WorkItem& it : pd.items) {
                if (!it.trip || it.role != ShiftRole::SecondDriver) continue;
                for (const RegimeData& rd : c.regimes) {
                    if (rd.day_level[di] == Certainty::No) continue;
                    for (const UntreatedCaseEntry& u : rd.regime->untreated)
                        if (u.kind == UntreatedCase::MultiManning)
                        {
                            Param regime = text_param("regime", rd.regime->code);
                            Param fonte = text_param("fonte", source_text(u.source, rd.regime->source));
                            if (depends_on_person(rd.regime)) {
                                regime = personal(regime, "un regime");
                                fonte = personal(fonte, "riservata");
                            }
                            out_.push_back(Problem{Check::UntreatedLawCase, 0, it.shift->src,
                                                   {ref_param("turno", Element::Shift, it.shift->code), ref_param("corsa", Element::Trip, it.trip->code),
                                                    regime, fonte, days_param("giorni", pd.day)}});
                        }
                }
            }
            // Rientri.
            day_returns(c, pd, nullptr);
        }
        if (!no_law.empty() && is_employee(x.employment)) {
            Param days;
            days.name = "giorni";
            days.kind = Param::Kind::Days;
            days.days = no_law;
            out_.push_back(Problem{Check::EmployeeWithoutLaw, 0, x.src, {ref_param("persona", Element::Person, x.code), days}});
        }
    }

    // I rientri di una giornata: tra due corse, la persona va in una rimessa o in un ricovero, resta li' e torna.
    void day_returns(const PersonCtx& c, const PersonDay& pd, std::vector<ReturnJudgment>* report_out) {
        const Person& x = *c.person;
        std::vector<std::size_t> trips;
        for (std::size_t i = 0; i < pd.items.size(); ++i)
            if (pd.items[i].trip) trips.push_back(i);
        for (std::size_t t = 1; t < trips.size(); ++t) {
            const WorkItem& before = pd.items[trips[t - 1]];
            const WorkItem& after = pd.items[trips[t]];
            const Code& end_place = before.trip->passages[before.to].place.code;
            const Code& next_place = after.trip->passages[after.from].place.code;
            const Code* place = nullptr;
            Minutes arrival = 0, departure = 0;
            bool arrived = false, left = false;
            int driving = 0;
            double km = 0.0;
            bool km_known = true;
            for (std::size_t i = trips[t - 1] + 1; i < trips[t]; ++i) {
                const WorkItem& it = pd.items[i];
                const ShiftActivity& a = it.item->activity;
                if (!a.to_place) continue;   // un'attivita' sul posto
                // Un trasferimento: con il mezzo conta come guida a vuoto in piu'.
                if (it.uses_vehicle) {
                    driving += it.end - it.start;
                    if (a.empty_km) km += *a.empty_km;
                    else km_known = false;
                }
                const Code& to = a.to_place->code;
                if (arrived && !left && place && a.place.code == *place) {
                    departure = it.start;
                    left = true;
                }
                if (!arrived && to != end_place && is_return_place(c, to)) {
                    arrived = true;
                    place = &a.to_place->code;
                    arrival = it.end;
                }
            }
            if (!arrived || !left) continue;   // pausa sul posto: la chiave non la riguarda
            // La chiave: campo per campo, la persona vince sull'azienda (quella del turno della corsa prima).
            const CompanySettings* s = idx_.settings(before.shift->company.code);
            const ReturnKey* ck = s && s->return_key ? &*s->return_key : nullptr;
            const ReturnKey* pk = x.return_key ? &*x.return_key : nullptr;
            if (!ck && !pk) continue;
            ReturnJudgment r;
            r.before = before.trip;
            r.after = after.trip;
            r.place = *place;
            r.arrival = arrival;
            r.departure = departure;
            r.stop = departure - arrival;
            r.transfers_driving = driving;
            r.transfers_km = km;
            r.km_known = km_known;
            r.same_place = end_place == next_place;
            r.personal_key = pk && (pk->min_stop || pk->driving_factor || pk->max_empty_km || pk->strength);
            r.min_stop = (pk && pk->min_stop) ? *pk->min_stop : ck && ck->min_stop ? *ck->min_stop : 0;
            r.factor = (pk && pk->driving_factor) ? *pk->driving_factor : ck && ck->driving_factor ? *ck->driving_factor : 0.0;
            r.max_km = (pk && pk->max_empty_km) ? pk->max_empty_km : ck ? ck->max_empty_km : std::nullopt;
            r.strength = (pk && pk->strength) ? *pk->strength : ck && ck->strength ? *ck->strength : ChoiceStrength::Preference;
            r.where = pk ? pk->src : ck->src;
            judge_return(r);
            if (report_out) {
                report_out->push_back(r);
                continue;
            }
            if (r.outcome == ReturnJudgment::Outcome::Justified) continue;
            std::vector<Param> params = {ref_param("persona", Element::Person, x.code), ref_param("corsa", Element::Trip, r.before->code),
                                         ref_param("corsa_dopo", Element::Trip, r.after->code), ref_param("luogo", Element::Place, r.place),
                                         duration_param("fermo", r.stop), duration_param("guida", r.transfers_driving),
                                         decimal_param("fattore", r.factor),
                                         duration_param("servono", static_cast<long long>(std::ceil(r.factor * r.transfers_driving - 1e-9))),
                                         duration_param("fermo_minimo", r.min_stop), decimal_param("km", r.transfers_km),
                                         decimal_param("km_massimi", r.max_km.value_or(0.0)), days_param("giorni", pd.day)};
            if (r.personal_key)
                for (std::size_t k = 4; k + 1 < params.size(); ++k) params[k] = personal(params[k], "riservato");
            // Varianti: non giustificato per il fermo minimo (0), per il fattore (1), per i km (2); non giudicato: servono le
            // distanze (0) o i km (1).
            Check check = Check::ReturnNotJudged;
            int variant = r.why == ReturnJudgment::Why::NeedKm ? 1 : 0;
            if (r.outcome == ReturnJudgment::Outcome::NotJustified) {
                check = r.strength == ChoiceStrength::Limit && !pd.day_assignment ? Check::ReturnNotJustifiedLimit : Check::ReturnNotJustified;
                variant = r.why == ReturnJudgment::Why::StopBelowMin ? 0 : r.why == ReturnJudgment::Why::StopBelowFactor ? 1 : 2;
            }
            out_.push_back(Problem{check, variant, x.src, std::move(params)});
        }
    }

    // --- Problemi per regola (verifica parziale) e per turno (cambio dell'ora) --------------------------------------------

    void partial_problems() {
        // Una sola riga per regola: quante persone e in quali giorni (le finestre che escono dal periodo del pacchetto).
        std::vector<std::pair<const WorkRule*, const Regime*>> rules;
        for (const Norms& n : p_.norms)
            for (const Regime& r : n.regimes)
                for (const WorkRule& w : r.rules)
                    if (partial_.count(&w)) rules.push_back({&w, &r});
        for (const auto& [w, r] : rules) {
            const auto& v = partial_.at(w);
            std::set<Code> persons;
            Param days;
            days.name = "giorni";
            days.kind = Param::Kind::Days;
            for (const auto& [code, d] : v) {
                persons.insert(code);
                days.days.push_back(d);
            }
            std::sort(days.days.begin(), days.days.end());
            days.days.erase(std::unique(days.days.begin(), days.days.end()), days.days.end());
            out_.push_back(Problem{Check::PartialVerification, 0, w->src,
                                   {text_param("regola", r->code + " " + w->code), text_param("fonte", source_text(w->source, r->source)),
                                    number_param("persone", static_cast<long long>(persons.size())), days}});
        }
    }

    void clock_problems() {
        for (const auto& [s, d] : clock_)
            out_.push_back(Problem{Check::ClockChangeCrossed, 0, s->src, {ref_param("turno", Element::Shift, s->code), days_param("giorni", d)}});
    }

    // --- Resoconto per kzero ore ----------------------------------------------------------------------------------

    void report(PersonCtx& c) {
        if (!req_.persons.empty() && !req_.persons.count(c.person->code)) return;
        PersonHours ph;
        ph.person = c.person;
        for (const DatedHours& h : c.person->hours)
            if (h.planned && h.planned->week && (!h.from || *h.from <= req_.to) && (!h.to || *h.to >= req_.from))
                ph.planned_week = std::llround(*h.planned->week * 60.0);
        std::vector<Free> free0 = free_periods(c, 0, false);
        for (const PersonDay& pd : c.plan->days) {
            if (pd.day < req_.from || pd.day > req_.to) continue;
            DayHours dh;
            dh.pd = &pd;
            const long long a = c.start_abs(pd), b = c.end_abs(pd);
            for (const Seg& s : c.segs) {
                if (s.b <= a || s.a >= b) continue;
                const long long len = std::min(s.b, b) - std::max(s.a, a);
                dh.minutes[static_cast<std::size_t>(s.cat[0])] += len;
                if (s.assumed) dh.assumed += len;
                if (s.unwritten) dh.unwritten += len;
            }
            // Lavoro per regime (senza regime se nessuno conta il lavoro quel giorno).
            const auto di = static_cast<std::size_t>(pd.day - c.d0);
            for (const RegimeData& rd : c.regimes) {
                RegimeOnDay ro;
                ro.regime = rd.regime;
                ro.family = rd.fam[di];
                report_reasons(rd.why[di], ro.reasons, dh.trips);
                if (ro.applies() || !ro.reasons.empty()) dh.regimes.push_back(ro);
                if (!ro.applies() || rd.measure < 0) continue;
                const Measure& m = c.measures[static_cast<std::size_t>(rd.measure)];
                WorkMeasure wm;
                wm.regime = rd.regime;
                wm.name = rd.count->certain.measure_name;
                wm.certain_x100 = sum_in(c.segs, m.prefix[0], m.weight[0], a, b);
                wm.prudent_x100 = sum_in(c.segs, m.prefix[1], m.weight[1], a, b);
                for (std::size_t k = 0; k < c.segs.size(); ++k) {
                    const Seg& s = c.segs[k];
                    if (s.b <= a || s.a >= b) continue;
                    if (m.stop_full[0][k]) wm.stops_counted += s.b - s.a;
                    else if (m.weight[0][k] > 0 && m.weight[0][k] < 100) wm.idle_counted_x100 += m.weight[0][k] * (s.b - s.a);
                }
                dh.work.push_back(wm);
                for (const WorkRule& w : rd.regime->rules) {
                    if (w.type != RuleType::NightWork) continue;
                    NightCount nc;
                    nc.regime = rd.regime;
                    nc.rule = &w;
                    for (int world = 0; world < 2; ++world) {
                        const RuleParams& par = world == 1 && w.prudent ? *w.prudent : w.certain;
                        if (!par.band_from || !par.band_to) continue;
                        long long n = 0;
                        for (Day k = pd.day - 1; k <= pd.day + 1; ++k) {
                            const long long ba = abs_min(k, *par.band_from);
                            const long long bb = abs_min(k, *par.band_to <= *par.band_from ? *par.band_to + 1440 : *par.band_to);
                            const auto ws = static_cast<std::size_t>(world);
                            n += sum_in(c.segs, m.prefix[ws], m.weight[ws], std::max(a, ba), std::min(b, bb));
                        }
                        (world == 0 ? nc.certain : nc.prudent) = (n + 99) / 100;
                    }
                    dh.night.push_back(nc);
                }
            }
            if (dh.work.empty()) {
                const Measure& m = c.measures.front();
                WorkMeasure wm;
                wm.certain_x100 = sum_in(c.segs, m.prefix[0], m.weight[0], a, b);
                wm.prudent_x100 = sum_in(c.segs, m.prefix[1], m.weight[1], a, b);
                dh.work.push_back(wm);
            }
            bool any_law = false;
            for (const RegimeOnDay& ro : dh.regimes) any_law = any_law || (ro.applies() && is_hours_law(*ro.regime));
            dh.no_law = !any_law;
            // Riposo prima della giornata.
            for (const Free& f : free0)
                if (f.b == a || (f.a < a && f.b >= a)) {
                    dh.rest_before.known = !f.open_left;
                    dh.rest_before.minutes = a - f.a;
                    dh.rest_before.from_day = day_of(f.a);
                    dh.rest_before.from_time = static_cast<Minutes>(f.a - abs_min(dh.rest_before.from_day, 0));
                    dh.rest_before.after_on_call = std::any_of(c.segs.begin(), c.segs.end(), [&](const Seg& s) {
                        return s.b == f.a && s.cat[0] == TimeCategory::OnCall;
                    });
                }
            day_returns(c, pd, &dh.returns);
            for (const Shift* s : pd.shifts) dh.clock_change = dh.clock_change || clock_.count({s, pd.day});
            for (const auto& [w, ds] : partial_days_)
                if (ds.count(pd.day))
                    for (const RegimeData& rd : c.regimes)
                        for (const WorkRule& x : rd.regime->rules)
                            if (&x == w) dh.partial.push_back({rd.regime, w, pd.day});
            for (int k = 0; k < kTimeCategories; ++k) ph.total_minutes[static_cast<std::size_t>(k)] += dh.minutes[static_cast<std::size_t>(k)];
            ph.total_driving += dh.minutes[static_cast<std::size_t>(TimeCategory::Driving)];
            dh.contract_x100 = day_work(c, pd, 0);
            dh.contract_prudent_x100 = day_work(c, pd, 1);
            ph.total_work_x100 += dh.contract_x100;
            ++ph.workdays;
            ph.days.push_back(std::move(dh));
        }
        for (Day m = monday_of(req_.from); m <= req_.to; m += 7) {
            WeekHours wh;
            wh.monday = m;
            const long long a = abs_min(m, 0), b = abs_min(m + 7, 0);
            wh.driving = sum_in(c.segs, c.drive_prefix[0], c.drive_weight[0], a, b);
            for (const RegimeData& rd : c.regimes) {
                if (rd.measure < 0 || !applies_any(rd, c, RuleFamily::Work, m, m + 6, 1)) continue;
                const Measure& ms = c.measures[static_cast<std::size_t>(rd.measure)];
                WorkMeasure wm;
                wm.regime = rd.regime;
                wm.name = rd.count->certain.measure_name;
                wm.certain_x100 = sum_in(c.segs, ms.prefix[0], ms.weight[0], a, b);
                wm.prudent_x100 = sum_in(c.segs, ms.prefix[1], ms.weight[1], a, b);
                wh.work.push_back(wm);
            }
            if (wh.work.empty()) {
                const Measure& ms = c.measures.front();
                WorkMeasure wm;
                wm.certain_x100 = sum_in(c.segs, ms.prefix[0], ms.weight[0], a, b);
                wm.prudent_x100 = sum_in(c.segs, ms.prefix[1], ms.weight[1], a, b);
                wh.work.push_back(wm);
            }
            for (Day d = std::max(m, c.d0); d <= std::min(m + 6, c.d1); ++d)
                if (const PersonDay* pd = c.pd(d)) wh.contract_x100 += day_work(c, *pd, 0);
            for (const DatedHours& h : c.person->hours)
                if (h.planned && h.planned->week && (!h.from || *h.from <= m + 6) && (!h.to || *h.to >= m))
                    wh.planned = std::llround(*h.planned->week * 60.0);
            for (const Free& f : free0)
                if (f.b > a && f.a < b && f.len() >= wh.longest_rest) {
                    wh.longest_rest = f.len();
                    wh.rest_known = !(f.open_left || f.open_right);
                }
            ph.weeks.push_back(wh);
        }
        if (!ph.days.empty() || req_.persons.count(c.person->code)) rep_->persons.push_back(std::move(ph));
    }

    const Package& p_;
    const Index& idx_;
    std::vector<Problem>& out_;
    const HoursRequest& req_;
    HoursReport* rep_;
    std::set<Code> depot_places_;
    std::map<const WorkRule*, std::vector<std::pair<Code, Day>>> partial_;
    std::map<const WorkRule*, std::set<Day>> partial_days_;   // della persona in corso (resoconto)
    std::set<std::pair<const Shift*, Day>> clock_;
};

}  // namespace

void check_hours(const Package& p, const Index& idx, const PlanData& plans, std::vector<Problem>& out,
                 const HoursRequest& request, HoursReport* report) {
    HoursChecker(p, idx, out, request, request.report ? report : nullptr).run(plans);
}

}  // namespace kzero
