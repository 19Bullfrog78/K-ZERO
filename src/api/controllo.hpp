#pragma once
// K-Zero API - controllo completo di un pacchetto dati: forma dei file (lettore), poi contenuto (nucleo).
// Lo usano "kzero controlla" (cartella sul disco) e "kzero dimostrazione" (pacchetto incorporato).

#include <optional>
#include <string>
#include <vector>

#include "controlli_catalogo.hpp"
#include "lettore.hpp"

namespace kzero::api {

struct ProblemaPresentato {
    Livello livello = Livello::Errore;
    std::string codice;       // K-CTR-nnn
    std::string messaggio;    // gia' filtrato secondo le classi dei dati
    // Posizione da mostrare, gia' filtrata (riservatezza.hpp): file vuoto = nessuna posizione da mostrare.
    kzero::Source posizione;
    // La posizione completa sta in un dato di questa classe e si vede con il suo parametro.
    std::optional<Classe> posizione_ridotta;
    // Problemi dei turni uguali in molti giorni: i giorni, raggruppati in periodi di giorni consecutivi (dal, al).
    std::vector<std::pair<kzero::Day, kzero::Day>> giorni;
    kzero::Source completa;   // la posizione vera: serve solo a un ordine stabile tra problemi che si mostrano uguali
};

struct Resoconto {
    Lettura lettura;
    bool contenuto_controllato = false;   // false se ci sono errori nella forma dei file
    // Problemi elencati: errori prima, poi avvisi; ciascuno in ordine di file e posizione (quella mostrata).
    std::vector<ProblemaPresentato> problemi;
    int errori = 0;   // tutti, anche quelli riservati
    int avvisi = 0;
    // Problemi che rivelano un dato particolare (salute) e che, senza --includi-dati-particolari, non si elencano uno
    // per uno: solo quanti sono (in errori e avvisi sono gia' contati).
    int errori_riservati = 0;
    int avvisi_riservati = 0;

    // Quanti elementi ci sono nel file: nessun valore se il file c'e' ma non e' stato letto per errori di forma.
    std::optional<std::size_t> quanti(const std::string& file, std::size_t n) const;
};

Resoconto controlla_pacchetto(const Sorgente& sorgente, const Visibilita& visibilita);

// Lo stesso controllo su un pacchetto gia' letto (leggi_pacchetto o leggi_file).
Resoconto controlla_lettura(Lettura lettura, const Visibilita& visibilita);

}  // namespace kzero::api
