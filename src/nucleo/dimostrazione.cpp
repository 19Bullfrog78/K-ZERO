// K-Zero - scenario di dimostrazione con DATI INVENTATI (coordinate indicative in Val di Non).
#include "dimostrazione.hpp"

namespace kzero {

Dimostrazione crea_dimostrazione() {
    Dimostrazione d;
    d.destinazioni = {
        {1, "Scuola Media Cles", DestinationKind::School, {46.3650, 11.0340},
         {7 * 60 + 40, 7 * 60 + 55}, {13 * 60, 13 * 60 + 15}},
        {2, "Primaria Revo'", DestinationKind::School, {46.3920, 11.0610},
         {7 * 60 + 55, 8 * 60 + 10}, {12 * 60 + 30, 12 * 60 + 45}},
        {3, "Doposcuola Cles", DestinationKind::ExternalCenter, {46.3630, 11.0360},
         {13 * 60 + 10, 13 * 60 + 30}, {17 * 60, 17 * 60 + 30}},
        {4, "Scuola dell'infanzia Rumo", DestinationKind::Kindergarten, {46.4420, 11.0170},
         {8 * 60, 8 * 60 + 30}, {15 * 60 + 30, 16 * 60}},
    };
    d.passeggeri = {
        // id, tipo, indirizzo, casa, destinazione, arrivo, ripresa, fermata, carrozzina, accompagnatore, salita, max a bordo
        {101, PassengerKind::Student, "Rumo, fraz. Marcena", {46.4410, 11.0190}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 1, 0},
        {102, PassengerKind::Student, "Livo, piazza", {46.4030, 11.0190}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 1, 0},
        {103, PassengerKind::Disability, "Cagno', via Centrale", {46.3960, 11.0450}, 1, 7 * 60 + 55, 13 * 60, 0, true, true, 5, 45},
        {104, PassengerKind::Student, "Cles, Scuola Media", {46.3650, 11.0340}, 3, 13 * 60 + 30, 17 * 60, 0, false, false, 1, 0},
    };
    // Flotta Alpinabus: 3 minibus + 1 auto (dati di prova: da sostituire con quelli veri)
    // id, targa, tipo, motore, posti, carrozzine, posti persi/carrozzina, pedana, tempo pedana,
    // fisso/giorno, deposito part., deposito rientro, EUR/km, consumo/km, autonomia
    d.mezzi = {
        {1, "MINIBUS-1", VehicleType::Minibus, Powertrain::Thermal, 16, 0, 0, false, 0, 45.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900},
        {2, "MINIBUS-2", VehicleType::Accessible, Powertrain::Thermal, 12, 2, 3, true, 6, 50.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900},
        {3, "MINIBUS-3", VehicleType::Minibus, Powertrain::Thermal, 16, 0, 0, false, 0, 45.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900},
        {4, "AUTO-1", VehicleType::Car, Powertrain::Thermal, 4, 0, 0, false, 0, 20.0,
         {46.4400, 11.0200}, {46.4400, 11.0200}, 0.30, 0.06, 800},
    };
    d.autisti = {
        {1, "Autista A", {46.4400, 11.0200}, {6 * 60 + 30, 14 * 60}, 6 * 60, {VehicleType::Bus, VehicleType::Minibus}},
        {2, "Autista B", {46.3650, 11.0300}, {7 * 60, 13 * 60 + 30}, 6 * 60, {VehicleType::Accessible, VehicleType::Car}},
    };
    d.ricoveri = {
        {1, "Rimessa Cles", {46.3600, 11.0300}, 30, true, true},
        {2, "Ricovero Rumo", {46.4400, 11.0200}, 2, false, false},
    };
    d.contratti = {
        {1, "Provincia - scolastico", EscortProvider::Client},
        {2, "Comune - asilo (assistente a carico ditta)", EscortProvider::Company},
    };
    d.assistenti = {
        {1, "Assistente comunale", {46.4420, 11.0170}, {7 * 60 + 30, 9 * 60}, EscortProvider::Client, 0.0},
        {2, "Assistente ditta", {46.4400, 11.0200}, {7 * 60 + 30, 9 * 60}, EscortProvider::Company, 18.0},
    };

    auto find_p = [&d](Id id) -> const Passenger* {
        for (const auto& p : d.passeggeri) if (p.id == id) return &p;
        return nullptr;
    };
    auto find_d = [&d](Id id) -> const Destination* {
        for (const auto& x : d.destinazioni) if (x.id == id) return &x;
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
    d.turno = Schedule{2, 2, 1, {r1, r2, r3}};

    // Autista di esempio: tragitti casa-ricovero e pause lunghe del turno
    const Driver& drv = d.autisti[1];
    d.autista_esempio_id = drv.id;
    for (const auto& p : d.ricoveri) d.tragitti.push_back({p.id, driver_commute_km(drv, p)});
    d.pause = check_home_breaks(d.turno, drv);

    // Posti: 38 ragazzi Romallo/Revo'/Tregiovo con un 16 posti
    const Vehicle& m16 = d.mezzi[0];
    d.ragazzi_da_portare = 38;
    d.mezzo_giri_id = m16.id;
    d.giri_necessari = trips_needed(m16, d.ragazzi_da_portare);

    // Mezzo attrezzato: carrozzine che tolgono posti
    const Vehicle& att = d.mezzi[1];
    d.mezzo_carrozzine_id = att.id;
    d.ragazzi_prova_carrozzine = 6;
    for (int w = 0; w <= 3; ++w) {
        CasoCarrozzine c;
        c.carrozzine = w;
        c.ammesso = w <= att.wheelchair_places;
        c.posti_a_sedere = c.ammesso ? att.seats - w * att.seats_lost_per_wheelchair : 0;
        c.ragazzi_ci_stanno = fits(att, d.ragazzi_prova_carrozzine, w);
        d.casi_carrozzine.push_back(c);
    }

    // Linea con richiesta "30 posti": confronto costi
    const Vehicle bus30 = {9, "BUS-30 (ipotetico)", VehicleType::Bus, Powertrain::Thermal, 30, 0, 0, false, 0,
                           90.0, {}, {}, 0.95, 0.22, 900};
    ConfrontoCosti& cc = d.confronto_costi;
    cc.km_al_giorno = 120;
    cc.mezzo_flotta_id = m16.id;
    cc.posti_flotta = m16.seats;
    cc.costo_flotta = day_cost(m16, cc.km_al_giorno);
    cc.posti_richiesti = bus30.seats;
    cc.costo_richiesto = day_cost(bus30, cc.km_al_giorno);
    cc.differenza = cc.costo_richiesto - cc.costo_flotta;

    // Assistente a bordo
    Route asilo;
    asilo.id = 10; asilo.destination_ids = {4};
    d.destinazione_infanzia_id = 4;
    d.assistente_infanzia = needs_escort(asilo, find_p, find_d);
    d.corsa_handicap_id = r1.id;
    d.assistente_corsa_handicap = needs_escort(r1, find_p, find_d);
    d.mezzo_bambini_id = m16.id;
    d.bambini_con_assistente = 16;
    d.bambini_con_assistente_ci_stanno = fits(m16, d.bambini_con_assistente, 0, true);
    const Escort& ditta = d.assistenti[1];
    d.assistente_ditta_id = ditta.id;
    d.ore_assistente_ditta = 1.5;
    d.costo_assistente_ditta = d.ore_assistente_ditta * ditta.hourly_cost;
    return d;
}

}  // namespace kzero
