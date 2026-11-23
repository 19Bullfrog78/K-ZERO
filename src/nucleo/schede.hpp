#pragma once
// K-Zero - schede dei servizi del committente (K-021): lettura dei tre fogli del modello con il profilo delle schede, controlli
// dei totali e delle righe, scelta della data da cui una scheda vale, versioni della stessa scheda (stesso ID), differenze tra due
// versioni, controlli sul registro (kzero controlla). Nessun testo: i problemi hanno il loro codice (catalogo dell'API).
//
// Regole (docs/formato-dati.md, "profili_schede.json" e "schede_servizi.json"; DECISIONI.md, K-021):
// - le parole, i titoli e i separatori sono del profilo (dati); il programma conosce solo le forme (cadenza a giorni segnati o a
//   date elencate), mai le parole di un committente;
// - una scheda che non si legge (totali che non tornano, punto di raccolta non trovato, cadenza sconosciuta...) si ferma, con
//   riga, colonna e motivo, mai con il valore di una cella; le altre schede del file vanno avanti;
// - mai un numero diviso indovinando (piu' scuole: un gruppo senza scuola), mai un accompagnatore contato come passeggero, mai un
//   ritmo ricavato da una cadenza: le date si scrivono una per una;
// - il programma non sceglie mai la data da cui vale una scheda con l'attivazione prima della presa in carico; i giorni prima
//   della data scelta non cambiano mai; due schede con ID diversi non si uniscono mai.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
#include "domain_models.hpp"
#include "elenco.hpp"
#include "pacchetto.hpp"

namespace kzero {

// --- La lettura del file ------------------------------------------------------------------------------------------------

struct CardTables {
    Table cards, stops, dates;   // i tre fogli (la prima riga di ognuno: i titoli); dates puo' essere vuoto
    Table breaks;                // K-023: il foglio delle interruzioni (vuoto se il file o il profilo non lo hanno)
};

// Varianti di K-CTR-160 (scheda che non si legge): il motivo. Il numero e' la variante del messaggio.
enum class CardFault {
    UnreadableCell = 0,         // cella con un errore di Excel o troppo lunga, in una colonna che serve
    MissingValue = 1,           // un campo che serve e' vuoto
    DuplicateNr = 2,            // lo stesso Nr su piu' righe del foglio delle schede
    DuplicateId = 3,            // lo stesso ID su piu' schede del file
    ServiceText = 4,            // verso e momento che il profilo non conosce
    SchoolUnknown = 5,          // una scuola dell'intestazione che non e' una scuola del pacchetto
    SchoolAmbiguous = 6,        // una scuola dell'intestazione che vale per piu' scuole del pacchetto
    BadDate = 7,                // data scritta male o che non esiste
    DatesReversed = 8,          // disattivazione prima dell'attivazione
    BadMark = 9,                // una casella con una parola che non e' un segno del profilo
    NoDays = 10,                // cadenza a giorni segnati senza nessuna X
    BadNumber = 11,             // numero scritto male (totali, St. Car., St. Scar., km, Tot Giorni)
    BadTime = 12,               // orario scritto male
    CalendarUnknown = 13,       // il calendario della scheda non e' un calendario del pacchetto (codici esterni)
    CalendarAmbiguous = 14,
    CadenceUnknown = 15,        // cadenza che il profilo non conosce
    NewOrChangeUnknown = 16,    // parola di "Scheda nuova / Modifica" che il profilo non conosce
    PointUnknown = 17,          // punto di raccolta che non e' un luogo del pacchetto
    PointAmbiguous = 18,        // punto di raccolta che vale per piu' luoghi
    PointAndHome = 19,          // nella stessa riga punto di raccolta e casa, o nessuno dei due
    HomeNotDoorToDoor = 20,     // il codice della casa e' un luogo pubblico o un ricovero, o non e' un codice
    EscortWordInPoint = 21,     // la parola dell'accompagnatore dentro il punto di raccolta: va nella sua colonna
    EscortWordUnknown = 22,     // nella colonna dell'accompagnatore una parola che il profilo non conosce
    EscortZero = 23,            // l'accompagnatore sale dove St. Car. e' 0, o scende dove St. Scar. e' 0
    EscortOrder = 24,           // l'accompagnatore scende prima di essere salito, o sale quando e' gia' a bordo
    EscortStillOnBoard = 25,    // alla fine l'accompagnatore e' ancora a bordo
    TimeBackwards = 26,         // l'orario di una riga e' prima di quello della riga prima
    MoreAlightingThanOnBoard = 27,   // scendono piu' persone di quelle a bordo
    TotalsBoarding = 28,        // somma St. Car. diversa da Totali alunni
    TotalsAlighting = 29,       // somma St. Scar. diversa da Totali alunni
    TotalLoadedKm = 30,         // somma dei km pieni diversa da Tot Km Carico
    TotalEmptyKm = 31,          // somma dei km vuoti diversa da Tot Km Vuoto
    SchoolRowMissing = 32,      // una scuola dell'intestazione senza la sua riga tra le fermate
    SchoolRowNotInHeader = 33,  // una riga in una scuola che non e' nell'intestazione
    DateOutsidePeriod = 34,     // una data delle date elencate fuori dal periodo
    NoListedDates = 35,         // cadenza a date elencate senza date
    TooFewStops = 36,           // meno di due righe tra le fermate
    SameStopKm = 37,            // due righe di fila nello stesso luogo alla stessa ora: la seconda ha dei km
    ChoiceBeforeActivation = 38,   // la data scelta e' prima dell'attivazione
    ChoiceAfterEnd = 39,        // la data scelta e' dopo la disattivazione
    OtherContract = 40,         // lo stesso ID e' gia' nel registro con un altro contratto o un altro committente
    EscortSamePassage = 41,     // l'accompagnatore sale e scende nello stesso passaggio
    HomeInNumbersOnly = 42,     // una casa in un pacchetto "solo numeri" (le case stanno solo nel livello completo)
    // K-023: le interruzioni (foglio delle interruzioni).
    BreakReversed = 43,         // l'interruzione finisce prima di cominciare
    BreakOutsidePeriod = 44,    // l'interruzione esce dal periodo della scheda (attivazione, disattivazione)
    BreaksOverlap = 45,         // due interruzioni della stessa scheda si sovrappongono (anche due righe uguali)
    ListedDateInBreak = 46,     // una data elencata cade dentro un'interruzione
    VersionWithoutDays = 47,    // la versione nuova non avrebbe nessun giorno, o la versione di prima accorciata resterebbe senza
    // K-024: il tipo per scheda e il numero dei disabili (varianti che rivelano un dato particolare).
    RulesDisagree = 48,         // due regole del tipo del profilo scattano sulla stessa scheda e danno tipi diversi
    BadDisabledNumber = 49,     // il numero dei disabili e' scritto male (non intero, negativo, testo, cella illeggibile)
    TooManyDisabled = 50,       // piu' disabili dei Totali alunni, accompagnatore escluso
};

struct CardRead {
    int row = 0;                 // riga del foglio delle schede
    std::string nr;              // il Nr come sul file (spazi intorno alla barra tolti)
    std::string id;
    Card card;
    std::vector<Code> new_homes; // codici delle case porta a porta che la scheda crea (non ancora nel pacchetto)
    std::optional<CardFault> fault;
    std::string fault_sheet;     // il foglio, la riga e il titolo della colonna del motivo (mai il valore)
    int fault_row = 0;
    std::string fault_column;
    std::vector<Code> fault_rules;   // K-024, RulesDisagree: le regole che scattano
};

struct CardsRead {
    TableFault fault = TableFault::None;
    std::string fault_sheet;     // il foglio con la riga dei titoli sbagliata
    std::string fault_title;     // il titolo che manca o e' ripetuto (una parola del profilo)
    int card_rows = 0, stop_rows = 0, date_rows = 0;   // righe dopo quella dei titoli
    int break_rows = 0;                                 // K-023: righe del foglio delle interruzioni (0 se non c'e')
    std::vector<CardRead> cards;   // nell'ordine del foglio delle schede
    std::vector<Problem> problems; // K-CTR-161 (righe senza la loro scheda)
};

// Il Nr con gli spazi intorno al separatore tolti ("4000123 / 3" -> "4000123/3"): la chiave con cui si confrontano i Nr.
std::string card_nr_key(const std::string& nr, const CardProfile& p);
// Il numero del servizio: la parte del Nr prima del separatore ("4000123").
std::string card_service_number(const std::string& nr, const CardProfile& p);

// K-025: i Nr del foglio delle schede (chiavi di card_nr_key, celle che si leggono), per confrontarli con --validita-scheda prima della
// password. nullopt se il foglio non ha la riga dei titoli o la colonna del Nr (manca o c'e' due volte): lo dira' read_cards.
std::optional<std::set<std::string>> card_nr_keys(const Table& cards, const CardProfile& profile);

// Legge le tre tabelle con il profilo. date1904: la cartella di lavoro usa il sistema delle date del 1904 (le date che Excel ha
// trasformato in numeri).
CardsRead read_cards(const CardTables& tables, const CardProfile& profile, const Index& idx, bool date1904);

// --- Scelta della data e versioni ---------------------------------------------------------------------------------------

struct DateChoice {
    enum class Kind { None, Activation, TakenInCharge, Date } kind = Kind::None;
    Day date = 0;
};
struct DateChoices {
    DateChoice file;                              // --validita
    std::map<std::string, DateChoice> by_nr;      // --validita-scheda, per chiave del Nr (card_nr_key)
};

// Una cosa che cambia tra due versioni della stessa scheda.
struct CardChange {
    std::string what;                 // nr, servizio, scuole, fermate, orari, numeri, km, totali, date, giorni, calendario, cadenza,
                                      // individualizzato, attrezzato
    std::string before, after;        // in breve (dati comuni: codici, orari, numeri, date)
    bool particular = false;          // le caselle: dati particolari
};

struct CardOutcome {
    enum class Kind {
        New,            // scheda nuova (ID mai visto): versione 1
        NewVersion,     // versione nuova della stessa scheda
        Same,           // stesso ID, contenuto uguale all'ultima versione: niente da fare
        Stopped,        // la scheda non si legge (fault)
        ChoiceMissing,  // attivazione prima della presa in carico, senza una scelta: ferma
    };
    Kind kind = Kind::New;
    int row = 0;
    std::string nr, id;
    Code card_code;
    int version = 0;
    DateRange valid;
    ChoiceMode mode = ChoiceMode::Activation;
    ChoiceFrom from = ChoiceFrom::None;
    std::optional<int> cut_version;       // la versione di prima accorciata
    Day cut_to = 0;                       // ... fino a questo giorno
    std::vector<int> removed_versions;    // versioni tolte (cominciavano dalla data scelta in poi)
    int days_before_taken = 0;            // giornate prima della presa in carico che cambiano (data scelta prima di --data)
    std::vector<CardChange> changes;      // NewVersion: cosa cambia
    std::vector<Code> new_homes;
    std::optional<CardFault> fault;
    Code service, group, trip, calendar;
    bool choice_unused = false;           // K-025: Same con una scelta --validita-scheda per lei (non serve: la scheda non cambia)
};

struct CardsMerge {
    std::vector<ServiceCard> cards;            // il registro nuovo (tutte le schede del pacchetto)
    std::vector<CardOutcome> outcomes;         // nell'ordine del file
    std::vector<Problem> problems;             // K-CTR-160, 162 ... 168: della lettura; K-CTR-169 sul registro nuovo
    std::vector<Code> new_homes;               // case porta a porta da creare, in ordine
    // Per scrivere: le versioni nuove (scheda, numero), le versioni tolte (elementi da togliere) e quelle accorciate.
    struct Cut {
        Code calendar;
        Day to = 0;
    };
    std::vector<std::pair<Code, int>> added;   // codice della scheda, numero della versione
    std::vector<CardVersion> removed;          // le versioni tolte, con i loro elementi
    std::vector<Cut> cuts;
};

// Confronta le schede lette con il registro del pacchetto (puo' non esserci: la prima volta tutte nuove). taken: la presa in
// carico (--data).
CardsMerge merge_cards(const Package& p, const CardProfile& profile, const Code& contract, Day taken, const std::string& reference,
                       const DateChoices& choices, const CardsRead& read);

// I giorni in cui vale il calendario fatto da una scheda nel periodo dato (con la base del pacchetto). K-023: le interruzioni della
// scheda sono sospensioni, tagliate al periodo (un'interruzione tutta fuori dal periodo non c'e'); il motivo lo scrive l'API.
Calendar card_calendar(const Card& c, const Code& code, DateRange valid);
// Quanti giorni conta la scheda (Tot Giorni): dall'attivazione alla disattivazione, interruzioni comprese (un insieme di giorni: un
// giorno di interruzione che non e' un giorno di servizio non si toglie due volte).
int card_days(const Card& c, const Index& idx);
// K-CTR-164: le date elencate di una scheda (cadenza a date elencate) che non cadono in un giorno con la X o non sono giorni del calendario
// di base: quante, e la prima. Una regola sola per la lettura e per kzero controlla (K-025).
struct CardDatesOutside {
    int count = 0;
    Day first = 0;
};
CardDatesOutside card_listed_dates_outside(const Card& k, const Index& idx);

// K-023: la coincidenza di una scheda. Una scheda con una scuola sola nell'intestazione e senza la riga di quella scuola e' ammessa solo
// se la corsa finisce (andata) o comincia (ritorno) in un punto di trasbordo autorizzato (un luogo pubblico con il blocco "trasbordo",
// lo stesso criterio di K-CTR-023), dove scende (andata) o sale (ritorno) almeno una persona, accompagnatore escluso: chi viaggia
// prosegue su un'altra corsa. Il gruppo tiene la sua scuola (la meta, e quello che lega la casa a un luogo sensibile). Il legame con
// l'altra corsa (stessa persona, l'altra corsa passa dopo) non c'e' ancora: mandato del confronto elenco/schede.
struct CardTransferEnd {
    Code school;              // la scuola dell'intestazione senza la sua riga
    Code place;               // il punto di trasbordo
    bool at_start = false;    // ritorno: la corsa comincia li'
    bool verbal = false;      // autorizzato solo a voce (senza documento)
    bool authorized = true;   // K-025 (D8, solo card_end_without_school): il luogo e' un punto di trasbordo autorizzato
};
std::optional<CardTransferEnd> card_transfer_end(const Card& k, const Index& idx);
// K-025 (D8): come card_transfer_end, ma anche quando il luogo dove la corsa finisce (o comincia) e' un luogo pubblico che non e' (piu') un
// punto di trasbordo autorizzato (authorized falso): in kzero controlla, una versione scritta quando lo era. Mai una scuola o una casa.
std::optional<CardTransferEnd> card_end_without_school(const Card& k, const Index& idx);

// K-024: le persone a bordo di una scheda, accompagnatore escluso (Totali alunni lo comprende, K-021).
int card_passengers(const Card& k);
// K-024: il numero dei disabili che non torna con il tipo del servizio (K-CTR-177): variante 0, tipo disabili con 0; 1, disabili a bordo
// di un servizio scolastico, di linea o NCC (promiscuo di fatto); 2, tipo disabili con a bordo anche persone che non sono disabili. Niente
// se il numero non e' scritto o il tipo e' "altro".
std::optional<int> card_disabled_mismatch(const Card& k, ServiceKind kind);
// K-024: il tipo di un servizio in breve, per i messaggi e per "cosa cambia" (dati particolari): "disabili", "altro: ...".
std::string card_kind_text(const CardKind& k);
// K-024: il tipo del servizio in parole ("scolastico", "di linea", "per disabili", "NCC", "altro"), per i messaggi (dati particolari).
std::string card_service_kind_text(ServiceKind k);

// Le cose che cambiano tra due schede (a: quella di prima).
std::vector<CardChange> card_changes(const Card& a, const Card& b);
// Le due schede sono uguali (la parola nuova/modifica e le righe del file non contano)?
bool same_card(const Card& a, const Card& b);

// --- Togliere una scheda scritta per errore ------------------------------------------------------------------------------

enum class CardRemovalRefusal {
    None,
    RemovedOthers,      // l'ultima versione ha tolto altre versioni: solo con --tutte
    TripInUse,          // una corsa da togliere e' usata da un turno o da un blocco
};
struct CardRemoval {
    CardRemovalRefusal refusal = CardRemovalRefusal::None;
    Code card;
    std::vector<CardVersion> removed;          // le versioni tolte, con i loro elementi
    std::optional<CardsMerge::Cut> restored;   // la versione di prima riprende la sua fine
    int restored_version = 0;
    bool card_gone = false;
    Code user;                                 // TripInUse: il turno o il blocco
    Code trip;
};
// Toglie dal registro l'ultima versione (o tutte) della scheda. Niente cambia se c'e' un rifiuto.
CardRemoval remove_card(std::vector<ServiceCard>& cards, std::vector<RemovedCardVersions>& removed, const Code& card, bool all,
                        const std::string& reference, Day date, const Package& p);

// --- Controlli sul registro (kzero controlla) ------------------------------------------------------------------------------
// K-CTR-169 (schede dello stesso servizio con periodi che si sovrappongono), K-CTR-171 (accompagnatore che manca su una corsa di una
// scheda, con la regola del regolamento), K-CTR-172 (pezzo di una corsa di una scheda con i km non noti), e i controlli di sempre
// sui codici doppi e sui riferimenti. Il confronto degli elementi con la scheda (K-CTR-170) lo fa l'API, che fa gli elementi.
void check_cards(const Package& p, const Index& idx, std::vector<Problem>& out);

// I km di un pezzo di corsa (dal passaggio from al passaggio to). Per una corsa fatta da una scheda i km di un passaggio coprono il
// tratto dal passaggio prima che ha dei km: i km del pezzo si sanno solo se i suoi due capi hanno dei km (o sono i capi della corsa).
struct PartKm {
    double loaded = 0, empty = 0;
    bool known = true;
    int ends_without_km = 0;   // K-022: quanti capi del pezzo (1 o 2) non hanno km, quando i km non si sanno
};
PartKm part_km(const Trip& t, std::size_t from, std::size_t to, bool from_card);
// Le corse fatte da una scheda (del registro).
std::set<Code> card_trips(const Package& p);

}  // namespace kzero
