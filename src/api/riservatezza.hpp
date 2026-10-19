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

}  // namespace kzero::api
