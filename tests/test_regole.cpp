// K-Zero - prove automatiche delle regole del mestiere (REGOLE.md) con i dati di prova.
// Ogni TEST viene trovato da solo da CTest (gtest_discover_tests): non va elencato altrove.
#include <vector>

#include <gtest/gtest.h>

#include "domain_models.hpp"

using namespace kzero;

// --- Dati di prova (gli stessi di src/main.cpp, valori inventati) ----------

static const std::vector<Destination> destinations = {
    {1, "Scuola Media Cles", DestinationKind::School, {46.3650, 11.0340},
     {7 * 60 + 40, 7 * 60 + 55}, {13 * 60, 13 * 60 + 15}},
    {2, "Primaria Revo'", DestinationKind::School, {46.3920, 11.0610},
     {7 * 60 + 55, 8 * 60 + 10}, {12 * 60 + 30, 12 * 60 + 45}},
    {3, "Doposcuola Cles", DestinationKind::ExternalCenter, {46.3630, 11.0360},
     {13 * 60 + 10, 13 * 60 + 30}, {17 * 60, 17 * 60 + 30}},
    {4, "Scuola dell'infanzia Rumo", DestinationKind::Kindergarten, {46.4420, 11.0170},
     {8 * 60, 8 * 60 + 30}, {15 * 60 + 30, 16 * 60}},
};

static const std::vector<Passenger> passengers = {
    {101, PassengerKind::Student, "Rumo, fraz. Marcena", {46.4410, 11.0190}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 1, 0},
    {102, PassengerKind::Student, "Livo, piazza", {46.4030, 11.0190}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 1, 0},
    {103, PassengerKind::Disability, "Cagno', via Centrale", {46.3960, 11.0450}, 1, 7 * 60 + 55, 13 * 60, 0, true, true, 5, 45},
    {104, PassengerKind::Student, "Cles, Scuola Media", {46.3650, 11.0340}, 3, 13 * 60 + 30, 17 * 60, 0, false, false, 1, 0},
    // Passeggero handicap senza accompagnatore: serve per la corsa "solo handicap"
    {105, PassengerKind::Disability, "Cles, via Roma", {46.3640, 11.0330}, 1, 7 * 60 + 55, 13 * 60, 0, false, false, 3, 0},
};

static const Vehicle minibus16 = {1, "MINIBUS-1", VehicleType::Minibus, Powertrain::Thermal, 16, 0, 0, false, 0, 45.0,
                                  {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900};
static const Vehicle attrezzato = {2, "MINIBUS-2", VehicleType::Accessible, Powertrain::Thermal, 12, 2, 3, true, 6, 50.0,
                                   {46.4400, 11.0200}, {46.4400, 11.0200}, 0.55, 0.11, 900};

static const Passenger* find_p(Id id) {
    for (const auto& p : passengers) if (p.id == id) return &p;
    return nullptr;
}
static const Destination* find_d(Id id) {
    for (const auto& d : destinations) if (d.id == id) return &d;
    return nullptr;
}

static Route corsa(std::vector<Id> pass, std::vector<Id> dest, bool linea = false) {
    Route r;
    r.passenger_ids = std::move(pass);
    r.destination_ids = std::move(dest);
    r.carries_line_users = linea;
    return r;
}

static ServiceType categoria(const Route& r) { return derive_service(r, find_p, find_d); }

// --- Prove -------------------------------------------------------------------

// 38 ragazzi su un 16 posti: servono 3 giri.
TEST(Regole, giri_38_su_16) {
    EXPECT_EQ(trips_needed(minibus16, 38), 3);
    EXPECT_EQ(trips_needed(minibus16, 16), 1);
    EXPECT_EQ(trips_needed(minibus16, 17), 2);
}

// L'assistente occupa un posto.
TEST(Regole, assistente_occupa_un_posto) {
    EXPECT_TRUE(fits(minibus16, 16, 0, false));    // 16 bambini da soli: ci stanno
    EXPECT_FALSE(fits(minibus16, 16, 0, true));    // 16 bambini + assistente: no
    EXPECT_TRUE(fits(minibus16, 15, 0, true));     // 15 bambini + assistente: si'
}

// Ogni carrozzina toglie posti a sedere (attrezzato: 12 posti, 3 persi per carrozzina, max 2).
TEST(Regole, carrozzina_toglie_posti) {
    EXPECT_TRUE(fits(attrezzato, 12, 0));
    EXPECT_FALSE(fits(attrezzato, 10, 1));   // 12 - 3 = 9 posti
    EXPECT_TRUE(fits(attrezzato, 9, 1));
    EXPECT_FALSE(fits(attrezzato, 7, 2));    // 12 - 6 = 6 posti
    EXPECT_TRUE(fits(attrezzato, 6, 2));
    EXPECT_FALSE(fits(attrezzato, 0, 3));    // oltre le carrozzine ammesse
}

// Senza pedana nessuna carrozzina.
TEST(Regole, senza_pedana_nessuna_carrozzina) {
    EXPECT_FALSE(fits(minibus16, 0, 1));
    Vehicle senza_pedana = attrezzato;
    senza_pedana.has_ramp = false;
    EXPECT_FALSE(fits(senza_pedana, 0, 1));
}

// Gruppi diversi sulla stessa corsa = Ibrido.
TEST(Regole, gruppi_diversi_ibrido) {
    EXPECT_EQ(categoria(corsa({101, 103}, {1})), ServiceType::Hybrid);        // studenti + handicap
    EXPECT_EQ(categoria(corsa({101}, {1}, true)), ServiceType::Hybrid);       // studenti + linea
    EXPECT_EQ(categoria(corsa({105}, {1}, true)), ServiceType::Hybrid);       // handicap + linea
}

// Studenti verso un centro esterno = Ibrido.
TEST(Regole, studenti_centro_esterno_ibrido) {
    EXPECT_EQ(categoria(corsa({104}, {3})), ServiceType::Hybrid);
}

// Solo studenti verso scuola = Scolastico.
TEST(Regole, solo_studenti_scolastico) {
    EXPECT_EQ(categoria(corsa({101, 102}, {1})), ServiceType::School);
}

// Solo handicap = Handicap.
TEST(Regole, solo_handicap) {
    EXPECT_EQ(categoria(corsa({103, 105}, {1})), ServiceType::Disability);
}

// Solo utenti di linea = Linea.
TEST(Regole, solo_linea) {
    EXPECT_EQ(categoria(corsa({}, {}, true)), ServiceType::Line);
}

// Corsa verso la scuola dell'infanzia = assistente obbligatorio.
TEST(Regole, infanzia_assistente_obbligatorio) {
    EXPECT_TRUE(needs_escort(corsa({}, {4}), find_p, find_d));
    EXPECT_FALSE(needs_escort(corsa({101, 102}, {1}), find_p, find_d));   // scuola media, nessuna richiesta
}

// Passeggero handicap che richiede l'accompagnatore = assistente obbligatorio;
// passeggero handicap che non lo richiede, da solo, no.
TEST(Regole, handicap_con_accompagnatore_assistente_obbligatorio) {
    EXPECT_TRUE(needs_escort(corsa({103}, {1}), find_p, find_d));    // 103: richiede l'accompagnatore
    EXPECT_FALSE(needs_escort(corsa({105}, {1}), find_p, find_d));   // 105: non lo richiede
}

// Pause: sotto i 60 minuti non contano; quella lunga lontano da casa viene segnalata.
static Route tratto(Coord da, Minutes partenza, Coord a, Minutes arrivo) {
    Route r;
    r.stops = {{da, "partenza", partenza, 0, 0}, {a, "arrivo", arrivo, 0, 0}};
    return r;
}

static const Coord cles = {46.3650, 11.0340};
static const Coord rumo = {46.4400, 11.0200};   // circa 8 km da Cles

static Driver autista_di_cles() {
    Driver d;
    d.home = {46.3650, 11.0300};
    return d;   // pausa da casa: almeno 60 min, entro 5 km
}

TEST(Regole, pausa_breve_non_conta) {
    Schedule s;
    s.routes = {tratto(cles, 7 * 60, rumo, 7 * 60 + 30),
                tratto(rumo, 8 * 60 + 29, cles, 9 * 60)};   // 59 minuti fermo a Rumo
    const BreakReport br = check_home_breaks(s, autista_di_cles());
    EXPECT_EQ(br.near_home, 0);
    EXPECT_EQ(br.far_from_home, 0);
}

TEST(Regole, pausa_lunga_lontano_segnalata) {
    Schedule lontano;
    lontano.routes = {tratto(cles, 7 * 60, rumo, 7 * 60 + 30),
                      tratto(rumo, 8 * 60 + 30, cles, 9 * 60)};   // 60 minuti fermo a Rumo
    const BreakReport br = check_home_breaks(lontano, autista_di_cles());
    EXPECT_EQ(br.far_from_home, 1);
    EXPECT_EQ(br.near_home, 0);

    // Controprova: la stessa pausa a Cles (vicino a casa) non e' segnalata.
    Schedule vicino;
    vicino.routes = {tratto(rumo, 7 * 60, cles, 7 * 60 + 30),
                     tratto(cles, 9 * 60, rumo, 9 * 60 + 30)};
    const BreakReport bv = check_home_breaks(vicino, autista_di_cles());
    EXPECT_EQ(bv.far_from_home, 0);
    EXPECT_EQ(bv.near_home, 1);
}
