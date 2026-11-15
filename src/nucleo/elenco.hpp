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
    };
    Kind kind = Kind::Unchanged;
    Code code;
    int row = 0;                       // riga del file (0 se non c'e')
    std::vector<std::string> fields;   // Changed: i campi cambiati (nome, casa, classe, sezione, stato, nota, fermata, tratte, orari, fuori_elenco)
};

struct ListMerge {
    ClientList list;                   // l'elenco nuovo del committente
    int last_number = 0;               // ultimo numero dei codici dopo la lettura
    std::vector<ListChange> changes;   // in ordine di codice
    bool complete = false;
    bool exits_applied = false;        // completo e senza righe illeggibili
    std::vector<Problem> warnings;     // K-CTR-150 (forse lo stesso), K-CTR-151 (uscite non applicate)
};

// Confronta le righe lette con l'elenco del committente nel pacchetto (puo' non esserci: la prima volta tutti nuovi).
ListMerge merge_list(const Package& p, const ReadingProfile& profile, int first_year, const ListRead& read, bool complete,
                     const std::string& fingerprint);

// Il codice di un utente dal suo numero: "U-000001".
std::string list_user_code(int number);

// I codici di tutti gli elementi del pacchetto con un codice (per tenere unici i codici degli utenti dell'elenco).
std::set<Code> all_codes(const Package& p, bool with_list_users);

// --- Controlli sull'elenco (kzero controlla) ----------------------------------------------------------------------------
void check_lists(const Package& p, const Index& idx, std::vector<Problem>& out);

}  // namespace kzero
