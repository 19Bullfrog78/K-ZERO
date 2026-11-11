#pragma once
// K-Zero API - controllo completo di un pacchetto dati: forma dei file (lettore), poi contenuto (nucleo).
// Lo usano "kzero controlla" (cartella sul disco) e "kzero dimostrazione" (pacchetto incorporato).

#include <optional>
#include <utility>
#include <string>
#include <vector>

#include "controlli_catalogo.hpp"
#include "lettore.hpp"

namespace kzero::api {

struct ProblemaPresentato {
    Livello livello = Livello::Errore;
    bool piano = false;       // problema del piano (K-011), non dei dati
    std::string codice;       // K-CTR-nnn
    std::string messaggio;    // gia' filtrato secondo le classi dei dati
    // Posizione da mostrare, gia' filtrata (riservatezza.hpp): file vuoto = nessuna posizione da mostrare.
    kzero::Source posizione;
    // La posizione completa sta in un dato di questa classe e si vede con il suo parametro.
    std::optional<Classe> posizione_ridotta;
    // Problemi dei turni uguali in molti giorni: i giorni, raggruppati in periodi di giorni consecutivi (dal, al).
    std::vector<std::pair<kzero::Day, kzero::Day>> giorni;
    kzero::Source completa;   // la posizione vera: serve solo a un ordine stabile tra problemi che si mostrano uguali
    std::string persona;      // la persona di cui parla il problema, se c'e' (kzero ore: i problemi di ogni persona)
    std::vector<kzero::Day> tutti_i_giorni;
    // K-011 aggiunta 1: la corsa e il servizio di cui parla un problema del modo delle corse, solo per quei controlli (kzero
    // ore: le forzature, le dichiarazioni e le scelte che toccano le corse della persona).
    std::string corsa, servizio;
};

struct Resoconto {
    Lettura lettura;
    bool contenuto_controllato = false;   // false se ci sono errori nella forma dei file
    // Problemi elencati: errori prima, poi avvisi; ciascuno in ordine di file e posizione (quella mostrata).
    std::vector<ProblemaPresentato> problemi;
    int errori = 0;   // tutti, anche quelli riservati
    int avvisi = 0;
    // Errori del piano (K-011: le violazioni delle regole sugli orari), compresi in errori: non bloccano kzero giornata e
    // kzero ore (catalogo, DefinizioneControllo::piano).
    int errori_del_piano = 0;
    int errori_nei_dati() const { return errori - errori_del_piano; }
    // Problemi che rivelano un dato particolare (salute) e che, senza --includi-dati-particolari, non si elencano uno
    // per uno: solo quanti sono (in errori e avvisi sono gia' contati).
    int errori_riservati = 0;
    int avvisi_riservati = 0;
    // K-017: per chi apre i dati personali ma non quelli particolari, i problemi dei controlli dei turni (catalogo,
    // DefinizioneControllo::turni) che dipendono dal tipo di servizio: nati da un risultato del piano non verificato o da un
    // requisito del tipo. Non verificati: non si elencano e non sono in errori e avvisi, si contano soltanto. Senza valore per
    // gli altri utenti (per chi apre i particolari non ce ne sono; senza i personali i controlli dei turni non sono verificati).
    std::optional<std::pair<int, int>> turni_non_verificati;   // errori, avvisi
    // Cassaforte (K-010): per ogni classe che l'utente non puo' aprire, i controlli non verificati (in ordine di codice): quelli
    // che la richiedono (catalogo). Anche i problemi che starebbero nei dati di quella classe non si possono calcolare.
    // Nessun loro risultato e' nel resoconto, ne' tra i problemi ne' nei conteggi.
    struct NonVerificati {
        Classe classe = Classe::Personale;
        std::vector<std::string> controlli;
    };
    std::vector<NonVerificati> non_verificati;

    // Quanti elementi ci sono nel file: nessun valore se il file c'e' ma non e' stato letto per errori di forma.
    std::optional<std::size_t> quanti(const std::string& file, std::size_t n) const;
};

Resoconto controlla_pacchetto(const Sorgente& sorgente, const Visibilita& visibilita);

// Lo stesso controllo su un pacchetto gia' letto (leggi_pacchetto o leggi_file). Con richiesta e resoconto degli orari
// (kzero ore, K-011) li riempie nella stessa passata.
Resoconto controlla_lettura(Lettura lettura, const Visibilita& visibilita, const kzero::HoursRequest* richiesta = nullptr,
                            kzero::HoursReport* orari = nullptr);

}  // namespace kzero::api
