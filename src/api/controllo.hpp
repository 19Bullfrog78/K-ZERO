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
    kzero::Source posizione;  // file vuoto = problema dell'intero pacchetto
    // Problemi dei turni uguali in molti giorni: i giorni, raggruppati in periodi di giorni consecutivi (dal, al).
    std::vector<std::pair<kzero::Day, kzero::Day>> giorni;
};

struct Resoconto {
    Lettura lettura;
    bool contenuto_controllato = false;   // false se ci sono errori nella forma dei file
    std::vector<ProblemaPresentato> problemi;   // errori prima, poi avvisi; ciascuno in ordine di file e posizione
    int errori = 0;
    int avvisi = 0;

    // Quanti elementi ci sono nel file: nessun valore se il file c'e' ma non e' stato letto per errori di forma.
    std::optional<std::size_t> quanti(const std::string& file, std::size_t n) const;
};

Resoconto controlla_pacchetto(const Sorgente& sorgente, const Visibilita& visibilita);

}  // namespace kzero::api
