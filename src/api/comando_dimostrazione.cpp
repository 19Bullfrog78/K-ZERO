// K-Zero API - comando "dimostrazione": stampa di prova con il pacchetto dimostrativo esempi/demo
// (DATI INVENTATI, incorporati nel programma), in testo o in JSON.
// Prima si costruisce il documento JSON, gia' filtrato secondo le classi dei dati (marcatura degli schemi);
// il testo si scrive da quel documento: cosi' il filtro e' lo stesso per le due uscite.
// Di base: dati personali sostituiti dal codice dell'elemento; dati particolari "riservato".
#include <algorithm>
#include <cmath>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include "controllo.hpp"
#include "dimostrazione.hpp"
#include "esecutori.hpp"
#include "pacchetto.hpp"
#include "schemi_dati.hpp"

namespace kzero::api {

namespace {

const char* const kPacchettoDemo = "esempi/demo";

// --- Filtro delle classi -----------------------------------------------------------------------------------

Classe piu_riservata(Classe a, Classe b) { return static_cast<int>(a) >= static_cast<int>(b) ? a : b; }

class Vista {
public:
    Vista(const Lettura& lettura, Visibilita visibilita) : lettura_(lettura), visibilita_(visibilita) {}

    const Visibilita& visibilita() const { return visibilita_; }

    // Classe del campo di un elemento, dalla marcatura dello schema del suo file.
    Classe classe(const kzero::Source& elemento, const std::string& campo) const {
        const auto v = lettura_.versioni.find(elemento.file);
        if (v == lettura_.versioni.end()) return Classe::Particolare;
        return SchemiDati::incorporati().classe(elemento.file.substr(0, elemento.file.rfind(".json")), v->second,
                                                elemento.pointer + "/" + campo);
    }

    // Valore di un campo dell'elemento: visibile, oppure sostituito dal codice (personale) o da "riservato".
    Json campo(const kzero::Source& elemento, const std::string& campo, Json valore, const std::string& codice) const {
        return filtra(classe(elemento, campo), std::move(valore), codice);
    }

    Json filtra(Classe c, Json valore, const std::string& codice) const {
        if (visibile(c, visibilita_)) return valore;
        return c == Classe::Personale ? Json(codice) : Json(kRiservato);
    }

private:
    const Lettura& lettura_;
    Visibilita visibilita_;
};

// --- Etichette --------------------------------------------------------------------------------------------

const char* nome_tipo_luogo(kzero::PlaceType t) {
    switch (t) {
        case kzero::PlaceType::Stop: return "fermata";
        case kzero::PlaceType::School: return "scuola";
        case kzero::PlaceType::Center: return "centro";
        case kzero::PlaceType::Depot: return "rimessa";
        case kzero::PlaceType::ExternalShelter: return "ricovero_esterno";
        case kzero::PlaceType::Parking: return "punto_sosta";
        case kzero::PlaceType::TransferPoint: return "punto_trasbordo";
        case kzero::PlaceType::DoorToDoor: return "porta_a_porta";
    }
    return "?";
}

const char* nome_categoria(kzero::ServiceType t) {
    switch (t) {
        case kzero::ServiceType::Line: return "linea";
        case kzero::ServiceType::Disability: return "disabili";
        case kzero::ServiceType::School: return "scolastico";
        case kzero::ServiceType::Hybrid: return "ibrido";
        case kzero::ServiceType::Other: return "altro";
    }
    return "?";
}

const char* nome_tipo_servizio(kzero::ServiceKind k) {
    switch (k) {
        case kzero::ServiceKind::Line: return "linea";
        case kzero::ServiceKind::School: return "scolastico";
        case kzero::ServiceKind::Disability: return "disabili";
        case kzero::ServiceKind::Other: return "altro";
    }
    return "?";
}

const char* nome_tipo_gruppo(kzero::GroupType t) {
    switch (t) {
        case kzero::GroupType::Students: return "studenti";
        case kzero::GroupType::Disabled: return "disabili";
        case kzero::GroupType::LineUsers: return "utenti_linea";
        case kzero::GroupType::AuthorizedAdults: return "adulti_autorizzati";
        case kzero::GroupType::Other: return "altro";
    }
    return "?";
}

const char* nome_categoria_mezzo(kzero::VehicleCategory c) {
    switch (c) {
        case kzero::VehicleCategory::M1: return "M1";
        case kzero::VehicleCategory::M2: return "M2";
        case kzero::VehicleCategory::M3: return "M3";
    }
    return "?";
}

const char* nome_grado(kzero::Grade g) {
    switch (g) {
        case kzero::Grade::Kindergarten: return "infanzia";
        case kzero::Grade::Primary: return "primaria";
        case kzero::Grade::LowerSecondary: return "secondaria_primo_grado";
        case kzero::Grade::UpperSecondary: return "secondaria_secondo_grado";
    }
    return "?";
}

const char* nome_stato(kzero::PassengerStatus s) {
    switch (s) {
        case kzero::PassengerStatus::Active: return "attivo";
        case kzero::PassengerStatus::Suspended: return "sospeso";
        case kzero::PassengerStatus::Unlocked: return "sbloccato";
    }
    return "?";
}

// Testo per le persone: "riservato" resta tale, gli altri codici con lo spazio al posto di "_".
std::string leggibile(const std::string& s) {
    std::string out = s;
    std::replace(out.begin(), out.end(), '_', ' ');
    return out;
}

// 12.5 -> "12,5"; con decimali fissi se richiesti (euro: 2).
std::string numero(double x, int decimali_fissi = -1) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    if (decimali_fissi >= 0) {
        o.setf(std::ios::fixed);
        o.precision(decimali_fissi);
        o << x;
    } else {
        o.setf(std::ios::fixed);
        o.precision(3);
        o << x;
    }
    std::string s = o.str();
    if (decimali_fissi < 0) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Testo di un valore del documento: numeri con la virgola, testi come sono.
std::string testo(const Json& j) {
    if (j.is_string()) return leggibile(j.get<std::string>());
    if (j.is_boolean()) return j.get<bool>() ? "si'" : "no";
    if (j.is_number_integer()) return std::to_string(j.get<long long>());
    if (j.is_number()) return numero(j.get<double>());
    if (j.is_null()) return "-";
    return j.dump();
}

std::string euro(const Json& j) { return j.is_null() ? "non indicato" : numero(j.get<double>(), 2) + " EUR"; }

// --- Il documento JSON ---------------------------------------------------------------------------------------

Json in_json(const DefinizioneComando& comando, const Resoconto& r, const Vista& vista) {
    const kzero::Package& p = r.lettura.pacchetto;
    const kzero::Index idx(p);
    Json j = documento(comando.schema);
    j["dati_inventati"] = p.manifest.invented;
    j["dati_personali_inclusi"] = vista.visibilita().personali;
    j["dati_particolari_inclusi"] = vista.visibilita().particolari;

    Json pacchetto = Json::object();
    pacchetto["codice"] = p.manifest.code;
    pacchetto["nome"] = p.manifest.name;
    pacchetto["periodo"] = {{"dal", kzero::text_from_day(p.manifest.period.from)}, {"al", kzero::text_from_day(p.manifest.period.to)}};
    pacchetto["livello"] = p.manifest.level == kzero::Level::Full ? "completo" : "solo_numeri";
    pacchetto["fuso_orario"] = p.manifest.timezone;
    pacchetto["valuta"] = p.manifest.currency;
    Json aziende = Json::array();
    for (const auto& a : p.manifest.companies) {
        Json x = Json::object();
        x["codice"] = a.code;
        x["nome"] = a.name;
        x["mezzi"] = std::count_if(p.vehicles.begin(), p.vehicles.end(), [&](const kzero::Vehicle& v) { return v.company.code == a.code; });
        aziende.push_back(x);
    }
    pacchetto["aziende"] = aziende;
    j["pacchetto"] = pacchetto;
    j["controllo"] = {{"errori", r.errori}, {"avvisi", r.avvisi}};

    Json mezzi = Json::array();
    for (const auto& v : p.vehicles) {
        Json x = Json::object();
        x["codice"] = v.code;
        x["azienda"] = v.company.code;
        x["targa"] = v.plate;
        x["categoria"] = nome_categoria_mezzo(v.category);
        x["posti"] = v.seats;
        x["posti_in_piedi"] = v.standing;
        x["carrozzine"] = v.wheelchair_places;
        x["posti_persi_per_carrozzina"] = v.seats_lost_per_wheelchair;
        x["pedana"] = v.has_ramp;
        x["ipotetico"] = v.hypothetical;
        x["costo_fisso_giornaliero"] = v.daily_fixed_cost ? Json(arrotonda(*v.daily_fixed_cost, 2)) : Json(nullptr);
        x["costo_al_km"] = v.cost_per_km ? Json(arrotonda(*v.cost_per_km, 2)) : Json(nullptr);
        mezzi.push_back(x);
    }
    j["mezzi"] = mezzi;

    Json luoghi = Json::array();
    for (const auto& l : p.places) {
        Json x = Json::object();
        x["codice"] = l.code;
        x["tipo"] = nome_tipo_luogo(l.type);
        x["nome"] = vista.campo(l.src, "nome", l.name, l.code);
        x["comune"] = vista.campo(l.src, "comune", l.municipality, l.code);
        luoghi.push_back(x);
    }
    j["luoghi"] = luoghi;

    Json servizi = Json::array();
    for (const auto& s : p.services) {
        Json x = Json::object();
        x["codice"] = s.code;
        x["nome"] = s.name;
        x["tipo"] = vista.campo(s.src, "tipo", nome_tipo_servizio(s.kind), s.code);
        x["contratto"] = s.contract.code;
        const kzero::Contract* c = idx.contract(s.contract.code);
        x["committente"] = c ? Json(c->client.code) : Json(nullptr);
        x["calendario"] = s.calendar.code;
        Json gruppi = Json::array();
        for (const auto& g : s.groups) {
            Json y = Json::object();
            y["codice"] = g.code;
            y["tipo"] = vista.campo(g.src, "tipo", nome_tipo_gruppo(g.type), g.code);
            y["scuola"] = g.school ? Json(g.school->code) : Json(nullptr);
            y["persone"] = g.counts == kzero::CountKind::Unknown ? Json(nullptr) : Json(g.total_boarding());
            y["fermate"] = g.stops.size();
            int carrozzine = 0;
            for (const auto& st : g.stops) carrozzine += st.wheelchairs;
            const kzero::Source prima_fermata = g.stops.empty() ? g.src : g.stops.front().src;
            y["carrozzine"] = vista.filtra(vista.classe(prima_fermata, "carrozzine"), carrozzine, g.code);
            y["assistente_richiesto"] = vista.campo(g.src, "assistente_richiesto", g.assistant_required, g.code);
            Json passeggeri = Json::array();
            for (const auto& q : g.passengers) {
                Json z = Json::object();
                z["codice"] = q.code;
                z["nome"] = vista.campo(q.src, "nome", q.name, q.code);
                z["stato"] = nome_stato(q.status);
                z["salita"] = q.boarding ? Json(q.boarding->place.code) : Json(nullptr);
                z["salita_bloccata"] = q.boarding && q.boarding->mode == kzero::Assignment::Locked;
                z["discesa"] = q.alighting ? Json(q.alighting->place.code) : Json(nullptr);
                // Il filtro vale per tutti, anche senza motivo: cosi' non si capisce chi ne ha uno.
                z["motivo"] = vista.campo(q.src, "motivo", q.reason.empty() ? Json(nullptr) : Json(q.reason), q.code);
                std::string esigenze;
                if (q.needs.wheelchair) esigenze += "carrozzina";
                if (q.needs.assistant) esigenze += std::string(esigenze.empty() ? "" : ", ") + "assistente";
                if (!q.needs.other.empty()) esigenze += (esigenze.empty() ? "" : ", ") + q.needs.other;
                z["esigenze"] = vista.campo(q.src, "esigenze", esigenze.empty() ? Json(nullptr) : Json(esigenze), q.code);
                passeggeri.push_back(z);
            }
            y["passeggeri"] = passeggeri;
            gruppi.push_back(y);
        }
        x["gruppi"] = gruppi;
        servizi.push_back(x);
    }
    j["servizi"] = servizi;

    // La categoria della corsa nasce dai tipi dei gruppi e dei servizi: ha la loro classe.
    Classe classe_categoria = Classe::Comune;
    for (const auto& s : p.services) {
        classe_categoria = piu_riservata(classe_categoria, vista.classe(s.src, "tipo"));
        for (const auto& g : s.groups) classe_categoria = piu_riservata(classe_categoria, vista.classe(g.src, "tipo"));
    }
    Json corse = Json::array();
    for (const auto& t : p.trips) {
        Json x = Json::object();
        x["codice"] = t.code;
        x["nome"] = t.name;
        const auto categoria = idx.category(t);
        x["categoria"] = vista.filtra(classe_categoria, categoria ? Json(nome_categoria(*categoria)) : Json(nullptr), t.code);
        Json servizi_corsa = Json::array();
        for (const auto& s : t.services) servizi_corsa.push_back(s.code);
        x["servizi"] = servizi_corsa;
        x["calendario"] = t.calendar.code;
        x["inizio"] = orario(t.start());
        x["fine"] = orario(t.end());
        x["km_pieni"] = arrotonda(t.loaded_km(), 3);
        x["km_vuoti"] = arrotonda(t.empty_km(), 3);
        const std::vector<kzero::OnBoardAfter> a_bordo = kzero::on_board(t, idx);
        // Con un gruppo dai conteggi non noti (es. utenti di una linea) le persone a bordo non si sanno.
        bool non_noti = false;
        for (const auto& sv : t.services)
            if (const kzero::Service* s = idx.service(sv.code))
                for (const auto& g : s->groups) non_noti = non_noti || g.counts == kzero::CountKind::Unknown;
        Json passaggi = Json::array();
        for (std::size_t i = 0; i < t.passages.size(); ++i) {
            const kzero::Passage& ps = t.passages[i];
            Json y = Json::object();
            y["orario"] = orario(ps.time);
            if (ps.departure) y["partenza"] = orario(*ps.departure);
            y["luogo"] = ps.place.code;
            const kzero::Place* luogo = idx.place(ps.place.code);
            y["nome_luogo"] = luogo ? vista.campo(luogo->src, "nome", luogo->name, luogo->code) : Json(nullptr);
            y["transito"] = ps.transit;
            int salgono = 0, scendono = 0;
            for (const auto& c : ps.persons) {
                salgono += c.boarding;
                scendono += c.alighting;
            }
            y["salgono"] = salgono;
            y["scendono"] = scendono;
            y["assistente"] = ps.assistant == kzero::AssistantMove::Boards    ? Json("sale")
                              : ps.assistant == kzero::AssistantMove::Alights ? Json("scende")
                                                                              : Json(nullptr);
            Json trasbordi = Json::array();
            for (const auto& h : ps.transfers)
                trasbordi.push_back({{"codice", h.code},
                                     {"azione", h.action == kzero::TransferAction::Deliver ? "consegna" : "ricezione"},
                                     {"persone", h.persons}});
            y["trasbordi"] = trasbordi;
            y["a_bordo"] = non_noti ? Json(nullptr) : Json(a_bordo[i].total);
            passaggi.push_back(y);
        }
        x["passaggi"] = passaggi;
        corse.push_back(x);
    }
    j["corse"] = corse;

    // Regole del mestiere applicate ai dati del pacchetto (calcoli del nucleo).
    const kzero::Dimostrazione d = kzero::calcola_dimostrazione(p);
    Json regole = Json::object();
    Json giri = Json::array();
    for (const auto& g : d.giri) {
        Json x = Json::object();
        x["servizio"] = g.servizio;
        x["persone"] = g.persone;
        Json m = Json::array();
        for (const auto& v : g.mezzi) m.push_back({{"mezzo", v.mezzo}, {"posti", v.posti}, {"giri", v.giri}});
        x["mezzi"] = m;
        giri.push_back(x);
    }
    regole["giri"] = giri;
    Json carrozzine = Json::array();
    for (const auto& c : d.carrozzine) {
        Json x = Json::object();
        x["mezzo"] = c.mezzo;
        Json casi = Json::array();
        for (const auto& k : c.casi)
            casi.push_back({{"carrozzine", k.carrozzine},
                            {"ammesso", k.ammesso},
                            {"posti_a_sedere", k.ammesso ? Json(k.posti_a_sedere) : Json(nullptr)}});
        x["casi"] = casi;
        carrozzine.push_back(x);
    }
    regole["carrozzine"] = carrozzine;
    Json assistente = Json::array();
    for (const auto& a : d.assistente) {
        Json x = Json::object();
        x["corsa"] = a.corsa;
        x["regolamento"] = a.regolamento;
        Json gradi = Json::array();
        for (const auto g : a.gradi) gradi.push_back(nome_grado(g));
        x["gradi"] = gradi;
        Json m = Json::array();
        for (const auto& v : a.mezzi) m.push_back({{"mezzo", v.mezzo}, {"posti", v.posti}, {"serve", v.serve}});
        x["mezzi"] = m;
        assistente.push_back(x);
    }
    regole["assistente"] = assistente;
    Json costi = Json::array();
    for (const auto& c : d.costi) {
        Json x = Json::object();
        x["servizio"] = c.servizio;
        x["posti_minimi"] = c.posti_minimi;
        x["posti_massimi"] = c.posti_massimi ? Json(*c.posti_massimi) : Json(nullptr);
        x["km_al_giorno"] = arrotonda(c.km_al_giorno, 3);
        Json m = Json::array();
        for (const auto& v : c.mezzi)
            m.push_back({{"mezzo", v.mezzo},
                         {"posti", v.posti},
                         {"ipotetico", v.ipotetico},
                         {"ammesso", v.ammesso},
                         {"costo_giornaliero", v.costo ? Json(arrotonda(*v.costo, 2)) : Json(nullptr)}});
        x["mezzi"] = m;
        costi.push_back(x);
    }
    regole["costi"] = costi;
    j["regole"] = regole;
    return j;
}

// --- Il testo, scritto dal documento JSON -----------------------------------------------------------------

std::string quanti(long long n, const char* uno, const char* molti) {
    return n == 0 ? std::string("nessun ") + uno : std::to_string(n) + " " + (n == 1 ? uno : molti);
}

std::string in_testo(const Json& j) {
    std::string t;
    const Json& pk = j.at("pacchetto");
    t += "=== PACCHETTO ===\n";
    t += pk.at("codice").get<std::string>() + "  " + pk.at("nome").get<std::string>() +
         (j.at("dati_inventati").get<bool>() ? "  (dati inventati)" : "") + "\n";
    t += "Periodo " + pk.at("periodo").at("dal").get<std::string>() + " - " + pk.at("periodo").at("al").get<std::string>() +
         ", livello " + testo(pk.at("livello")) + ", fuso orario " + pk.at("fuso_orario").get<std::string>() + ", valuta " +
         pk.at("valuta").get<std::string>() + "\n";
    t += "Controllo: " + quanti(j.at("controllo").at("errori").get<long long>(), "errore", "errori") + ", " +
         quanti(j.at("controllo").at("avvisi").get<long long>(), "avviso", "avvisi") + " (dettagli con kzero controlla)\n";
    t += std::string("Dati personali: ") + (j.at("dati_personali_inclusi").get<bool>() ? "mostrati" : "sostituiti dal codice") +
         "; dati particolari: " + (j.at("dati_particolari_inclusi").get<bool>() ? "mostrati" : "riservati") + ".\n";

    t += "\n=== AZIENDE E MEZZI ===\n";
    for (const auto& a : pk.at("aziende")) {
        t += a.at("codice").get<std::string>() + "  " + a.at("nome").get<std::string>() + "\n";
        for (const auto& v : j.at("mezzi")) {
            if (v.at("azienda") != a.at("codice")) continue;
            t += "   " + v.at("codice").get<std::string>() + "  " + v.at("targa").get<std::string>() + "  " +
                 v.at("categoria").get<std::string>() + "  posti " + testo(v.at("posti"));
            if (v.at("posti_in_piedi").get<int>() > 0) t += " + " + testo(v.at("posti_in_piedi")) + " in piedi";
            if (v.at("carrozzine").get<int>() > 0)
                t += "  carrozzine " + testo(v.at("carrozzine")) + " (" + testo(v.at("posti_persi_per_carrozzina")) +
                     " posti persi ciascuna)";
            if (v.at("pedana").get<bool>()) t += "  pedana";
            t += "  costo " + euro(v.at("costo_fisso_giornaliero")) + "/giorno + " + euro(v.at("costo_al_km")) + "/km";
            if (v.at("ipotetico").get<bool>()) t += "  (ipotetico)";
            t += "\n";
        }
    }

    t += "\n=== LUOGHI ===\n";
    for (const auto& l : j.at("luoghi")) {
        const std::string codice = l.at("codice").get<std::string>();
        t += codice + "  " + testo(l.at("tipo"));
        // Un nome sostituito dal codice non si ripete.
        if (l.at("nome") != l.at("codice")) t += "  " + l.at("nome").get<std::string>();
        if (l.at("comune") != l.at("codice")) t += "  (" + l.at("comune").get<std::string>() + ")";
        t += "\n";
    }

    t += "\n=== SERVIZI ===\n";
    for (const auto& s : j.at("servizi")) {
        t += s.at("codice").get<std::string>() + "  " + s.at("nome").get<std::string>() + "\n";
        t += "   tipo: " + testo(s.at("tipo")) + "  contratto " + s.at("contratto").get<std::string>() + " (committente " +
             testo(s.at("committente")) + ")  calendario " + s.at("calendario").get<std::string>() + "\n";
        for (const auto& g : s.at("gruppi")) {
            t += "   gruppo " + g.at("codice").get<std::string>() + "  tipo: " + testo(g.at("tipo"));
            if (!g.at("scuola").is_null()) t += "  scuola " + g.at("scuola").get<std::string>();
            t += "  persone " + (g.at("persone").is_null() ? std::string("non note") : testo(g.at("persone"))) + " in " +
                 testo(g.at("fermate")) + " fermate  carrozzine: " + testo(g.at("carrozzine")) +
                 "  assistente richiesto: " + testo(g.at("assistente_richiesto")) + "\n";
            for (const auto& q : g.at("passeggeri")) {
                t += "      passeggero " + q.at("codice").get<std::string>();
                if (q.at("nome") != q.at("codice")) t += "  " + q.at("nome").get<std::string>();
                t += "  " + testo(q.at("stato")) + "  sale a " + testo(q.at("salita")) +
                     (q.at("salita_bloccata").get<bool>() ? " (bloccata)" : "") + ", scende a " + testo(q.at("discesa"));
                if (!q.at("motivo").is_null() && q.at("motivo") != q.at("codice"))
                    t += "  motivo: " + q.at("motivo").get<std::string>();
                if (!q.at("esigenze").is_null()) t += "  esigenze: " + q.at("esigenze").get<std::string>();
                t += "\n";
            }
        }
    }

    t += "\n=== CORSE ===\n";
    for (const auto& c : j.at("corse")) {
        std::string servizi;
        for (const auto& s : c.at("servizi")) servizi += (servizi.empty() ? "" : ", ") + s.get<std::string>();
        t += c.at("codice").get<std::string>() + "  " + c.at("nome").get<std::string>() + "\n";
        t += "   categoria: " + testo(c.at("categoria")) + "  servizi " + servizi + "  calendario " +
             c.at("calendario").get<std::string>() + "  " + c.at("inizio").get<std::string>() + "-" +
             c.at("fine").get<std::string>() + "  km a pieno " + testo(c.at("km_pieni")) + ", a vuoto " +
             testo(c.at("km_vuoti")) + "\n";
        for (const auto& ps : c.at("passaggi")) {
            t += "   " + ps.at("orario").get<std::string>() + "  " + ps.at("luogo").get<std::string>();
            if (!ps.at("nome_luogo").is_null() && ps.at("nome_luogo") != ps.at("luogo"))
                t += "  " + ps.at("nome_luogo").get<std::string>();
            if (ps.contains("partenza")) t += "  riparte " + ps.at("partenza").get<std::string>();
            if (ps.at("transito").get<bool>()) t += "  transito";
            if (ps.at("salgono").get<int>() > 0) t += "  salgono " + testo(ps.at("salgono"));
            if (ps.at("scendono").get<int>() > 0) t += "  scendono " + testo(ps.at("scendono"));
            if (!ps.at("assistente").is_null()) t += "  " + ps.at("assistente").get<std::string>() + " l'assistente";
            for (const auto& h : ps.at("trasbordi"))
                t += std::string("  ") + (h.at("azione") == "consegna" ? "consegna" : "riceve") + " il trasbordo " +
                     h.at("codice").get<std::string>() + " (" + testo(h.at("persone")) + ")";
            t += "  a bordo " + (ps.at("a_bordo").is_null() ? std::string("non noto") : testo(ps.at("a_bordo"))) + "\n";
        }
    }

    const Json& r = j.at("regole");
    t += "\n=== REGOLE ===\n";
    for (const auto& g : r.at("giri")) {
        t += "Giri per il servizio " + g.at("servizio").get<std::string>() + " (" + testo(g.at("persone")) + " persone):";
        for (const auto& m : g.at("mezzi"))
            t += "  " + m.at("mezzo").get<std::string>() + " (" + testo(m.at("posti")) + " posti) " + testo(m.at("giri")) +
                 (m.at("giri").get<int>() == 1 ? " giro" : " giri") + ";";
        t.back() = '\n';
    }
    for (const auto& c : r.at("carrozzine")) {
        t += "Carrozzine su " + c.at("mezzo").get<std::string>() + ":";
        for (const auto& k : c.at("casi"))
            t += "  " + testo(k.at("carrozzine")) + " -> " +
                 (k.at("ammesso").get<bool>() ? testo(k.at("posti_a_sedere")) + " posti a sedere" : std::string("non ammesso")) +
                 ";";
        t.back() = '\n';
    }
    for (const auto& a : r.at("assistente")) {
        std::string gradi;
        for (const auto& g : a.at("gradi")) gradi += (gradi.empty() ? "" : ", ") + testo(g);
        t += "Assistente a bordo, corsa " + a.at("corsa").get<std::string>() + " (" + gradi + ", regolamento " +
             a.at("regolamento").get<std::string>() + "):";
        for (const auto& m : a.at("mezzi"))
            t += "  con " + m.at("mezzo").get<std::string>() + " (" + testo(m.at("posti")) + " posti) " +
                 (m.at("serve").get<bool>() ? "serve" : "non serve") + ";";
        t.back() = '\n';
    }
    for (const auto& c : r.at("costi")) {
        t += "Costo di una giornata del servizio " + c.at("servizio").get<std::string>() + " (mezzo minimo " +
             testo(c.at("posti_minimi")) + " posti" +
             (c.at("posti_massimi").is_null() ? std::string() : ", massimo " + testo(c.at("posti_massimi"))) + "; " +
             testo(c.at("km_al_giorno")) + " km al giorno):\n";
        for (const auto& m : c.at("mezzi"))
            t += "   " + m.at("mezzo").get<std::string>() + "  " + testo(m.at("posti")) + " posti" +
                 (m.at("ipotetico").get<bool>() ? " (ipotetico)" : "") + "  " +
                 (m.at("ammesso").get<bool>() ? "ammesso" : "non ammesso") + "  " + euro(m.at("costo_giornaliero")) + "\n";
    }
    return t;
}

}  // namespace

Risultato esegui_dimostrazione(const DefinizioneComando& comando, const Parametri& parametri) {
    const Visibilita visibilita{parametri.includi_dati_personali, parametri.includi_dati_particolari};
    const Resoconto r = controlla_pacchetto(PacchettoIncorporato(kPacchettoDemo), visibilita);
    // Il pacchetto dimostrativo e' provato a ogni compilazione: un suo errore e' un guasto del programma.
    if (r.errori > 0 || !r.contenuto_controllato)
        throw std::logic_error("il pacchetto dimostrativo " + std::string(kPacchettoDemo) + " ha errori (kzero controlla " +
                               kPacchettoDemo + ")");
    const Json j = in_json(comando, r, Vista(r.lettura, visibilita));
    return {parametri.formato == Formato::Json ? testo_json(j) : in_testo(j), 0};
}

}  // namespace kzero::api
