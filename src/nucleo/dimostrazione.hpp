#pragma once
// K-Zero - scenario di dimostrazione: DATI INVENTATI e risultati calcolati dal nucleo.
// Il nucleo non stampa nulla: testo e JSON li prepara l'API (src/api).
// I valori di questo scenario non vanno usati come dati reali.

#include <vector>

#include "domain_models.hpp"

namespace kzero {

// Mezzo attrezzato con un certo numero di carrozzine a bordo.
struct CasoCarrozzine {
    int carrozzine = 0;
    bool ammesso = false;            // false = piu' carrozzine di quante il mezzo ne porti
    int posti_a_sedere = 0;          // posti rimasti; vale solo se ammesso
    bool ragazzi_ci_stanno = false;  // ci stanno i ragazzi della prova (Dimostrazione::ragazzi_prova_carrozzine)
};

// Tragitto casa -> ricovero del mezzo (andata e ritorno) dell'autista di esempio.
struct TragittoCasa {
    Id ricovero_id = 0;
    double km_al_giorno = 0.0;
};

// Costo di una giornata di linea con il mezzo della flotta e con un mezzo piu' grande richiesto dal committente.
struct ConfrontoCosti {
    double km_al_giorno = 0.0;
    Id mezzo_flotta_id = 0;
    int posti_flotta = 0;
    double costo_flotta = 0.0;       // EUR/giorno
    int posti_richiesti = 0;         // mezzo ipotetico, non in flotta
    double costo_richiesto = 0.0;    // EUR/giorno
    double differenza = 0.0;         // EUR/giorno
};

struct Dimostrazione {
    // --- Dati inventati ---------------------------------------------------------
    std::vector<Destination> destinazioni;
    std::vector<Passenger> passeggeri;
    std::vector<Vehicle> mezzi;
    std::vector<Driver> autisti;
    std::vector<ParkingSpot> ricoveri;
    std::vector<Contract> contratti;
    std::vector<Escort> assistenti;

    // --- Turno di esempio: corse scritte a mano, categoria ricavata dal nucleo ---
    Schedule turno;

    // --- Autista di esempio: tragitti casa-ricovero e pause lunghe ------------
    Id autista_esempio_id = 0;
    std::vector<TragittoCasa> tragitti;
    BreakReport pause;

    // --- Posti a bordo --------------------------------------------------------
    int ragazzi_da_portare = 0;
    Id mezzo_giri_id = 0;
    int giri_necessari = 0;
    Id mezzo_carrozzine_id = 0;
    int ragazzi_prova_carrozzine = 0;
    std::vector<CasoCarrozzine> casi_carrozzine;
    ConfrontoCosti confronto_costi;

    // --- Assistente a bordo ---------------------------------------------------
    Id destinazione_infanzia_id = 0;
    bool assistente_infanzia = false;
    Id corsa_handicap_id = 0;
    bool assistente_corsa_handicap = false;
    Id mezzo_bambini_id = 0;
    int bambini_con_assistente = 0;
    bool bambini_con_assistente_ci_stanno = false;
    Id assistente_ditta_id = 0;
    double ore_assistente_ditta = 0.0;
    double costo_assistente_ditta = 0.0;   // EUR/giorno
};

// Costruisce lo scenario e fa i calcoli. Ogni chiamata da' lo stesso risultato.
Dimostrazione crea_dimostrazione();

}  // namespace kzero
