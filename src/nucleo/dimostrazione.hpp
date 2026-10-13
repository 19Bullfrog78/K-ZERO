#pragma once
// K-Zero - calcoli della dimostrazione: alcune regole del mestiere applicate a un pacchetto dati.
// Nessun dato e' scritto qui: tutto viene dal pacchetto (per "kzero dimostrazione": esempi/demo).
// Il nucleo non stampa nulla: testo e JSON li prepara l'API (src/api).

#include <optional>
#include <vector>

#include "domain_models.hpp"

namespace kzero {

// Giri necessari per portare la domanda di un servizio con ogni mezzo dell'azienda del contratto.
struct GiriServizio {
    Code servizio;
    int persone = 0;   // persone che salgono (domanda con conteggi esatti)
    struct PerMezzo {
        Code mezzo;
        int posti = 0;
        int giri = 0;
    };
    std::vector<PerMezzo> mezzi;
};

// Posti a sedere di un mezzo con pedana al crescere delle carrozzine a bordo.
struct CarrozzineMezzo {
    Code mezzo;
    struct Caso {
        int carrozzine = 0;
        bool ammesso = false;     // false = piu' carrozzine di quante il mezzo ne porti
        int posti_a_sedere = 0;   // vale solo se ammesso
    };
    std::vector<Caso> casi;
};

// Serve l'assistente su una corsa, secondo il grado delle scuole servite e i posti del mezzo?
// (Solo le condizioni del regolamento sul grado e sui posti: le esigenze di salute sono dati particolari.)
struct AssistenteCorsa {
    Code corsa;
    Code regolamento;
    std::vector<Grade> gradi;
    struct PerMezzo {
        Code mezzo;
        int posti = 0;
        bool serve = false;
    };
    std::vector<PerMezzo> mezzi;
};

// Costo di una giornata di un servizio con ogni mezzo dell'azienda, quando il committente chiede un mezzo minimo.
struct CostiServizio {
    Code servizio;
    int posti_minimi = 0;
    std::optional<int> posti_massimi;
    double km_al_giorno = 0.0;   // km a pieno e a vuoto delle corse del servizio
    struct PerMezzo {
        Code mezzo;
        int posti = 0;
        bool ipotetico = false;
        bool ammesso = false;
        std::optional<double> costo;   // nessun valore se il mezzo non ha i costi
    };
    std::vector<PerMezzo> mezzi;
};

struct Dimostrazione {
    std::vector<GiriServizio> giri;
    std::vector<CarrozzineMezzo> carrozzine;
    std::vector<AssistenteCorsa> assistente;
    std::vector<CostiServizio> costi;
};

// Applica le regole al pacchetto. Ogni chiamata con gli stessi dati da' lo stesso risultato.
Dimostrazione calcola_dimostrazione(const Package& p);

}  // namespace kzero
