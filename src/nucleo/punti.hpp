#pragma once
// K-Zero - punti degli utenti dell'elenco (K-027): su quale scheda e a quale passaggio sale (andata) o scende (ritorno) ogni utente,
// per ogni tratta della nostra azienda, da quando e chi l'ha deciso; la proposta dei punti (K-Zero propone, una persona conferma); il
// confronto con i numeri delle schede (St. Car. e St. Scar.); i controlli. Nessun testo: i problemi hanno il loro codice (catalogo
// dell'API).
//
// Regole (docs/formato-dati.md, "Punti degli utenti"; DECISIONI.md, K-027):
// - il punto punta al passaggio (luogo e volta), mai al luogo: una scheda puo' passare due volte dallo stesso luogo in giri diversi. In
//   un'altra versione della scheda e' lo stesso passaggio solo se il luogo c'e' lo stesso numero di volte: altrimenti un avviso, mai
//   riagganciato (D2 dell'arbitro);
// - una voce nuova dal giorno D chiude quella di prima al giorno D-1; quelle che cominciavano da D in poi restano scritte, sostituite.
//   I giorni prima di D non cambiano mai;
// - mai un punto senza conferma: la proposta non scrive niente; scrivono solo i comandi che la persona lancia con i punti scritti;
// - la scheda e l'elenco non si correggono mai da soli: il confronto dice le differenze, e basta;
// - i giorni in cui un utente viaggia una tratta vengono dai suoi orari, solo nei giorni di scuola (i calendari di base delle schede
//   della sua scuola, D4); senza orari i giorni non si sanno, mai inventati;
// - mai dividere i St. Car. tra piu' scuole: le discese a ogni scuola si confrontano con chi va in quella scuola.

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
#include "domain_models.hpp"
#include "pacchetto.hpp"

namespace kzero {

// --- Le schede nei giorni ------------------------------------------------------------------------------------------------

// La scheda del registro con quell'ID del committente (nullptr se non c'e').
const ServiceCard* card_by_id(const Package& p, const std::string& id);
// La versione che vale quel giorno in K-Zero (nullptr se nessuna) e la versione con quel numero (nullptr se tolta o mai scritta).
const CardVersion* card_version_on(const ServiceCard& s, Day d);
const CardVersion* card_version_number(const ServiceCard& s, int number);
// Quante volte la scheda passa da un luogo, e la riga della volta "occurrence" (da 1).
int card_place_count(const Card& k, const Code& place);
std::optional<std::size_t> card_passage(const Card& k, const Code& place, int occurrence);
// La volta di una riga: quante volte il suo luogo c'e' fino a quella riga compresa.
int card_occurrence(const Card& k, std::size_t row);
// St. Car. e St. Scar. di una riga senza l'accompagnatore (dove lo dice la sua colonna).
int card_boarding(const CardStop& s);
int card_alighting(const CardStop& s);

// I calendari che servono ai punti, letti una volta: i giorni in cui gira ogni versione di scheda e i giorni di scuola (D4: i calendari
// di base delle schede di una scuola, per il committente).
class PointsCalendar {
public:
    PointsCalendar(const Package& p, const Index& idx);
    // La versione vale quel giorno e il suo calendario (con le interruzioni) gira.
    bool runs(const CardVersion& v, Day d) const;
    // Giorno di scuola per gli utenti di quella scuola del committente: nessun valore se nessuna scheda del committente passa dalla
    // scuola (i giorni di scuola non si sanno).
    std::optional<bool> school_day(const Code& client, const Code& school, Day d) const;

private:
    const Index& idx_;
    mutable std::map<const CardVersion*, Calendar> calendari_;
    mutable std::map<std::pair<const CardVersion*, Day>, bool> gira_;
    std::map<std::pair<Code, Code>, std::set<Code>> basi_;   // (committente, scuola) -> calendari di base
    mutable std::map<std::pair<Code, Day>, bool> attivo_;
};

// --- La storia dei punti -------------------------------------------------------------------------------------------------

// La voce che vale quel giorno (le sostituite non contano; nullptr se nessuna) e le sue regole per quel giorno della settimana.
const PointEntry* point_entry_on(const UserPoints& up, Day d);
std::vector<const PointRule*> point_rules_on(const UserPoints& up, Day d);
// Doppia tratta: le regole della voce hanno luoghi diversi (D7).
bool point_entry_double(const PointEntry& e);
// Scrive una voce nuova nella storia: le voci (non ancora sostituite) con un dal uguale o dopo diventano sostituite, con la data e il
// riferimento della voce nuova; la voce che valeva il giorno prima resta com'e' (finisce da sola il giorno prima della nuova). I
// giorni prima del dal della voce nuova non cambiano mai. Restituisce i dal delle voci sostituite.
std::vector<Day> add_point_entry(UserPoints& up, const PointEntry& e);

// Il periodo "non frequenta" che vale quel giorno per la tratta (nullptr se nessuno).
const NotAttending* not_attending_on(const ListUser& u, Day d, const std::string& leg);
// Ha diritto al servizio su quella tratta quel giorno: nel piano e attivo, oppure sospeso con uno sblocco che vale quel giorno.
bool point_right(const ListUser& u, const std::string& leg, Day d);
// Viaggia la tratta quel giorno? Dai suoi orari (giorno della settimana e date), solo nei giorni di scuola. Senza orari per la tratta,
// o senza i giorni di scuola: non noto.
enum class Travel { Yes, No, Unknown };
Travel point_travel(const ListUser& u, const Code& client, const std::string& leg, Day d, const PointsCalendar& cal);
// L'ora dell'utente per la tratta quel giorno (l'ora delle lezioni, entrata o uscita: D6), se gli orari la dicono.
std::optional<Minutes> point_user_time(const ListUser& u, const std::string& leg, Day d);

// La riga della scuola dell'utente per un passaggio della scheda: all'andata la prima riga dopo nel luogo della sua scuola (dove scende),
// al ritorno l'ultima prima (dove sale). Nella coincidenza (K-023: la scheda finisce, all'andata, o comincia, al ritorno, in un punto di
// trasbordo per la sua scuola) la riga del punto di trasbordo, con coincidence vero. Nessuna riga: la scheda non porta alla sua scuola.
struct PointSchoolRow {
    std::optional<std::size_t> row;
    bool coincidence = false;
};
PointSchoolRow point_school_row(const Card& k, std::size_t row, const Code& school, const Index& idx);

// --- La proposta -----------------------------------------------------------------------------------------------------------

// Un candidato: la scheda e il passaggio, nella versione in vigore il giorno della proposta.
struct PointCandidate {
    const ServiceCard* card = nullptr;
    const CardVersion* version = nullptr;
    Code place;
    int occurrence = 1;
    std::size_t row = 0;                 // la riga del passaggio
    std::size_t school_row = 0;          // la riga della scuola (andata: dopo; ritorno: prima)
    bool operator==(const PointCandidate& o) const { return card == o.card && place == o.place && occurrence == o.occurrence; }
};
struct PointDayProposal {
    Weekday weekday = Weekday::Monday;
    std::optional<Minutes> user_time;    // l'ora dell'utente quel giorno (D6)
    std::vector<PointCandidate> candidates;
};
struct PointProposal {
    const ClientList* list = nullptr;
    const ListUser* user = nullptr;
    std::string leg;
    Direction direction = Direction::Outbound;
    Code company;                         // la ditta della tratta (un'azienda del pacchetto)
    PointSource source = PointSource::None;   // la prima fonte con almeno un candidato
    std::vector<Code> places;             // i luoghi di quella fonte
    bool days_known = true;               // false: senza orari per la tratta (D4): i giorni sono quelli delle schede
    std::vector<PointDayProposal> days;   // un giorno della settimana alla volta, in ordine
    std::vector<PointRule> ready;         // le regole pronte: i giorni con un candidato solo
    bool has_point = false;               // ha gia' un punto quel giorno (si propone solo con "tutti")
};
// Le fonti, in ordine: la fermata indicata nell'elenco, lo stradario dell'azienda della tratta. Per ogni utente nel piano con diritto
// al servizio e ogni tratta della nostra azienda senza un punto valido il giorno "from" (con "all" anche quelle con un punto).
// user_filter e leg_filter vuoti = tutti. Non scrive niente.
std::vector<PointProposal> propose_points(const Package& p, const Index& idx, Day from, const Code& user_filter, const std::string& leg_filter,
                                          bool all);
// I luoghi dello stradario per una casa (la riga piu' precisa: la scuola, poi il grado, poi tutte; a pari livello con i civici prima che
// senza). Vuoto se nessuna riga.
std::vector<Code> street_points(const StreetTable& t, const ListUserHome& home, const School* school);
// I candidati per un luogo, un utente e una tratta (scheda del committente con il contratto dell'azienda, del verso della tratta, in
// vigore il giorno "from", che gira in quel giorno della settimana, che passa dal luogo e poi dalla scuola, all'andata, o prima dalla
// scuola e poi dal luogo, al ritorno; con l'orario compatibile, D6).
std::vector<PointCandidate> point_candidates(const Package& p, const Index& idx, const PointsCalendar& cal, const ClientList& list,
                                             const ListUser& u, const std::string& leg, Direction direction, const Code& company,
                                             const Code& place, Weekday weekday, Day from, std::optional<Minutes> user_time);

// --- Il confronto con le schede ----------------------------------------------------------------------------------------------

struct ComparedPassage {
    int card_boarding = 0, card_alighting = 0;   // dalla scheda, accompagnatore tolto
    // Gli utenti contati, in ordine. A una fermata ognuno una volta; alla scuola una volta per ogni passaggio contato quel giorno sulla
    // versione (D7: il committente conta la doppia tratta su tutte e due le fermate, quindi anche due volte nelle discese alla scuola).
    std::vector<Code> boarding, alighting;
    std::vector<Code> doubles;                   // utenti "doppi" (D7): contati anche su un altro passaggio quel giorno
    std::vector<Code> doubles_same_card;         // ... su un altro passaggio della stessa versione (a bordo un posto solo: K-028)
    bool school = false;                         // e' il passaggio di una scuola (andata: discese; ritorno: salite)
};
struct ComparedPeriod {
    Weekday weekday = Weekday::Monday;
    std::vector<Day> days;                       // i giorni del periodo (quel giorno della settimana), in ordine
    std::vector<ComparedPassage> passages;       // una per riga della scheda
    std::vector<Code> not_attending;             // con un punto qui, ma "non frequenta"
    std::vector<Code> not_counted;               // con un punto qui, non contati: sospesi senza sblocco, punti che non valgono
    bool differs = false;
};
struct ComparedVersion {
    const ServiceCard* card = nullptr;
    const CardVersion* version = nullptr;
    std::vector<ComparedPeriod> periods;         // per giorno della settimana, poi per data
};
struct PointsComparison {
    std::vector<ComparedVersion> versions;       // nell'ordine del registro
    std::vector<Problem> problems;               // K-CTR-181 ... 189 (giorni non ancora raggruppati)
};
// Confronta i punti degli utenti con i numeri delle schede, nei giorni di range (dentro l'anno scolastico dell'elenco del committente
// della scheda). only_card: solo la scheda con quel codice o ID (vuoto = tutte).
PointsComparison compare_points(const Package& p, const Index& idx, DateRange range, const std::string& only_card = std::string());

// --- I valori dall'ingresso standard (kzero assegna-punto, conferma-punti, non-frequenta, sblocco --a-voce) ---------------------
// Come i campi del K-020: una riga per valore, mai come argomenti; le righe arrivano gia' pulite e nei limiti (lo fa l'API, che le legge).
// - assegna-punto: "punto: <scheda> / <luogo> [/ <volta>] [/ <giorni>]" (una riga per regola), "consenso: <testo>", oppure "nessuno";
// - conferma-punti: "<utente> <tratta>: <scheda> / <luogo> / <volta> [/ <giorni>]" (le righe pronte di kzero proponi-punti);
// - non-frequenta: "motivo: famiglia|altro_mezzo|assente|altro", "nota: <testo>";
// - sblocco --a-voce: "chi: <testo>".
// La scheda e' il codice del registro o l'ID; il luogo il codice; i giorni nomi della settimana separati da virgole (es. "lunedi,
// mercoledi"), senza = sempre.
enum class PointLinesMode { Assign, Confirm, NotAttending, Voice };
struct PointLine {
    Code user;               // conferma: l'utente
    std::string leg;         // conferma: la tratta
    std::string card;        // codice o ID della scheda, com'e' scritto
    Code place;
    std::optional<int> occurrence;
    bool all_days = true;
    Weekdays days;
    int line = 0;            // da 1
};
enum class PointLinesFault {
    None,
    UnknownField,     // una riga che non e' di quel comando
    RepeatedField,    // lo stesso campo due volte (consenso, motivo, nota, chi)
    Syntax,           // le parti del punto non si leggono (scheda / luogo / volta / giorni)
    Day,              // un giorno della settimana che non esiste
    Number,           // la volta non e' un numero da 1 a 99
    Reason,           // un motivo che non e' tra quelli fissi
    Missing,          // manca il valore che serve (nessun punto, il motivo, chi)
    NobodyWithPoints, // "nessuno" insieme a dei punti (o al consenso)
};
struct PointLines {
    std::vector<PointLine> points;
    std::string consent;
    bool nobody = false;
    std::optional<NotAttendingReason> reason;
    std::string note;
    std::string who;
    PointLinesFault fault = PointLinesFault::None;
    int line = 0;
};
PointLines parse_point_lines(const std::vector<std::string>& lines, PointLinesMode mode);
// "lunedi, mercoledi" -> i giorni (senza accenti ne' maiuscole); nessun valore se un nome non e' un giorno o l'elenco e' vuoto.
std::optional<Weekdays> parse_weekdays(const std::string& text);

// --- Controlli (kzero controlla) -------------------------------------------------------------------------------------------
// I dati (K-CTR-140 varianti 5-10, 010, 012, 187, 184 variante 3, 190 stradari) e il confronto nel periodo del pacchetto.
void check_points(const Package& p, const Index& idx, std::vector<Problem>& out);

}  // namespace kzero
