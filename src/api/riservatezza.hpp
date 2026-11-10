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
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
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

// --- Luoghi sensibili (K-009; regola nuova dal K-010) -----------------------------------------------------------
// Un luogo sensibile (luoghi.json, "sensibile": es. un centro diurno) lascia capire un dato sulla salute di chi ci va. Dal
// K-010 la regola vale per la persona, non per il luogo (DECISIONI.md, K-010): il nome, la casa e gli altri dati
// personali di un passeggero legato a un luogo sensibile valgono come dati particolari. Si vedono solo insieme ai dati
// particolari (--includi-dati-particolari, e il permesso di quei dati); altrimenti al loro posto c'e' il codice, che
// nessuna uscita lega mai al nome: cosi' la regola regge tra uscite diverse, e il luogo si vede sempre (un luogo
// "riservato" diceva comunque che la destinazione era sensibile). Nella cassaforte quei dati stanno nella sezione dei
// legati, cifrata con tutte e due le chiavi di classe: chi non ha tutti e due i permessi non li puo' decifrare.
// Chi e' legato (calcolato una volta sul pacchetto, dai dati comuni):
// - i passeggeri di un gruppo che va (o parte) da una scuola o un centro in un luogo sensibile, o che ha una fermata in
//   un luogo sensibile; i passeggeri che salgono o scendono in un luogo sensibile;
// - le case (porta a porta) dei passeggeri legati e quelle tra le fermate di un gruppo legato; le case toccate da una
//   corsa che, da sola o con i trasbordi, passa da un luogo sensibile; e i passeggeri che salgono o scendono in una casa
//   legata;
// - con "porta_a_porta_sensibile" nel manifesto (versione 2): tutte le case porta a porta e i loro passeggeri;
// - (K-012) i committenti privati con un servizio (di un loro contratto) legato: che ha gruppi, fermate o passeggeri che
//   salgono o scendono in un luogo sensibile (anche la scuola o il centro di un gruppo); che ha una corsa che passa da un luogo
//   sensibile, da sola o con i suoi trasbordi; oppure, con "porta_a_porta_sensibile", che ha case porta a porta (fermate dei
//   gruppi, salite e discese dei passeggeri, passaggi delle sue corse). Il nome di un privato che noleggia per andare a un
//   centro diurno lo lega al centro come il nome di un passeggero; altrimenti resta solo personale (il tipo del servizio
//   resta un dato particolare a parte).
// Non riguarda il personale (es. il garage presso la casa di un autista), i committenti che sono enti ne' i conteggi. Una
// regola sola nel codice, usata da tutte le uscite (vista.hpp, controlli_catalogo.cpp) e dalla cassaforte (classi.hpp).
class LegameSensibile {
public:
    // Senza indice (nessun pacchetto letto) nessuno e' legato.
    explicit LegameSensibile(const kzero::Index* indice);

    bool passeggero(const kzero::Code& codice) const { return passeggeri_.count(codice) > 0; }
    bool casa(const kzero::Code& codice) const { return case_.count(codice) > 0; }
    bool committente(const kzero::Code& codice) const { return committenti_.count(codice) > 0; }   // un privato (K-012)
    // Per un elemento marcato "x-kzero-trasportato" negli schemi ("passeggero", "casa" o "committente") e il suo codice.
    bool legato(const std::string& tipo, const kzero::Code& codice) const;
    bool nessuno() const { return passeggeri_.empty() && case_.empty() && committenti_.empty(); }

private:
    std::set<kzero::Code> passeggeri_;
    std::set<kzero::Code> case_;
    std::set<kzero::Code> committenti_;
};

// Il tipo "x-kzero-trasportato" dell'elemento che sta in quella posizione (es. "/servizi/3/domanda/gruppi/0/passeggeri/0"),
// se c'e'; vuoto altrimenti. versione = la versione letta del file.
std::string trasportato(const kzero::Source& elemento, const Lettura& lettura);
// La posizione sta dentro un passeggero o una casa (un livello del percorso marcato "x-kzero-trasportato")?
bool in_trasportato(const kzero::Source& posizione, const Lettura& lettura);

// --- Turni scoperti o senza mezzo (K-014, scelta A del supervisore del 5/11) ---------------------------------------------------
// Un turno ricorrente resta scoperto o senza mezzo per un fatto con le date (assenza, disponibilita', patente o autorizzazione
// scaduta, fermo, uso del mezzo non autorizzato quel giorno, prestito). Due di questi fatti contano solo per alcuni tipi di
// servizio: l'autorizzazione del committente e l'uso del mezzo (kind_requirements, turni.hpp). Senza i dati particolari (per
// permesso o per parametro), se il pacchetto ha almeno un requisito che dipende dal tipo, resta visibile che il turno e'
// scoperto o senza mezzo, in quali giorni e con quale persona o mezzo previsti (serve per lavorare), il perche' no: per ogni
// turno scoperto o senza mezzo per un fatto con le date lo stesso testo e la stessa posizione (la persona o il mezzo
// previsto), anche per le cause che non dipendono dal tipo, perche' un testo a parte solo per le cause del tipo direbbe
// proprio quello. Con i dati particolari tutto come prima. Una regola sola per kzero controlla (K-CTR-078 e 079, anche in
// kzero ore) e kzero giornata. I turni scoperti perche' nessun modo da' una persona (K-CTR-077) o con il mezzo da assegnare
// (K-CTR-083) restano come sono: dipendono solo da dati comuni (turni, rotazioni, servizi fissi, assegnazioni).
bool motivo_dei_turni_riservato(const kzero::Package& pacchetto, const Visibilita& visibilita);

// Varianti del catalogo senza il motivo: K-CTR-078 (turno scoperto) e K-CTR-079 (turno senza mezzo).
constexpr int kScopertoSenzaMotivo = 5;
constexpr int kSenzaMezzoSenzaMotivo = 3;

// I problemi K-CTR-078 e K-CTR-079 senza il motivo: la variante senza motivo, la posizione della persona o del mezzo previsto,
// solo il turno, la persona o il mezzo e i giorni; poi di nuovo raggruppati per giorni (lo stesso turno scoperto per cause
// diverse diventa un problema solo).
std::vector<kzero::Problem> turni_senza_motivo(std::vector<kzero::Problem> problemi, const kzero::Package& pacchetto);

}  // namespace kzero::api
