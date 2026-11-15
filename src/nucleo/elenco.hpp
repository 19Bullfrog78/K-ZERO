#pragma once
// K-Zero - elenco utenti del committente (K-019): lettura delle righe di un file del committente con il suo profilo di lettura,
// confronto con l'elenco gia' nel pacchetto, controlli sull'elenco. Il file e' una tabella di testi (dal lettore Excel dell'API;
// domani anche CSV, senza cambiare niente qui). Nessun testo: i problemi hanno il loro codice (catalogo dell'API).
//
// Regole (docs/formato-dati.md, "elenco_utenti.json"; DECISIONI.md, K-019):
// - le parole, le sigle e i separatori sono del profilo (dati); il programma conosce solo le forme (orari per tratta o per
//   giorno), mai le parole di un committente;
// - lo stesso utente tra due letture si riconosce da nome e scuola, normalizzati come i nomi dei luoghi (ricerca.hpp); due
//   righe con lo stesso nome nella stessa scuola si fermano (mai indovinare);
// - il codice dell'utente lo da' il programma, in ordine, mai ricavato dal nome, mai riusato, unico in tutto il pacchetto;
// - un elenco parziale non fa mai uscire nessuno; uno completo solo se tutte le righe sono state lette;
// - una lettura non cancella mai uno sblocco: un uscito con uno sblocco resta, "fuori elenco", e non entra mai nel piano.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
#include "domain_models.hpp"
#include "pacchetto.hpp"

namespace kzero {

// --- La tabella del file ----------------------------------------------------------------------------------------------

struct TableCell {
    int column = 0;            // da 1
    std::string text;
    bool unreadable = false;   // la cella non ha un valore leggibile (es. un errore di Excel, oppure troppo lunga)
};
struct TableRow {
    int number = 0;            // da 1, come nel file
    std::vector<TableCell> cells;
};
struct Table {
    std::vector<TableRow> rows;   // solo le righe con almeno un valore; la prima e' quella dei titoli
};

// Un testo della cella come si tiene: spazi di ogni tipo (anche a capo, tabulazioni, spazio indivisibile) ridotti a uno,
// niente spazi ai bordi. Maiuscole e minuscole come nel file.
std::string clean_cell(const std::string& text);

// Anno scolastico: dal primo giorno del mese d'inizio al giorno prima dello stesso mese dell'anno dopo.
DateRange school_year(int first_year, int start_month);
// La data di un giorno e mese senza anno, nell'anno scolastico: dal mese d'inizio a dicembre il primo anno, gli altri il
// secondo. Nessun valore se la data non esiste (es. 29/02 in un anno non bisestile, 31/04).
std::optional<Day> school_date(int first_year, int start_month, int day, int month);

// --- Lettura delle celle (anche per il fuzzing) -----------------------------------------------------------------------

// Varianti di K-CTR-148 (riga illeggibile): il motivo.
enum class RowFault {
    UnreadableCell = 0,      // un errore di Excel o una cella troppo lunga in una colonna che serve
    EmptyRequired = 1,       // nome, scuola o stato vuoti
    SchoolUnknown = 2,       // la scuola del file non e' una scuola del pacchetto (codici esterni del profilo)
    SchoolAmbiguous = 3,     // la scuola del file vale per piu' scuole del pacchetto
    StatusUnknown = 4,       // lo stato non e' una delle parole del profilo
    RoutesForm = 5,          // percorsi: forma che il profilo non conosce
    RoutesLegTwice = 6,      // percorsi: la stessa tratta due volte
    TimesForm = 7,           // orari: forma che il profilo non conosce
    TimesLegUnknown = 8,     // orari: sigla di tratta sconosciuta
    TimesDayUnknown = 9,     // orari: sigla di giorno sconosciuta
    TimesPeriodUnknown = 10, // orari: sigla di periodo sconosciuta
    TimesHour = 11,          // orari: ora scritta male o che non esiste
    TimesDate = 12,          // orari: data che non esiste
    TimesDatesReversed = 13, // orari: periodo che finisce prima di cominciare
    ClassNotNumber = 14,     // classe che non e' un numero da 0 a 20
    HomonymExisting = 15,    // nome e scuola di due utenti gia' nell'elenco: non si sa quale sia
};

struct RoutesParse {
    std::vector<ListUserLeg> legs;
    std::optional<RowFault> fault;
};
// "AM/ALP RM/DITTA B RP/LINEA C SCOL." -> tratte e ditte (le ditte del profilo diventano aziende del pacchetto).
RoutesParse parse_routes(const std::string& text, const ReadingProfile& p);

struct TimesParse {
    std::vector<ListUserTime> times;
    std::optional<RowFault> fault;
};
TimesParse parse_times(const std::string& text, const ProfileTimes& column, const ReadingProfile& p, int first_year);

struct StatusParse {
    bool active = true;
    std::string note;
    bool ok = false;
};
StatusParse parse_status(const std::string& text, const ReadingProfile& p);

// --- Lettura del file ---------------------------------------------------------------------------------------------------

enum class TableFault { None, NoRows, MissingTitle, DuplicateTitle };

struct ParsedUser {
    int row = 0;
    ListUser user;   // senza codice
};

struct ListRead {
    TableFault fault = TableFault::None;
    std::string fault_title;              // il titolo che manca o e' ripetuto (una parola del profilo)
    int data_rows = 0;                    // righe dopo quella dei titoli
    std::vector<ParsedUser> users;        // righe lette, nell'ordine del file
    std::set<int> unreadable_rows;        // righe illeggibili, comprese quelle ferme per omonimia
    std::set<std::string> unreadable_keys;   // nome e scuola delle righe illeggibili, quando si sono letti
    std::vector<Problem> problems;        // K-CTR-148, K-CTR-149, K-CTR-152 (posizione: riga e colonna del file)
};

// La chiave per riconoscere lo stesso utente: nome e scuola, il nome normalizzato come i nomi dei luoghi.
std::string list_key(const std::string& name, const Code& school);
// Tutte le chiavi con cui la lettura ritrova un utente (K-020): il nome e la scuola di oggi e, se cambiati a mano, quelli di prima
// (ogni nome con ogni scuola). Mai "uscito + nuovo" per una correzione a mano.
std::set<std::string> list_keys(const ListUser& u);

// --- Campi dell'elenco (K-020) -----------------------------------------------------------------------------------------------

// Il nome di un campo nei messaggi e nelle uscite: "nome", "casa", ..., "tratte", "fuori_elenco"; gli orari "orari <colonna>".
std::string field_name(ListField f, const std::string& column);
// Lo stesso valore nel campo? Come la lettura dice "cambiato": testo pulito carattere per carattere; la fermata per il luogo, o
// per il testo se non riconosciuta; gli orari della sola colonna, senza badare all'ordine.
bool same_field(ListField f, const std::string& column, const ListUser& a, const ListUser& b);
// Copia il campo da "from" a "to" (per gli orari: solo quelli della colonna).
void copy_field(ListField f, const std::string& column, const ListUser& from, ListUser& to);
// Nel piano: utenti che non sono ne' fuori elenco ne' tolti a mano.
inline bool list_user_in_plan(const ListUser& u) { return u.in_plan(); }

// Legge le righe del file con il profilo. existing = l'elenco gia' nel pacchetto per il committente del profilo (puo' mancare):
// due suoi utenti con la stessa chiave fermano le righe con quella chiave (RowFault::HomonymExisting).
ListRead read_list(const Table& table, const ReadingProfile& profile, const Index& idx, int first_year, const ClientList* existing);

// --- Confronto con l'elenco del pacchetto -------------------------------------------------------------------------------

struct ListChange {
    enum class Kind {
        New,             // nuovo (codice nuovo)
        Changed,         // gia' nell'elenco, con campi cambiati
        Unchanged,       // gia' nell'elenco, uguale
        Exited,          // uscito: non e' nel file completo, tolto
        KeptForUnlock,   // uscito, ma ha uno sblocco: resta "fuori elenco"
        NotInFile,       // non e' nel file, ma le uscite non si applicano (elenco parziale o righe illeggibili): resta com'e'
        Unread,          // la sua riga e' illeggibile: resta com'e'
        // K-020
        RemovedInFile,   // tolto a mano, ma il file lo ha ancora: resta fuori (nessun campo si aggiorna)
        RemovedGone,     // tolto a mano e non nel file completo (senza righe illeggibili): se ne va davvero, con gli sblocchi
        ManualNotInFile, // aggiunto o rimesso a mano, non nel file completo: resta
    };
    Kind kind = Kind::Unchanged;
    Code code;
    int row = 0;                       // riga del file (0 se non c'e')
    std::vector<std::string> fields;   // Changed: i campi cambiati (nome, casa, classe, sezione, stato, nota, fermata, tratte, orari, fuori_elenco)
    std::vector<std::string> confirmed;   // K-020: modifiche a mano confermate dal file (field_name)
};

struct ListMerge {
    ClientList list;                   // l'elenco nuovo del committente
    int last_number = 0;               // ultimo numero dei codici dopo la lettura
    std::vector<ListChange> changes;   // in ordine di codice
    bool complete = false;
    bool exits_applied = false;        // completo e senza righe illeggibili
    std::vector<Problem> warnings;     // K-CTR-150 (forse lo stesso), K-CTR-151 (uscite non applicate), K-CTR-156 ... 159 (a mano)
};

// Confronta le righe lette con l'elenco del committente nel pacchetto (puo' non esserci: la prima volta tutti nuovi).
ListMerge merge_list(const Package& p, const ReadingProfile& profile, int first_year, const ListRead& read, bool complete,
                     const std::string& fingerprint);

// Il codice di un utente dal suo numero: "U-000001".
std::string list_user_code(int number);

// I codici di tutti gli elementi del pacchetto con un codice (per tenere unici i codici degli utenti dell'elenco).
std::set<Code> all_codes(const Package& p, bool with_list_users);

// --- Modifiche a mano (K-020) -------------------------------------------------------------------------------------------------
// I campi si scrivono con le notazioni del file del committente, letti con il profilo del suo elenco (nessuna sintassi nuova):
// una riga per campo, "campo: valore". Nomi dei campi: nome, scuola, stato, percorsi, fermata, comune, frazione, via, civico,
// classe, sezione e "orari <titolo della colonna>". Le righe arrivano gia' pulite e nei limiti (lo fa l'API, che le legge).

enum class ManualFault {
    None,
    UnknownField,       // riga che non comincia con il nome di un campo seguito da ":"
    RepeatedField,      // lo stesso campo due volte (anche una parte della casa)
    NotInProfile,       // un campo (o una colonna degli orari) che il profilo non ha
    HomePartMissing,    // una parte della casa che il profilo ha manca: la casa si riscrive intera
    RequiredEmpty,      // nome, scuola o stato vuoti
    RequiredMissing,    // aggiungere: manca nome, scuola o stato
    NoFields,           // nessun campo scritto
    Value,              // il valore non si legge con il profilo (row_fault dice perche')
};
struct ManualError {
    ManualFault fault = ManualFault::None;
    int line = 0;                 // da 1; 0 se non e' di una riga
    std::string field;            // il nome del campo, com'e' nell'elenco dei campi (mai il valore)
    std::optional<RowFault> row_fault;
};
struct ManualParse {
    ListUser values;                                             // i valori scritti, solo nei campi di "fields"
    std::vector<std::pair<ListField, std::string>> fields;       // i campi scritti (con la colonna per gli orari), in ordine
    ManualError error;
};
// Legge le righe con il profilo. current: l'utente che si cambia (per la fermata dal nome serve il comune della casa, quello
// scritto se c'e', altrimenti il suo); nullptr per un utente nuovo (adding: nome, scuola e stato obbligatori).
ManualParse parse_manual(const std::vector<std::string>& lines, const ReadingProfile& profile, const Index& idx, int first_year,
                         const ListUser* current, bool adding);

// Perche' una modifica a mano non si fa (errore K-ERR-051; niente cambia).
enum class ManualRefusal {
    None,
    NothingChanges,     // i valori scritti sono quelli di oggi
    DateBeforeLast,     // la data e' prima di quella dell'ultima modifica a mano dello stesso campo, o dell'aggiunta
    Homonym,            // nome e scuola di un altro utente dell'elenco (anche di prima di una correzione a mano)
    TimesWithoutColumn, // orari della versione 1 (senza colonna): una colonna sola si cambia dopo la prossima lettura del file
    UserRemoved,        // l'utente e' tolto a mano: prima rimetterlo (rimetti-in-elenco)
    AlreadyRemoved,     // togliere: e' gia' tolto
    NotRemovedNorOut,   // rimettere: e' gia' nell'elenco (ne' tolto a mano ne' fuori elenco)
};
struct ManualFieldChange {
    ListField field = ListField::Name;
    std::string column;
    ListUser before;   // il valore di oggi (prima della modifica), solo in quel campo
    ListUser after;    // il valore scritto, solo in quel campo
    enum class Memory { New, Updated, Dropped } memory = Memory::New;   // valore di prima: ricordato ora, gia' ricordato, tolto (rimesso il valore di prima)
};
struct ManualOutcome {
    ManualRefusal refusal = ManualRefusal::None;
    Day last_date = 0;                     // DateBeforeLast: la data dell'ultima modifica (o dell'aggiunta)
    std::string field;                     // DateBeforeLast, TimesWithoutColumn: il campo
    Code other;                            // Homonym: l'altro utente
    std::vector<ManualFieldChange> changes;
    Code code;                             // l'utente (nuovo: il codice dato)
    int last_number = 0;                   // ultimo numero dei codici dopo un'aggiunta
    std::size_t unlocks = 0;               // togliere, cancellare: quanti sblocchi ha (se ne vanno con lui)
    bool out_of_list = false;              // l'utente e' fuori elenco (togliere, rimettere)
    bool removed = false;                  // rimettere: era tolto a mano (altrimenti era fuori elenco)
};
// Cambia l'utente "code" dell'elenco con i campi letti (gia' senza errori). Niente cambia se c'e' un rifiuto.
ManualOutcome manual_change(ClientList& list, const Code& code, const ManualParse& in, const ManualMark& mark);
// Aggiunge un utente con i campi letti: codice nuovo dopo l'ultimo dato del pacchetto, saltando quelli usati da qualunque
// elemento (come la lettura). Niente cambia se c'e' un rifiuto (Homonym).
ManualOutcome manual_add(const Package& p, ClientList& list, const ManualParse& in, const ManualMark& mark);
// Toglie a mano (resta fino alla prima lettura completa senza di lui), rimette (annulla il togliere, o rimette nell'elenco un
// utente fuori elenco: D3), cancella (subito, da tutto il pacchetto, con gli sblocchi).
ManualOutcome manual_remove(ClientList& list, const Code& code, const ManualMark& mark);
ManualOutcome manual_restore(ClientList& list, const Code& code, const ManualMark& mark);
ManualOutcome manual_delete(ClientList& list, const Code& code);

// --- Controlli sull'elenco (kzero controlla) ----------------------------------------------------------------------------
void check_lists(const Package& p, const Index& idx, std::vector<Problem>& out);

}  // namespace kzero
