// K-Zero - orari di lavoro (K-011): i tipi di regola. Ogni tipo sa COME si conta; i numeri vengono dalla regola (dati), nella
// lettura del mondo in cui si valuta (certa o prudente). Le finestre che escono dal periodo del pacchetto si contano con i
// dati che ci sono: una violazione si dice solo se i dati noti bastano gia' a farla; altrimenti "verifica parziale", mai
// "regolare".
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <tuple>

#include "orari_interno.hpp"

namespace kzero::orari {

namespace {

// Giorno dello stesso mese tanti mesi dopo (o prima), con il giorno riportato all'ultimo del mese se serve.
Day add_months(Day d, int months) {
    int y = 0, m = 0, dd = 0;
    ymd_from_day(d, y, m, dd);
    int total = y * 12 + (m - 1) + months;
    const int ny = total >= 0 ? total / 12 : -((-total + 11) / 12);
    const int nm = total - ny * 12 + 1;
    static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (ny % 4 == 0 && ny % 100 != 0) || ny % 400 == 0;
    const int last = nm == 2 && leap ? 29 : len[nm - 1];
    return day_from_ymd(ny, nm, std::min(dd, last));
}

Day add_years(Day d, int years) { return add_months(d, 12 * years); }

struct Ctx {
    const PersonCtx& c;
    const RegimeData& rd;
    const WorkRule& rule;
    const RuleParams& par;
    int world;
    const CompanySettings* settings;
    RuleOut& out;

    std::size_t ws() const { return static_cast<std::size_t>(world); }
    const Measure& measure() const { return c.measures[static_cast<std::size_t>(rd.measure >= 0 ? rd.measure : 0)]; }
    bool ok(Day d) const { return applies(rd, c, rule.family, d, world); }
    bool ok_any(Day a, Day b) const { return applies_any(rd, c, rule.family, a, b, world); }
    long long work(long long a, long long b) const {   // centesimi di minuto
        const Measure& m = measure();
        return sum_in(c.segs, m.prefix[ws()], m.weight[ws()], a, b);
    }
    // Guida per le regole di guida e di interruzione del regime: con la guida delle corse escluse nei dati del regime (own_drive),
    // i pesi del regime; altrimenti tutta la guida.
    long long drive(long long a, long long b) const {
        if (rd.own_drive) return sum_in(c.segs, rd.drive_prefix[ws()], rd.drive_weight[ws()], a, b);
        return sum_in(c.segs, c.drive_prefix[ws()], c.drive_weight[ws()], a, b);
    }
    bool drives(std::size_t i) const { return rd.own_drive ? rd.drive_weight[ws()][i] != 0 : cat(i) == TimeCategory::Driving; }
    TimeCategory cat(std::size_t i) const { return c.segs[i].cat[ws()]; }

    Breach make(Check certain, Check prudent, int variant, Day day, long long from, long long to) const {
        Breach b;
        b.certain_check = certain;
        b.prudent_check = prudent;
        b.variant = variant;
        b.day = std::clamp(day, c.d0, c.d1);
        b.from = from;
        b.to = to;
        b.params = rule_params(*rd.regime, rule);
        b.key = rd.regime->code + "|" + rule.code + "|" + std::to_string(variant) + "|" + std::to_string(b.day);
        b.day_assignment = has_day_assignment(c, day_of(from), day_of(std::max(from, to - 1)));
        return b;
    }
    void partial(Day d) const { out.partial.push_back(std::clamp(d, c.d0, c.d1)); }
};

// Codici dei problemi per famiglia di regola: errore nel mondo certo, avviso nel prudente.
std::pair<Check, Check> checks_for(const WorkRule& r) {
    switch (r.type) {
        case RuleType::ContinuousDriving:
        case RuleType::MaxDriving: return {Check::LawDrivingViolated, Check::LawDrivingToConfirm};
        case RuleType::DailyRest: return {Check::LawDailyRestViolated, Check::LawDailyRestToConfirm};
        case RuleType::WeeklyRest: return {Check::LawWeeklyRestViolated, Check::LawWeeklyRestToConfirm};
        default: return {Check::LawWorkViolated, Check::LawWorkToConfirm};
    }
}

Param dur(const char* name, long long minutes) { return personal(duration_param(name, minutes), "riservato"); }
Param num(const char* name, long long n) { return personal(number_param(name, n), "riservato"); }
Param txt(const char* name, std::string t) { return personal(text_param(name, std::move(t)), "riservato"); }

long long ceil100(long long x100) { return x100 <= 0 ? 0 : (x100 + 99) / 100; }

// --- lavoro_massimo ------------------------------------------------------------------------------------------------------

void overtime(Ctx& x, int variant_base, Day day, long long from, long long to, long long v, long long normal, long long max) {
    if (!x.rule.overtime) return;
    const Overtime& o = *x.rule.overtime;
    Breach b = x.make(Check::LawOvertime, Check::LawOvertime, variant_base, day, from, to);
    b.key = x.rd.regime->code + "|" + x.rule.code + "|lavoro|" + std::to_string(b.day) + "|" + std::to_string(variant_base / 3);
    const SpecialNeed* need = nullptr;
    if (x.settings)
        for (const SpecialNeed& s : x.settings->special_needs)
            if (s.rule.code == x.rule.code) need = &s;
    int variant = variant_base;
    std::string text = o.notice, reference;
    if (!b.day_assignment) {
        if (need) {
            variant += 2;
            reference = need->reference;
        } else {
            variant += 1;
            text = o.recurring;
            if (o.recurring_to_confirm) b.extra_reasons.push_back("straordinario_ricorrente");
        }
    }
    b.variant = variant;
    b.params.push_back(dur("misurato", ceil100(v)));
    b.params.push_back(dur("normale", normal));
    b.params.push_back(dur("limite", max));
    b.params.push_back(txt("obbligo", text));
    b.params.push_back(txt("riferimento", reference));
    b.params.push_back(txt("misura", x.rd.count ? x.rd.count->certain.measure_name : ""));
    x.out.notices.push_back(b);
}

void average_work(Ctx& x) {
    const RuleParams& p = x.par;
    const long long lim = static_cast<long long>(*p.max_minutes) * 100;
    std::vector<Day> starts;
    long long worst = 0;
    const int longest = p.average_months ? 31 * *p.average_months + 1 : p.average_weeks ? 7 * *p.average_weeks : p.average_days.value_or(1);
    for (Day s = x.c.d0 - longest + 1; s <= x.c.d1; ++s) {
        const Day e = p.average_months ? add_months(s, *p.average_months)
                      : p.average_weeks ? s + 7 * *p.average_weeks
                                        : s + p.average_days.value_or(1);
        if (e <= x.c.d0) continue;
        if (!x.ok_any(s, e - 1)) continue;
        const long long v = x.work(abs_min(s, 0), abs_min(e, 0));
        const long long avg = v * 7 / std::max(1, e - s);   // media alla settimana, in centesimi di minuto
        const bool inside = s >= x.c.d0 && e - 1 <= x.c.d1;
        if (avg > lim) {
            starts.push_back(std::clamp(s, x.c.d0, x.c.d1));
            worst = std::max(worst, avg);
        } else if (!inside) {
            x.partial(s);
        }
    }
    if (starts.empty()) return;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    // Varianti: media su settimane (4), su mesi (11), su giorni (12).
    const int variant = p.average_months ? 11 : p.average_weeks ? 4 : 12;
    Breach b = x.make(ce, cp, variant, starts.front(), abs_min(starts.front(), 0), abs_min(starts.back() + 1, 0));
    b.key = x.rd.regime->code + "|" + x.rule.code + "|media";
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    b.days = starts;
    b.params.push_back(dur("misurato", ceil100(worst)));
    b.params.push_back(dur("limite", *p.max_minutes));
    b.params.push_back(num("periodo", p.average_months ? *p.average_months : p.average_weeks ? *p.average_weeks : p.average_days.value_or(0)));
    b.params.push_back(txt("misura", x.rd.count ? x.rd.count->certain.measure_name : ""));
    x.out.breaches.push_back(b);
}

void max_work(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.max_minutes) return;
    if (p.average_weeks || p.average_months || p.average_days) {
        average_work(x);
        return;
    }
    const long long lim = static_cast<long long>(*p.max_minutes) * 100;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    const auto check = [&](int variant, Day day, long long a, long long b, bool may_be_partial) {
        const long long v = x.work(a, b);
        if (v > lim) {
            Breach br = x.make(ce, cp, variant, day, a, b);
            br.params.push_back(dur("misurato", ceil100(v)));
            br.params.push_back(dur("limite", *p.max_minutes));
            br.params.push_back(txt("misura", x.rd.count ? x.rd.count->certain.measure_name : ""));
            br.key = x.rd.regime->code + "|" + x.rule.code + "|lavoro|" + std::to_string(std::clamp(day, x.c.d0, x.c.d1)) + "|" +
                     std::to_string(variant == 2 ? 1 : 0);
            x.out.breaches.push_back(br);
            return;
        }
        if (p.normal_minutes && v > static_cast<long long>(*p.normal_minutes) * 100)
            overtime(x, variant == 2 ? 3 : 0, day, a, b, v, *p.normal_minutes, *p.max_minutes);
        if (may_be_partial) x.partial(day);
    };
    switch (x.rule.period) {
        case RulePeriod::Workday:
            for (const PersonDay& pd : x.c.plan->days)
                if (x.c.in_period(pd.day) && x.ok(pd.day)) check(0, pd.day, x.c.start_abs(pd), x.c.end_abs(pd), false);
            break;
        case RulePeriod::Day24:
            for (const PersonDay& pd : x.c.plan->days)
                if (x.c.in_period(pd.day) && x.ok(pd.day)) {
                    const long long s = x.c.start_abs(pd);
                    check(1, pd.day, s, s + kDayMin, s + kDayMin > x.c.h1);
                }
            break;
        case RulePeriod::Week:
            for (Day m = monday_of(x.c.d0); m <= x.c.d1; m += 7)
                if (x.ok_any(m, m + 6)) check(2, m, abs_min(m, 0), abs_min(m + 7, 0), m < x.c.d0 || m + 6 > x.c.d1);
            break;
        case RulePeriod::Month: {
            int y = 0, mo = 0, dd = 0;
            ymd_from_day(x.c.d0, y, mo, dd);
            for (Day first = day_from_ymd(y, mo, 1); first <= x.c.d1; first = add_months(first, 1)) {
                const Day next = add_months(first, 1);
                if (x.ok_any(first, next - 1)) check(3, first, abs_min(first, 0), abs_min(next, 0), first < x.c.d0 || next - 1 > x.c.d1);
            }
            break;
        }
        default: break;
    }
}

// --- guida_continua --------------------------------------------------------------------------------------------------------

struct Piece {
    enum class Kind { Drive, Interruption, Other } kind = Kind::Other;
    long long a = 0, b = 0;
    int seg = -1;          // il pezzo di guida (per sapere la corsa)
    bool open_left = false, open_right = false;
};

std::vector<Piece> pieces(const Ctx& x, const std::vector<TimeCategory>& interrupting) {
    std::vector<Piece> out;
    const auto interr = [&](TimeCategory k) {
        return std::find(interrupting.begin(), interrupting.end(), k) != interrupting.end();
    };
    const auto push = [&](Piece p) {
        if (p.b <= p.a && !p.open_right) return;
        if (!out.empty() && out.back().kind == p.kind && p.kind == Piece::Kind::Interruption && out.back().b == p.a) {
            out.back().b = p.b;
            out.back().open_right = p.open_right;
            return;
        }
        out.push_back(p);
    };
    long long cursor = x.c.h0;
    bool first = true;
    for (std::size_t i = 0; i < x.c.segs.size(); ++i) {
        const Seg& s = x.c.segs[i];
        if (s.a > cursor) push({Piece::Kind::Interruption, cursor, s.a, -1, first, false});
        first = false;
        Piece p;
        p.a = s.a;
        p.b = s.b;
        p.seg = static_cast<int>(i);
        const TimeCategory k = x.cat(i);
        // La guida che per il regime e' altre mansioni (corse escluse) non azzera e non aumenta il conto: un pezzo "altro".
        p.kind = x.drives(i)                                    ? Piece::Kind::Drive
                 : k == TimeCategory::Driving                   ? Piece::Kind::Other
                 : interr(k) || k == TimeCategory::Break        ? Piece::Kind::Interruption
                                                                : Piece::Kind::Other;
        push(p);
        cursor = std::max(cursor, s.b);
    }
    push({Piece::Kind::Interruption, cursor, std::max(cursor, x.c.h1), -1, first, true});
    return out;
}

// La corsa di un pezzo di guida e' di un servizio occasionale del regime? 2 = certo, 1 = da confermare, 0 = no. Ogni servizio
// della corsa deve esserlo; la certezza e' la piu' bassa tra i suoi tipi (K-012: es. l'NCC certo, "altro" da confermare). Un
// trasferimento a vuoto segue la corsa a cui serve, come per la guida delle corse escluse (orari.cpp, link_driving): quella
// dopo se arriva dove comincia, altrimenti quella prima se parte da dove finisce; se non si capisce, da confermare.
int occasional_trip(const Regime& r, const Index& idx, const Trip& t) {
    if (t.services.empty()) return 0;
    int level = 2;
    for (const Ref& sr : t.services) {
        const Service* sv = idx.service(sr.code);
        if (!sv) return 0;
        const auto has = [&](const std::vector<ServiceKind>& v) { return std::find(v.begin(), v.end(), sv->kind) != v.end(); };
        level = std::min(level, has(r.occasional.certain) ? 2 : has(r.occasional.to_confirm) ? 1 : 0);
    }
    return level;
}

int occasional(const Ctx& x, int seg) {
    const Regime& r = *x.rd.regime;
    if (seg < 0 || !r.occasional.present) return 0;
    const Seg& s = x.c.segs[static_cast<std::size_t>(seg)];
    if (s.item < 0) return 0;
    const PersonDay& pd = x.c.plan->days[static_cast<std::size_t>(s.day)];
    const auto ii = static_cast<std::size_t>(s.item);
    const WorkItem& it = pd.items[ii];
    const Index& idx = *x.c.index;
    if (it.trip) return occasional_trip(r, idx, *it.trip);
    std::optional<std::size_t> next, prev;
    for (std::size_t j = ii + 1; j < pd.items.size() && !next; ++j)
        if (pd.items[j].shift == it.shift && pd.items[j].trip) next = j;
    for (std::size_t j = ii; j-- > 0 && !prev;)
        if (pd.items[j].shift == it.shift && pd.items[j].trip) prev = j;
    if (!it.item) return 0;
    const ShiftActivity& a = it.item->activity;
    if (next && a.to_place && a.to_place->code == pd.items[*next].trip->passages[pd.items[*next].from].place.code)
        return occasional_trip(r, idx, *pd.items[*next].trip);
    if (prev && a.place.code == pd.items[*prev].trip->passages[pd.items[*prev].to].place.code)
        return occasional_trip(r, idx, *pd.items[*prev].trip);
    // Non si sa a quale corsa serve: da confermare, se almeno una corsa vicina e' di un servizio occasionale.
    const int vicine = std::max(next ? occasional_trip(r, idx, *pd.items[*next].trip) : 0,
                                prev ? occasional_trip(r, idx, *pd.items[*prev].trip) : 0);
    return std::min(1, vicine);
}

// Tutte le soste ai capolinea della giornata (tra due corse guidate) superano N minuti? (L. 138/1958 art. 5 c. 4)
bool terminus_exception(const Ctx& x, const PersonDay& pd, int over) {
    const WorkItem* prev = nullptr;
    int trips = 0;
    bool all = true;
    for (const WorkItem& it : pd.items) {
        if (!it.trip || it.role != ShiftRole::Driver) continue;
        if (prev) all = all && it.start - prev->end > over;
        prev = &it;
        ++trips;
    }
    (void)x;
    return trips >= 2 && all;
}

void continuous_driving(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.max_minutes) return;
    const long long max = *p.max_minutes;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    const std::vector<Piece> ps = pieces(x, p.categories);
    long long counter = 0, top = 0;
    int short_used = 0;
    std::vector<std::size_t> progress(p.resets.size(), 0);
    bool started = false, breached = false, first_period = true, first_unknown = false;
    int occ_min = 2;   // il piu' basso tra i pezzi di guida dall'ultimo azzeramento
    long long period_start = 0;
    const PersonDay* period_pd = nullptr;
    const auto close = [&](long long end) {
        if (started && breached && period_pd) {
            const bool exception = p.terminus_stops_over && terminus_exception(x, *period_pd, *p.terminus_stops_over);
            if (!(exception && x.world == 0) && x.ok(period_pd->day)) {
                Breach b = x.make(ce, cp, 0, period_pd->day, period_start, end);
                b.params.push_back(dur("misurato", top));
                b.params.push_back(dur("limite", max));
                b.params.push_back(personal(time_param("dalle", static_cast<Minutes>(period_start - abs_min(period_pd->day, 0))), "riservato"));
                if (exception) b.extra_reasons.push_back("eccezione_soste_capolinea");
                if (occ_min == 1 && x.world != 0) b.extra_reasons.push_back("servizi_occasionali");
                x.out.breaches.push_back(b);
            }
        } else if (started && first_period && first_unknown && period_pd && x.ok(period_pd->day)) {
            x.partial(period_pd->day);
        }
        started = breached = false;
        counter = top = 0;
        short_used = 0;
        occ_min = 2;
        std::fill(progress.begin(), progress.end(), 0);
        first_period = false;
    };
    for (const Piece& pc : ps) {
        switch (pc.kind) {
            case Piece::Kind::Drive: {
                if (!started) {
                    started = true;
                    period_start = pc.a;
                    const Seg& s = x.c.segs[static_cast<std::size_t>(pc.seg)];
                    period_pd = &x.c.plan->days[static_cast<std::size_t>(s.day)];
                }
                counter += pc.b - pc.a;
                top = std::max(top, counter);
                occ_min = std::min(occ_min, occasional(x, pc.seg));
                if (counter > max) breached = true;
                break;
            }
            case Piece::Kind::Interruption: {
                const long long len = pc.b - pc.a;
                if (pc.open_right) {
                    close(pc.a);
                    break;
                }
                bool reset = false;
                for (std::size_t k = 0; k < p.resets.size(); ++k) {
                    const BreakPattern& bp = p.resets[k];
                    if (bp.parts.empty()) continue;
                    // Le interruzioni dei soli servizi occasionali: nel mondo certo anche quelli da confermare.
                    if (bp.occasional_only && !(started && (x.world == 0 ? occ_min >= 1 : occ_min >= 2))) continue;
                    if (len >= bp.parts[progress[k]]) ++progress[k];
                    if (progress[k] >= bp.parts.size()) reset = true;
                }
                if (pc.open_left) first_unknown = !reset;
                if (reset) {
                    close(pc.a);
                    break;
                }
                if (started && p.short_counted_up_to && len <= *p.short_counted_up_to && short_used < p.short_counted_count.value_or(0)) {
                    counter += len;
                    ++short_used;
                    top = std::max(top, counter);
                    if (counter > max) breached = true;
                }
                break;
            }
            case Piece::Kind::Other: break;
        }
    }
}

// --- riposo_giornaliero ---------------------------------------------------------------------------------------------------

// La sequenza dei riposi giornalieri "entro 24 ore dalla fine del riposo precedente" (Reg. 561/2006 art. 8 c. 2): regolare,
// frazionato (parti nell'ordine), ridotto. Restituisce i riposi riconosciuti; con out aggiunge le violazioni.
std::vector<Free> rest_sequence(const PersonCtx& c, const RegimeData& rd, const WorkRule& rule, const RuleParams& p, int world,
                                Ctx* x) {
    std::vector<Free> rests;
    if (!p.min_minutes) return rests;
    const std::vector<Free> fr = free_periods(c, world, !p.on_call_interrupts);
    const long long regular = *p.min_minutes;
    const long long reduced = p.reduced_min.value_or(regular);
    // Soglia del riposo settimanale (per contare i ridotti tra due settimanali).
    std::optional<long long> weekly;
    if (rd.weekly_rest) {
        const RuleParams& wp = world == 1 && rd.weekly_rest->prudent ? *rd.weekly_rest->prudent : rd.weekly_rest->certain;
        if (wp.reduced_min) weekly = *wp.reduced_min;
        else if (wp.min_minutes) weekly = *wp.min_minutes;
    }
    if (fr.empty()) return rests;
    std::size_t i = 0;
    long long t = fr.front().open_left ? fr.front().b : c.h0;
    if (fr.front().open_left) {
        rests.push_back(fr.front());
        i = 1;
    }
    int reduced_count = 0;
    const std::pair<Check, Check> checks = checks_for(rule);
    const Check ce = checks.first, cp = checks.second;
    while (true) {
        const long long end = t + kDayMin;
        bool part1 = false, found = false, is_reduced = false;
        std::size_t k = i;
        long long longest = 0;
        for (; k < fr.size() && fr[k].a < end; ++k) {
            if (fr[k].a < t) continue;
            const long long portion = std::min(fr[k].b, end) - fr[k].a;
            longest = std::max(longest, portion);
            if (portion >= regular) found = true;
            else if (part1 && p.split.size() >= 2 && portion >= p.split[1]) found = true;
            else if (portion >= reduced) found = is_reduced = true;
            if (found) break;
            if (p.split.size() >= 2 && portion >= p.split[0]) part1 = true;
        }
        const Day day = day_of(t);
        if (found) {
            const Free& f = fr[k];
            rests.push_back(f);
            const bool is_weekly = weekly && f.len() >= *weekly;
            if (is_weekly) reduced_count = 0;
            else if (is_reduced) {
                ++reduced_count;
                if (x && p.reduced_times && reduced_count > *p.reduced_times && x->ok(day_of(f.a))) {
                    Breach b = x->make(ce, cp, 2, day_of(f.a), f.a, f.b);
                    b.params.push_back(num("misurato", reduced_count));
                    b.params.push_back(num("limite", *p.reduced_times));
                    b.params.push_back(dur("riposo", f.len()));
                    x->out.breaches.push_back(b);
                }
            }
            if (f.open_right) break;
            t = f.b;
            i = k + 1;
            continue;
        }
        // Nessun riposo entro 24 ore. Se la finestra esce dal periodo noto non si sa.
        const bool unknown = end > c.h1;
        if (x && x->ok(day)) {
            if (unknown) x->partial(day);
            else {
                Breach b = x->make(ce, cp, 1, day, t, end);
                b.params.push_back(dur("misurato", longest));
                b.params.push_back(dur("limite", reduced));
                b.params.push_back(personal(time_param("dalle", static_cast<Minutes>(t - abs_min(day, 0))), "riservato"));
                x->out.breaches.push_back(b);
            }
        }
        if (unknown) break;
        // Si riparte dal prossimo riposo vero (anche fuori dalla finestra).
        std::size_t n = i;
        while (n < fr.size() && (fr[n].a < t || fr[n].len() < reduced)) ++n;
        if (n >= fr.size()) break;
        rests.push_back(fr[n]);
        if (fr[n].open_right) break;
        t = fr[n].b;
        i = n + 1;
        if (weekly && fr[n].len() >= *weekly) reduced_count = 0;
    }
    return rests;
}

void daily_rest(Ctx& x) {
    const RuleParams& p = x.par;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    if (x.rule.measure == RestMeasure::From24hEndRest) {
        rest_sequence(x.c, x.rd, x.rule, p, x.world, &x);
        return;
    }
    if (!p.min_minutes) return;
    if (x.rule.measure == RestMeasure::BetweenWorkdays) {
        for (std::size_t i = 1; i < x.c.plan->days.size(); ++i) {
            const PersonDay& a = x.c.plan->days[i - 1];
            const PersonDay& b = x.c.plan->days[i];
            if (!x.c.in_period(b.day) || !x.ok(b.day)) continue;
            const long long gap = x.c.start_abs(b) - x.c.end_abs(a);
            if (gap < *p.min_minutes) {
                Breach br = x.make(ce, cp, 4, b.day, x.c.end_abs(a), x.c.start_abs(b));
                br.params.push_back(dur("misurato", std::max(0LL, gap)));
                br.params.push_back(dur("limite", *p.min_minutes));
                x.out.breaches.push_back(br);
            }
        }
        return;
    }
    // Nelle 24 ore dall'inizio della giornata un riposo ininterrotto (L. 138/1958 art. 7).
    const std::vector<Free> fr = free_periods(x.c, x.world, !p.on_call_interrupts);
    struct Value {
        Day day;
        long long rest;
        bool excluded;
    };
    std::vector<Value> values;
    for (const PersonDay& pd : x.c.plan->days) {
        if (!x.c.in_period(pd.day) || !x.ok(pd.day)) continue;
        const long long s = x.c.start_abs(pd), e = s + kDayMin;
        long long best = 0, best_len = 0;
        for (const Free& f : fr) {
            if (f.b <= s || f.a >= e) continue;
            const long long portion = std::min(f.b, e) - std::max(f.a, s);
            if (portion > best) {
                best = portion;
                best_len = f.len();
            }
        }
        const bool unknown = e > x.c.h1;
        if (best < *p.min_minutes) {
            if (unknown) x.partial(pd.day);
            else {
                Breach b = x.make(ce, cp, 0, pd.day, s, e);
                b.params.push_back(dur("misurato", best));
                b.params.push_back(dur("limite", *p.min_minutes));
                x.out.breaches.push_back(b);
            }
        }
        // Media: senza i riposi settimanali e senza le giornate con le interruzioni indicate.
        bool excluded = p.rest_average_exclude_from && best_len >= *p.rest_average_exclude_from;
        const auto [s0, s1] = x.c.day_segs[static_cast<std::size_t>(x.c.day_of_index[static_cast<std::size_t>(pd.day - x.c.d0)])];
        for (const ExceptBreaks& eb : p.rest_average_except) {
            int n = 0;
            long long run = 0;
            for (std::size_t k = s0; k <= s1; ++k) {
                const bool brk = k < s1 && x.cat(k) == TimeCategory::Break;
                if (brk) run += x.c.segs[k].b - x.c.segs[k].a;
                if (!brk) {
                    if (run >= eb.minutes) ++n;
                    run = 0;
                }
            }
            excluded = excluded || n >= eb.count;
        }
        values.push_back({pd.day, unknown ? -1 : best, excluded});
    }
    if (!p.rest_average_min || !p.rest_average_days) return;
    std::vector<Day> starts;
    long long worst = -1;
    const int n = *p.rest_average_days;
    for (Day s = x.c.d0 - n + 1; s <= x.c.d1; ++s) {
        long long sum = 0;
        int cnt = 0;
        bool unknown = s < x.c.d0 || s + n - 1 > x.c.d1;
        for (const Value& v : values) {
            if (v.day < s || v.day >= s + n || v.excluded) continue;
            if (v.rest < 0) {
                unknown = true;
                continue;
            }
            sum += v.rest;
            ++cnt;
        }
        if (cnt == 0) continue;
        const long long avg = sum / cnt;
        if (avg >= *p.rest_average_min) continue;
        if (unknown) {
            x.partial(s);
            continue;
        }
        starts.push_back(s);
        if (worst < 0 || avg < worst) worst = avg;
    }
    if (starts.empty()) return;
    Breach b = x.make(ce, cp, 3, starts.front(), abs_min(starts.front(), 0), abs_min(starts.back() + n, 0));
    b.key = x.rd.regime->code + "|" + x.rule.code + "|media";
    b.days = starts;
    b.params.push_back(dur("misurato", worst));
    b.params.push_back(dur("limite", *p.rest_average_min));
    b.params.push_back(num("periodo", n));
    x.out.breaches.push_back(b);
}

// --- guida_massima --------------------------------------------------------------------------------------------------------

void max_driving(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.max_minutes) return;
    const long long max = *p.max_minutes;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    const auto breach = [&](int variant, Day day, long long a, long long b, long long v, long long limit) {
        Breach br = x.make(ce, cp, variant, day, a, b);
        br.params.push_back(dur("misurato", v));
        br.params.push_back(dur("limite", limit));
        x.out.breaches.push_back(br);
    };
    switch (x.rule.period) {
        case RulePeriod::BetweenRests: {
            if (!x.rd.daily_rest) return;
            const RuleParams& rp = x.world == 1 && x.rd.daily_rest->prudent ? *x.rd.daily_rest->prudent : x.rd.daily_rest->certain;
            const std::vector<Free> rests = rest_sequence(x.c, x.rd, *x.rd.daily_rest, rp, x.world, nullptr);
            std::map<Day, int> extended;   // per settimana: periodi con l'estensione
            for (std::size_t i = 0; i <= rests.size(); ++i) {
                const long long a = i == 0 ? x.c.h0 : rests[i - 1].b;
                const long long b = i < rests.size() ? rests[i].a : x.c.h1;
                if (b <= a) continue;
                const long long v = x.drive(a, b);
                if (v == 0) continue;
                const Day day = day_of(a);
                if (!x.ok_any(day, day_of(b - 1))) continue;
                const bool edge = i == 0 || i == rests.size();
                if (v <= max) {
                    if (edge) x.partial(day);
                    continue;
                }
                const long long top = p.extension_max.value_or(max);
                if (v > top) {
                    breach(1, day, a, b, v, top);
                    continue;
                }
                const int n = ++extended[monday_of(day)];
                if (n > p.extension_times.value_or(0)) {
                    Breach br = x.make(ce, cp, 2, day, a, b);
                    br.params.push_back(dur("misurato", v));
                    br.params.push_back(dur("limite", max));
                    br.params.push_back(num("volte", n));
                    br.params.push_back(num("volte_massime", p.extension_times.value_or(0)));
                    x.out.breaches.push_back(br);
                } else if (edge) {
                    x.partial(day);
                }
            }
            break;
        }
        case RulePeriod::Workday:
            for (const PersonDay& pd : x.c.plan->days)
                if (x.c.in_period(pd.day) && x.ok(pd.day)) {
                    const long long v = x.drive(x.c.start_abs(pd), x.c.end_abs(pd));
                    if (v > max) breach(3, pd.day, x.c.start_abs(pd), x.c.end_abs(pd), v, max);
                }
            break;
        case RulePeriod::Week:
        case RulePeriod::TwoWeeks: {
            const int weeks = x.rule.period == RulePeriod::Week ? 1 : 2;
            for (Day m = monday_of(x.c.d0) - 7 * (weeks - 1); m <= x.c.d1; m += 7) {
                if (!x.ok_any(m, m + 7 * weeks - 1)) continue;
                const long long v = x.drive(abs_min(m, 0), abs_min(m + 7 * weeks, 0));
                if (v > max) breach(weeks == 1 ? 4 : 5, m, abs_min(m, 0), abs_min(m + 7 * weeks, 0), v, max);
                else if (m < x.c.d0 || m + 7 * weeks - 1 > x.c.d1) x.partial(m);
            }
            break;
        }
        case RulePeriod::Month: {
            int y = 0, mo = 0, dd = 0;
            ymd_from_day(x.c.d0, y, mo, dd);
            for (Day first = day_from_ymd(y, mo, 1); first <= x.c.d1; first = add_months(first, 1)) {
                const Day next = add_months(first, 1);
                if (!x.ok_any(first, next - 1)) continue;
                const long long v = x.drive(abs_min(first, 0), abs_min(next, 0));
                if (v > max) breach(6, first, abs_min(first, 0), abs_min(next, 0), v, max);
                else if (first < x.c.d0 || next - 1 > x.c.d1) x.partial(first);
            }
            break;
        }
        default: break;
    }
}

// --- lavoro_continuo (pause) ------------------------------------------------------------------------------------------------

void continuous_work(Ctx& x) {
    const RuleParams& p = x.par;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    const Measure& m = x.measure();
    const long long fraction = p.fraction_min.value_or(0);
    for (std::size_t di = 0; di < x.c.plan->days.size(); ++di) {
        const PersonDay& pd = x.c.plan->days[di];
        if (!x.c.in_period(pd.day) || !x.ok(pd.day)) continue;
        long long counter = 0, top = 0, pauses = 0, run = 0;
        const auto end_run = [&] {
            if (run >= fraction && run > 0) {
                pauses += run;
                counter = 0;
            }
            run = 0;
        };
        for (std::size_t k = x.c.day_segs[di].first; k < x.c.day_segs[di].second; ++k) {
            const Seg& s = x.c.segs[k];
            if (x.cat(k) == TimeCategory::Break) {
                run += s.b - s.a;
                continue;
            }
            end_run();
            counter += m.weight[x.ws()][k] * (s.b - s.a);
            top = std::max(top, counter);
        }
        end_run();
        const long long work = x.work(x.c.start_abs(pd), x.c.end_abs(pd));
        if (p.max_minutes && top > static_cast<long long>(*p.max_minutes) * 100) {
            Breach b = x.make(ce, cp, 5, pd.day, x.c.start_abs(pd), x.c.end_abs(pd));
            b.params.push_back(dur("misurato", ceil100(top)));
            b.params.push_back(dur("limite", *p.max_minutes));
            b.params.push_back(dur("frazione", fraction));
            x.out.breaches.push_back(b);
        }
        long long required = 0;
        for (const PauseNeed& n : p.pauses)
            if (work > static_cast<long long>(n.work_over) * 100) required = std::max<long long>(required, n.pause);
        if (pauses < required) {
            Breach b = x.make(ce, cp, 6, pd.day, x.c.start_abs(pd), x.c.end_abs(pd));
            b.params.push_back(dur("misurato", pauses));
            b.params.push_back(dur("limite", required));
            b.params.push_back(dur("lavoro", ceil100(work)));
            b.params.push_back(dur("frazione", fraction));
            x.out.breaches.push_back(b);
        }
    }
}

// --- riposo_settimanale ----------------------------------------------------------------------------------------------------

void weekly_rest(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.min_minutes) return;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    const long long regular = *p.min_minutes;
    const long long threshold = p.reduced_min.value_or(regular);
    const std::vector<Free> fr = free_periods(x.c, x.world, !p.on_call_interrupts);
    const Day m0 = monday_of(x.c.d0), mn = monday_of(x.c.d1);
    const int weeks = (mn - m0) / 7 + 1;
    const long long w0 = abs_min(m0, 0);
    const auto week_of = [&](long long t) { return static_cast<int>((t - w0) / kWeekMin); };
    struct Rest {
        long long a, b;
        int wa, wb;
        bool regular, cum;
    };
    std::vector<Rest> rests;
    for (const Free& f : fr) {
        if (f.len() < threshold || f.b <= w0) continue;
        Rest r;
        r.a = f.a;
        r.b = f.b;
        r.wa = std::clamp(week_of(std::max(f.a, w0)), 0, weeks - 1);
        r.wb = std::clamp(week_of(f.b - 1), 0, weeks - 1);
        r.regular = f.len() >= regular;
        r.cum = p.cumulation_weeks && *p.cumulation_weeks >= 2 && f.len() >= regular * *p.cumulation_weeks;
        rests.push_back(r);
    }
    std::vector<char> required(static_cast<std::size_t>(weeks), 0), edge(static_cast<std::size_t>(weeks), 0);
    for (int w = 0; w < weeks; ++w) {
        const Day m = m0 + 7 * w;
        required[static_cast<std::size_t>(w)] = x.ok_any(m, m + 6) ? 1 : 0;
        edge[static_cast<std::size_t>(w)] = (m < x.c.d0 || m + 6 > x.c.d1) ? 1 : 0;
    }
    if (!fr.empty() && fr.front().open_left && fr.front().len() < threshold) edge[static_cast<std::size_t>(std::clamp(week_of(fr.front().b), 0, weeks - 1))] = 1;
    if (!fr.empty() && fr.back().open_right && fr.back().len() < threshold) edge[static_cast<std::size_t>(std::clamp(week_of(fr.back().a), 0, weeks - 1))] = 1;
    // Programmazione dinamica settimana per settimana: ogni riposo conta in una sola settimana (Reg. 561/2006 art. 8 c. 9).
    // Stato all'inizio della settimana w: il riposo che entra da sinistra e' ancora da assegnare; la settimana prima ha un
    // riposo, uno regolare, un riposo lungo ancora da usare per il cumulo; la settimana prima aspetta un cumulo.
    // A parita' di violazioni e di cumuli vince la scelta con meno settimane coperte solo da un riposo ridotto: un riposo
    // ridotto conta come settimanale (e va compensato) solo se nella settimana non c'e' un riposo regolare.
    struct State {
        int breaches = 1 << 28, cumulations = 0, reduced = 0;
        int parent = -1;
        bool cov = false;
        std::vector<int> assigned;   // riposi assegnati a questa settimana
        std::vector<std::pair<int, int>> events;   // (tipo, settimana): 0 settimana senza riposo, 1 due settimane senza regolare, 2 cumulo
    };
    const auto idx = [](int carry, int cov, int reg, int cum, int pend) { return carry | (cov << 1) | (reg << 2) | (cum << 3) | (pend << 4); };
    std::vector<std::array<State, 32>> dp(static_cast<std::size_t>(weeks) + 1);
    dp[0][static_cast<std::size_t>(idx(0, 1, 1, 0, 0))].breaches = 0;   // prima del periodo: nessun vincolo
    std::size_t first_rest = 0;
    for (int w = 0; w < weeks; ++w) {
        // I riposi che toccano la settimana: L (entra da sinistra), interni, R (esce a destra).
        int L = -1, R = -1;
        std::vector<int> inner;
        while (first_rest < rests.size() && rests[first_rest].wb < w) ++first_rest;
        for (std::size_t r = first_rest; r < rests.size() && rests[r].wa <= w; ++r) {
            const Rest& z = rests[r];
            if (z.wa < w && z.wb >= w) L = static_cast<int>(r);
            if (z.wb > w && z.wa <= w) R = static_cast<int>(r);
            if (z.wa == w && z.wb == w) inner.push_back(static_cast<int>(r));
        }
        for (int s = 0; s < 32; ++s) {
            const State& st = dp[static_cast<std::size_t>(w)][static_cast<std::size_t>(s)];
            if (st.breaches >= (1 << 28)) continue;
            const int carry = s & 1, pcov = (s >> 1) & 1, preg = (s >> 2) & 1, pcum = (s >> 3) & 1, ppend = (s >> 4) & 1;
            // Scelte: L (se ancora da assegnare) qui o avanti (solo se continua); R qui o avanti.
            const bool has_l = L >= 0 && carry;
            const bool l_continues = has_l && L == R;
            for (int lc = 0; lc < (has_l ? (l_continues ? 2 : 1) : 1); ++lc) {
                const bool r_distinct = R >= 0 && R != L;
                for (int rc = 0; rc < (r_distinct ? 2 : 1); ++rc) {
                    std::vector<int> assigned = inner;
                    bool carry_next = false;
                    if (has_l) {
                        if (lc == 0) assigned.push_back(L);
                        else carry_next = true;
                    }
                    if (r_distinct) {
                        if (rc == 0) assigned.push_back(R);
                        else carry_next = true;
                    }
                    bool cov = false, reg = false, cumcap = false;
                    for (int r : assigned) {
                        cov = true;
                        reg = reg || rests[static_cast<std::size_t>(r)].regular;
                        cumcap = cumcap || rests[static_cast<std::size_t>(r)].cum;
                    }
                    State next = st;
                    next.parent = s;
                    next.assigned = assigned;
                    next.events.clear();
                    bool cum_left = cumcap;
                    bool prev_cum = pcum;
                    // La settimana prima aspettava un cumulo: un riposo lungo di questa settimana la copre.
                    if (ppend) {
                        if (cum_left) {
                            cum_left = false;
                            ++next.cumulations;
                            next.events.push_back({2, w - 1});
                        } else {
                            ++next.breaches;
                            next.events.push_back({0, w - 1});
                        }
                    }
                    bool pend = false;
                    bool covered = cov;
                    if (required[static_cast<std::size_t>(w)] && !cov) {
                        if (prev_cum) {
                            prev_cum = false;
                            covered = true;
                            ++next.cumulations;
                            next.events.push_back({2, w});
                        } else if (p.cumulation_weeks && w + 1 < weeks) {
                            pend = true;
                        } else {
                            ++next.breaches;
                            next.events.push_back({0, w});
                        }
                    }
                    if (p.two_weeks_regular && w > 0 && (required[static_cast<std::size_t>(w - 1)] || required[static_cast<std::size_t>(w)]) &&
                        !(preg || reg)) {
                        ++next.breaches;
                        next.events.push_back({1, w - 1});
                    }
                    (void)pcov;
                    if (required[static_cast<std::size_t>(w)] && cov && !reg) ++next.reduced;
                    next.cov = covered;
                    const int ns = idx(carry_next ? 1 : 0, covered ? 1 : 0, reg ? 1 : 0, cum_left ? 1 : 0, pend ? 1 : 0);
                    State& target = dp[static_cast<std::size_t>(w) + 1][static_cast<std::size_t>(ns)];
                    if (std::tie(next.breaches, next.cumulations, next.reduced) < std::tie(target.breaches, target.cumulations, target.reduced))
                        target = next;
                }
            }
        }
    }
    // La soluzione migliore e i suoi eventi, ricostruiti all'indietro.
    int best = -1;
    const auto score = [&](int s) {
        const State& st = dp[static_cast<std::size_t>(weeks)][static_cast<std::size_t>(s)];
        return std::make_tuple(st.breaches + ((s >> 4) & 1), st.cumulations, st.reduced);
    };
    for (int s = 0; s < 32; ++s) {
        if (dp[static_cast<std::size_t>(weeks)][static_cast<std::size_t>(s)].breaches >= (1 << 28)) continue;
        if (best < 0 || score(s) < score(best)) best = s;
    }
    if (best < 0) return;
    std::vector<std::pair<int, int>> events;
    // Settimana di cui ogni riposo e' il riposo settimanale. In una settimana con un riposo regolare i riposi ridotti sono
    // solo riposi lunghi (niente da compensare); senza un regolare conta il ridotto piu' lungo.
    std::vector<int> owner(rests.size(), -1);
    if ((best >> 4) & 1) events.push_back({0, weeks - 1});
    for (int w = weeks, s = best; w > 0; --w) {
        const State& st = dp[static_cast<std::size_t>(w)][static_cast<std::size_t>(s)];
        for (const auto& e : st.events) events.push_back(e);
        int longest_reduced = -1;
        bool any_regular = false;
        for (int r : st.assigned) {
            const Rest& z = rests[static_cast<std::size_t>(r)];
            if (z.regular) {
                any_regular = true;
                owner[static_cast<std::size_t>(r)] = w - 1;
            } else if (longest_reduced < 0 ||
                       z.b - z.a > rests[static_cast<std::size_t>(longest_reduced)].b - rests[static_cast<std::size_t>(longest_reduced)].a) {
                longest_reduced = r;
            }
        }
        if (!any_regular && longest_reduced >= 0) owner[static_cast<std::size_t>(longest_reduced)] = w - 1;
        s = st.parent;
    }
    std::sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return std::tie(a.second, a.first) < std::tie(b.second, b.first); });
    for (const auto& [kind, w] : events) {
        const Day m = m0 + 7 * w;
        const bool on_edge = edge[static_cast<std::size_t>(w)] || (kind == 1 && w + 1 < weeks && edge[static_cast<std::size_t>(w) + 1]);
        if (kind == 2) {   // cumulo: un avviso; in una settimana che esce dal periodo non si sa (verifica parziale)
            if (on_edge) {
                x.partial(m);
                continue;
            }
            Breach b = x.make(cp, cp, 4, m, abs_min(m, 0), abs_min(m + 7, 0));
            b.params.push_back(dur("limite", regular));
            x.out.notices.push_back(b);
            continue;
        }
        if (on_edge) {
            x.partial(m);
            continue;
        }
        long long longest = 0;
        for (const Free& f : fr)
            if (f.b > abs_min(m, 0) && f.a < abs_min(m + 7 * (kind == 1 ? 2 : 1), 0)) longest = std::max(longest, f.len());
        Breach b = x.make(ce, cp, kind == 0 ? 0 : 1, m, abs_min(m, 0), abs_min(m + 7 * (kind == 1 ? 2 : 1), 0));
        b.params.push_back(dur("misurato", longest));
        b.params.push_back(dur("limite", kind == 0 ? threshold : regular));
        x.out.breaches.push_back(b);
    }
    // Entro N periodi di 24 ore dalla fine del riposo settimanale precedente (riposi di almeno la soglia, in ordine).
    if (p.within_24h_periods) {
        const long long limit = static_cast<long long>(*p.within_24h_periods) * kDayMin;
        std::size_t k = 0;
        long long prev_end = x.c.h0;
        bool known = false;   // la fine del riposo settimanale precedente e' nota
        if (!rests.empty() && rests[0].a <= x.c.h0) {
            prev_end = rests[0].b;
            known = true;
            k = 1;
        }
        // Il regime deve valere quando scade il termine (mondo certo); nel mondo prudente basta che valga in un giorno tra la
        // fine del riposo e la scadenza (es. la settimana dopo quella in cui vale il 561 per estensione).
        const auto binds = [&](long long from, long long deadline) {
            return x.world == 0 ? x.ok(std::min(day_of(deadline), x.c.d1)) : x.ok_any(day_of(from), std::min(day_of(deadline), x.c.d1));
        };
        for (; k < rests.size(); ++k) {
            const long long gap = rests[k].a - prev_end;
            if (gap > limit && binds(prev_end, prev_end + limit)) {
                if (known) {
                    Breach b = x.make(ce, cp, 2, day_of(prev_end), prev_end, rests[k].a);
                    b.params.push_back(dur("misurato", gap));
                    b.params.push_back(num("limite", *p.within_24h_periods));
                    x.out.breaches.push_back(b);
                } else {
                    x.partial(day_of(rests[k].a));
                }
            }
            prev_end = rests[k].b;
            known = true;
        }
        // Dopo l'ultimo riposo: tutti i periodi liberi prima dell'ultimo (aperto) sono piu' corti della soglia; se la scadenza
        // cade prima dell'ultimo, il riposo non e' cominciato in tempo; altrimenti l'ultimo potrebbe esserlo, oltre il periodo.
        const Free& tail = fr.back();
        const long long deadline = prev_end + limit;
        if (known && prev_end < tail.a && binds(prev_end, deadline)) {
            if (deadline < tail.a) {
                Breach b = x.make(ce, cp, 2, day_of(prev_end), prev_end, deadline);
                b.params.push_back(dur("misurato", tail.a - prev_end));
                b.params.push_back(num("limite", *p.within_24h_periods));
                x.out.breaches.push_back(b);
            } else {
                x.partial(day_of(prev_end));
            }
        }
    }
    // Compensazione dei riposi ridotti entro la fine della N-esima settimana dopo, attaccata a un riposo di almeno M minuti.
    // Un riposo ridotto va compensato se e' il riposo settimanale della sua settimana, oppure se senza di lui il riposo
    // settimanale dopo comincerebbe oltre i 6 periodi di 24 ore (allora e' per forza un riposo settimanale); altrimenti e'
    // solo un riposo lungo. Si tolgono in ordine i ridotti che non servono (una scelta sola, la stessa nei due mondi).
    if (p.compensation_weeks && p.reduced_min) {
        std::vector<char> weekly(rests.size(), 0);
        long long prev_end = x.c.h0;
        for (std::size_t r = 0; r < rests.size(); ++r) {
            const Rest& z = rests[r];
            bool counts = z.regular || owner[r] >= 0;
            if (!counts && p.within_24h_periods) {
                const long long next_start = r + 1 < rests.size() ? rests[r + 1].a : x.c.h1;
                counts = next_start - prev_end > static_cast<long long>(*p.within_24h_periods) * kDayMin;
            }
            weekly[r] = counts ? 1 : 0;
            if (counts) prev_end = z.b;
        }
        std::vector<char> used(fr.size(), 0);
        for (std::size_t r = 0; r < rests.size(); ++r) {
            const Rest& z = rests[r];
            const int w = owner[r] >= 0 ? owner[r] : z.wa;
            if (z.regular || !weekly[r] || !required[static_cast<std::size_t>(w)]) continue;
            const long long deficit = regular - (z.b - z.a);
            const Day deadline_day = m0 + 7 * (w + *p.compensation_weeks + 1);
            const long long deadline = abs_min(deadline_day, 0);
            bool found = false;
            for (std::size_t k = 0; k < fr.size() && !found; ++k) {
                const Free& f = fr[k];
                if (used[k] || f.a < z.b || f.b > deadline) continue;
                bool weekly_assigned = false;
                for (std::size_t q = 0; q < rests.size(); ++q)
                    if (rests[q].a == f.a && owner[q] >= 0) weekly_assigned = true;
                const long long base = weekly_assigned ? regular : p.compensation_attached.value_or(0);
                if (f.len() - base >= deficit) {
                    used[k] = 1;
                    found = true;
                }
            }
            if (found) continue;
            const Day m = m0 + 7 * w;
            if (deadline > x.c.h1) {
                x.partial(m);
                continue;
            }
            Breach b = x.make(ce, cp, 3, m, z.a, deadline);
            b.params.push_back(dur("misurato", deficit));
            b.params.push_back(num("limite", *p.compensation_weeks));
            x.out.breaches.push_back(b);
        }
    }
}

// --- lavoro_notturno, giornate_massime, ampiezza_massima, giorni_consecutivi -------------------------------------------

void night_work(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.band_from || !p.band_to || !p.max_minutes) return;   // senza massimo: solo il conteggio (kzero ore)
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    const Minutes from = *p.band_from, to = *p.band_to <= *p.band_from ? *p.band_to + 1440 : *p.band_to;
    for (const PersonDay& pd : x.c.plan->days) {
        if (!x.c.in_period(pd.day) || !x.ok(pd.day)) continue;
        const long long a = x.c.start_abs(pd), b = x.c.end_abs(pd);
        long long night = 0;
        for (Day k = pd.day - 1; k <= pd.day + 1; ++k) night += x.work(std::max(a, abs_min(k, from)), std::min(b, abs_min(k, to)));
        if (night <= 0) continue;
        const long long v = x.work(a, a + kDayMin);
        if (v > static_cast<long long>(*p.max_minutes) * 100) {
            Breach br = x.make(ce, cp, 7, pd.day, a, a + kDayMin);
            br.params.push_back(dur("misurato", ceil100(v)));
            br.params.push_back(dur("limite", *p.max_minutes));
            br.params.push_back(personal(time_param("notte_dalle", *p.band_from), "riservato"));
            br.params.push_back(personal(time_param("notte_alle", *p.band_to), "riservato"));
            x.out.breaches.push_back(br);
        } else if (a + kDayMin > x.c.h1) {
            x.partial(pd.day);
        }
    }
}

void max_days(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.max_days || !p.years) return;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    std::vector<Day> days;
    for (const PersonDay& pd : x.c.plan->days)
        if (x.c.in_period(pd.day)) days.push_back(pd.day);
    const std::optional<PriorDays>& prior = x.c.person->prior_days;
    std::vector<Day> over, near;
    long long top = 0, top_near = 0;   // il massimo dei giorni oltre il limite, e di quelli vicini
    bool prior_counted = false;   // nel mondo prudente si contano tutte le giornate prima del pacchetto
    const int margin = x.settings && x.settings->days_margin ? *x.settings->days_margin : -1;
    for (std::size_t i = 0; i < days.size(); ++i) {
        const Day d = days[i];
        if (!x.ok(d)) continue;
        const Day ws = add_years(d, -*p.years) + 1;   // tre anni a ritroso dal giorno
        const auto first = std::lower_bound(days.begin(), days.end(), ws);
        long long n = static_cast<long long>(i) + 1 - (first - days.begin());
        bool unknown = false;
        if (ws < x.c.d0) {
            if (prior) {
                if (x.world == 0) n += std::max(0, prior->days - std::max(0, ws - prior->counted_from));
                else n += prior->days;
                unknown = prior->counted_from > ws;
            } else {
                unknown = true;
            }
        }
        if (n > *p.max_days) {
            top = std::max(top, n);
            over.push_back(d);
            if (ws < x.c.d0 && prior && x.world != 0) prior_counted = true;
        }
        else if (unknown) x.partial(d);
        else if (margin >= 0 && n >= *p.max_days - margin) {
            near.push_back(d);
            top_near = std::max(top_near, n);
        }
    }
    const auto make = [&](std::vector<Day>& v, bool is_near) {
        if (v.empty()) return;
        Breach b = x.make(ce, cp, 8, v.front(), abs_min(v.front(), 0), abs_min(v.back() + 1, 0));
        b.key = x.rd.regime->code + "|" + x.rule.code + "|giornate" + (is_near ? "|vicino" : "");
        b.days = v;
        b.params.push_back(num("misurato", is_near ? top_near : top));
        b.params.push_back(num("limite", *p.max_days));
        b.params.push_back(num("anni", *p.years));
        if (prior_counted) b.extra_reasons.push_back("giornate_prima_del_pacchetto");
        if (is_near) {
            b.params.push_back(num("margine", margin));
            x.out.near.push_back(b);
        } else {
            x.out.breaches.push_back(b);
        }
    };
    make(over, false);
    if (x.world == 1) make(near, true);
}

void max_span(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.max_minutes) return;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    for (const PersonDay& pd : x.c.plan->days)
        if (x.c.in_period(pd.day) && x.ok(pd.day) && pd.end - pd.start > *p.max_minutes) {
            Breach b = x.make(ce, cp, 9, pd.day, x.c.start_abs(pd), x.c.end_abs(pd));
            b.params.push_back(dur("misurato", pd.end - pd.start));
            b.params.push_back(dur("limite", *p.max_minutes));
            x.out.breaches.push_back(b);
        }
}

void consecutive_days(Ctx& x) {
    const RuleParams& p = x.par;
    if (!p.max_days) return;
    const std::pair<Check, Check> checks = checks_for(x.rule);
    const Check ce = checks.first, cp = checks.second;
    int run = 0;
    Day last = 0, run_start = 0;
    for (const PersonDay& pd : x.c.plan->days) {
        if (!x.c.in_period(pd.day)) continue;
        if (run > 0 && pd.day == last + 1) ++run;
        else {
            run = 1;
            run_start = pd.day;
        }
        last = pd.day;
        if (!x.ok(pd.day)) continue;
        if (run > *p.max_days) {
            Breach b = x.make(ce, cp, 10, pd.day, x.c.start_abs(pd), x.c.end_abs(pd));
            b.params.push_back(num("misurato", run));
            b.params.push_back(num("limite", *p.max_days));
            x.out.breaches.push_back(b);
        } else if (run_start == x.c.d0) {
            x.partial(pd.day);   // la serie poteva cominciare prima del periodo del pacchetto
        }
    }
}

}  // namespace

std::vector<Free> recognized_rests(const PersonCtx& c, const RegimeData& rd, int world) {
    if (!rd.daily_rest) return {};
    const RuleParams& p = world == 1 && rd.daily_rest->prudent ? *rd.daily_rest->prudent : rd.daily_rest->certain;
    return rest_sequence(c, rd, *rd.daily_rest, p, world, nullptr);
}

void evaluate_rule(const PersonCtx& c, const RegimeData& rd, const WorkRule& rule, int world, const CompanySettings* settings,
                   RuleOut& out) {
    const RuleParams& par = world == 1 && rule.prudent ? *rule.prudent : rule.certain;
    Ctx x{c, rd, rule, par, world, settings, out};
    switch (rule.type) {
        case RuleType::WorkCount: break;
        case RuleType::MaxWork: max_work(x); break;
        case RuleType::ContinuousDriving: continuous_driving(x); break;
        case RuleType::MaxDriving: max_driving(x); break;
        case RuleType::ContinuousWork: continuous_work(x); break;
        case RuleType::DailyRest: daily_rest(x); break;
        case RuleType::WeeklyRest: weekly_rest(x); break;
        case RuleType::NightWork: night_work(x); break;
        case RuleType::MaxDays: max_days(x); break;
        case RuleType::MaxSpan: max_span(x); break;
        case RuleType::ConsecutiveDays: consecutive_days(x); break;
    }
}

}  // namespace kzero::orari
