#pragma once
// K-Zero API - riservatezza delle posizioni dei problemi (docs/controlli.md, "Dati personali e particolari nei
// problemi"). Una regola generale ricavata dalle classi degli schemi, non un elenco di casi scritto a mano; vale allo
// stesso modo per testo e JSON e per tutti i comandi:
// - la classe del dato in cui sta una posizione e' la piu' riservata tra quelle marcate lungo il percorso;
// - di base una posizione non entra mai in un dato personale o particolare: si ferma all'elemento comune piu' vicino
//   (es. la persona in persone.json, non la sua abilitazione);
// - un elemento i cui dati sono tutti personali (es. un'assenza, in un file di soli dati personali) lascia il posto
//   all'elemento comune a cui appartiene (marcatura "x-kzero-titolare": la persona in persone.json); se non c'e',
//   la posizione non si mostra per niente;
// - la posizione completa si vede con il parametro della classe nascosta (--includi-dati-personali o
//   --includi-dati-particolari).

#include <optional>
#include <string>

#include "lettore.hpp"
#include "pacchetto.hpp"
#include "schemi_dati.hpp"

namespace kzero::api {

// La classe piu' riservata tra due.
Classe piu_riservata(Classe a, Classe b);

// Il parametro che mostra i dati di una classe: "--includi-dati-personali", "--includi-dati-particolari".
const char* parametro_per(Classe c);

// Classe del dato in cui sta la posizione: la piu' riservata marcata lungo il percorso. Un file che non fa parte del
// formato, o un pezzo del percorso non previsto dallo schema (es. un campo scritto male), non aggiungono nulla.
Classe classe_della_posizione(const kzero::Source& posizione, const Lettura& lettura);

struct PosizioneMostrata {
    kzero::Source posizione;          // file vuoto: nessuna posizione da mostrare
    std::optional<Classe> ridotta;    // la posizione completa sta in un dato di questa classe (si vede con il parametro)
};

// La posizione da mostrare con la visibilita' data (vedi sopra).
PosizioneMostrata mostra_posizione(const kzero::Source& posizione, const Lettura& lettura, const Visibilita& visibilita);

// Il file di dati, preso tutto intero, e' un dato personale o particolare (es. assenze.json)? Serve agli errori K-ERR,
// che non hanno percorsi JSON: la loro posizione non nomina mai un file cosi'.
bool file_riservato(const std::string& file);

// --- Luoghi sensibili (K-009, punto 3 e aggiunta 1) -------------------------------------------------------------
// Un luogo sensibile (luoghi.json, "sensibile": es. un centro diurno) lascia capire un dato sulla salute di chi ci va.
// Una regola sola per tutte le uscite, testo e JSON allo stesso modo:
//   quando si vedono i dati personali ma non quelli particolari, in un "insieme" che lascia riconoscere una persona
//   trasportata - il nome di un passeggero, oppure la sua casa (un punto porta a porta con il nome o l'indirizzo) -
//   ogni luogo sensibile dell'insieme diventa "riservato": il codice e il nome, e le scuole o i centri che ci stanno.
// Il personale non conta (es. il garage presso la casa di un autista); i conteggi neppure. L'insieme e' quello che
// un'uscita lega con i codici che mostra (DECISIONI.md, 2026-10-19): il messaggio di un problema (controlla); servizi e
// corse insieme (dimostrazione: legati dai codici dei servizi e dei trasbordi; l'elenco dei luoghi no); l'intera
// giornata mostrata (giornata: i turni sono legati dal mezzo, dalla persona, dai trasbordi e dall'assistente a bordo).
// Uso: si dichiara tutto quello che l'insieme mostra (passeggero, luogo, scuola), poi si chiede cosa nascondere.
class LegameSensibile {
public:
    // Senza indice (nessun pacchetto letto) non c'e' nulla da riconoscere.
    LegameSensibile(const kzero::Index* indice, const Visibilita& visibilita);

    void passeggero(const kzero::Code& codice);   // un passeggero mostrato nell'insieme
    void luogo(const kzero::Code& codice);        // un luogo mostrato nell'insieme

    // La regola puo' nascondere qualcosa: dati personali visibili, particolari nascosti, almeno un luogo sensibile.
    bool attivo() const { return attivo_; }
    // L'insieme lascia riconoscere una persona trasportata e non mostra i dati particolari.
    bool riservato() const { return riservato_; }
    // Il luogo, o la scuola o il centro, va mostrato come "riservato".
    bool nasconde_luogo(const kzero::Code& luogo) const;
    bool nasconde_scuola(const kzero::Code& scuola) const;

private:
    const kzero::Index* indice_;
    bool attivo_;
    bool riservato_ = false;
};

}  // namespace kzero::api
