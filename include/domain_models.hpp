#pragma once
// Modelli di dominio K-Zero: trasporto scolastico provinciale.
// Orari = minuti dalla mezzanotte (es. 7:45 -> 465). Distanze in km.

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace kzero {

using Id = std::uint32_t;
using Minutes = int;

struct Coord {
    double lat = 0.0;
    double lon = 0.0;
};

// Distanza in linea d'aria (km). Provvisoria: verra' sostituita dalla matrice delle distanze stradali.
inline double air_km(const Coord& a, const Coord& b) {
    constexpr double R = 6371.0, D = 3.14159265358979 / 180.0;
    const double dlat = (b.lat - a.lat) * D, dlon = (b.lon - a.lon) * D;
    const double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(a.lat * D) * std::cos(b.lat * D) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * R * std::asin(std::sqrt(h));
}

struct TimeWindow {
    Minutes open = 0;   // primo istante ammesso
    Minutes close = 0;  // ultimo istante ammesso
};

enum class Powertrain { Thermal, Electric };
enum class VehicleType { Car, Minibus, Bus, Accessible };   // Accessible = con pedana

// Macro categorie di servizio
enum class ServiceType {
    Line,        // Servizi di Linee
    Disability,  // Servizi Handicap
    School,      // Servizi Scolastici
    Hybrid       // Servizi Ibridi (calcolato, vedi derive_service)
};

// Tipo di passeggero trasportato (non di corsa)
enum class PassengerKind { Student, Disability };

// Dove si porta il passeggero
enum class DestinationKind {
    Kindergarten,    // scuola dell'infanzia (asilo): assistente sempre a bordo
    School,          // primaria, media, superiore
    ExternalCenter   // doposcuola, centro diurno...
};

// --- Entita' --------------------------------------------------------------

struct Destination {
    Id id = 0;
    std::string name;
    DestinationKind kind = DestinationKind::School;
    Coord location;
    TimeWindow entry;   // finestra di arrivo
    TimeWindow exit;    // finestra di ripresa
};

struct Passenger {
    Id id = 0;
    PassengerKind kind = PassengerKind::Student;
    std::string address;
    Coord home;
    Id destination_id = 0;
    Minutes required_arrival = 0;   // entro quando deve arrivare
    Minutes exit_time = 0;          // ora di ripresa
    Id stop_id = 0;                 // fermata assegnata (0 = porta a porta)
    // Esigenze particolari (tipiche dei servizi handicap)
    bool needs_wheelchair = false;
    bool needs_escort = false;      // serve accompagnatore a bordo
    Minutes boarding_time = 1;      // tempo di salita/discesa
    Minutes max_ride_time = 0;      // tempo massimo a bordo (0 = nessun limite)
};

// Nomi richiesti nella prima versione, mantenuti come sinonimi
using School = Destination;
using Student = Passenger;

struct Vehicle {
    Id id = 0;
    std::string plate;
    VehicleType type = VehicleType::Bus;
    Powertrain powertrain = Powertrain::Thermal;
    int seats = 0;                  // posti a sedere senza carrozzine a bordo
    int wheelchair_places = 0;      // carrozzine trasportabili al massimo
    int seats_lost_per_wheelchair = 0;  // posti a sedere persi per ogni carrozzina caricata
    bool has_ramp = false;          // pedana o sollevatore
    Minutes ramp_time = 0;          // tempo per azionare la pedana a ogni fermata
    double daily_fixed_cost = 0.0;  // EUR/giorno: assicurazione, bollo, ammortamento
    Coord start_depot;
    Coord end_depot;
    double cost_per_km = 0.0;       // EUR/km
    double consumption_per_km = 0;  // litri/km (termico) o kWh/km (elettrico)
    double range_km = 0.0;          // autonomia; rilevante per gli elettrici
};

// Punto dove un mezzo passa la notte: la rimessa oppure un ricovero esterno
// vicino a casa dell'autista, per accorciargli il tragitto casa-lavoro.
struct ParkingSpot {
    Id id = 0;
    std::string name;
    Coord location;
    int capacity = 1;           // quanti mezzi ci stanno
    bool is_main_depot = false; // true = rimessa aziendale
    bool has_charger = false;   // necessario per i mezzi elettrici che ci dormono
};

// Ci stanno questi passeggeri sul mezzo? L'assistente, se presente, occupa un posto.
inline bool fits(const Vehicle& v, int seated, int wheelchairs, bool with_escort = false) {
    if (wheelchairs > 0 && !v.has_ramp) return false;
    if (wheelchairs > v.wheelchair_places) return false;
    const int free_seats = v.seats - wheelchairs * v.seats_lost_per_wheelchair - (with_escort ? 1 : 0);
    return seated <= free_seats;
}

// Chi fornisce e paga l'assistente: lo stabilisce il contratto.
enum class EscortProvider {
    Client,     // Provincia, Comune, associazione: per l'azienda occupa solo un posto
    Company     // l'azienda: va pianificato e pagato come l'autista
};

// Contratto con un committente: le regole che valgono per le sue corse.
struct Contract {
    Id id = 0;
    std::string client;                 // "Provincia di Trento", "Comune di ..."
    EscortProvider escort_provider = EscortProvider::Client;
};

// Assistente/accompagnatore a bordo: una persona con i suoi orari, come l'autista.
struct Escort {
    Id id = 0;
    std::string name;
    Coord home;                     // dove sale a bordo se non e' gia' sul mezzo
    TimeWindow availability;        // inizio e fine servizio
    EscortProvider provider = EscortProvider::Client;
    double hourly_cost = 0.0;       // EUR/ora, conta solo se provider = Company
};

// Costo di una giornata del mezzo: quota fissa + km percorsi.
inline double day_cost(const Vehicle& v, double total_km) {
    return v.daily_fixed_cost + total_km * v.cost_per_km;
}

// Quanti giri servono per portare tutti con un mezzo (solo posti a sedere).
inline int trips_needed(const Vehicle& v, int passengers) {
    return v.seats > 0 ? (passengers + v.seats - 1) / v.seats : 0;
}

struct Driver {
    Id id = 0;
    std::string name;
    Coord home;                     // abitazione
    TimeWindow availability;        // inizio e fine servizio
    Minutes max_driving = 0;        // guida massima nel turno
    std::vector<VehicleType> allowed_types;  // patenti / abilitazioni
    // Preferenza (non obbligo): pause lunghe vicino a casa
    bool wants_break_near_home = true;
    double home_radius_km = 5.0;    // entro quanti km la pausa conta "vicino a casa"
    Minutes min_home_break = 60;    // sotto questa durata non vale la pena andare a casa
    double max_home_detour_km = 10.0;   // km in piu' accettati per andare verso casa;
                                        // oltre, l'autista aspetta alla fermata
};

// Quanto pesa ogni voce nel calcolo. I valori sono DA DECIDERE con l'azienda:
// dicono al programma, ad esempio, quanti km a vuoto vale una pausa vicino a casa.
struct ObjectiveWeights {
    double per_empty_km = 1.0;              // ogni km a vuoto
    double per_idle_minute = 0.05;          // ogni minuto di mezzo fermo
    double per_commute_km = 0.5;            // ogni km casa-ricovero dell'autista
    double per_missed_home_break = 10.0;    // pausa lunga lontano da casa
};

// --- Risultato della pianificazione ---------------------------------------

struct StopVisit {
    Coord where;
    std::string label;          // "Deposito", "Fermata 12", "Scuola X"...
    Minutes arrival = 0;
    int boarding = 0;           // + saliti, - scesi
    int load_after = 0;         // passeggeri a bordo dopo la fermata
};

// Una corsa: dal primo carico all'ultimo scarico (+ trasferimenti attorno).
struct Route {
    Id id = 0;
    ServiceType service = ServiceType::School;
    std::vector<Id> passenger_ids;      // passeggeri nominativi (studenti, handicap)
    std::vector<Id> destination_ids;    // destinazioni servite
    bool carries_line_users = false;    // utenti di linea anonimi a bordo
    // Richieste del committente sul mezzo (0/false = nessuna richiesta)
    int min_seats = 0;                  // es. "serve un 30 posti"
    bool requires_ramp = false;
    Id contract_id = 0;                 // contratto di riferimento
    Id escort_id = 0;                   // assistente assegnato (0 = nessuno)
    std::vector<StopVisit> stops;
    double loaded_km = 0.0;     // km con almeno un passeggero
    double empty_km = 0.0;      // km a vuoto (trasferimenti)
    Minutes start() const { return stops.empty() ? 0 : stops.front().arrival; }
    Minutes end() const { return stops.empty() ? 0 : stops.back().arrival; }
};

// Categoria della corsa ricavata da chi trasporta e dove lo porta:
// - solo utenti di linea                -> Linea
// - solo passeggeri handicap            -> Handicap
// - solo studenti verso scuole          -> Scolastico
// - categorie miste, oppure studenti
//   verso un centro esterno             -> Ibrido
template <class FindPassenger, class FindDestination>
ServiceType derive_service(const Route& r, FindPassenger find_p, FindDestination find_d) {
    bool students = false, disability = false, external = false;
    for (Id pid : r.passenger_ids) {
        if (const Passenger* p = find_p(pid)) {
            if (p->kind == PassengerKind::Student) students = true;
            else disability = true;
        }
    }
    for (Id did : r.destination_ids)
        if (const Destination* d = find_d(did); d && d->kind == DestinationKind::ExternalCenter)
            external = true;

    const int kinds = int(students) + int(disability) + int(r.carries_line_users);
    if (kinds > 1) return ServiceType::Hybrid;
    if (students && external) return ServiceType::Hybrid;
    if (disability) return ServiceType::Disability;
    if (students) return ServiceType::School;
    return ServiceType::Line;
}

// Serve l'assistente su questa corsa? Si' se c'e' un bambino d'asilo
// o un passeggero che lo richiede.
template <class FindPassenger, class FindDestination>
bool needs_escort(const Route& r, FindPassenger find_p, FindDestination find_d) {
    for (Id pid : r.passenger_ids)
        if (const Passenger* p = find_p(pid); p && p->needs_escort) return true;
    for (Id did : r.destination_ids)
        if (const Destination* d = find_d(did); d && d->kind == DestinationKind::Kindergarten) return true;
    return false;
}

// Giornata tipo di un mezzo: piu' corse in sequenza con lo stesso autista.
struct Schedule {
    Id vehicle_id = 0;
    Id driver_id = 0;
    Id parking_id = 0;          // dove il mezzo inizia e finisce la giornata
    std::vector<Route> routes;

    double loaded_km() const {
        double t = 0; for (const auto& r : routes) t += r.loaded_km; return t;
    }
    double empty_km() const {
        double t = 0; for (const auto& r : routes) t += r.empty_km; return t;
    }
    Minutes idle_minutes() const {  // tempi morti tra una corsa e l'altra
        Minutes t = 0;
        for (std::size_t i = 1; i < routes.size(); ++i)
            t += routes[i].start() - routes[i - 1].end();
        return t;
    }
};

// Pause lunghe del turno: quante cadono vicino a casa dell'autista e quante no.
struct BreakReport {
    int near_home = 0;
    int far_from_home = 0;
};

inline BreakReport check_home_breaks(const Schedule& s, const Driver& d) {
    BreakReport rep;
    for (std::size_t i = 1; i < s.routes.size(); ++i) {
        const auto& prev = s.routes[i - 1];
        const auto& next = s.routes[i];
        if (prev.stops.empty() || next.stops.empty()) continue;
        if (next.start() - prev.end() < d.min_home_break) continue;   // pausa breve: non conta
        const bool near = air_km(prev.stops.back().where, d.home) <= d.home_radius_km &&
                          air_km(next.stops.front().where, d.home) <= d.home_radius_km;
        near ? ++rep.near_home : ++rep.far_from_home;
    }
    return rep;
}

// Tragitto casa -> ricovero del mezzo (andata e ritorno), in km.
inline double driver_commute_km(const Driver& d, const ParkingSpot& p) {
    return 2.0 * air_km(d.home, p.location);
}

}  // namespace kzero
