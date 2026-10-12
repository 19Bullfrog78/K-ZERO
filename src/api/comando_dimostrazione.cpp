// K-Zero API - comando "dimostrazione": la stampa di prova con dati inventati, in testo o in JSON.
// I calcoli li fa il nucleo (crea_dimostrazione); qui si prepara solo la presentazione.
// JSON: di base nessun dato personale; nomi, indirizzi e luoghi delle fermate (nel porta a porta
// la fermata e' la casa del passeggero) stanno nel blocco "dati_personali", solo se richiesto.
#include <cmath>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "dimostrazione.hpp"
#include "esecutori.hpp"

namespace kzero::api {

namespace {

// --- Etichette (testo); nel JSON diventano codici con "_" al posto degli spazi ---

const char* etichetta(Powertrain p) { return p == Powertrain::Electric ? "elettrico" : "termico"; }

const char* etichetta(VehicleType t) {
    switch (t) {
        case VehicleType::Car: return "auto";
        case VehicleType::Minibus: return "minibus";
        case VehicleType::Bus: return "autobus";
        case VehicleType::Accessible: return "attrezzato";
    }
    return "?";
}

const char* etichetta(ServiceType s) {
    switch (s) {
        case ServiceType::Line: return "linea";
        case ServiceType::Disability: return "handicap";
        case ServiceType::School: return "scolastico";
        case ServiceType::Hybrid: return "ibrido";
    }
    return "?";
}

const char* etichetta(DestinationKind k) {
    switch (k) {
        case DestinationKind::Kindergarten: return "infanzia";
        case DestinationKind::School: return "scuola";
        case DestinationKind::ExternalCenter: return "centro esterno";
    }
    return "?";
}

const char* etichetta(PassengerKind k) { return k == PassengerKind::Student ? "studente" : "handicap"; }

const char* etichetta(EscortProvider e) { return e == EscortProvider::Company ? "azienda" : "committente"; }

std::string codice(const char* etichetta) {
    std::string s = etichetta;
    for (char& c : s)
        if (c == ' ') c = '_';
    return s;
}

template <class T>
const T& trova(const std::vector<T>& elenco, Id id) {
    for (const auto& x : elenco)
        if (x.id == id) return x;
    throw std::logic_error("codice " + std::to_string(id) + " assente nella dimostrazione");
}

// 1.5 -> "1,5"
std::string con_virgola(double x) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << x;
    std::string s = o.str();
    for (char& c : s)
        if (c == '.') c = ',';
    return s;
}

// --- Testo: la stampa di prova di sempre ---------------------------------------

std::string in_testo(const Dimostrazione& d) {
    std::ostringstream o;
    o.imbue(std::locale::classic());

    o << "=== DESTINAZIONI ===\n";
    for (const auto& x : d.destinazioni)
        o << x.id << "  " << x.name << " (" << etichetta(x.kind) << ")  arrivo " << orario(x.entry.open) << "-"
          << orario(x.entry.close) << "  ripresa " << orario(x.exit.open) << "-" << orario(x.exit.close) << "\n";

    o << "\n=== PASSEGGERI ===\n";
    for (const auto& p : d.passeggeri)
        o << p.id << "  " << etichetta(p.kind) << "  " << p.address << "  -> dest. " << p.destination_id
          << "  arrivo entro " << orario(p.required_arrival) << (p.needs_wheelchair ? "  [carrozzina]" : "")
          << (p.needs_escort ? "  [accompagnatore]" : "")
          << (p.max_ride_time ? "  [max " + std::to_string(p.max_ride_time) + " min a bordo]" : "") << "\n";

    o << "\n=== MEZZI ===\n";
    for (const auto& v : d.mezzi)
        o << v.id << "  " << v.plate << "  " << etichetta(v.type) << "  " << etichetta(v.powertrain) << "  posti "
          << v.seats << " + carrozzine " << v.wheelchair_places << "  costo " << v.cost_per_km
          << " EUR/km  autonomia " << v.range_km << " km\n";

    o << "\n=== AUTISTI ===\n";
    for (const auto& a : d.autisti)
        o << a.id << "  " << a.name << "  servizio " << orario(a.availability.open) << "-"
          << orario(a.availability.close) << "\n";

    const Schedule& s = d.turno;
    o << "\n=== TURNO MEZZO " << s.vehicle_id << " / AUTISTA " << s.driver_id << " ===\n";
    for (const auto& r : s.routes) {
        o << "Corsa " << r.id << " (" << etichetta(r.service) << ") " << orario(r.start()) << "-" << orario(r.end())
          << "  km pieni " << r.loaded_km << "  km vuoti " << r.empty_km << "\n";
        for (const auto& f : r.stops)
            o << "   " << orario(f.arrival) << "  " << f.label << "  a bordo " << f.load_after << "\n";
    }
    o << "Totale: km pieni " << s.loaded_km() << ", km vuoti " << s.empty_km() << ", tempi morti "
      << s.idle_minutes() << " min\n";

    o << "\n=== RICOVERI ===\n";
    for (const auto& p : d.ricoveri)
        o << p.id << "  " << p.name << (p.is_main_depot ? "  [rimessa]" : "  [esterno]") << "  posti " << p.capacity
          << (p.has_charger ? "  [ricarica]" : "") << "\n";

    o << "\n=== AUTISTA " << trova(d.autisti, d.autista_esempio_id).name << " ===\n";
    for (const auto& t : d.tragitti)
        o << "Tragitto casa-" << trova(d.ricoveri, t.ricovero_id).name << ": " << t.km_al_giorno << " km/giorno\n";
    o << "Pause lunghe vicino a casa: " << d.pause.near_home << ", lontano da casa: " << d.pause.far_from_home
      << "\n";

    o << "\n=== POSTI A BORDO ===\n";
    const Vehicle& mezzo_giri = trova(d.mezzi, d.mezzo_giri_id);
    o << d.ragazzi_da_portare << " ragazzi su " << mezzo_giri.plate << " (" << mezzo_giri.seats
      << " posti): servono " << d.giri_necessari << " giri\n";
    const Vehicle& attrezzato = trova(d.mezzi, d.mezzo_carrozzine_id);
    const std::string ragazzi = std::to_string(d.ragazzi_prova_carrozzine) + " ragazzi";
    for (const auto& c : d.casi_carrozzine)
        o << attrezzato.plate << " con " << c.carrozzine << " carrozzine: posti a sedere "
          << (c.ammesso ? std::to_string(c.posti_a_sedere) : std::string("non ammesso"))
          << (c.ragazzi_ci_stanno ? "  -> " + ragazzi + " ci stanno" : "  -> " + ragazzi + " NON ci stanno") << "\n";
    const ConfrontoCosti& cc = d.confronto_costi;
    o << "Linea da " << cc.km_al_giorno << " km/giorno: " << cc.posti_flotta << " posti " << cc.costo_flotta
      << " EUR, " << cc.posti_richiesti << " posti " << cc.costo_richiesto << " EUR (differenza " << cc.differenza
      << " EUR/giorno)\n";

    o << "\n=== ASSISTENTE ===\n";
    o << "Corsa verso " << trova(d.destinazioni, d.destinazione_infanzia_id).name << ": assistente "
      << (d.assistente_infanzia ? "OBBLIGATORIO" : "non serve") << "\n";
    o << "Corsa " << d.corsa_handicap_id << " (con ragazzo handicap): assistente "
      << (d.assistente_corsa_handicap ? "OBBLIGATORIO" : "non serve") << "\n";
    o << trova(d.mezzi, d.mezzo_bambini_id).plate << " con " << d.bambini_con_assistente << " bambini + assistente: "
      << (d.bambini_con_assistente_ci_stanno ? "ci stanno" : "NON ci stanno (l'assistente occupa un posto)") << "\n";
    const Escort& ditta = trova(d.assistenti, d.assistente_ditta_id);
    for (const auto& c : d.contratti) {
        o << "Contratto \"" << c.client << "\": assistente ";
        if (c.escort_provider == EscortProvider::Company)
            o << "da pianificare e pagare (es. " << con_virgola(d.ore_assistente_ditta) << " ore x "
              << std::lround(ditta.hourly_cost) << " EUR = " << std::lround(d.costo_assistente_ditta)
              << " EUR/giorno)";
        else
            o << "fornito dal committente";
        o << "\n";
    }
    return o.str();
}

// --- JSON: codici e conteggi; dati personali solo nel blocco "dati_personali" ---

Json finestra(const TimeWindow& w) {
    Json j = Json::object();
    j["inizio"] = w.open;
    j["fine"] = w.close;
    return j;
}

Json destinazioni(const Dimostrazione& d) {
    Json elenco = Json::array();
    for (const auto& x : d.destinazioni) {
        Json j = Json::object();
        j["codice"] = x.id;
        j["nome"] = x.name;
        j["tipo"] = codice(etichetta(x.kind));
        j["arrivo"] = finestra(x.entry);
        j["ripresa"] = finestra(x.exit);
        elenco.push_back(j);
    }
    return elenco;
}

Json passeggeri(const Dimostrazione& d) {
    Json elenco = Json::array();
    for (const auto& p : d.passeggeri) {
        Json j = Json::object();
        j["codice"] = p.id;
        j["tipo"] = codice(etichetta(p.kind));
        j["destinazione"] = p.destination_id;
        j["arrivo_entro"] = p.required_arrival;
        j["carrozzina"] = p.needs_wheelchair;
        j["accompagnatore"] = p.needs_escort;
        j["massimo_a_bordo_minuti"] = p.max_ride_time > 0 ? Json(p.max_ride_time) : Json(nullptr);
        elenco.push_back(j);
    }
    return elenco;
}

Json mezzi(const Dimostrazione& d) {
    Json elenco = Json::array();
    for (const auto& v : d.mezzi) {
        Json j = Json::object();
        j["codice"] = v.id;
        j["targa"] = v.plate;
        j["tipo"] = codice(etichetta(v.type));
        j["motore"] = codice(etichetta(v.powertrain));
        j["posti"] = v.seats;
        j["carrozzine"] = v.wheelchair_places;
        j["costo_euro_km"] = v.cost_per_km;
        j["autonomia_km"] = v.range_km;
        elenco.push_back(j);
    }
    return elenco;
}

Json autisti(const Dimostrazione& d) {
    Json elenco = Json::array();
    for (const auto& a : d.autisti) {
        Json j = Json::object();
        j["codice"] = a.id;
        j["servizio"] = finestra(a.availability);
        elenco.push_back(j);
    }
    return elenco;
}

Json turno(const Dimostrazione& d) {
    const Schedule& s = d.turno;
    Json j = Json::object();
    j["mezzo"] = s.vehicle_id;
    j["autista"] = s.driver_id;
    Json corse = Json::array();
    for (const auto& r : s.routes) {
        Json c = Json::object();
        c["codice"] = r.id;
        c["categoria"] = codice(etichetta(r.service));
        c["inizio"] = r.start();
        c["fine"] = r.end();
        c["km_pieni"] = r.loaded_km;
        c["km_vuoti"] = r.empty_km;
        Json fermate = Json::array();
        for (std::size_t i = 0; i < r.stops.size(); ++i) {
            Json f = Json::object();
            f["numero"] = i + 1;
            f["orario"] = r.stops[i].arrival;
            f["a_bordo"] = r.stops[i].load_after;
            fermate.push_back(f);
        }
        c["fermate"] = fermate;
        corse.push_back(c);
    }
    j["corse"] = corse;
    Json totale = Json::object();
    totale["km_pieni"] = arrotonda(s.loaded_km(), 3);
    totale["km_vuoti"] = arrotonda(s.empty_km(), 3);
    totale["tempi_morti_minuti"] = s.idle_minutes();
    j["totale"] = totale;
    return j;
}

Json ricoveri(const Dimostrazione& d) {
    Json elenco = Json::array();
    for (const auto& p : d.ricoveri) {
        Json j = Json::object();
        j["codice"] = p.id;
        j["nome"] = p.name;
        j["rimessa"] = p.is_main_depot;
        j["posti"] = p.capacity;
        j["ricarica"] = p.has_charger;
        elenco.push_back(j);
    }
    return elenco;
}

Json autista_esempio(const Dimostrazione& d) {
    Json j = Json::object();
    j["autista"] = d.autista_esempio_id;
    Json tragitti = Json::array();
    for (const auto& t : d.tragitti) {
        Json x = Json::object();
        x["ricovero"] = t.ricovero_id;
        x["km_al_giorno"] = arrotonda(t.km_al_giorno, 3);
        tragitti.push_back(x);
    }
    j["tragitti"] = tragitti;
    Json pause = Json::object();
    pause["vicino_a_casa"] = d.pause.near_home;
    pause["lontano_da_casa"] = d.pause.far_from_home;
    j["pause_lunghe"] = pause;
    return j;
}

Json posti_a_bordo(const Dimostrazione& d) {
    Json j = Json::object();
    Json giri = Json::object();
    giri["ragazzi"] = d.ragazzi_da_portare;
    giri["mezzo"] = d.mezzo_giri_id;
    giri["giri_necessari"] = d.giri_necessari;
    j["giri"] = giri;

    Json carrozzine = Json::object();
    carrozzine["mezzo"] = d.mezzo_carrozzine_id;
    carrozzine["ragazzi"] = d.ragazzi_prova_carrozzine;
    Json casi = Json::array();
    for (const auto& c : d.casi_carrozzine) {
        Json x = Json::object();
        x["carrozzine"] = c.carrozzine;
        x["ammesso"] = c.ammesso;
        x["posti_a_sedere"] = c.ammesso ? Json(c.posti_a_sedere) : Json(nullptr);
        x["ragazzi_ci_stanno"] = c.ragazzi_ci_stanno;
        casi.push_back(x);
    }
    carrozzine["casi"] = casi;
    j["carrozzine"] = carrozzine;

    const ConfrontoCosti& cc = d.confronto_costi;
    Json costi = Json::object();
    costi["km_al_giorno"] = cc.km_al_giorno;
    Json flotta = Json::object();
    flotta["codice"] = cc.mezzo_flotta_id;
    flotta["posti"] = cc.posti_flotta;
    flotta["euro_giorno"] = arrotonda(cc.costo_flotta, 2);
    costi["mezzo_flotta"] = flotta;
    Json richiesto = Json::object();
    richiesto["posti"] = cc.posti_richiesti;
    richiesto["euro_giorno"] = arrotonda(cc.costo_richiesto, 2);
    costi["mezzo_richiesto"] = richiesto;
    costi["differenza_euro_giorno"] = arrotonda(cc.differenza, 2);
    j["confronto_costi"] = costi;
    return j;
}

Json assistente(const Dimostrazione& d) {
    Json j = Json::object();
    Json infanzia = Json::object();
    infanzia["destinazione"] = d.destinazione_infanzia_id;
    infanzia["obbligatorio"] = d.assistente_infanzia;
    j["infanzia"] = infanzia;
    Json corsa = Json::object();
    corsa["corsa"] = d.corsa_handicap_id;
    corsa["obbligatorio"] = d.assistente_corsa_handicap;
    j["corsa_handicap"] = corsa;
    Json bambini = Json::object();
    bambini["mezzo"] = d.mezzo_bambini_id;
    bambini["bambini"] = d.bambini_con_assistente;
    bambini["ci_stanno"] = d.bambini_con_assistente_ci_stanno;
    j["bambini_con_assistente"] = bambini;
    const Escort& ditta = trova(d.assistenti, d.assistente_ditta_id);
    Json contratti = Json::array();
    for (const auto& c : d.contratti) {
        Json x = Json::object();
        x["codice"] = c.id;
        x["committente"] = c.client;
        x["assistente_fornito_da"] = codice(etichetta(c.escort_provider));
        if (c.escort_provider == EscortProvider::Company) {
            Json costo = Json::object();
            costo["assistente"] = ditta.id;
            costo["ore"] = d.ore_assistente_ditta;
            costo["euro_ora"] = ditta.hourly_cost;
            costo["euro_giorno"] = arrotonda(d.costo_assistente_ditta, 2);
            x["costo_esempio"] = costo;
        }
        contratti.push_back(x);
    }
    j["contratti"] = contratti;
    return j;
}

Json dati_personali(const Dimostrazione& d) {
    Json j = Json::object();
    Json pass = Json::array();
    for (const auto& p : d.passeggeri) {
        Json x = Json::object();
        x["codice"] = p.id;
        x["indirizzo"] = p.address;
        pass.push_back(x);
    }
    j["passeggeri"] = pass;
    Json aut = Json::array();
    for (const auto& a : d.autisti) {
        Json x = Json::object();
        x["codice"] = a.id;
        x["nome"] = a.name;
        aut.push_back(x);
    }
    j["autisti"] = aut;
    Json fermate = Json::array();
    for (const auto& r : d.turno.routes)
        for (std::size_t i = 0; i < r.stops.size(); ++i) {
            Json x = Json::object();
            x["corsa"] = r.id;
            x["numero"] = i + 1;
            x["luogo"] = r.stops[i].label;
            fermate.push_back(x);
        }
    j["fermate"] = fermate;
    return j;
}

std::string in_json(const DefinizioneComando& comando, const Dimostrazione& d, bool includi_dati_personali) {
    Json j = documento(comando.schema);
    j["dati_inventati"] = true;
    j["dati_personali_inclusi"] = includi_dati_personali;
    j["destinazioni"] = destinazioni(d);
    j["passeggeri"] = passeggeri(d);
    j["mezzi"] = mezzi(d);
    j["autisti"] = autisti(d);
    j["turno"] = turno(d);
    j["ricoveri"] = ricoveri(d);
    j["autista_esempio"] = autista_esempio(d);
    j["posti_a_bordo"] = posti_a_bordo(d);
    j["assistente"] = assistente(d);
    if (includi_dati_personali) j["dati_personali"] = dati_personali(d);
    return testo_json(j);
}

}  // namespace

std::string esegui_dimostrazione(const DefinizioneComando& comando, const Parametri& parametri) {
    const Dimostrazione d = crea_dimostrazione();
    if (parametri.formato == Formato::Json) return in_json(comando, d, parametri.includi_dati_personali);
    return in_testo(d);
}

}  // namespace kzero::api
