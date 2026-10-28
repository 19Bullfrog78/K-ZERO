// K-Zero API - comando "ore" (K-011): gli orari di lavoro delle persone, giorno per giorno, in un giorno o in un periodo:
// turni, minuti per categoria di tempo, lavoro contato da ogni regime, notturno, riposo prima della giornata, quali leggi e
// contratti collettivi valgono e perche', rientri con la chiave; per settimana e per il periodo i totali; i problemi degli
// orari con gli stessi codici di kzero controlla. Il documento JSON si costruisce gia' filtrato secondo le classi dei dati e
// i permessi (vista.hpp); il testo si scrive da quel documento.
// Se il pacchetto ha errori nei dati non si mostra nulla (K-ERR-018); gli errori del piano (le regole sugli orari) no: il
// comando serve proprio a correggerli (DECISIONI.md).
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "accesso.hpp"
#include "controllo.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "orari.hpp"
#include "vista.hpp"

namespace kzero::api {

namespace {

using kzero::Certainty;
using kzero::RuleFamily;
using kzero::TimeCategory;

// --- Etichette, come nei file dei dati -----------------------------------------------------------------------------

const char* nome_categoria_tempo(TimeCategory c) {
    switch (c) {
        case TimeCategory::Driving: return "guida";
        case TimeCategory::OtherWork: return "altre_mansioni";
        case TimeCategory::Availability: return "disponibilita";
        case TimeCategory::OnCall: return "reperibilita";
        case TimeCategory::Break: return "interruzione";
    }
    return "?";
}

const char* testo_categoria(const std::string& c) {
    if (c == "guida") return "guida";
    if (c == "altre_mansioni") return "altre mansioni";
    if (c == "disponibilita") return "disponibilita'";
    if (c == "reperibilita") return "reperibilita'";
    return "interruzione";
}

const char* nome_famiglia(RuleFamily f) {
    switch (f) {
        case RuleFamily::Driving: return "guida";
        case RuleFamily::Rest: return "riposi";
        case RuleFamily::Work: return "lavoro";
        case RuleFamily::Night: return "notturno";
        case RuleFamily::Days: return "giornate";
    }
    return "?";
}

const char* nome_certezza(Certainty c) {
    switch (c) {
        case Certainty::Certain: return "certo";
        case Certainty::ToConfirm: return "da_confermare";
        case Certainty::No: break;
    }
    return "no";
}

const char* nome_rapporto(kzero::EmploymentKind k) {
    using E = kzero::EmploymentKind;
    switch (k) {
        case E::Owner: return "titolare";
        case E::FamilyHelper: return "collaboratore_familiare";
        case E::Partner: return "socio";
        case E::FullTime: return "dipendente_tempo_pieno";
        case E::PartTime: return "dipendente_part_time";
        case E::OnCall: return "a_chiamata";
        case E::FixedTerm: return "tempo_determinato";
        case E::Seasonal: return "stagionale";
        case E::SelfEmployed: return "autonomo";
        case E::Volunteer: return "volontario";
        case E::Other: break;
    }
    return "altro";
}

const char* nome_ruolo(kzero::ShiftRole r) {
    switch (r) {
        case kzero::ShiftRole::Driver: return "autista";
        case kzero::ShiftRole::Escort: return "assistente";
        case kzero::ShiftRole::SecondDriver: return "secondo_autista";
    }
    return "?";
}

const char* modo_persona(kzero::PersonSource s) {
    switch (s) {
        case kzero::PersonSource::Rotation: return "rotazione";
        case kzero::PersonSource::Fixed: return "servizio_fisso";
        case kzero::PersonSource::Day: return "giorno";
        case kzero::PersonSource::None: break;
    }
    return "nessuno";
}

const char* chiave_giorno(kzero::Weekday g) {
    static const char* nomi[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
    return nomi[static_cast<int>(g)];
}

std::string data(kzero::Day d) { return kzero::text_from_day(d); }

// Minuti da centesimi di minuto: per eccesso, come nei messaggi dei problemi (un limite superato di poco resta superato).
long long minuti(long long x100) { return x100 <= 0 ? 0 : (x100 + 99) / 100; }

// Il regime vale per la persona per il suo rapporto o il suo contratto collettivo: quale legge vale e' un dato personale.
bool regime_personale(const kzero::Regime& r) { return r.employments.present || r.agreements.present; }

// --- Il documento JSON ------------------------------------------------------------------------------------------

class Ore {
public:
    Ore(const Resoconto& r, const kzero::HoursReport& rep, const Vista& vista)
        : r_(r), rep_(rep), vista_(vista), idx_(r.lettura.pacchetto),
          verificabile_(r.lettura.leggibili.personali && r.lettura.leggibili.particolari) {}

    // Media tra le persone (per l'equita', solo da vedere): giornate, guida e lavoro contato per il contratto.
    Json media() const {
        if (rep_.persons.empty()) return Json(nullptr);
        long long giornate = 0, guida = 0, lavoro = 0;
        for (const kzero::PersonHours& ph : rep_.persons) {
            giornate += ph.workdays;
            guida += ph.total_driving;
            lavoro += minuti(ph.total_work_x100);
        }
        const auto n = static_cast<double>(rep_.persons.size());
        Json j = Json::object();
        j["persone"] = rep_.persons.size();
        j["giornate"] = arrotonda(static_cast<double>(giornate) / n, 1);
        j["guida_minuti"] = arrotonda(static_cast<double>(guida) / n, 1);
        j["lavoro_minuti"] = verificabile_ ? Json(arrotonda(static_cast<double>(lavoro) / n, 1)) : Json(nullptr);
        return j;
    }

private:
    bool personali() const { return vista_.visibilita().personali; }
    bool particolari() const { return vista_.visibilita().particolari; }
    Json personale(Json v) const { return personali() ? v : Json(kRiservato); }
    Json particolare(Json v) const { return particolari() ? v : Json(kRiservato); }
    // Un regime si mostra se non dipende dalla persona, oppure con i dati personali.
    bool mostra_regime(const kzero::Regime* r) const { return !r || !regime_personale(*r) || personali(); }

    Json luogo(const kzero::Code& codice) const {
        Json j = Json::object();
        j["codice"] = codice;
        const kzero::Place* l = idx_.place(codice);
        j["nome"] = l ? vista_.campo(l->src, "nome", l->name, l->code) : Json(nullptr);
        return j;
    }

    Json categorie(const std::array<long long, kzero::kTimeCategories>& m) const {
        Json j = Json::object();
        for (int k = 0; k < kzero::kTimeCategories; ++k)
            j[nome_categoria_tempo(static_cast<TimeCategory>(k))] = m[static_cast<std::size_t>(k)];
        return j;
    }

    // Le famiglie di regole che il regime ha, con la certezza del giorno.
    Json famiglie(const kzero::Regime& r, const std::array<Certainty, 5>& f) const {
        Json j = Json::object();
        for (int k = 0; k < 5; ++k) {
            const auto fam = static_cast<RuleFamily>(k);
            if (std::any_of(r.rules.begin(), r.rules.end(), [&](const kzero::WorkRule& w) { return w.family == fam; }))
                j[nome_famiglia(fam)] = nome_certezza(f[static_cast<std::size_t>(k)]);
        }
        return j;
    }

    // Perche' un regime vale (o vale da confermare, o non vale) quel giorno: i dati della corsa, del turno o della settimana,
    // con il testo. Il tipo di servizio e' un dato particolare; il rapporto e il contratto collettivo personali.
    // Per le corse: un motivo per tutte le corse del giorno con lo stesso esito e le stesse condizioni ("corse").
    Json perche(const kzero::ApplyReason& w, const std::vector<std::string>& corse = {}) const {
        using W = kzero::ApplyReason::What;
        Json j = Json::object();
        std::string t;
        const std::string livello = w.level == Certainty::Certain ? "vale con certezza" : w.level == Certainty::ToConfirm ? "vale da confermare" : "non vale";
        switch (w.what) {
            case W::Trip: {
                // I dati delle corse (turno, servizio, mezzo, percorso) stanno una volta sola nella giornata ("corse").
                j["tipo"] = "corse";
                Json elenco = Json::array();
                std::string codici;
                for (const std::string& c : corse) {
                    elenco.push_back(c);
                    codici += (codici.empty() ? "" : ", ") + c;
                }
                j["corse"] = std::move(elenco);
                Json d = Json::object();
                d["ruolo"] = nome_certezza(w.role_level);
                d["rapporto"] = personale(nome_certezza(w.employment_level));
                d["contratto_collettivo"] = personale(nome_certezza(w.agreement_level));
                d["tipo_di_servizio"] = particolare(nome_certezza(w.kinds_level));
                d["mezzo"] = nome_certezza(w.vehicle_level);
                d["percorso"] = nome_certezza(w.route_level);
                j["condizioni"] = std::move(d);
                t = (corse.size() == 1 ? "corsa " : "corse ") + codici + ": " + livello;
                std::vector<std::string> dc;
                if (w.level == Certainty::ToConfirm) {
                    if (w.role_level == Certainty::ToConfirm) dc.push_back("il ruolo");
                    if (w.employment_level == Certainty::ToConfirm) dc.push_back(personali() ? "il rapporto" : "un dato personale");
                    if (w.agreement_level == Certainty::ToConfirm) dc.push_back(personali() ? "il contratto collettivo" : "un dato personale");
                    if (w.kinds_level == Certainty::ToConfirm) dc.push_back(particolari() ? "il tipo di servizio" : "un dato particolare");
                    if (w.vehicle_level == Certainty::ToConfirm) dc.push_back(w.vehicle ? "le persone trasportabili" : "il mezzo");
                    if (w.route_level == Certainty::ToConfirm) dc.push_back("i km del percorso");
                }
                if (w.level == Certainty::No) {
                    if (w.role_level == Certainty::No) dc.push_back("il ruolo");
                    if (w.employment_level == Certainty::No || w.agreement_level == Certainty::No)
                        dc.push_back(personali() ? "il rapporto o il contratto collettivo" : "un dato personale");
                    if (w.kinds_level == Certainty::No)
                        dc.push_back(particolari() ? "il tipo di servizio e le esclusioni del regime" : "un dato particolare");
                    if (w.vehicle_level == Certainty::No) dc.push_back("il mezzo");
                    if (w.route_level == Certainty::No) dc.push_back("il percorso");
                }
                std::sort(dc.begin(), dc.end());
                dc.erase(std::unique(dc.begin(), dc.end()), dc.end());
                Json per = Json::array();
                for (const std::string& x : dc) per.push_back(x);
                j["per"] = std::move(per);
                if (!dc.empty()) {
                    t += " (per ";
                    for (std::size_t i = 0; i < dc.size(); ++i) t += (i ? ", " : "") + dc[i];
                    t += ")";
                }
                break;
            }
            case W::Shift:
            case W::NoTrips:
                j["tipo"] = w.what == W::Shift ? "turno" : "giornata_senza_corse";
                j["turno"] = w.shift ? Json(w.shift->code) : Json(nullptr);
                j["ruolo"] = nome_ruolo(w.role);
                t = "turno " + (w.shift ? w.shift->code : std::string("?")) +
                    (w.what == W::NoTrips ? " senza corse: non si sa quale legge vale, " : ": ") + livello;
                break;
            case W::Week: {
                j["tipo"] = "settimana";
                j["dal"] = data(w.week_from);
                j["al"] = data(w.week_to);
                j["giorno_della_corsa"] = data(w.trigger_day);
                Json f = Json::array();
                std::string ff;
                for (RuleFamily x : w.families) {
                    f.push_back(nome_famiglia(x));
                    ff += (ff.empty() ? "" : ", ") + std::string(nome_famiglia(x));
                }
                j["famiglie"] = std::move(f);
                // K-011 aggiunta 1: l'estensione dell'altro regime (come_regime), seguita con certezza o da confermare.
                j["come_regime"] = w.like ? Json(w.like->code) : Json(nullptr);
                t = livello + " da lunedi' " + data(w.week_from) + " a domenica " + data(w.week_to) + " per " + ff;
                if (w.like)
                    t += " (estensione alla settimana del regime " + w.like->code + ", per una corsa del " + data(w.trigger_day) +
                         (w.follow == Certainty::ToConfirm ? ": va confermato se vale per tutta la settimana)" : ")");
                else
                    t += " (estensione alla settimana, per una corsa del " + data(w.trigger_day) + ")";
                break;
            }
            case W::OverriddenBy: {
                j["tipo"] = "escluso_da";
                const bool vedi = mostra_regime(w.other);
                j["regime"] = vedi && w.other ? Json(w.other->code) : Json(kRiservato);
                Json f = Json::array();
                std::string ff;
                for (RuleFamily x : w.families) {
                    f.push_back(nome_famiglia(x));
                    ff += (ff.empty() ? "" : ", ") + std::string(nome_famiglia(x));
                }
                j["famiglie"] = std::move(f);
                j["effetto"] = w.effect == kzero::ExclusionEffect::Exclude ? "esclude" : "da_confermare";
                const std::string altro = vedi && w.other ? w.other->code : "un altro regime";
                t = w.effect == kzero::ExclusionEffect::Exclude ? "per " + ff + " non vale: lo esclude " + altro
                                                                 : "per " + ff + " vale solo da confermare insieme a " + altro;
                break;
            }
            case W::RegimeToConfirm:
                j["tipo"] = "regime_da_confermare";
                t = "il regime e' da confermare nei dati";
                break;
        }
        j["livello"] = nome_certezza(w.level);
        j["testo"] = t;
        return j;
    }

    Json regime(const kzero::RegimeOnDay& ro) const {
        Json j = Json::object();
        if (!mostra_regime(ro.regime)) {
            j["codice"] = kRiservato;
            j["nome"] = kRiservato;
            j["tipo"] = kRiservato;
            j["famiglie"] = kRiservato;
            j["perche"] = kRiservato;
            j["guida_per_le_regole_minuti"] = kRiservato;
            return j;
        }
        j["codice"] = ro.regime->code;
        j["nome"] = ro.regime->name;
        j["tipo"] = ro.regime->kind == kzero::RegimeKind::Law ? "legge" : "contratto_collettivo";
        j["famiglie"] = famiglie(*ro.regime, ro.family);
        // Il nucleo ha gia' raggruppato le corse con lo stesso esito e tolto i motivi ripetuti (HoursReport).
        Json p = Json::array();
        for (const kzero::ApplyReason& w : ro.reasons) {
            std::vector<std::string> codici;
            for (const kzero::Trip* t : w.trips) codici.push_back(t->code);
            p.push_back(perche(w, codici));
        }
        j["perche"] = std::move(p);
        // K-011 aggiunta 1: la guida che contano le regole di guida del regime (guida delle corse escluse nei suoi dati).
        if (ro.drive_certain)
            j["guida_per_le_regole_minuti"] = {{"lettura_certa", *ro.drive_certain},
                                               {"lettura_prudente", *ro.drive_prudent},
                                               {"corse_escluse_come", nome_categoria_tempo(ro.regime->excluded_driving->certain)}};
        else
            j["guida_per_le_regole_minuti"] = nullptr;
        return j;
    }

    // Il lavoro contato da un regime (senza regime: guida, altre mansioni e disponibilita'); per la giornata anche le soste
    // contate per intero e il tempo inoperoso contato in parte.
    Json lavoro(const kzero::WorkMeasure& m, bool giornata) const {
        Json j = Json::object();
        const bool vedi = mostra_regime(m.regime);
        j["regime"] = m.regime ? (vedi ? Json(m.regime->code) : Json(kRiservato)) : Json(nullptr);
        j["misura"] = m.regime ? (vedi ? Json(m.name) : Json(kRiservato)) : Json("guida, altre mansioni e disponibilita'");
        j["minuti"] = vedi ? Json(minuti(m.certain_x100)) : Json(kRiservato);
        j["minuti_lettura_prudente"] = vedi ? Json(minuti(m.prudent_x100)) : Json(kRiservato);
        if (giornata) {
            j["soste_contate_minuti"] = vedi ? Json(m.stops_counted) : Json(kRiservato);
            j["inoperoso_contato_minuti"] = vedi ? Json(minuti(m.idle_counted_x100)) : Json(kRiservato);
        }
        return j;
    }

    // Il tipo del riposo prima della giornata per ogni regime che vale per i riposi, con le sue soglie (lettura certa).
    Json tipi_riposo(const kzero::DayHours& dh) const {
        Json v = Json::array();
        for (const kzero::RegimeOnDay& ro : dh.regimes) {
            if (ro.family[static_cast<std::size_t>(RuleFamily::Rest)] == Certainty::No) continue;
            const kzero::WorkRule *giornaliero = nullptr, *settimanale = nullptr;
            for (const kzero::WorkRule& w : ro.regime->rules) {
                if (w.type == kzero::RuleType::DailyRest && !giornaliero) giornaliero = &w;
                if (w.type == kzero::RuleType::WeeklyRest && !settimanale) settimanale = &w;
            }
            if (!giornaliero && !settimanale) continue;
            const long long m = dh.rest_before.minutes;
            std::string tipo = "sotto_il_minimo";
            if (settimanale && settimanale->certain.min_minutes && m >= *settimanale->certain.min_minutes) tipo = "settimanale_regolare";
            else if (settimanale && settimanale->certain.reduced_min && m >= *settimanale->certain.reduced_min) tipo = "settimanale_ridotto";
            else if (giornaliero && giornaliero->certain.min_minutes && m >= *giornaliero->certain.min_minutes) tipo = "giornaliero_regolare";
            else if (giornaliero && giornaliero->certain.reduced_min && m >= *giornaliero->certain.reduced_min) tipo = "giornaliero_ridotto";
            else if (!giornaliero) tipo = "non_settimanale";
            Json x = Json::object();
            const bool vedi = mostra_regime(ro.regime);
            x["regime"] = vedi ? Json(ro.regime->code) : Json(kRiservato);
            x["tipo"] = vedi ? Json(tipo) : Json(kRiservato);
            x["livello"] = vedi ? Json(nome_certezza(ro.family[static_cast<std::size_t>(RuleFamily::Rest)])) : Json(kRiservato);
            v.push_back(std::move(x));
        }
        return v;
    }

    Json rientro(const kzero::ReturnJudgment& r) const {
        using O = kzero::ReturnJudgment::Outcome;
        using Y = kzero::ReturnJudgment::Why;
        Json j = Json::object();
        j["corsa_prima"] = r.before->code;
        j["corsa_dopo"] = r.after->code;
        j["luogo"] = luogo(r.place);
        j["arrivo"] = orario(r.arrival);
        j["partenza"] = orario(r.departure);
        j["fermo_minuti"] = r.stop;
        j["guida_trasferimenti_minuti"] = r.transfers_driving;
        j["stesso_luogo"] = r.same_place;
        j["km_trasferimenti"] = r.km_known ? Json(arrotonda(r.transfers_km, 3)) : Json(nullptr);
        const bool vedi = !r.personal_key || personali();
        Json k = Json::object();
        k["fermo_minimo_minuti"] = r.min_stop;
        k["fattore_guida"] = arrotonda(r.factor, 3);
        k["km_vuoti_massimi"] = r.max_km ? Json(arrotonda(*r.max_km, 3)) : Json(nullptr);
        k["forza"] = r.strength == kzero::ChoiceStrength::Limit ? "limite" : "preferenza";
        k["della_persona"] = r.personal_key;
        j["chiave"] = vedi ? k : Json(kRiservato);
        const char* esito = r.outcome == O::Justified ? "giustificato" : r.outcome == O::NotJustified ? "non_giustificato" : "non_giudicato";
        j["esito"] = esito;
        const long long servono = static_cast<long long>(std::ceil(r.factor * r.transfers_driving - 1e-9));
        std::string t = "fermo " + durata(r.stop) + " a " + r.place;
        if (!vedi) {
            t += ", secondo la chiave del rientro della persona (dati personali: riservati)";
        } else {
            if (r.same_place)
                t += "; " + durata(r.transfers_driving) + " di guida a vuoto in piu' (il luogo dopo e' lo stesso), con il fattore " +
                     numero(r.factor) + " ne servono almeno " + durata(servono);
            else
                t += "; " + durata(r.transfers_driving) + " di guida a vuoto nei trasferimenti (la guida diretta non e' nota: "
                     "contando tutta come in piu', con il fattore " + numero(r.factor) + " ne servono almeno " + durata(servono) + ")";
            t += "; fermo minimo " + durata(r.min_stop);
            if (r.km_known) t += "; " + numero(r.transfers_km) + " km";
            if (r.max_km) t += " (al massimo " + numero(*r.max_km) + " km in piu')";
        }
        t += ": ";
        switch (r.outcome) {
            case O::Justified: t += "giustificato"; break;
            case O::NotJustified: t += "non giustificato"; break;
            case O::NotJudged: t += r.why == Y::NeedKm ? "non giudicato, servono i km dei trasferimenti" : "non giudicato, servono le distanze"; break;
        }
        if (r.outcome == O::NotJustified && vedi)
            t += r.why == Y::StopBelowMin ? " (fermo sotto il minimo)" : r.why == Y::StopBelowFactor ? " (fermo troppo corto per la guida in piu')" : " (troppi km in piu')";
        j["perche"] = t;
        return j;
    }

    Json problemi(const kzero::Person& x) const {
        Json v = Json::array();
        for (const ProblemaPresentato& p : r_.problemi) {
            if (p.persona != x.code) continue;
            const bool dentro = p.tutti_i_giorni.empty() || std::any_of(p.tutti_i_giorni.begin(), p.tutti_i_giorni.end(), [&](kzero::Day d) {
                                    return d >= rep_.from && d <= rep_.to;
                                });
            if (!dentro) continue;
            Json j = Json::object();
            j["livello"] = nome_livello(p.livello);
            j["codice"] = p.codice;
            j["messaggio"] = p.messaggio;
            Json g = Json::array();
            for (const auto& [dal, al] : p.giorni) g.push_back({{"dal", data(dal)}, {"al", data(al)}});
            j["giorni"] = std::move(g);
            v.push_back(std::move(j));
        }
        return v;
    }

    // Le corse del giorno che decidono quali regimi valgono, una volta sola (le stesse in tutti i regimi), nell'ordine.
    Json corse(const kzero::DayHours& dh) const {
        Json v = Json::array();
        for (const kzero::TripOnDay& w : dh.trips) {
            Json j = Json::object();
            j["codice"] = w.trip->code;
            j["turno"] = w.shift ? Json(w.shift->code) : Json(nullptr);
            j["ruolo"] = nome_ruolo(w.role);
            Json kinds = Json::array();
            for (kzero::ServiceKind k : w.kinds) kinds.push_back(nome_tipo_servizio(k));
            j["tipi_servizio"] = particolare(kinds);
            j["mezzo"] = w.vehicle ? Json(w.vehicle->code) : Json(nullptr);
            j["persone_trasportabili"] = w.vehicle ? Json(w.persons) : Json(nullptr);
            j["percorso_km"] = arrotonda(w.km, 3);
            j["percorso_noto"] = w.km_known;
            v.push_back(std::move(j));
        }
        return v;
    }

    Json giornata(const kzero::DayHours& dh) const {
        const kzero::PersonDay& pd = *dh.pd;
        Json j = Json::object();
        j["data"] = data(pd.day);
        j["giorno_della_settimana"] = chiave_giorno(kzero::weekday_of(pd.day));
        Json turni = Json::array();
        for (std::size_t i = 0; i < pd.shifts.size(); ++i)
            turni.push_back({{"codice", pd.shifts[i]->code},
                             {"azienda", pd.shifts[i]->company.code},
                             {"ruolo", nome_ruolo(pd.shifts[i]->role)},
                             {"assegnazione", modo_persona(pd.shift_sources[i])}});
        j["turni"] = std::move(turni);
        j["inizio"] = orario(pd.start);
        j["fine"] = orario(pd.end);
        j["ampiezza_minuti"] = pd.end - pd.start;
        j["minuti"] = categorie(dh.minutes);
        j["minuti_categoria_presunta"] = dh.assumed;
        j["soste_non_scritte_minuti"] = dh.unwritten;
        j["cambio_ora"] = dh.clock_change;
        Json riposo = Json::object();
        riposo["minuti"] = dh.rest_before.minutes;
        riposo["noto"] = dh.rest_before.known;
        riposo["dal"] = dh.rest_before.known ? Json(data(dh.rest_before.from_day) + " " + orario(dh.rest_before.from_time)) : Json(nullptr);
        riposo["dopo_reperibilita"] = dh.rest_before.after_on_call;
        if (!verificabile_) {
            j["corse"] = nullptr;
            j["regimi"] = nullptr;
            j["nessuna_legge"] = nullptr;
            j["lavoro"] = nullptr;
            j["lavoro_contratto_minuti"] = nullptr;
            j["notturno"] = nullptr;
            riposo["tipi"] = nullptr;
            j["riposo_prima"] = std::move(riposo);
            j["rientri"] = nullptr;
            j["verifica_parziale"] = nullptr;
            return j;
        }
        j["corse"] = corse(dh);
        Json regimi = Json::array();
        for (const kzero::RegimeOnDay& ro : dh.regimes) regimi.push_back(regime(ro));
        j["regimi"] = std::move(regimi);
        // Che nessuna legge valga dice qualcosa del rapporto, se una delle leggi dipende dalla persona.
        const bool dipende = std::any_of(dh.regimes.begin(), dh.regimes.end(), [](const kzero::RegimeOnDay& ro) { return regime_personale(*ro.regime); });
        j["nessuna_legge"] = dipende ? personale(dh.no_law) : Json(dh.no_law);
        Json lav = Json::array();
        for (const kzero::WorkMeasure& m : dh.work) lav.push_back(lavoro(m, true));
        j["lavoro"] = std::move(lav);
        j["lavoro_contratto_minuti"] = personale(minuti(dh.contract_x100));
        Json notte = Json::array();
        for (const kzero::NightCount& n : dh.night) {
            const bool vedi = mostra_regime(n.regime);
            notte.push_back({{"regime", vedi ? Json(n.regime->code) : Json(kRiservato)},
                             {"regola", vedi ? Json(n.rule->code) : Json(kRiservato)},
                             {"minuti", vedi ? Json(n.certain) : Json(kRiservato)},
                             {"minuti_lettura_prudente", vedi ? Json(n.prudent) : Json(kRiservato)}});
        }
        j["notturno"] = std::move(notte);
        riposo["tipi"] = tipi_riposo(dh);
        j["riposo_prima"] = std::move(riposo);
        Json rientri = Json::array();
        for (const kzero::ReturnJudgment& r : dh.returns) rientri.push_back(rientro(r));
        j["rientri"] = std::move(rientri);
        Json parz = Json::array();
        std::set<std::string> viste;
        for (const kzero::PartialCheck& pc : dh.partial) {
            const bool vedi = mostra_regime(pc.regime);
            const std::string chiave = vedi ? pc.regime->code + " " + pc.rule->code : "";
            if (!viste.insert(chiave).second) continue;
            parz.push_back({{"regime", vedi ? Json(pc.regime->code) : Json(kRiservato)}, {"regola", vedi ? Json(pc.rule->code) : Json(kRiservato)}});
        }
        j["verifica_parziale"] = std::move(parz);
        return j;
    }

    Json settimana(const kzero::WeekHours& w) const {
        Json j = Json::object();
        j["lunedi"] = data(w.monday);
        j["guida_minuti"] = w.driving;
        if (verificabile_) {
            Json lav = Json::array();
            for (const kzero::WorkMeasure& m : w.work) lav.push_back(lavoro(m, false));
            j["lavoro"] = std::move(lav);
            j["lavoro_contratto_minuti"] = personale(minuti(w.contract_x100));
            j["ore_previste_minuti"] = w.planned ? personale(*w.planned) : Json(nullptr);
            j["differenza_minuti"] = w.planned ? personale(minuti(w.contract_x100) - *w.planned) : Json(nullptr);
        } else {
            j["lavoro"] = nullptr;
            j["lavoro_contratto_minuti"] = nullptr;
            j["ore_previste_minuti"] = nullptr;
            j["differenza_minuti"] = nullptr;
        }
        j["riposo_piu_lungo_minuti"] = w.longest_rest;
        j["riposo_noto"] = w.rest_known;
        return j;
    }

public:
    // Una persona: il documento si scrive una persona alla volta (un anno di 1.000 persone non sta tutto in memoria).
    Json persona(const kzero::PersonHours& ph) const {
        const kzero::Person& x = *ph.person;
        Json j = Json::object();
        j["codice"] = x.code;
        j["nome"] = x.name.empty() ? Json(nullptr) : vista_.campo(x.src, "nome", x.name, x.code);
        j["azienda"] = x.company.code;
        j["rapporto"] = r_.lettura.leggibili.personali ? personale(nome_rapporto(x.employment)) : Json(nullptr);
        j["contratto_collettivo"] =
            r_.lettura.leggibili.personali ? (x.agreement_code.empty() ? Json(nullptr) : personale(x.agreement_code)) : Json(nullptr);
        j["ore_previste_settimana_minuti"] = ph.planned_week ? personale(*ph.planned_week) : Json(nullptr);
        Json g = Json::array();
        for (const kzero::DayHours& dh : ph.days) g.push_back(giornata(dh));
        j["giornate"] = std::move(g);
        Json s = Json::array();
        for (const kzero::WeekHours& w : ph.weeks) s.push_back(settimana(w));
        j["settimane"] = std::move(s);
        Json t = Json::object();
        t["giornate"] = ph.workdays;
        t["guida_minuti"] = ph.total_driving;
        t["minuti"] = categorie(ph.total_minutes);
        t["lavoro_contratto_minuti"] = verificabile_ ? personale(minuti(ph.total_work_x100)) : Json(nullptr);
        j["totali"] = std::move(t);
        j["problemi"] = verificabile_ ? problemi(x) : Json(nullptr);
        return j;
    }

private:
    const Resoconto& r_;
    const kzero::HoursReport& rep_;
    const Vista& vista_;
    kzero::Index idx_;
    bool verificabile_;
};

// Cassaforte (K-010): cosa non e' verificato per chi non puo' aprire una classe.
Json non_verificato(const Visibilita& leggibili) {
    Json nv = Json::array();
    if (!leggibili.personali)
        nv.push_back({{"servono", "personali"},
                      {"cosa", "quali leggi e contratti collettivi valgono (dipendono dal rapporto e dal contratto collettivo), il "
                               "lavoro contato, il contratto della persona, i rientri e i problemi degli orari; anche chi fa i "
                               "turni dipende da assenze, disponibilita' e abilitazioni"}});
    if (!leggibili.particolari)
        nv.push_back({{"servono", "particolari"},
                      {"cosa", "quali leggi valgono (dipendono dal tipo di servizio delle corse, dato particolare), il lavoro "
                               "contato, i rientri e i problemi degli orari; anche chi fa i turni e con quali mezzi"}});
    return nv;
}

// Il documento senza le persone ("persone" vuoto): le persone si aggiungono una alla volta (esegui_ore).
Json testa_json(const DefinizioneComando& comando, const Resoconto& r, const kzero::HoursReport& rep, const Vista& vista,
                const accesso::LetturaPerComando& cf, const Parametri& par) {
    const kzero::Package& p = r.lettura.pacchetto;
    Json j = documento(comando.schema);
    j["dati_inventati"] = p.manifest.invented;
    j["dati_personali_inclusi"] = vista.visibilita().personali;
    j["dati_particolari_inclusi"] = vista.visibilita().particolari;
    Json pacchetto = Json::object();
    pacchetto["codice"] = p.manifest.code;
    pacchetto["nome"] = p.manifest.name;
    pacchetto["livello"] = p.manifest.level == kzero::Level::Full ? "completo" : "solo_numeri";
    pacchetto["periodo"] = {{"dal", data(p.manifest.period.from)}, {"al", data(p.manifest.period.to)}};
    j["pacchetto"] = std::move(pacchetto);
    j["controllo"] = {{"errori", r.errori}, {"errori_del_piano", r.errori_del_piano}, {"avvisi", r.avvisi}};
    if (cf.cassaforte) {
        Json x = Json::object();
        x["utente"] = cf.utente;
        x["permessi"] = accesso::classi(cf.permessi.personali, cf.permessi.particolari);
        x["conservazione"] = cf.cassaforte->conservazione;
        x["conservazione_scaduta"] = cf.scaduta;
        Json avvisi = Json::array();
        for (const kzero::Problem& q : cf.avvisi) {
            const accesso::AvvisoMostrato a = accesso::mostra(q);
            avvisi.push_back({{"codice", a.codice}, {"messaggio", a.messaggio}});
        }
        x["avvisi"] = std::move(avvisi);
        j["cassaforte"] = std::move(x);
    } else {
        j["cassaforte"] = nullptr;
    }
    j["non_verificato"] = non_verificato(r.lettura.leggibili);
    j["periodo"] = {{"dal", data(rep.from)}, {"al", data(rep.to)}};
    j["persona"] = par.persona.empty() ? Json(nullptr) : Json(par.persona);
    const Ore ore(r, rep, vista);
    j["persone"] = Json::array();
    j["media_tra_le_persone"] = par.persona.empty() ? ore.media() : Json(nullptr);
    return j;
}

// --- Il testo, dal documento JSON ---------------------------------------------------------------------------

std::string min_testo(const Json& v) { return v.is_number() ? durata(v.get<long long>()) : testo(v); }

std::string riga_minuti(const Json& m) {
    std::string t;
    for (const auto& [k, v] : m.items()) {
        if (v.get<long long>() == 0) continue;
        t += (t.empty() ? "" : "; ") + std::string(testo_categoria(k)) + " " + durata(v.get<long long>());
    }
    return t.empty() ? "nessun minuto" : t;
}

// Le corse del giorno, raggruppate per mezzo: "con il mezzo V-1 (20 persone): C-1 (servizio scolastico, 23,6 km), C-2 (...)".
std::string corse_testo(const Json& corse) {
    std::vector<std::pair<std::string, std::vector<std::string>>> gruppi;
    for (const auto& c : corse) {
        const std::string mezzo = c.at("mezzo").is_null() ? "con il mezzo non noto"
                                                          : "con il mezzo " + c.at("mezzo").get<std::string>() + " (" +
                                                                std::to_string(c.at("persone_trasportabili").get<long long>()) + " persone)";
        std::vector<std::string> dett;
        if (c.at("tipi_servizio").is_array() && !c.at("tipi_servizio").empty()) {
            std::string k;
            for (const auto& x : c.at("tipi_servizio")) k += (k.empty() ? "" : ", ") + testo(x);
            dett.push_back("servizio " + k);
        }
        const double km = c.at("percorso_km").get<double>();
        dett.push_back(c.at("percorso_noto").get<bool>() ? numero(km) + " km"
                       : km > 0 ? "km non noti (almeno " + numero(km) + ")" : std::string("km non noti"));
        std::string x = c.at("codice").get<std::string>() + " (";
        for (std::size_t i = 0; i < dett.size(); ++i) x += (i ? ", " : "") + dett[i];
        x += ")";
        auto it = std::find_if(gruppi.begin(), gruppi.end(), [&](const auto& g) { return g.first == mezzo; });
        if (it == gruppi.end()) gruppi.push_back({mezzo, {x}});
        else it->second.push_back(x);
    }
    std::string t;
    for (const auto& [mezzo, elenco] : gruppi) {
        t += (t.empty() ? "" : "; ") + mezzo + ": ";
        for (std::size_t i = 0; i < elenco.size(); ++i) t += (i ? ", " : "") + elenco[i];
    }
    return t;
}

// Perche' un regime vale: le corse raggruppate per esito ("tutte le corse" se l'esito e' uno solo), poi le altre ragioni.
std::string perche_testo(const Json& perche, std::size_t corse_del_giorno) {
    struct Gruppo {
        std::string livello, per;
        std::vector<std::string> corse;
    };
    std::vector<Gruppo> gruppi;
    std::vector<std::string> altre;
    for (const auto& x : perche) {
        if (x.at("tipo") != "corse") {
            altre.push_back(x.at("testo").get<std::string>());
            continue;
        }
        std::string per;
        for (const auto& y : x.at("per")) per += (per.empty() ? "" : ", ") + y.get<std::string>();
        const std::string livello = x.at("livello").get<std::string>();
        auto it = std::find_if(gruppi.begin(), gruppi.end(), [&](const Gruppo& g) { return g.livello == livello && g.per == per; });
        if (it == gruppi.end()) it = gruppi.insert(gruppi.end(), Gruppo{livello, per, {}});
        for (const auto& c : x.at("corse"))
            if (std::find(it->corse.begin(), it->corse.end(), c.get<std::string>()) == it->corse.end()) it->corse.push_back(c.get<std::string>());
    }
    std::string t;
    for (const Gruppo& g : gruppi) {
        std::string corse;
        for (std::size_t i = 0; i < g.corse.size(); ++i) corse += (i ? ", " : "") + g.corse[i];
        const bool tutte = gruppi.size() == 1 && g.corse.size() == corse_del_giorno && corse_del_giorno > 1;
        t += (t.empty() ? "" : "; ") +
             std::string(g.livello == "certo" ? "vale con certezza" : g.livello == "da_confermare" ? "vale da confermare" : "non vale") +
             (tutte ? std::string(g.livello == "no" ? " per nessuna corsa" : " per tutte le corse")
                    : (g.corse.size() == 1 ? " per la corsa " : " per le corse ") + corse) +
             (g.per.empty() ? "" : " (per " + g.per + ")");
    }
    for (const std::string& a : altre) t += (t.empty() ? "" : "; ") + a;
    return t;
}

void giornata_testo(std::string& t, const Json& g) {
    std::string turni;
    for (const auto& x : g.at("turni"))
        turni += (turni.empty() ? "" : ", ") + x.at("codice").get<std::string>() + " (" + testo(x.at("ruolo")) + ", " +
                 testo(x.at("assegnazione")) + ")";
    const std::string giorno = nome_giorno(kzero::weekday_of(*kzero::day_from_text(g.at("data").get<std::string>())));
    t += "\n  " + giorno + " " + g.at("data").get<std::string>() + ": " + turni + ", dalle " + g.at("inizio").get<std::string>() +
         " alle " + g.at("fine").get<std::string>() + " (ampiezza " + durata(g.at("ampiezza_minuti").get<long long>()) + ")\n";
    t += "    Tempi: " + riga_minuti(g.at("minuti"));
    std::vector<std::string> note;
    if (g.at("soste_non_scritte_minuti").get<long long>() > 0)
        note.push_back("soste non scritte " + durata(g.at("soste_non_scritte_minuti").get<long long>()));
    if (g.at("minuti_categoria_presunta").get<long long>() > 0)
        note.push_back("categoria presunta per " + durata(g.at("minuti_categoria_presunta").get<long long>()) + ": aziende.json versione 1");
    if (!note.empty()) {
        t += " (";
        for (std::size_t i = 0; i < note.size(); ++i) t += (i ? "; " : "") + note[i];
        t += ")";
    }
    t += ".\n";
    if (g.at("cambio_ora").get<bool>()) t += "    Cambio dell'ora: un turno lo attraversa, le durate vanno controllate a mano.\n";
    if (g.at("regimi").is_null()) {
        t += "    Leggi, lavoro e rientri: non verificati (servono insieme i dati personali e quelli particolari).\n";
    } else {
        if (!g.at("corse").empty()) t += "    Corse " + corse_testo(g.at("corse")) + ".\n";
        if (g.at("nessuna_legge") == true)
            t += "    Nessuna legge sugli orari nei dati per questo caso: gli orari di questo giorno non si giudicano (non vuol dire "
                 "\"regolare\").\n";
        long long riservati = 0;
        std::string non_valgono;
        for (const auto& r : g.at("regimi")) {
            if (r.at("codice") == kRiservato) {
                ++riservati;
                continue;
            }
            // Le famiglie: se hanno tutte la stessa certezza la dicono i motivi; altrimenti quelle da confermare sono segnate.
            std::vector<std::pair<std::string, bool>> fam;
            for (const auto& [k, v] : r.at("famiglie").items())
                if (v != "no") fam.push_back({k, v == "da_confermare"});
            if (fam.empty()) {
                non_valgono += (non_valgono.empty() ? "" : ", ") + r.at("codice").get<std::string>();
                continue;
            }
            const bool uguali = std::all_of(fam.begin(), fam.end(), [&](const auto& f) { return f.second == fam.front().second; });
            std::string ff;
            for (std::size_t i = 0; i < fam.size(); ++i)
                ff += (i == 0 ? "" : i + 1 == fam.size() ? " e " : ", ") + fam[i].first + (!uguali && fam[i].second ? " (da confermare)" : "");
            std::string guida;
            if (const Json& gr = r.at("guida_per_le_regole_minuti"); gr.is_object()) {
                const long long certa = gr.at("lettura_certa").get<long long>(), prudente = gr.at("lettura_prudente").get<long long>();
                const long long tutta = g.at("minuti").at("guida").get<long long>();
                if (certa != tutta || prudente != tutta)
                {
                    std::string come = gr.at("corse_escluse_come").get<std::string>();
                    std::replace(come.begin(), come.end(), '_', ' ');
                    guida = "; per le sue regole di guida conta " + durata(certa) + " di guida (la guida delle corse escluse conta come " +
                            come + (prudente != certa ? "; con la lettura prudente " + durata(prudente) : std::string()) + ")";
                }
            }
            t += "    Regime " + r.at("codice").get<std::string>() + " per " + ff + ": " + perche_testo(r.at("perche"), g.at("corse").size()) +
                 guida + ".\n";
        }
        if (!non_valgono.empty()) t += "    Non valgono: " + non_valgono + ".\n";
        if (riservati > 0)
            t += "    " + std::string(riservati == 1 ? "Un regime riservato: dipende" : std::to_string(riservati) + " regimi riservati: dipendono") +
                 " dal rapporto o dal contratto collettivo della persona (--includi-dati-personali).\n";
        std::string lav;
        bool lav_riservato = false;
        for (const auto& m : g.at("lavoro")) {
            if (m.at("minuti") == kRiservato) {
                lav_riservato = true;
                continue;
            }
            std::string x = (m.at("regime").is_null() ? std::string("senza regime") : m.at("regime").get<std::string>()) + " " +
                            m.at("misura").get<std::string>() + " " + durata(m.at("minuti").get<long long>());
            std::vector<std::string> dett;
            if (m.at("soste_contate_minuti").get<long long>() > 0) dett.push_back("soste contate " + durata(m.at("soste_contate_minuti").get<long long>()));
            if (m.at("inoperoso_contato_minuti").get<long long>() > 0)
                dett.push_back("inoperoso contato " + durata(m.at("inoperoso_contato_minuti").get<long long>()));
            if (m.at("minuti_lettura_prudente") != m.at("minuti"))
                dett.push_back("lettura prudente " + durata(m.at("minuti_lettura_prudente").get<long long>()));
            if (!dett.empty()) {
                x += " (";
                for (std::size_t i = 0; i < dett.size(); ++i) x += (i ? ", " : "") + dett[i];
                x += ")";
            }
            lav += (lav.empty() ? "" : "; ") + x;
        }
        if (lav_riservato) lav += (lav.empty() ? "" : "; ") + std::string("altro riservato");
        t += "    Lavoro: " + lav + "; per il contratto " + min_testo(g.at("lavoro_contratto_minuti")) + ".\n";
        for (const auto& n : g.at("notturno")) {
            if (n.at("minuti") == kRiservato) continue;
            if (n.at("minuti").get<long long>() == 0 && n.at("minuti_lettura_prudente").get<long long>() == 0) continue;
            t += "    Notturno " + n.at("regime").get<std::string>() + ": " + durata(n.at("minuti").get<long long>()) +
                 (n.at("minuti_lettura_prudente") != n.at("minuti") ? " (lettura prudente " + durata(n.at("minuti_lettura_prudente").get<long long>()) + ")" : "") +
                 ".\n";
        }
    }
    const Json& rp = g.at("riposo_prima");
    t += "    Riposo prima: " + std::string(rp.at("noto").get<bool>() ? "" : "almeno ") + durata(rp.at("minuti").get<long long>()) +
         (rp.at("dal").is_null() ? std::string() : " dal " + rp.at("dal").get<std::string>());
    std::vector<std::string> note_riposo;
    if (rp.at("dal").is_null()) note_riposo.push_back("comincia prima del periodo del pacchetto");
    if (rp.at("tipi").is_array() && !rp.at("tipi").empty()) {
        std::string tipi;
        for (const auto& x : rp.at("tipi"))
            if (x.at("regime") != kRiservato) tipi += (tipi.empty() ? "" : ", ") + x.at("regime").get<std::string>() + " " + testo(x.at("tipo"));
        if (!tipi.empty()) note_riposo.push_back("per la durata: " + tipi);
    }
    if (!note_riposo.empty()) {
        t += " (";
        for (std::size_t i = 0; i < note_riposo.size(); ++i) t += (i ? "; " : "") + note_riposo[i];
        t += ")";
    }
    if (rp.at("dopo_reperibilita").get<bool>()) t += ", dopo la reperibilita', che qui non conta come riposo";
    t += ".\n";
    if (g.at("rientri").is_array())
        for (const auto& r : g.at("rientri"))
            t += "    Rientro tra " + r.at("corsa_prima").get<std::string>() + " e " + r.at("corsa_dopo").get<std::string>() + ": " +
                 r.at("perche").get<std::string>() + ".\n";
    if (g.at("verifica_parziale").is_array() && !g.at("verifica_parziale").empty()) {
        std::string v;
        for (const auto& x : g.at("verifica_parziale"))
            v += (v.empty() ? "" : ", ") + (x.at("regola") == kRiservato ? std::string("riservata") : x.at("regime").get<std::string>() + " " + x.at("regola").get<std::string>());
        t += "    Verifica parziale (i dati di fuori dal periodo del pacchetto non ci sono, mai \"regolare\"): " + v + ".\n";
    }
}

std::string testa_testo(const Json& j) {
    std::string t;
    const Json& pk = j.at("pacchetto");
    const Json& per = j.at("periodo");
    const std::string dal = per.at("dal").get<std::string>(), al = per.at("al").get<std::string>();
    t += "Orari di lavoro " + (dal == al ? "del " + dal : "dal " + dal + " al " + al) + " - pacchetto " + pk.at("codice").get<std::string>() +
         " \"" + pk.at("nome").get<std::string>() + "\"" + (j.at("dati_inventati").get<bool>() ? " (dati inventati)" : "") + "\n";
    if (!j.at("cassaforte").is_null()) {
        const Json& c = j.at("cassaforte");
        std::string permessi;
        for (const auto& x : c.at("permessi")) permessi += (permessi.empty() ? "dati " : " e ") + x.get<std::string>();
        t += "Cassaforte aperta dall'utente " + c.at("utente").get<std::string>() + " (" + permessi + ").\n";
        for (const auto& a : c.at("avvisi"))
            t += "Avviso " + a.at("codice").get<std::string>() + ": " + a.at("messaggio").get<std::string>() + "\n";
    }
    for (const auto& x : j.at("non_verificato"))
        t += "Non verificato (servono i dati " + x.at("servono").get<std::string>() + "): " + x.at("cosa").get<std::string>() + ".\n";
    const Json& ctr = j.at("controllo");
    t += "Controllo del pacchetto: " + quanti(ctr.at("errori").get<long long>(), "errore", "errori");
    if (ctr.at("errori_del_piano").get<long long>() > 0)
        t += " (" + std::to_string(ctr.at("errori_del_piano").get<long long>()) + " del piano degli orari, che non bloccano)";
    t += ", " + quanti(ctr.at("avvisi").get<long long>(), "avviso", "avvisi") + " (dettagli con kzero controlla).\n";
    t += std::string("Dati personali: ") + (j.at("dati_personali_inclusi").get<bool>() ? "mostrati" : "sostituiti dal codice") +
         "; dati particolari: " + (j.at("dati_particolari_inclusi").get<bool>() ? "mostrati" : "riservati") + ".\n";
    return t;
}

void persona_testo(std::string& t, const Json& p) {
    {
        t += "\nPersona " + p.at("codice").get<std::string>();
        if (!p.at("nome").is_null() && p.at("nome") != p.at("codice")) t += " (" + p.at("nome").get<std::string>() + ")";
        t += ", azienda " + p.at("azienda").get<std::string>();
        if (!p.at("rapporto").is_null()) t += ", rapporto " + testo(p.at("rapporto"));
        if (!p.at("contratto_collettivo").is_null()) t += ", contratto collettivo " + testo(p.at("contratto_collettivo"));
        if (!p.at("ore_previste_settimana_minuti").is_null())
            t += p.at("ore_previste_settimana_minuti").is_number()
                     ? ", ore previste " + min_testo(p.at("ore_previste_settimana_minuti")) + " a settimana"
                     : std::string(", ore previste riservate");
        t += "\n";
        if (p.at("giornate").empty()) t += "  Nessuna giornata di lavoro nel periodo.\n";
        for (const auto& g : p.at("giornate")) giornata_testo(t, g);
        for (const auto& s : p.at("settimane")) {
            t += "  Settimana dal lunedi' " + s.at("lunedi").get<std::string>() + ": guida " + durata(s.at("guida_minuti").get<long long>());
            if (s.at("lavoro").is_array())
                for (const auto& m : s.at("lavoro")) {
                    if (m.at("minuti") == kRiservato) continue;
                    t += "; " + (m.at("regime").is_null() ? std::string("senza regime") : m.at("regime").get<std::string>()) + " " +
                         m.at("misura").get<std::string>() + " " + durata(m.at("minuti").get<long long>());
                }
            if (!s.at("lavoro_contratto_minuti").is_null()) {
                t += "; per il contratto " + min_testo(s.at("lavoro_contratto_minuti"));
                if (s.at("ore_previste_minuti") == kRiservato) {
                    t += " (ore previste riservate)";
                } else if (!s.at("ore_previste_minuti").is_null()) {
                    t += " (previste " + min_testo(s.at("ore_previste_minuti"));
                    if (s.at("differenza_minuti").is_number()) {
                        const long long d = s.at("differenza_minuti").get<long long>();
                        t += ", differenza " + std::string(d >= 0 ? "+" : "-") + durata(d >= 0 ? d : -d);
                    }
                    t += ")";
                }
            }
            t += "; riposo piu' lungo " + std::string(s.at("riposo_noto").get<bool>() ? "" : "almeno ") +
                 durata(s.at("riposo_piu_lungo_minuti").get<long long>()) + ".\n";
        }
        const Json& tot = p.at("totali");
        const long long giornate = tot.at("giornate").get<long long>();
        t += "  Totali del periodo: " + (giornate == 0 ? std::string("nessuna giornata") : quanti(giornate, "giornata", "giornate")) + "; " +
             riga_minuti(tot.at("minuti"));
        if (!tot.at("lavoro_contratto_minuti").is_null()) t += "; lavoro per il contratto " + min_testo(tot.at("lavoro_contratto_minuti"));
        t += ".\n";
        if (p.at("problemi").is_array()) {
            if (p.at("problemi").empty()) t += "  Problemi degli orari: nessuno in questo periodo.\n";
            for (const auto& q : p.at("problemi"))
                t += std::string("  ") + (q.at("livello") == "errore" ? "Errore " : "Avviso ") + q.at("codice").get<std::string>() + ": " +
                     q.at("messaggio").get<std::string>() + "\n";
        }
    }
}

std::string coda_testo(const Json& j) {
    std::string t;
    if (!j.at("media_tra_le_persone").is_null()) {
        const Json& m = j.at("media_tra_le_persone");
        t += "\nMedia tra le " + std::to_string(m.at("persone").get<long long>()) + " persone (per l'equita', solo da vedere): " +
             numero(m.at("giornate").get<double>()) + " giornate; guida " + durata(std::llround(m.at("guida_minuti").get<double>()));
        if (!m.at("lavoro_minuti").is_null()) t += "; lavoro per il contratto " + durata(std::llround(m.at("lavoro_minuti").get<double>()));
        t += ".\n";
    }
    return t;
}

}  // namespace

Risultato esegui_ore(const DefinizioneComando& comando, const Parametri& parametri) {
    const std::string& cartella = parametri.argomenti.at(0);
    // Il giorno o il periodo: --data, oppure --dal e --al insieme (--data con --dal o --al: K-ERR-015, dalla tabella).
    if (!parametri.data && (!parametri.dal || !parametri.al))
        throw ErroreKZero(Errore::PeriodoIncompleto,
                          !parametri.dal && !parametri.al
                              ? "Manca il giorno o il periodo da mostrare: --data AAAA-MM-GG, oppure --dal AAAA-MM-GG e --al AAAA-MM-GG."
                              : std::string("Il periodo e' scritto a meta': manca ") + (parametri.dal ? "--al" : "--dal") +
                                    ". Si scrivono tutti e due (oppure --data per un giorno solo).");
    const kzero::Day dal = parametri.data ? *parametri.data : *parametri.dal;
    const kzero::Day al = parametri.data ? *parametri.data : *parametri.al;
    if (al < dal)
        throw ErroreKZero(Errore::PeriodoRovesciato, "Il periodo finisce (--al " + data(al) + ") prima di cominciare (--dal " + data(dal) +
                                                         "): scrivere prima il primo giorno, poi l'ultimo.");
    std::string nota = "ore " + (dal == al ? "del " + data(dal) : "dal " + data(dal) + " al " + data(al));
    if (!parametri.persona.empty()) nota += ", persona " + parametri.persona;
    accesso::LetturaPerComando lc = accesso::leggi_per_comando(cartella, parametri, comando.nome, nota);
    const Visibilita visibilita = lc.visibilita;
    // Prima la forma e il periodo, senza calcolare: poi il controllo completo con il resoconto degli orari, in una passata.
    if (lc.lettura.forma_valida()) {
        const kzero::Package& p = lc.lettura.pacchetto;
        for (const kzero::Day d : {dal, al})
            if (d < p.manifest.period.from || d > p.manifest.period.to)
                throw ErroreKZero(Errore::DataFuoriPeriodo, "La data " + data(d) + " e' fuori dal periodo del pacchetto " + p.manifest.code +
                                                                " (dal " + data(p.manifest.period.from) + " al " + data(p.manifest.period.to) + ").");
        bool trovata = parametri.persona.empty();
        for (const kzero::Person& x : p.persons) trovata = trovata || x.code == parametri.persona;
        if (!trovata)
            throw ErroreKZero(Errore::ElementoNonTrovato,
                              "La persona " + parametri.persona + " non c'e' nel pacchetto " + p.manifest.code + " (persone.json).");
    }
    kzero::HoursRequest richiesta;
    richiesta.report = true;
    richiesta.from = dal;
    richiesta.to = al;
    if (!parametri.persona.empty()) richiesta.persons.insert(parametri.persona);
    kzero::HoursReport orari;
    const Resoconto r = controlla_lettura(std::move(lc.lettura), visibilita, &richiesta, &orari);
    // Solo gli errori nei dati bloccano: gli errori del piano (K-011) no, il comando serve a correggerli.
    if (r.errori_nei_dati() > 0)
        throw ErroreKZero(Errore::PacchettoConErrori,
                          "Il pacchetto ha " + quanti(r.errori_nei_dati(), "errore", "errori") +
                              " nei dati: gli orari non si mostrano, perche' sarebbero falsati. Correggere prima gli errori: "
                              "kzero controlla " + cartella + (lc.utente.empty() ? "" : " --utente " + lc.utente));
    // Una persona chiesta che non lavora nel periodo: si mostra lo stesso, senza giornate.
    if (!parametri.persona.empty() && orari.persons.empty())
        for (const kzero::Person& x : r.lettura.pacchetto.persons)
            if (x.code == parametri.persona) {
                kzero::PersonHours ph;
                ph.person = &x;
                orari.persons.push_back(ph);
            }
    const Vista vista(r.lettura, visibilita);
    const Json testa = testa_json(comando, r, orari, vista, lc, parametri);
    const Ore ore(r, orari, vista);
    if (parametri.formato != Formato::Json) {
        std::string t = testa_testo(testa);
        if (orari.persons.empty()) t += "\nNessuna persona lavora in questo periodo.\n";
        for (const kzero::PersonHours& ph : orari.persons) persona_testo(t, ore.persona(ph));
        t += coda_testo(testa);
        return {std::move(t), 0};
    }
    // JSON: il documento con "persone" vuoto, poi le persone al loro posto, con la stessa forma di testo_json (rientro di
    // 2 spazi: ogni persona e' un elemento dell'elenco, 4 spazi piu' in dentro). Le stringhe JSON non hanno a capo.
    const std::string t = testo_json(testa);
    const std::string vuoto = "\n  \"persone\": [],\n";
    const std::size_t pos = t.find(vuoto);
    if (pos == std::string::npos) throw std::logic_error("kzero ore: manca l'elenco delle persone nel documento");
    if (orari.persons.empty()) return {t, 0};
    std::string out = t.substr(0, pos) + "\n  \"persone\": [";
    for (std::size_t i = 0; i < orari.persons.size(); ++i) {
        const std::string d = ore.persona(orari.persons[i]).dump(2, ' ', false, Json::error_handler_t::replace);
        out += i ? ",\n    " : "\n    ";
        for (const char c : d) {
            out += c;
            if (c == '\n') out += "    ";
        }
    }
    out += "\n  ],\n";
    out.append(t, pos + vuoto.size(), std::string::npos);
    return {std::move(out), 0};
}

}  // namespace kzero::api
