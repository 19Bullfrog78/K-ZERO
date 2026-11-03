// K-Zero - controlli sui dati degli orari di lavoro (K-011) che non dipendono dal piano dei turni: categorie di tempo
// mancanti (aziende.json versione 1), corse senza km dove il regime dipende dal percorso, regole scritte in modo incoerente,
// codici dei regimi e delle regole, scelte dell'azienda o della persona piu' larghe della legge, fermo minimo del rientro
// sotto le soste che il regime conta come lavoro. I controlli sul piano (giorno per giorno) stanno in orari.cpp.
#include <algorithm>
#include <map>
#include <set>

#include "orari.hpp"
#include "orari_interno.hpp"

namespace kzero {

namespace {

const Regime* trip_conditions(const Norms& n, const Regime& r) {
    if (!r.like) return &r;
    for (const Regime& o : n.regimes)
        if (o.code == r.like->code && !o.like && &o != &r) return &o;
    return &r;
}

bool depends_on_km(const Norms& n, const Regime& r) {
    const Regime& tc = *trip_conditions(n, r);
    if (tc.route) return true;
    return std::any_of(tc.exclusions.begin(), tc.exclusions.end(), [](const RegimeExclusion& e) { return e.route_up_to_km.has_value(); });
}

const char* type_code(RuleType t) {
    switch (t) {
        case RuleType::WorkCount: return "conteggio_lavoro";
        case RuleType::MaxWork: return "lavoro_massimo";
        case RuleType::ContinuousDriving: return "guida_continua";
        case RuleType::MaxDriving: return "guida_massima";
        case RuleType::ContinuousWork: return "lavoro_continuo";
        case RuleType::DailyRest: return "riposo_giornaliero";
        case RuleType::WeeklyRest: return "riposo_settimanale";
        case RuleType::NightWork: return "lavoro_notturno";
        case RuleType::MaxDays: return "giornate_massime";
        case RuleType::MaxSpan: return "ampiezza_massima";
        case RuleType::ConsecutiveDays: return "giorni_consecutivi";
    }
    return "?";
}

class HoursDataChecker {
public:
    HoursDataChecker(const Package& p, const Index& idx, std::vector<Problem>& out) : p_(p), idx_(idx), out_(out) {}

    void run() {
        categories();
        trips_without_km();
        for (const Norms& n : p_.norms) norms(n);
        companies();
        persons();
    }

private:
    void add(Check c, const Source& where, std::vector<Param> params, int variant = 0) {
        out_.push_back(Problem{c, variant, where, std::move(params)});
    }

    // Le aziende con turni che non dicono le categorie di tempo (aziende.json versione 1): un avviso per azienda.
    void categories() {
        std::set<Code> with_shifts;
        for (const Shift& s : p_.shifts) with_shifts.insert(s.company.code);
        for (const CompanySettings& s : p_.company_settings) {
            if (!with_shifts.count(s.company.code)) continue;
            std::string missing;
            for (const ActivityType& a : s.activities)
                if (!a.time) missing += (missing.empty() ? "" : ", ") + a.code;
            if (missing.empty() && s.unwritten) continue;
            const int variant = !s.unwritten ? (missing.empty() ? 1 : 0) : 2;
            add(Check::TimeCategoryMissing, s.src, {ref_param("azienda", Element::Company, s.company.code), text_param("tipi", missing)}, variant);
        }
    }

    // Corse senza km fatte in turni di aziende le cui norme hanno un regime che dipende dal percorso.
    void trips_without_km() {
        std::map<const Trip*, std::set<Code>> companies;
        for (const Shift& s : p_.shifts)
            for (const ShiftItem& it : s.items)
                if (it.is_trip)
                    if (const Trip* t = idx_.trip(it.trip.trip.code)) companies[t].insert(s.company.code);
        for (const Trip& t : p_.trips) {
            const auto it = companies.find(&t);
            if (it == companies.end() || route_km(t).known) continue;
            std::string regimes;
            for (const Code& c : it->second)
                if (const Norms* n = idx_.norms_of(c))
                    for (const Regime& r : n->regimes)
                        if (depends_on_km(*n, r) && regimes.find(r.code) == std::string::npos) regimes += (regimes.empty() ? "" : ", ") + r.code;
            if (!regimes.empty())
                add(Check::RegimeUndeterminable, t.src, {ref_param("corsa", Element::Trip, t.code), text_param("regimi", regimes)}, 0);
        }
    }

    void incoherent(const Regime& r, const WorkRule* w, int variant) {
        add(Check::WorkRuleIncoherent, w ? w->src : r.src,
            {text_param("regime", r.code), text_param("regola", w ? w->code : ""), text_param("tipo", w ? type_code(w->type) : "")}, variant);
    }

    void norms(const Norms& n) {
        std::map<Code, const Source*> regimes, rules;
        for (const Regime& r : n.regimes) {
            if (const auto [it, ins] = regimes.emplace(r.code, &r.src); !ins)
                add(Check::DuplicateCode, r.src,
                    {text_param("codice", r.code), elements_param("elementi", Element::Regime), text_param("prima", it->second->file + " " + it->second->pointer)});
            for (const WorkRule& w : r.rules)
                if (const auto [it, ins] = rules.emplace(w.code, &w.src); !ins)
                    add(Check::DuplicateCode, w.src,
                        {text_param("codice", w.code), elements_param("elementi", Element::WorkRule), text_param("prima", it->second->file + " " + it->second->pointer)});
        }
        const auto known = [&](const Code& c) { return regimes.count(c) > 0; };
        for (const Regime& r : n.regimes) {
            if (r.like && !known(r.like->code))
                add(Check::UnknownReference, r.like->src, {text_param("codice", r.like->code), elements_param("elementi", Element::Regime)});
            for (const RegimeOverride& o : r.overrides)
                if (!known(o.regime.code))
                    add(Check::UnknownReference, o.regime.src, {text_param("codice", o.regime.code), elements_param("elementi", Element::Regime)});
            if (r.valid_from && r.valid_to && *r.valid_to < *r.valid_from)
                add(Check::InvertedPeriod, r.src, {date_param("dal", *r.valid_from), date_param("al", *r.valid_to)});
            // Regole scritte in modo incoerente.
            int counts = 0;
            bool work_rules = false, daily_from_rest = false, weekly = false;
            for (const WorkRule& w : r.rules) {
                counts += w.type == RuleType::WorkCount ? 1 : 0;
                work_rules = work_rules || w.type == RuleType::MaxWork || w.type == RuleType::ContinuousWork ||
                             (w.type == RuleType::NightWork && w.certain.max_minutes);
                daily_from_rest = daily_from_rest || (w.type == RuleType::DailyRest && w.measure == RestMeasure::From24hEndRest);
                weekly = weekly || w.type == RuleType::WeeklyRest;
            }
            if (counts > 1) incoherent(r, nullptr, 0);
            if (work_rules && counts == 0) incoherent(r, nullptr, 1);
            if (r.like) {
                const Regime* o = nullptr;
                for (const Regime& x : n.regimes)
                    if (x.code == r.like->code) o = &x;
                if (o && (o == &r || o->like)) incoherent(r, nullptr, 2);
            }
            for (const WorkRule& w : r.rules)
                for (const RuleParams* p : {&w.certain, w.prudent ? &*w.prudent : nullptr}) {
                    if (!p) continue;
                    if (w.type == RuleType::MaxDriving && w.period == RulePeriod::BetweenRests && !daily_from_rest) incoherent(r, &w, 3);
                    if (p->normal_minutes && p->max_minutes && *p->normal_minutes >= *p->max_minutes) incoherent(r, &w, 4);
                    if (p->extension_max && p->max_minutes && *p->extension_max <= *p->max_minutes) incoherent(r, &w, 5);
                    if (p->reduced_min && p->min_minutes && *p->reduced_min > *p->min_minutes) incoherent(r, &w, 6);
                    if (p->normal_minutes && !w.overtime) incoherent(r, &w, 7);
                    if (w.type == RuleType::DailyRest && p->reduced_times && !weekly) incoherent(r, &w, 8);
                    if (p->band_from && p->band_to && *p->band_from == *p->band_to) incoherent(r, &w, 9);
                }
        }
    }

    // Una scelta e' piu' larga della legge: la legge resta sopra (si accetta con un avviso che lo dice).
    void looser(const WorkChoice& c, const Code& company, const Source& where, const Code* person) {
        const Norms* n = idx_.norms_of(company);
        if (!n) return;
        for (const Regime& r : n->regimes) {
            if (r.kind != RegimeKind::Law) continue;
            for (const WorkRule& w : r.rules) {
                const RuleParams& p = w.certain;
                bool loose = false;
                long long law = 0;
                switch (c.type) {
                    case RuleType::MaxWork:
                        if (w.type == RuleType::MaxWork && p.max_minutes && !p.average_weeks && !p.average_months && !p.average_days &&
                            ((c.period == RulePeriod::Workday && (w.period == RulePeriod::Workday || w.period == RulePeriod::Day24)) ||
                             (c.period == RulePeriod::Week && w.period == RulePeriod::Week))) {
                            law = *p.max_minutes;
                            loose = c.value > law;
                        }
                        break;
                    case RuleType::MaxDriving:
                        if (w.type == RuleType::MaxDriving && p.max_minutes &&
                            ((c.period == RulePeriod::Workday && (w.period == RulePeriod::Workday || w.period == RulePeriod::BetweenRests)) ||
                             (c.period == RulePeriod::Week && w.period == RulePeriod::Week))) {
                            law = p.extension_max.value_or(*p.max_minutes);
                            loose = c.value > law;
                        }
                        break;
                    case RuleType::MaxSpan:
                        if (w.type == RuleType::MaxSpan && p.max_minutes) {
                            law = *p.max_minutes;
                            loose = c.value > law;
                        }
                        break;
                    case RuleType::DailyRest:
                        if (w.type == RuleType::DailyRest && p.min_minutes) {
                            law = p.reduced_min.value_or(*p.min_minutes);
                            loose = c.value < law;
                        }
                        break;
                    case RuleType::ConsecutiveDays:
                        if (w.type == RuleType::ConsecutiveDays && p.max_days) {
                            law = *p.max_days;
                            loose = c.value > law;
                        }
                        break;
                    default: break;
                }
                if (!loose) continue;
                Param value = c.type == RuleType::ConsecutiveDays ? number_param("valore", c.value) : duration_param("valore", c.value);
                if (person) value = orari::personal(value, "riservato");   // le scelte della persona sono dati personali
                std::vector<Param> params = {text_param("di_chi", person ? *person : company), text_param("tipo", type_code(c.type)), value,
                                             text_param("regola", r.code + " " + w.code),
                                             text_param("fonte", orari::source_text(w.source, r.source)),
                                             c.type == RuleType::ConsecutiveDays ? number_param("legge", law) : duration_param("legge", law)};
                add(Check::ChoiceLooserThanLaw, where, std::move(params), person ? 1 : 0);
            }
        }
    }

    // Fermo minimo del rientro uguale o sotto le soste che il regime conta come lavoro (L. 138/1958 art. 6: 30 minuti).
    void return_stop(const ReturnKey& k, const Code& company, const Code* person) {
        const Norms* n = idx_.norms_of(company);
        if (!n || !k.min_stop) return;
        for (const Regime& r : n->regimes)
            for (const WorkRule& w : r.rules)
                if (w.type == RuleType::WorkCount && w.certain.stops_up_to && *k.min_stop <= *w.certain.stops_up_to)
                    add(Check::ReturnStopBelowCountedStops, k.src,
                        {text_param("di_chi", person ? *person : company),
                         person ? orari::personal(duration_param("fermo", *k.min_stop), "riservato") : duration_param("fermo", *k.min_stop),
                         duration_param("soste", *w.certain.stops_up_to), text_param("regola", r.code + " " + w.code),
                         text_param("fonte", orari::source_text(w.source, r.source)), text_param("misura", w.certain.measure_name)},
                        person ? 1 : 0);
    }

    void companies() {
        for (const CompanySettings& s : p_.company_settings) {
            for (const WorkChoice& c : s.choices) looser(c, s.company.code, c.src, nullptr);
            if (s.return_key) return_stop(*s.return_key, s.company.code, nullptr);
            const Norms* n = idx_.norms_of(s.company.code);
            for (const SpecialNeed& need : s.special_needs) {
                bool found = false;
                if (n)
                    for (const Regime& r : n->regimes)
                        for (const WorkRule& w : r.rules) found = found || w.code == need.rule.code;
                if (!found)
                    add(Check::UnknownReference, need.rule.src, {text_param("codice", need.rule.code), elements_param("elementi", Element::WorkRule)});
            }
            if (s.trip_modes) trip_modes(s, *s.trip_modes, n);
        }
    }

    // Modo delle corse (K-011 aggiunta 1): i codici scritti, e una nota sempre visibile per ogni forzatura e ogni dichiarazione
    // (chi, quando, perche': il motivo e chi sono dati personali).
    void trip_modes(const CompanySettings& s, const TripModes& m, const Norms* n) {
        // Senza norme (K-CTR-052) il regime non si controlla, come gli usi dei mezzi.
        const bool regime_found = !n || std::any_of(n->regimes.begin(), n->regimes.end(), [&](const Regime& r) { return r.code == m.regime.code; });
        if (!regime_found)
            add(Check::UnknownReference, m.regime.src, {text_param("codice", m.regime.code), elements_param("elementi", Element::Regime)});
        for (const ModeEntry& e : m.by_service)
            if (e.service && !idx_.service(e.service->code))
                add(Check::UnknownReference, e.service->src, {text_param("codice", e.service->code), elements_param("elementi", Element::Service)});
        for (const ModeEntry& e : m.by_trip)
            if (e.trip && !idx_.trip(e.trip->code))
                add(Check::UnknownReference, e.trip->src, {text_param("codice", e.trip->code), elements_param("elementi", Element::Trip)});
        for (const std::vector<ModeEntry>* v : {&m.by_service, &m.by_trip})
            for (const ModeEntry& e : *v) {
                const auto params = [&](const ModeJustification& j) {
                    std::vector<Param> x = {ref_param("azienda", Element::Company, s.company.code),
                                            e.trip ? ref_param("corsa", Element::Trip, e.trip->code)
                                                   : ref_param("servizio", Element::Service, e.service ? e.service->code : Code()),
                                            text_param("regime", m.regime.code), orari::personal(text_param("motivo", j.reason), "riservato"),
                                            orari::personal(text_param("chi", j.who), "riservato"), date_param("quando", j.when)};
                    if (j.until) x.push_back(date_param("fino_al", *j.until));
                    return x;
                };
                // Varianti: per la corsa 0, per il servizio 1; con la data fino a cui vale +2.
                if (e.forcing) add(Check::ModeForced, e.forcing->src, params(*e.forcing), (e.trip ? 0 : 1) + (e.forcing->until ? 2 : 0));
                if (e.declaration) add(Check::ModeDeclared, e.declaration->src, params(*e.declaration), e.trip ? 0 : 1);
            }
    }

    void persons() {
        for (const Person& x : p_.persons) {
            for (const WorkChoice& c : x.choices) looser(c, x.company.code, c.src, &x.code);
            if (x.return_key) return_stop(*x.return_key, x.company.code, &x.code);
        }
    }

    const Package& p_;
    const Index& idx_;
    std::vector<Problem>& out_;
};

}  // namespace

void check_hours_data(const Package& p, const Index& idx, std::vector<Problem>& out) { HoursDataChecker(p, idx, out).run(); }

}  // namespace kzero
