// K-Zero - caricamento dati di prova e stampa a schermo.
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "domain_models.hpp"

using namespace kzero;

static std::string hhmm(Minutes m) {
    const int h = (m / 60) % 24, mm = m % 60;
    std::string s = std::to_string(h) + ":" + (mm < 10 ? "0" : "") + std::to_string(mm);
    return h < 10 ? "0" + s : s;
}
static const char* to_str(Powertrain p) { return p == Powertrain::Electric ? "elettrico" : "termico"; }
static const char* to_str(VehicleType t) {
    switch (t) {
        case VehicleType::Car: return "auto";
        case VehicleType::Minibus: return "minibus";
        case VehicleType::Bus: return "autobus";
        case VehicleType::Accessible: return "attrezzato";
    }
    return "?";
}
static const char* to_str(ServiceType s) {
    switch (s) {
        case ServiceType::Line: return "linea";
        case ServiceType::Disability: return "handicap";
        case ServiceType::School: return "scolastico";
        case ServiceType::Hybrid: return "ibrido";
    }
    return "?";
}

static const char* to_str(DestinationKind k) {
    switch (k) {
        case DestinationKind::Kindergarten: return "infanzia";
        case DestinationKind::School: return "scuola";
        case DestinationKind::ExternalCenter: return "centro esterno";
    }
    return "?";
}
static const char* to_str(PassengerKind k) { return k == PassengerKind::Student ? "studente" : "handicap"; }

int main() {
    // Dati di prova (coordinate indicative in Val di Non)
    std::vector<Destination> destinations = {
        {1, "Scuola Media Cles", DestinationKind::School, {46.3650, 11.0340},
         {7 * 60 + 40, 7 * 60 + 55}, {13 * 60, 13 * 60 + 15}},
        {2, "Primaria Revo'", DestinationKind::School, {46.3920, 11.0610},
         {7 * 60 + 55, 8 * 60 + 10}, {12 * 60 + 30, 12 * 60 + 45}},
        {3, "Doposcuola Cles", DestinationKind::ExternalCenter, {46.3630, 11.0360},
         {13 * 60 + 10, 13 * 60 + 30}, {17 * 60, 17 * 60 + 30}},
        {4, "Scuola dell'infanzia Rumo", DestinationKind::Kindergarten, {46.4420, 11.0170},
         {8 * 60, 8 * 60 + 30}, {15 * 60 + 30, 16 * 60}},
    };
    std::vector<Passenger> passengers = {
        // id, tipo, indirizzo, casa, destinazione, arrivo, ripresa, fermata, carrozzina, accompagnatore, salita, max a bordo
        {101, PassengerKind::Student, "Rumo, fraz. Marcena", {46.4410, 11.0190}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 1, 0},
        {102, PassengerKind::Student, "Livo, piazza", {46.4030, 11.0190}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 1, 0},
        {103, PassengerKind::Disability, "Cagno', via Centrale", {46.3960, 11.0450}, 1, 7 * 60 + 55, 13 * 60, 0, true, true, 5, 45},
        {104, PassengerKind::Student, "Cles, Scuola Media", {46.3650, 11.0340}, 3, 13 * 60 + 30, 17 * 60, 0, false, false, 1, 0},
    };
    // Flotta Alpinabus: 3 minibus + 1 auto (dati di prova: targhe, posti e consumi da sostituire)
    // Flotta Alpinabus: 3 minibus + 1 auto (dati di prova: da sostituire con quelli veri)
    // id, targa, tipo, motore, posti, carrozzine, posti persi/carrozzina, pedana, tempo pedana,
    // fisso/giorno, deposito part., deposito rientro, EUR/km, consumo/km, autonomia
    std::vector<Vehicle> vehicles = {
        {1, "MINIBUS-1", VehicleType::Minibus, Powertrain::Thermal, 16, 0, 0, false, 0, 45.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900},
        {2, "MINIBUS-2", VehicleType::Accessible, Powertrain::Thermal, 12, 2, 3, true, 6, 50.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900},
        {3, "MINIBUS-3", VehicleType::Minibus, Powertrain::Thermal, 16, 0, 0, false, 0, 45.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900},
        {4, "AUTO-1", VehicleType::Car, Powertrain::Thermal, 4, 0, 0, false, 0, 20.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.30, 0.06, 800},
    };
    std::vector<Driver> drivers = {
        {1, "Autista A", {46.4400, 11.0200}, {6 * 60 + 30, 14 * 60}, 6 * 60, {VehicleType::Bus, VehicleType::Minibus}},
        {2, "Autista B", {46.3650, 11.0300}, {7 * 60, 13 * 60 + 30}, 6 * 60, {VehicleType::Accessible, VehicleType::Car}},
    };
    std::vector<ParkingSpot> parkings = {
        {1, "Rimessa Cles", {46.3600, 11.0300}, 30, true, true},
        {2, "Ricovero Rumo", {46.4400, 11.0200}, 2, false, false},
    };

    auto find_p = [&](Id id) -> const Passenger* {
        for (const auto& p : passengers) if (p.id == id) return &p;
        return nullptr;
    };
    auto find_d = [&](Id id) -> const Destination* {
        for (const auto& d : destinations) if (d.id == id) return &d;
        return nullptr;
    };

    // Turno di esempio del mezzo attrezzato (scritto a mano: il calcolo vero lo fara' OR-Tools)
    Route r1;   // studenti + handicap insieme -> ibrido
    r1.id = 1; r1.passenger_ids = {101, 102, 103}; r1.destination_ids = {1};
    r1.stops = {{{46.4400, 11.0200}, "Deposito Rumo", 7 * 60 + 0, 0, 0},
                {{46.4410, 11.0190}, "Marcena", 7 * 60 + 5, 1, 1},
                {{46.4030, 11.0190}, "Livo", 7 * 60 + 20, 1, 2},
                {{46.3960, 11.0450}, "Cagno' (carrozzina)", 7 * 60 + 30, 1, 3},
                {{46.3650, 11.0340}, "Scuola Media Cles", 7 * 60 + 45, -3, 0}};
    r1.loaded_km = 16.0; r1.empty_km = 0.5;

    Route r2;   // solo utenti di linea -> linea
    r2.id = 2; r2.carries_line_users = true;
    r2.stops = {{{46.3650, 11.0340}, "Cles autostazione", 8 * 60 + 5, 6, 6},
                {{46.4400, 11.0200}, "Rumo", 8 * 60 + 40, -6, 0}};
    r2.loaded_km = 15.0; r2.empty_km = 0.3;

    Route r3;   // studente verso doposcuola -> ibrido
    r3.id = 3; r3.passenger_ids = {104}; r3.destination_ids = {3};
    r3.stops = {{{46.3650, 11.0340}, "Scuola Media Cles", 13 * 60 + 5, 1, 1},
                {{46.3630, 11.0360}, "Doposcuola Cles", 13 * 60 + 15, -1, 0}};
    r3.loaded_km = 0.8; r3.empty_km = 15.0;

    for (Route* r : {&r1, &r2, &r3}) r->service = derive_service(*r, find_p, find_d);
    Schedule s1{2, 2, 1, {r1, r2, r3}};

    std::cout << "=== DESTINAZIONI ===\n";
    for (const auto& d : destinations)
        std::cout << d.id << "  " << d.name << " (" << to_str(d.kind) << ")  arrivo " << hhmm(d.entry.open) << "-"
                  << hhmm(d.entry.close) << "  ripresa " << hhmm(d.exit.open) << "-" << hhmm(d.exit.close) << "\n";

    std::cout << "\n=== PASSEGGERI ===\n";
    for (const auto& p : passengers)
        std::cout << p.id << "  " << to_str(p.kind) << "  " << p.address << "  -> dest. " << p.destination_id
                  << "  arrivo entro " << hhmm(p.required_arrival)
                  << (p.needs_wheelchair ? "  [carrozzina]" : "") << (p.needs_escort ? "  [accompagnatore]" : "")
                  << (p.max_ride_time ? "  [max " + std::to_string(p.max_ride_time) + " min a bordo]" : "") << "\n";

    std::cout << "\n=== MEZZI ===\n";
    for (const auto& v : vehicles)
        std::cout << v.id << "  " << v.plate << "  " << to_str(v.type) << "  " << to_str(v.powertrain)
                  << "  posti " << v.seats << " + carrozzine " << v.wheelchair_places << "  costo " << v.cost_per_km
                  << " EUR/km  autonomia " << v.range_km << " km\n";

    std::cout << "\n=== AUTISTI ===\n";
    for (const auto& d : drivers)
        std::cout << d.id << "  " << d.name << "  servizio " << hhmm(d.availability.open) << "-"
                  << hhmm(d.availability.close) << "\n";

    std::cout << "\n=== TURNO MEZZO " << s1.vehicle_id << " / AUTISTA " << s1.driver_id << " ===\n";
    for (const auto& r : s1.routes) {
        std::cout << "Corsa " << r.id << " (" << to_str(r.service) << ") " << hhmm(r.start()) << "-"
                  << hhmm(r.end()) << "  km pieni " << r.loaded_km << "  km vuoti " << r.empty_km << "\n";
        for (const auto& sv : r.stops)
            std::cout << "   " << hhmm(sv.arrival) << "  " << sv.label << "  a bordo " << sv.load_after << "\n";
    }
    std::cout << "Totale: km pieni " << s1.loaded_km() << ", km vuoti " << s1.empty_km()
              << ", tempi morti " << s1.idle_minutes() << " min\n";

    std::cout << "\n=== RICOVERI ===\n";
    for (const auto& p : parkings)
        std::cout << p.id << "  " << p.name << (p.is_main_depot ? "  [rimessa]" : "  [esterno]")
                  << "  posti " << p.capacity << (p.has_charger ? "  [ricarica]" : "") << "\n";

    const Driver& drv = drivers[1];
    std::cout << "\n=== AUTISTA " << drv.name << " ===\n";
    for (const auto& p : parkings)
        std::cout << "Tragitto casa-" << p.name << ": " << driver_commute_km(drv, p) << " km/giorno\n";
    const BreakReport br = check_home_breaks(s1, drv);
    std::cout << "Pause lunghe vicino a casa: " << br.near_home << ", lontano da casa: " << br.far_from_home << "\n";

    // --- Esempio posti: 38 ragazzi Romallo/Revo'/Tregiovo con un 16 posti ---
    std::cout << "\n=== POSTI A BORDO ===\n";
    const Vehicle& m16 = vehicles[0];
    std::cout << "38 ragazzi su " << m16.plate << " (" << m16.seats << " posti): servono "
              << trips_needed(m16, 38) << " giri\n";

    // --- Mezzo attrezzato: carrozzine che tolgono posti ---
    const Vehicle& att = vehicles[1];
    for (int w = 0; w <= 3; ++w)
        std::cout << att.plate << " con " << w << " carrozzine: posti a sedere "
                  << (w <= att.wheelchair_places ? std::to_string(att.seats - w * att.seats_lost_per_wheelchair)
                                                 : std::string("non ammesso"))
                  << (fits(att, 6, w) ? "  -> 6 ragazzi ci stanno" : "  -> 6 ragazzi NON ci stanno") << "\n";

    // --- Linea con richiesta "30 posti": confronto costi ---
    Vehicle bus30 = {9, "BUS-30 (ipotetico)", VehicleType::Bus, Powertrain::Thermal, 30, 0, 0, false, 0,
                     90.0, {}, {}, 0.95, 0.22, 900};
    const double km = 120;
    std::cout << "Linea da " << km << " km/giorno: 16 posti " << day_cost(m16, km) << " EUR, 30 posti "
              << day_cost(bus30, km) << " EUR (differenza " << day_cost(bus30, km) - day_cost(m16, km)
              << " EUR/giorno)\n";

    // --- Assistente a bordo ---
    std::vector<Contract> contracts = {
        {1, "Provincia - scolastico", EscortProvider::Client},
        {2, "Comune - asilo (assistente a carico ditta)", EscortProvider::Company},
    };
    std::vector<Escort> escorts = {
        {1, "Assistente comunale", {46.4420, 11.0170}, {7 * 60 + 30, 9 * 60}, EscortProvider::Client, 0.0},
        {2, "Assistente ditta", {46.4400, 11.0200}, {7 * 60 + 30, 9 * 60}, EscortProvider::Company, 18.0},
    };
    std::cout << "\n=== ASSISTENTE ===\n";
    Route asilo; asilo.id = 10; asilo.destination_ids = {4};
    std::cout << "Corsa verso " << destinations[3].name << ": assistente "
              << (needs_escort(asilo, find_p, find_d) ? "OBBLIGATORIO" : "non serve") << "\n";
    std::cout << "Corsa 1 (con ragazzo handicap): assistente "
              << (needs_escort(r1, find_p, find_d) ? "OBBLIGATORIO" : "non serve") << "\n";
    std::cout << m16.plate << " con 16 bambini + assistente: "
              << (fits(m16, 16, 0, true) ? "ci stanno" : "NON ci stanno (l'assistente occupa un posto)") << "\n";
    for (const auto& c : contracts) {
        const bool company = c.escort_provider == EscortProvider::Company;
        std::cout << "Contratto \"" << c.client << "\": assistente "
                  << (company ? "da pianificare e pagare (es. 1,5 ore x " : "fornito dal committente")
                  << (company ? std::to_string(int(escorts[1].hourly_cost)) + " EUR = " +
                                    std::to_string(int(1.5 * escorts[1].hourly_cost)) + " EUR/giorno)"
                              : "")
                  << "\n";
    }
    return 0;
}
