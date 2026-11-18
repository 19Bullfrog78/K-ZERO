// K-Zero API - schede dei servizi: elementi, registro, confronto (vedi schede_scrittura.hpp).
#include "schede_scrittura.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>

#include "chiusura.hpp"

namespace kzero::api::schede {

namespace {

const char* kGiorni[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};

double km(double v) { return std::round(v * 1000.0) / 1000.0; }

const char* tipo_servizio(kzero::ServiceKind k) {
    switch (k) {
        case kzero::ServiceKind::Line: return "linea";
        case kzero::ServiceKind::School: return "scolastico";
        case kzero::ServiceKind::Disability: return "disabili";
        case kzero::ServiceKind::Ncc: return "ncc";
        case kzero::ServiceKind::Other: return "altro";
    }
    return "altro";
}
const char* tipo_gruppo(kzero::GroupType g) {
    switch (g) {
        case kzero::GroupType::Students: return "studenti";
        case kzero::GroupType::Disabled: return "disabili";
        case kzero::GroupType::LineUsers: return "utenti_linea";
        case kzero::GroupType::AuthorizedAdults: return "adulti_autorizzati";
        case kzero::GroupType::Other: return "altro";
    }
    return "altro";
}

// Un passaggio della corsa: le righe di fila nello stesso luogo alla stessa ora sono un passaggio solo (la seconda senza km).
struct Passaggio {
    kzero::Code luogo;
    kzero::Minutes orario = 0;
    int salgono = 0, scendono = 0;   // senza l'accompagnatore
    kzero::AssistantMove assistente = kzero::AssistantMove::None;
    std::optional<double> pieni, vuoti;
};
std::vector<Passaggio> passaggi(const kzero::Card& k) {
    std::vector<Passaggio> out;
    for (const kzero::CardStop& s : k.stops) {
        // I numeri della scheda comprendono l'accompagnatore: nel gruppo si toglie sulle righe dove sale o scende (lo dice la
        // colonna, mai indovinato); sulla corsa e' il movimento dell'assistente, mai un passeggero.
        const int su = s.boarding - (s.escort == kzero::AssistantMove::Boards ? 1 : 0);
        const int giu = s.alighting - (s.escort == kzero::AssistantMove::Alights ? 1 : 0);
        if (!out.empty() && out.back().luogo == s.place && out.back().orario == s.time) {
            Passaggio& p = out.back();
            p.salgono += su;
            p.scendono += giu;
            if (s.escort != kzero::AssistantMove::None) p.assistente = s.escort;
            continue;
        }
        out.push_back({s.place, s.time, su, giu, s.escort, s.loaded_km, s.empty_km});
    }
    return out;
}

std::string nome_di(const kzero::Card& k) { return k.nr + " " + k.service_text; }

}  // namespace

Elementi elementi(const kzero::ServiceCard& s, const kzero::CardVersion& v, const kzero::CardProfile& pr, const kzero::Index& idx) {
    const kzero::Card& k = v.card;
    Elementi e;
    const std::vector<Passaggio> pp = passaggi(k);
    const bool accompagnatore = std::any_of(k.stops.begin(), k.stops.end(), [](const kzero::CardStop& x) { return x.escort != kzero::AssistantMove::None; });
    // Il gruppo: uno solo (D3). Con una scuola sola, la sua scuola; con piu' scuole nessuna (le salite non si dividono mai).
    JsonDati g = JsonDati::object();
    g["codice"] = v.group;
    g["tipo"] = tipo_gruppo(pr.group_type);
    if (pr.group_type == kzero::GroupType::Other) g["tipo_descrizione"] = pr.group_type_description;
    g["verso"] = k.direction == kzero::Direction::Return ? "ritorno" : "andata";
    if (k.schools.size() == 1) g["scuola"] = k.schools.front();
    g["conteggi"] = "esatti";
    // Nella domanda un luogo una volta sola (K-022): una scheda con due giri passa due volte dallo stesso luogo (es. la scuola a meta'
    // e alla fine); le persone di tutti i passaggi in quel luogo si sommano, nell'ordine del primo passaggio. La corsa tiene i
    // passaggi uno per uno: il confronto con le corse (K-CTR-042) somma le corse per luogo.
    std::vector<Passaggio> per_luogo;
    for (const Passaggio& p : pp) {
        if (!p.salgono && !p.scendono) continue;
        const auto it = std::find_if(per_luogo.begin(), per_luogo.end(), [&](const Passaggio& x) { return x.luogo == p.luogo; });
        if (it == per_luogo.end()) {
            per_luogo.push_back(p);
            continue;
        }
        it->salgono += p.salgono;
        it->scendono += p.scendono;
    }
    JsonDati fermate = JsonDati::array();
    for (const Passaggio& p : per_luogo) {
        JsonDati f = {{"luogo", p.luogo}};
        if (p.salgono) f["salgono"] = p.salgono;
        if (p.scendono) f["scendono"] = p.scendono;
        fermate.push_back(f);
    }
    g["fermate"] = fermate;
    if (accompagnatore) g["assistente_richiesto"] = true;
    // Il servizio.
    JsonDati sv = JsonDati::object();
    sv["codice"] = v.service;
    sv["nome"] = nome_di(k);
    sv["codici_esterni"] = JsonDati::array({{{"sistema", pr.ext_system}, {"tipo", pr.ext_card}, {"codice", s.id}},
                                            {{"sistema", pr.ext_system}, {"tipo", pr.ext_nr}, {"codice", k.nr}},
                                            {{"sistema", pr.ext_system}, {"tipo", pr.ext_service}, {"codice", kzero::card_service_number(k.nr, pr)}}});
    sv["tipo"] = tipo_servizio(pr.service_kind);
    if (pr.service_kind == kzero::ServiceKind::Other) sv["tipo_descrizione"] = pr.service_kind_description;
    sv["contratto"] = s.contract.code;
    sv["calendario"] = v.calendar;
    JsonDati lib = {{"tipo", pr.freedom == kzero::TimeFreedom::Free ? "libera" : "fissa"}};
    if (pr.agree_threshold_min) lib["soglia_da_concordare_minuti"] = *pr.agree_threshold_min;
    sv["liberta_oraria"] = lib;
    if (k.equipped.value_or(false)) sv["regole"] = {{"pedana_obbligatoria", true}};
    sv["domanda"] = {{"gruppi", JsonDati::array({g})}};
    if (k.individual.value_or(false)) sv["individualizzato"] = true;
    e.servizio = sv;
    // La corsa: un passaggio per riga, con i km scritti sulla riga (dal passaggio prima che ha dei km) e i totali del piede.
    JsonDati c = JsonDati::object();
    c["codice"] = v.trip;
    c["nome"] = nome_di(k);
    c["servizi"] = JsonDati::array({v.service});
    c["calendario"] = v.calendar;
    JsonDati ps = JsonDati::array();
    for (const Passaggio& p : pp) {
        JsonDati x = {{"luogo", p.luogo}, {"orario", kzero::text_from_minutes(p.orario)}};
        if (p.salgono || p.scendono) {
            JsonDati q = {{"gruppo", v.group}};
            if (p.salgono) q["salgono"] = p.salgono;
            if (p.scendono) q["scendono"] = p.scendono;
            x["persone"] = JsonDati::array({q});
        }
        if (p.assistente != kzero::AssistantMove::None) x["assistente"] = p.assistente == kzero::AssistantMove::Boards ? "sale" : "scende";
        if (p.pieni || p.vuoti) {
            JsonDati m = JsonDati::object();
            if (p.pieni) m["pieni"] = km(*p.pieni);
            if (p.vuoti) m["vuoti"] = km(*p.vuoti);
            x["km"] = m;
        }
        ps.push_back(x);
    }
    c["passaggi"] = ps;
    c["km_totali"] = {{"pieni", km(k.total_loaded_km)}, {"vuoti", km(k.total_empty_km)}};
    e.corsa = c;
    // Il calendario: la base del committente, il periodo della versione, i giorni segnati o le date una per una.
    const kzero::Calendar cal = kzero::card_calendar(k, v.calendar, v.valid);
    JsonDati kj = JsonDati::object();
    kj["codice"] = v.calendar;
    kj["nome"] = "Scheda " + k.nr + (v.number > 1 ? " (versione " + std::to_string(v.number) + ")" : std::string());
    kj["base"] = k.base_calendar;
    kj["dal"] = kzero::text_from_day(v.valid.from);
    kj["al"] = kzero::text_from_day(v.valid.to);
    JsonDati giorni = JsonDati::array();
    for (int i = 0; i < 7; ++i)
        if (cal.days.has(static_cast<kzero::Weekday>(i))) giorni.push_back(kGiorni[i]);
    kj["giorni"] = giorni;
    if (!cal.added.empty()) {
        JsonDati d = JsonDati::array();
        for (const kzero::DayEntry& x : cal.added) d.push_back(kzero::text_from_day(x.day));
        kj["date_aggiunte"] = d;
    }
    // K-023: le interruzioni della scheda, tagliate al periodo della versione (card_calendar).
    if (!cal.suspensions.empty()) {
        JsonDati d = JsonDati::array();
        for (const kzero::Suspension& x : cal.suspensions)
            d.push_back({{"dal", kzero::text_from_day(x.range.from)}, {"al", kzero::text_from_day(x.range.to)}, {"motivo", kMotivoInterruzione}});
        kj["sospensioni"] = d;
    }
    e.calendario = kj;
    (void)idx;
    return e;
}

JsonDati casa(const kzero::Code& codice) { return {{"codice", codice}, {"tipo", "porta_a_porta"}}; }

namespace {

JsonDati scheda_json(const kzero::Card& k) {
    JsonDati j = JsonDati::object();
    j["nr"] = k.nr;
    j["servizio"] = k.service_text;
    j["verso"] = k.direction == kzero::Direction::Return ? "ritorno" : "andata";
    j["scuole"] = k.schools;
    j["attivazione"] = kzero::text_from_day(k.from);
    j["disattivazione"] = kzero::text_from_day(k.to);
    JsonDati giorni = JsonDati::array();
    for (int i = 0; i < 7; ++i)
        if (k.days.has(static_cast<kzero::Weekday>(i))) giorni.push_back(kGiorni[i]);
    j["giorni"] = giorni;
    j["totali_alunni"] = k.pupils;
    if (k.individual) j["individualizzato"] = *k.individual;
    if (k.equipped) j["attrezzato"] = *k.equipped;
    j["tot_giorni"] = k.total_days;
    j["tot_km_carico"] = km(k.total_loaded_km);
    j["tot_km_vuoto"] = km(k.total_empty_km);
    j["calendario"] = k.calendar_word;
    j["calendario_base"] = k.base_calendar;
    j["cadenza"] = k.cadence_word;
    j["forma_cadenza"] = k.cadence_form == kzero::CadenceForm::ListedDates ? "date_elencate" : "giorni_segnati";
    j["nuova_modifica"] = k.is_change ? "modifica" : "nuova";
    JsonDati f = JsonDati::array();
    for (const kzero::CardStop& s : k.stops) {
        JsonDati x = {{"orario", kzero::text_from_minutes(s.time)}, {"luogo", s.place}};
        x["riconosciuto"] = s.match == kzero::CardStopMatch::School ? "scuola"
                            : s.match == kzero::CardStopMatch::Name  ? "nome"
                            : s.match == kzero::CardStopMatch::Home  ? "casa"
                                                                     : "codice_esterno";
        x["salgono"] = s.boarding;
        x["scendono"] = s.alighting;
        if (s.loaded_km) x["km_pieni"] = km(*s.loaded_km);
        if (s.empty_km) x["km_vuoti"] = km(*s.empty_km);
        if (s.escort != kzero::AssistantMove::None) x["accompagnatore"] = s.escort == kzero::AssistantMove::Boards ? "sale" : "scende";
        f.push_back(x);
    }
    j["fermate"] = f;
    if (!k.dates.empty()) {
        JsonDati d = JsonDati::array();
        for (kzero::Day x : k.dates) d.push_back(kzero::text_from_day(x));
        j["date"] = d;
    }
    if (!k.breaks.empty()) {   // K-023 (registro versione 2)
        JsonDati d = JsonDati::array();
        for (const kzero::DateRange& x : k.breaks) d.push_back({{"dal", kzero::text_from_day(x.from)}, {"al", kzero::text_from_day(x.to)}});
        j["interruzioni"] = d;
    }
    return j;
}

const char* modo_json(kzero::ChoiceMode m) {
    return m == kzero::ChoiceMode::Activation ? "attivazione" : m == kzero::ChoiceMode::TakenInCharge ? "presa_in_carico" : "data";
}
const char* da_json(kzero::ChoiceFrom f) { return f == kzero::ChoiceFrom::File ? "file" : f == kzero::ChoiceFrom::Card ? "scheda" : "nessuna"; }

}  // namespace

JsonDati registro(const std::vector<kzero::ServiceCard>& schede, const std::vector<kzero::RemovedCardVersions>& tolte) {
    JsonDati d = {{"schema", "schede_servizi"}, {"schema_versione", 2}};   // K-023: sempre la 2 (contiene la 1)
    JsonDati v = JsonDati::array();
    for (const kzero::ServiceCard& s : schede) {
        JsonDati j = {{"codice", s.code}, {"id", s.id}, {"committente", s.client.code}, {"profilo", s.profile.code}, {"contratto", s.contract.code}};
        JsonDati ve = JsonDati::array();
        for (const kzero::CardVersion& x : s.versions) {
            JsonDati y = {{"numero", x.number}, {"dal", kzero::text_from_day(x.valid.from)}, {"al", kzero::text_from_day(x.valid.to)}};
            if (x.previous_end) y["al_prima"] = kzero::text_from_day(*x.previous_end);
            if (!x.removed.empty()) y["tolte"] = x.removed;
            y["presa_in_carico"] = kzero::text_from_day(x.taken);
            y["riferimento"] = x.reference;
            y["scelta"] = {{"modo", modo_json(x.mode)}, {"da", da_json(x.from)}};
            y["elementi"] = {{"servizio", x.service}, {"gruppo", x.group}, {"corsa", x.trip}, {"calendario", x.calendar}};
            y["scheda"] = scheda_json(x.card);
            ve.push_back(y);
        }
        j["versioni"] = ve;
        v.push_back(j);
    }
    d["schede"] = v;
    if (!tolte.empty()) {
        JsonDati t = JsonDati::array();
        for (const kzero::RemovedCardVersions& x : tolte)
            t.push_back({{"scheda", x.card}, {"id", x.id}, {"versioni", x.versions}, {"riferimento", x.reference}, {"data", kzero::text_from_day(x.date)}});
        d["tolte"] = t;
    }
    return d;
}

Lettura con_documenti(const Lettura& vecchia, const std::map<std::string, JsonDati>& documenti) {
    PacchettoInMemoria m;
    for (const auto& [f, d] : vecchia.documenti)
        m.file[f] = chiusura::testo_file(f == "pacchetto.json" ? chiusura::manifesto_in_chiaro(d) : d);
    for (const auto& [f, d] : documenti) m.file[f] = chiusura::testo_file(d);
    Lettura l = leggi_pacchetto(m);
    if (!l.forma_valida()) throw std::logic_error("i documenti preparati non rispettano lo schema");
    return l;
}

std::map<std::string, JsonDati> documenti_dopo(const Lettura& vecchia, const std::vector<kzero::ServiceCard>& schede,
                                               const std::vector<kzero::RemovedCardVersions>& tolte,
                                               const std::vector<std::pair<kzero::Code, int>>& aggiunte,
                                               const std::vector<kzero::CardVersion>& via,
                                               const std::vector<kzero::CardsMerge::Cut>& fini, const std::vector<kzero::Code>& case_nuove) {
    std::map<std::string, JsonDati> out;
    const auto doc = [&](const std::string& file, const char* schema, int versione, const char* elenco) -> JsonDati& {
        if (!out.count(file)) {
            out[file] = vecchia.documenti.count(file) ? vecchia.documenti.at(file)
                                                      : JsonDati{{"schema", schema}, {"schema_versione", versione}, {elenco, JsonDati::array()}};
            if (!out[file].contains(elenco)) out[file][elenco] = JsonDati::array();
        }
        return out[file];
    };
    // Gli elementi delle versioni tolte se ne vanno.
    std::set<kzero::Code> via_servizi, via_corse, via_calendari;
    for (const kzero::CardVersion& v : via) {
        via_servizi.insert(v.service);
        via_corse.insert(v.trip);
        via_calendari.insert(v.calendar);
    }
    const auto togli = [](JsonDati& elenco, const std::set<kzero::Code>& codici) {
        JsonDati resta = JsonDati::array();
        for (const JsonDati& x : elenco)
            if (!codici.count(x.at("codice").get<std::string>())) resta.push_back(x);
        elenco = resta;
    };
    JsonDati& servizi = doc("servizi.json", "servizi", 4, "servizi");
    servizi["schema_versione"] = 4;   // la versione 4 contiene le altre: si legge "individualizzato" (K-021)
    JsonDati& corse = doc("corse.json", "corse", 1, "corse");
    JsonDati& calendari = doc("calendari.json", "calendari", 2, "calendari");
    togli(servizi["servizi"], via_servizi);
    togli(corse["corse"], via_corse);
    togli(calendari["calendari"], via_calendari);
    // I calendari accorciati (o ripresi): si rifanno dalla versione del registro, che ha gia' la fine nuova (anche le date una per
    // una dopo la fine se ne vanno: una data aggiunta vale anche fuori dal periodo).
    const kzero::Index idx(vecchia.pacchetto);
    for (const kzero::CardsMerge::Cut& f : fini) {
        const kzero::ServiceCard* s = nullptr;
        const kzero::CardVersion* v = nullptr;
        for (const kzero::ServiceCard& x : schede)
            for (const kzero::CardVersion& y : x.versions)
                if (y.calendar == f.calendar) {
                    s = &x;
                    v = &y;
                }
        const kzero::CardProfile* pr = s ? idx.card_profile(s->profile.code) : nullptr;
        if (!v || !pr || v->valid.to != f.to) throw std::logic_error("calendario accorciato senza la sua versione");
        for (JsonDati& c : calendari["calendari"])
            if (c.at("codice").get<std::string>() == f.calendar) c = elementi(*s, *v, *pr, idx).calendario;
    }
    // Gli elementi delle versioni nuove.
    for (const auto& [codice, numero] : aggiunte) {
        const kzero::ServiceCard* s = nullptr;
        for (const kzero::ServiceCard& x : schede)
            if (x.code == codice) s = &x;
        if (!s) throw std::logic_error("scheda aggiunta che non c'e' nel registro");
        const kzero::CardVersion* v = nullptr;
        for (const kzero::CardVersion& x : s->versions)
            if (x.number == numero) v = &x;
        const kzero::CardProfile* pr = idx.card_profile(s->profile.code);
        if (!v || !pr) throw std::logic_error("versione aggiunta o profilo che non c'e'");
        const Elementi e = elementi(*s, *v, *pr, idx);
        servizi["servizi"].push_back(e.servizio);
        corse["corse"].push_back(e.corsa);
        calendari["calendari"].push_back(e.calendario);
    }
    if (!case_nuove.empty()) {
        JsonDati& luoghi = doc("luoghi.json", "luoghi", 2, "luoghi");
        if (!luoghi.contains("luoghi_privati")) luoghi["luoghi_privati"] = JsonDati::array();
        for (const kzero::Code& c : case_nuove) luoghi["luoghi_privati"].push_back(casa(c));
    }
    out[kFile] = registro(schede, tolte);
    return out;
}

// --- K-CTR-170 ------------------------------------------------------------------------------------------------------------

namespace {

// I dati comuni che vengono dalla scheda.
JsonDati proietta(const JsonDati& x, const std::vector<const char*>& campi) {
    JsonDati o = JsonDati::object();
    for (const char* c : campi)
        if (x.contains(c)) o[c] = x.at(c);
    return o;
}
JsonDati proietta_servizio(const JsonDati& s) {
    JsonDati o = proietta(s, {"contratto", "calendario"});
    JsonDati gruppi = JsonDati::array();
    if (s.contains("domanda"))
        for (const JsonDati& g : s.at("domanda").at("gruppi")) gruppi.push_back(proietta(g, {"codice", "verso", "scuola", "conteggi", "fermate"}));
    o["gruppi"] = gruppi;
    return o;
}
JsonDati proietta_corsa(const JsonDati& c) { return proietta(c, {"servizi", "calendario", "passaggi", "km_totali"}); }
JsonDati proietta_calendario(const JsonDati& c) { return proietta(c, {"base", "dal", "al", "giorni", "date_aggiunte", "sospensioni"}); }

const JsonDati* trova(const Lettura& l, const std::string& file, const char* elenco, const kzero::Code& codice) {
    const auto it = l.documenti.find(file);
    if (it == l.documenti.end() || !it->second.contains(elenco)) return nullptr;
    for (const JsonDati& x : it->second.at(elenco))
        if (x.contains("codice") && x.at("codice") == codice) return &x;
    return nullptr;
}

std::string primo_diverso(const JsonDati& a, const JsonDati& b) {
    std::set<std::string> chiavi;
    for (const auto& [k, v] : a.items()) chiavi.insert(k);
    for (const auto& [k, v] : b.items()) chiavi.insert(k);
    for (const std::string& k : chiavi)
        if (!a.contains(k) || !b.contains(k) || a.at(k) != b.at(k)) return k;
    return std::string();
}

}  // namespace

std::vector<kzero::Problem> controlla_elementi(const Lettura& l) {
    std::vector<kzero::Problem> out;
    const kzero::Index idx(l.pacchetto);
    for (std::size_t i = 0; i < l.pacchetto.service_cards.size(); ++i) {
        const kzero::ServiceCard& s = l.pacchetto.service_cards[i];
        const kzero::CardProfile* pr = idx.card_profile(s.profile.code);
        if (!pr) continue;   // il riferimento che manca lo dice K-CTR-010
        for (const kzero::CardVersion& v : s.versions) {
            const Elementi e = elementi(s, v, *pr, idx);
            struct Cosa {
                const char* nome;
                const char* file;
                const char* elenco;
                kzero::Code codice;
                kzero::Element elemento;
                JsonDati atteso;
                JsonDati (*proiezione)(const JsonDati&);
            };
            const std::vector<Cosa> cose = {
                {"servizio", "servizi.json", "servizi", v.service, kzero::Element::Service, e.servizio, proietta_servizio},
                {"corsa", "corse.json", "corse", v.trip, kzero::Element::Trip, e.corsa, proietta_corsa},
                {"calendario", "calendari.json", "calendari", v.calendar, kzero::Element::Calendar, e.calendario, proietta_calendario}};
            for (const Cosa& c : cose) {
                const JsonDati* x = trova(l, c.file, c.elenco, c.codice);
                std::vector<kzero::Param> par = {kzero::text_param("cosa", c.nome), kzero::ref_param("codice", c.elemento, c.codice),
                                                 kzero::ref_param("scheda", kzero::Element::ServiceCard, s.code),
                                                 kzero::number_param("versione", v.number)};
                if (!x) {
                    out.push_back(kzero::Problem{kzero::Check::CardElementChanged, 1, v.src, par});
                    continue;
                }
                const std::string d = primo_diverso(c.proiezione(c.atteso), c.proiezione(*x));
                if (d.empty()) continue;
                par.push_back(kzero::text_param("campo", d));
                out.push_back(kzero::Problem{kzero::Check::CardElementChanged, 0, v.src, par});
            }
        }
    }
    return out;
}

}  // namespace kzero::api::schede
