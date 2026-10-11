// K-Zero - prove automatiche delle regole del mestiere (REGOLE.md) con i dati di prova.
// Uso: test_regole <nome_prova>   (senza argomenti esegue tutte le prove)
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

#include "domain_models.hpp"

using namespace kzero;

static int failures = 0;

#define CHECK(cond)                                                                      \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cout << "  FALLITO: " #cond "  (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++failures;                                                                  \
        }                                                                                \
    } while (0)

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
static void giri_38_su_16() {
    CHECK(trips_needed(minibus16, 38) == 3);
    CHECK(trips_needed(minibus16, 16) == 1);
    CHECK(trips_needed(minibus16, 17) == 2);
}

// L'assistente occupa un posto.
static void assistente_occupa_un_posto() {
    CHECK(fits(minibus16, 16, 0, false));    // 16 bambini da soli: ci stanno
    CHECK(!fits(minibus16, 16, 0, true));    // 16 bambini + assistente: no
    CHECK(fits(minibus16, 15, 0, true));     // 15 bambini + assistente: si'
}

// Ogni carrozzina toglie posti a sedere (attrezzato: 12 posti, 3 persi per carrozzina, max 2).
static void carrozzina_toglie_posti() {
    CHECK(fits(attrezzato, 12, 0));
    CHECK(!fits(attrezzato, 10, 1));   // 12 - 3 = 9 posti
    CHECK(fits(attrezzato, 9, 1));
    CHECK(!fits(attrezzato, 7, 2));    // 12 - 6 = 6 posti
    CHECK(fits(attrezzato, 6, 2));
    CHECK(!fits(attrezzato, 0, 3));    // oltre le carrozzine ammesse
}

// Senza pedana nessuna carrozzina.
static void senza_pedana_nessuna_carrozzina() {
    CHECK(!fits(minibus16, 0, 1));
    Vehicle senza_pedana = attrezzato;
    senza_pedana.has_ramp = false;
    CHECK(!fits(senza_pedana, 0, 1));
}

// Gruppi diversi sulla stessa corsa = Ibrido.
static void gruppi_diversi_ibrido() {
    CHECK(categoria(corsa({101, 103}, {1})) == ServiceType::Hybrid);        // studenti + handicap
    CHECK(categoria(corsa({101}, {1}, true)) == ServiceType::Hybrid);       // studenti + linea
    CHECK(categoria(corsa({105}, {1}, true)) == ServiceType::Hybrid);       // handicap + linea
}

// Studenti verso un centro esterno = Ibrido.
static void studenti_centro_esterno_ibrido() {
    CHECK(categoria(corsa({104}, {3})) == ServiceType::Hybrid);
}

// Solo studenti verso scuola = Scolastico.
static void solo_studenti_scolastico() {
    CHECK(categoria(corsa({101, 102}, {1})) == ServiceType::School);
}

// Solo handicap = Handicap.
static void solo_handicap() {
    CHECK(categoria(corsa({103, 105}, {1})) == ServiceType::Disability);
}

// Solo utenti di linea = Linea.
static void solo_linea() {
    CHECK(categoria(corsa({}, {}, true)) == ServiceType::Line);
}

// Corsa verso la scuola dell'infanzia = assistente obbligatorio.
static void infanzia_assistente_obbligatorio() {
    CHECK(needs_escort(corsa({}, {4}), find_p, find_d));
    CHECK(!needs_escort(corsa({101, 102}, {1}), find_p, find_d));   // scuola media, nessuna richiesta
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

static void pausa_breve_non_conta() {
    Schedule s;
    s.routes = {tratto(cles, 7 * 60, rumo, 7 * 60 + 30),
                tratto(rumo, 8 * 60 + 29, cles, 9 * 60)};   // 59 minuti fermo a Rumo
    const BreakReport br = check_home_breaks(s, autista_di_cles());
    CHECK(br.near_home == 0);
    CHECK(br.far_from_home == 0);
}

static void pausa_lunga_lontano_segnalata() {
    Schedule lontano;
    lontano.routes = {tratto(cles, 7 * 60, rumo, 7 * 60 + 30),
                      tratto(rumo, 8 * 60 + 30, cles, 9 * 60)};   // 60 minuti fermo a Rumo
    const BreakReport br = check_home_breaks(lontano, autista_di_cles());
    CHECK(br.far_from_home == 1);
    CHECK(br.near_home == 0);

    // Controprova: la stessa pausa a Cles (vicino a casa) non e' segnalata.
    Schedule vicino;
    vicino.routes = {tratto(rumo, 7 * 60, cles, 7 * 60 + 30),
                     tratto(cles, 9 * 60, rumo, 9 * 60 + 30)};
    const BreakReport bv = check_home_breaks(vicino, autista_di_cles());
    CHECK(bv.far_from_home == 0);
    CHECK(bv.near_home == 1);
}

// --- Esecuzione ----------------------------------------------------------------

struct Prova {
    const char* nome;
    std::function<void()> fn;
};

int main(int argc, char** argv) {
    const std::vector<Prova> prove = {
        {"giri_38_su_16", giri_38_su_16},
        {"assistente_occupa_un_posto", assistente_occupa_un_posto},
        {"carrozzina_toglie_posti", carrozzina_toglie_posti},
        {"senza_pedana_nessuna_carrozzina", senza_pedana_nessuna_carrozzina},
        {"gruppi_diversi_ibrido", gruppi_diversi_ibrido},
        {"studenti_centro_esterno_ibrido", studenti_centro_esterno_ibrido},
        {"solo_studenti_scolastico", solo_studenti_scolastico},
        {"solo_handicap", solo_handicap},
        {"solo_linea", solo_linea},
        {"infanzia_assistente_obbligatorio", infanzia_assistente_obbligatorio},
        {"pausa_breve_non_conta", pausa_breve_non_conta},
        {"pausa_lunga_lontano_segnalata", pausa_lunga_lontano_segnalata},
    };

    const char* scelta = argc > 1 ? argv[1] : nullptr;
    int eseguite = 0, fallite = 0;
    for (const auto& p : prove) {
        if (scelta && std::strcmp(scelta, p.nome) != 0) continue;
        const int prima = failures;
        p.fn();
        ++eseguite;
        const bool ok = failures == prima;
        if (!ok) ++fallite;
        std::cout << (ok ? "OK      " : "FALLITA ") << p.nome << "\n";
    }
    if (eseguite == 0) {
        std::cout << "Prova sconosciuta: " << (scelta ? scelta : "") << "\n";
        return 1;
    }
    std::cout << eseguite - fallite << " prove passate su " << eseguite << "\n";
    return fallite == 0 ? 0 : 1;
}
