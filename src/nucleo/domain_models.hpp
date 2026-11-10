#pragma once
// Modelli di dominio K-Zero: un solo modello dati per 4 o 500 mezzi, per aziende, consorzi ed enti di ogni paese.
// Orari = minuti dalla mezzanotte del giorno di servizio, anche oltre le 24:00 (07:45 -> 465; 25:30 -> 1530).
// Date = giorni dal 1970-01-01. Distanze in km; dove il nome dice altro vale il nome (_m = metri, _t = tonnellate).
// Ogni elemento ha un codice testuale scelto da chi scrive i dati, unico per tipo di elemento.
// Il nucleo riceve dati gia' controllati nella forma (dagli schemi JSON, nell'API): qui ci sono il modello
// e le regole del mestiere. Nessun valore di regola e nessun nome di committente: stanno nei dati
// (regolamenti e contratti).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "tempo.hpp"

namespace kzero {

using Code = std::string;   // codice K-Zero di un elemento

struct Coord {
    double lat = 0.0;   // WGS84, gradi decimali
    double lon = 0.0;
};

// Distanza in linea d'aria (km). Provvisoria: verra' sostituita dalla matrice delle distanze stradali.
inline double air_km(const Coord& a, const Coord& b) {
    constexpr double R = 6371.0, D = 3.14159265358979 / 180.0;
    const double dlat = (b.lat - a.lat) * D, dlon = (b.lon - a.lon) * D;
    const double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(a.lat * D) * std::cos(b.lat * D) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * R * std::asin(std::sqrt(h));
}

struct TimeWindow {
    Minutes open = 0;   // primo istante ammesso
    Minutes close = 0;  // ultimo istante ammesso
};

struct DateRange {
    Day from = 0;   // primo giorno compreso
    Day to = 0;     // ultimo giorno compreso
};

// Dove sta un dato nel pacchetto: file e percorso JSON standard (es. "corse.json", "/corse/3/passaggi/4").
// Lo scrive chi legge i file (l'API); il nucleo lo usa solo per dire dove sta un problema.
struct Source {
    std::string file;
    std::string pointer;
    int line = 0;     // riga e colonna nel file (0 = non note): servono quando il file non e' un JSON valido
    int column = 0;
};

// Riferimento a un altro elemento: il suo codice e il punto dei dati in cui e' scritto.
struct Ref {
    Code code;
    Source src;
};

// Codice dello stesso elemento in un altro sistema, es. sistema "Trasporti Provinciali", tipo "fermata", codice "6220PF".
struct ExternalCode {
    std::string system;
    std::string type;
    std::string code;
};

// --- Elenchi fissi ----------------------------------------------------------------
// Dove un valore dipende dal paese o dal committente c'e' "Other" con una descrizione nei dati.

enum class Grade { Kindergarten, Primary, LowerSecondary, UpperSecondary };   // infanzia ... secondaria di II grado

enum class VehicleCategory { M1, M2, M3 };   // categorie europee dei veicoli per persone

// Tipo di servizio dichiarato dal committente. Dal K-012 anche l'NCC (noleggio con conducente: gite, turismo, eventi): dice la
// natura del servizio, non la licenza del mezzo. "Promiscuo" non si dichiara: lo ricava derive_service sulla corsa.
enum class ServiceKind { Line, School, Disability, Ncc, Other };

// Chi viaggia: tipo di un gruppo di passeggeri della domanda.
enum class GroupType { Students, Disabled, LineUsers, AuthorizedAdults, Other };

enum class CenterType { Day, Therapy, SocioEducational, Other };   // diurno, terapie, socio-educativo, altro

// Esigenze di salute di una persona o di un gruppo (dati particolari).
enum class Need { Wheelchair, Assistant, Other };

// Rapporto di lavoro di una persona (persone.json). Sono "lavoro dipendente", per il significato del formato: tempo pieno,
// part-time, a chiamata, tempo determinato, stagionale (docs/formato-dati.md).
enum class EmploymentKind {
    Owner, FamilyHelper, Partner, FullTime, PartTime, OnCall, FixedTerm, Seasonal, SelfEmployed, Volunteer, Other
};
inline bool is_employee(EmploymentKind k) {
    return k == EmploymentKind::FullTime || k == EmploymentKind::PartTime || k == EmploymentKind::OnCall ||
           k == EmploymentKind::FixedTerm || k == EmploymentKind::Seasonal;
}

enum class ShiftRole { Driver, Escort, SecondDriver };   // autista, assistente a bordo, secondo autista

// Macro categorie di una corsa, ricavate da chi trasporta (vedi derive_service).
enum class ServiceType {
    Line,        // Servizi di Linee
    Disability,  // Servizi Handicap (disabili)
    School,      // Servizi Scolastici
    Ncc,         // Noleggio con conducente (K-012): gite, turismo, eventi
    Mixed,       // Servizi promiscui (fino al K-011 "ibridi")
    Other        // solo gruppi di tipo "altro"
};

// --- Luoghi -------------------------------------------------------------------------

enum class PlaceType {
    Stop,             // fermata pubblica
    School,           // scuola
    Center,           // centro (diurno, terapie, socio-educativo...)
    Depot,            // rimessa
    ExternalShelter,  // ricovero esterno
    Parking,          // punto di sosta del mezzo
    TransferPoint,    // punto di trasbordo
    DoorToDoor,       // punto porta a porta: la casa del passeggero (luogo privato)
};

enum class BanReason { Sign, Experience, Other };   // cartello, esperienza dell'autista, altro

// Divieto di accesso per un mezzo preciso oppure per una categoria di mezzi.
struct AccessBan {
    std::optional<Ref> vehicle;
    std::optional<VehicleCategory> category;
    BanReason reason = BanReason::Sign;
    std::string note;
    Source src;
};

struct AccessLimits {
    std::optional<double> max_length_m, max_width_m, max_height_m, max_mass_t;
    std::vector<AccessBan> bans;
};

// Autorizzazione di un punto di trasbordo: chi l'ha data, quando, con quale documento oppure "a voce".
struct TransferAuthorization {
    std::string authorized_by;
    Day date = 0;
    std::string document;   // vuoto = nessun documento
    bool verbal = false;    // autorizzato a voce
    std::string note;
    Source src;
};

struct Place {
    Code code;
    PlaceType type = PlaceType::Stop;
    bool is_private = false;   // casa di una persona: punto porta a porta o ricovero presso un'abitazione
    std::string name;          // nome ufficiale
    std::vector<std::string> alt_names;
    std::string address;       // indirizzo (solo luoghi privati)
    std::string municipality;  // comune
    std::optional<Coord> coord;
    std::vector<ExternalCode> external;
    std::optional<AccessLimits> limits;
    std::optional<TransferAuthorization> transfer;   // presente = punto di trasbordo autorizzato
    // Luogo sensibile (K-009, scelta di chi scrive i dati): dire che una persona ci va lascia capire un dato sulla
    // salute (es. un centro diurno o di terapia). Nelle uscite che lasciano riconoscere un passeggero (il nome o la
    // casa) senza i dati particolari, il legame con questo luogo e' riservato (API: LegameSensibile).
    bool sensitive = false;
    std::string note;
    Source src;
};

// Tratto di strada tra due luoghi con i suoi limiti (es. sottopasso, strada stretta).
struct RoadSegment {
    Code code;
    std::string name;
    Ref from, to;
    bool both_directions = true;
    AccessLimits limits;
    std::string note;
    Source src;
};

// --- Regolamenti: i parametri di un territorio o di un committente -------------------
// Il programma non conosce nessun valore: li legge da qui. Ogni parametro ha la sua fonte.

struct RuleSource {
    std::string document;     // vuoto = il documento del regolamento
    std::string point;        // punto del documento (es. "1.B.1")
    std::string references;   // altre norme citate
};

struct ValueRule {
    int value = 0;
    RuleSource source;
    Source src;
};

struct GradeValue {
    std::vector<Grade> grades;
    int value = 0;
};

struct GradeValuesRule {
    std::vector<GradeValue> values;
    RuleSource source;
    Source src;

    // Valore per un grado, se il regolamento lo fissa.
    std::optional<int> for_grade(Grade g) const {
        for (const auto& v : values)
            if (std::find(v.grades.begin(), v.grades.end(), g) != v.grades.end()) return v.value;
        return std::nullopt;
    }
};

// Una condizione vale se a bordo c'e' almeno uno dei gradi, dei tipi di gruppo e delle esigenze elencati
// (un elenco vuoto non pone condizioni) e il mezzo ha piu' posti per i passeggeri di seats_over.
struct PassengerCondition {
    std::vector<Grade> grades;
    std::vector<GroupType> group_types;
    std::vector<Need> needs;
    std::vector<ServiceKind> service_kinds;
    std::vector<CenterType> center_types;
    std::optional<int> seats_over;
};

// Regola fatta di condizioni alternative: basta che ne valga una.
struct ConditionsRule {
    std::vector<PassengerCondition> conditions;
    RuleSource source;
    Source src;
};

struct GradesRule {
    std::vector<Grade> grades;
    RuleSource source;
    Source src;
};

struct StandingRule {
    std::vector<Grade> forbidden_grades;   // gradi per cui i passeggeri in piedi non sono mai ammessi
    std::optional<int> max_minutes;
    std::optional<double> max_km;
    RuleSource source;
    Source src;
};

struct PassengerSet {
    std::vector<Grade> grades;
    std::vector<GroupType> group_types;
};

struct ForbiddenCombination {
    PassengerSet first, second;   // chi sta nel primo insieme non viaggia con chi sta nel secondo
};

struct CombinationsRule {
    std::vector<ForbiddenCombination> combinations;
    RuleSource source;
    Source src;
};

// Usi dei mezzi (carta di circolazione) ammessi per alcuni tipi di servizio, es. scolastico e disabili: linea o
// noleggio con conducente. Sta nelle norme di un paese e, se un territorio decide diversamente, nel regolamento.
struct ServiceUses {
    std::vector<ServiceKind> kinds;
    std::vector<Ref> uses;   // codici degli usi (norme del paese)
    Source src;
};

struct ServiceUsesRule {
    std::vector<ServiceUses> values;
    RuleSource source;
    Source src;
};

// Per alcuni tipi di servizio l'autista ha bisogno di un'autorizzazione del committente (es. un cartellino di
// riconoscimento per i servizi speciali).
struct ClientAuthorizationRule {
    std::vector<ServiceKind> kinds;
    RuleSource source;
    Source src;
};

struct Regulation {
    Code code;
    std::string authority;   // ente
    std::string title;
    std::string document;
    std::optional<Day> valid_from, valid_to;
    std::optional<ValueRule> max_wait_before_min;   // attesa massima prima dell'inizio delle lezioni
    std::optional<ValueRule> max_wait_after_min;    // attesa massima dopo la fine delle lezioni
    std::optional<ValueRule> max_home_stop_m;
    std::optional<ValueRule> max_stop_school_m;
    std::optional<ValueRule> min_stop_distance_m;
    std::optional<GradeValuesRule> min_home_school_m;
    std::optional<GradeValuesRule> min_pupils;      // alunni minimi per attivare un servizio
    std::optional<GradeValuesRule> max_ride_min;    // durata massima a bordo
    std::optional<ConditionsRule> assistant;        // quando serve l'assistente a bordo
    std::optional<ConditionsRule> adult_at_dropoff; // quando serve un adulto alla fermata di discesa
    std::optional<GradesRule> dropoff_without_adult;  // discesa senza adulto ammessa con l'autorizzazione dei genitori
    std::optional<StandingRule> standing;
    std::optional<CombinationsRule> forbidden;
    std::optional<ServiceUsesRule> vehicle_uses;                 // usi dei mezzi per tipo di servizio (versione 2)
    std::optional<ClientAuthorizationRule> client_authorization; // autorizzazione del committente (versione 2)
    Source src;
};

// --- Orari di lavoro (K-011): categorie di tempo, regimi e tipi di regola -----------------------------------------
// Le leggi sugli orari, i contratti collettivi e le scelte delle aziende sono dati (norme.json, aziende.json,
// persone.json): il programma conosce solo i tipi di regola, cioe' COME si conta. I NUMERI e le fonti stanno nei dati.
// Ogni regola ha una lettura certa e, se la legge e' incerta, una lettura prudente piu' severa: violare la certa e' un
// errore, violare solo la prudente un avviso "da confermare". Il programma non sceglie mai la lettura piu' comoda per
// dire "regolare" e, senza dati, non dice mai "regolare".

// Categoria di un minuto del lavoro di una persona (Reg. CE 561/2006 art. 4 e 16, D.Lgs. 234/2007 art. 3). Il riposo
// (tra due giornate) non e' una categoria degli elementi: e' il tempo fuori dalle giornate.
enum class TimeCategory {
    Driving,        // guida
    OtherWork,      // altre mansioni
    Availability,   // disponibilita' sul posto (pronto a partire, attesa con il mezzo)
    OnCall,         // reperibilita' (fuori dal posto di lavoro, a chiamata)
    Break,          // interruzione (pausa, libero da ogni servizio)
};
inline constexpr int kTimeCategories = 5;

// Soste non scritte tra due elementi della giornata (aziende.json versione 2): fino a N minuti una categoria, oltre
// un'altra (es. fino a 30 minuti disponibilita', oltre interruzione). Senza "oltre" vale sempre la prima.
struct UnwrittenStops {
    TimeCategory category = TimeCategory::Availability;
    std::optional<int> over_minutes;
    TimeCategory category_over = TimeCategory::Break;
    Source src;
};

// Famiglia di una regola: guida, riposi, lavoro, notturno, giornate. Un regime che vale per tutta la settimana (nota INL
// 61/2021) dice per quali famiglie e quali famiglie di un altro regime esclude.
enum class RuleFamily { Driving, Rest, Work, Night, Days };

// Tipi di regola: un elenco chiuso. Ognuno sa come si conta; i numeri stanno nei dati.
enum class RuleType {
    WorkCount,           // conteggio_lavoro: quali categorie e quali soste contano come lavoro
    MaxWork,             // lavoro_massimo: nel giorno, nella settimana, nel mese, in media su un periodo; straordinario
    ContinuousDriving,   // guida_continua: guida massima prima di un'interruzione che azzera il conto
    MaxDriving,          // guida_massima: tra due riposi, nel giorno, nella settimana, in due settimane; estensioni
    ContinuousWork,      // lavoro_continuo: lavoro massimo senza pausa e pause in base al lavoro del giorno
    DailyRest,           // riposo_giornaliero: nelle 24 ore, regolare, frazionato, ridotto, media
    WeeklyRest,          // riposo_settimanale: minimo, ridotto, entro N periodi di 24 ore, due settimane, cumulo, compensazione
    NightWork,           // lavoro_notturno: conteggio di una fascia e limite di lavoro con lavoro notturno
    MaxDays,             // giornate_massime: giornate di lavoro in un periodo mobile di anni
    MaxSpan,             // ampiezza_massima: dalla prima all'ultima attivita' della giornata
    ConsecutiveDays,     // giorni_consecutivi: giorni di lavoro di fila
};

// Il "giorno" di una regola lo dice la regola.
enum class RulePeriod {
    Workday,        // giornata: i turni che la persona comincia in quel giorno di calendario, anche oltre la mezzanotte
    Day24,          // 24_ore: le 24 ore dall'inizio della giornata
    Week,           // settimana: lunedi' 00:00 - domenica 24:00
    TwoWeeks,       // due_settimane: due settimane consecutive
    Month,          // mese di calendario
    BetweenRests,   // tra_due_riposi: il periodo tra due riposi giornalieri (o settimanali)
};

enum class RestMeasure {
    From24hStart,     // 24_ore_dall_inizio: nelle 24 ore dall'inizio della giornata un riposo ininterrotto
    From24hEndRest,   // 24_ore_dalla_fine_del_riposo: entro 24 ore dalla fine del riposo precedente un nuovo riposo
    BetweenWorkdays,  // tra_due_giornate: dalla fine di una giornata all'inizio della successiva
};

// Interruzione (o pausa) in parti, nell'ordine: es. 45, oppure 15 seguita da 30.
struct BreakPattern {
    std::vector<int> parts;
    bool occasional_only = false;   // vale solo per i servizi occasionali del regime
};

// Tempo inoperoso contato in parte come lavoro (L. 138/1958 art. 6 lett. f: il 12% fuori residenza).
struct IdleShare {
    std::vector<TimeCategory> categories;
    int percent = 0;
};

struct PauseNeed {
    int work_over = 0;   // con un lavoro del giorno oltre questi minuti...
    int pause = 0;       // ...pause per almeno questi minuti
};

struct ExceptBreaks {   // giornate escluse dalla media: con "quante" interruzioni di almeno "minuti"
    int minutes = 0;
    int count = 1;
};

// I parametri di una lettura (certa o prudente) di una regola. Ogni tipo usa i suoi; gli altri restano vuoti.
struct RuleParams {
    // conteggio_lavoro
    std::string measure_name;                  // es. "lavoro effettivo", "orario di lavoro"
    std::vector<TimeCategory> categories;      // categorie contate (conteggio); categorie che interrompono (guida continua)
    std::optional<int> stops_up_to;            // soste (interruzioni, reperibilita') fino a N minuti contate per intero
    std::optional<IdleShare> idle_away;        // tempo inoperoso fuori residenza contato in parte
    std::optional<IdleShare> idle_home;        // tempo inoperoso in residenza contato in parte
    // lavoro_massimo, guida_massima, ampiezza_massima, lavoro_notturno, guida_continua, lavoro_continuo
    std::optional<int> max_minutes;
    std::optional<int> normal_minutes;         // oltre il normale e fino al massimo: straordinario (avviso)
    std::optional<int> average_weeks, average_months, average_days;   // media su un periodo mobile
    std::optional<int> extension_max;          // guida_massima: estensione ammessa...
    std::optional<int> extension_times;        // ...al massimo tante volte a settimana
    // guida_continua
    std::vector<BreakPattern> resets;
    std::optional<int> short_counted_up_to;    // interruzioni brevi contate nella guida...
    std::optional<int> short_counted_count;    // ...le prime N
    std::optional<int> terminus_stops_over;    // eccezione: tutte le soste ai capolinea della giornata oltre N minuti
    // lavoro_continuo
    std::optional<int> fraction_min;           // pause in parti di almeno N minuti
    std::vector<PauseNeed> pauses;
    // riposo_giornaliero, riposo_settimanale
    std::optional<int> min_minutes;
    std::vector<int> split;                    // riposo frazionato: parti nell'ordine (es. 180, 540)
    std::optional<int> reduced_min;            // riposo ridotto: almeno tanti minuti...
    std::optional<int> reduced_times;          // ...al massimo tante volte (tra due riposi settimanali)
    std::optional<int> rest_average_min;       // media del riposo giornaliero...
    std::optional<int> rest_average_days;      // ...su tanti giorni consecutivi
    std::optional<int> rest_average_exclude_from;   // senza i riposi di almeno tanti minuti (il settimanale)
    std::vector<ExceptBreaks> rest_average_except;   // salvo le giornate con queste interruzioni
    bool on_call_interrupts = true;            // la reperibilita' interrompe il riposo
    std::optional<int> two_weeks_regular;      // in due settimane consecutive almeno tanti riposi regolari
    std::optional<int> within_24h_periods;     // il riposo settimanale comincia entro tanti periodi di 24 ore
    std::optional<int> cumulation_weeks;       // un riposo lungo vale per tante settimane accanto (cumulo)
    std::optional<int> compensation_weeks;     // riduzione compensata entro la fine della N-esima settimana dopo...
    std::optional<int> compensation_attached;  // ...attaccata a un riposo di almeno tanti minuti
    // lavoro_notturno
    std::optional<Minutes> band_from, band_to; // fascia (anche a cavallo della mezzanotte: 22:00-05:00)
    // giornate_massime, giorni_consecutivi
    std::optional<int> max_days;
    std::optional<int> years;
};

// Straordinario (es. L. 138/1958 art. 3): cosa chiede la legge, scritto nei dati.
struct Overtime {
    std::string notice;            // es. "denuncia entro 48 ore all'Ispettorato del lavoro e alla Motorizzazione civile"
    std::string recurring;         // es. "lo straordinario non meramente saltuario e' vietato, salvo speciali esigenze"
    bool recurring_to_confirm = false;
};

struct WorkRule {
    Code code;
    RuleType type = RuleType::MaxWork;
    RuleFamily family = RuleFamily::Work;
    RulePeriod period = RulePeriod::Workday;
    RestMeasure measure = RestMeasure::From24hStart;
    RuleParams certain;                    // lettura certa
    std::optional<RuleParams> prudent;     // lettura prudente (piu' severa), se la legge e' incerta
    std::string prudent_note;
    std::optional<Overtime> overtime;
    RuleSource source;
    bool to_confirm = false;               // senza lettura certa: solo avvisi
    std::string note;
    Source src;
};

// Valori di una condizione, certi o da confermare (es. i rapporti di lavoro a cui vale una legge).
template <class T>
struct Applies {
    bool present = false;            // la condizione c'e' (senza: vale per tutti)
    std::vector<T> certain, to_confirm;
};

// Persone trasportabili compreso il conducente (posti + posti in piedi + 1).
struct VehicleCondition {
    int persons_over = 0;
    bool to_confirm = false;
};

struct RouteCondition {
    std::optional<double> up_to_km, over_km;   // percorso della corsa fino a / oltre N km
    bool to_confirm = false;
};

// Corse escluse da un regime (es. servizi regolari di linea con percorso fino a 50 km, Reg. 561/2006 art. 3 a).
struct RegimeExclusion {
    std::vector<ServiceKind> kinds;
    // insieme_a (K-011 aggiunta 1): la voce vale per i suoi tipi solo se la corsa ha anche uno di questi tipi (es. i disabili
    // sulla stessa corsa dello scolastico: "chi sale in piu' non cambia il tipo di servizio"). Vuoto: sempre.
    std::vector<ServiceKind> together_with;
    std::optional<double> route_up_to_km;
    bool to_confirm = false;
    RuleSource source;
    std::string note;
    Source src;
};

// Come contano, per le regole di guida e di interruzione del regime, i minuti di guida delle corse che il regime esclude
// (o forse esclude: da confermare), es. Reg. 561/2006 art. 6 c. 5: "altre mansioni" (K-011 aggiunta 1). Nella lettura certa
// la categoria "certain", nella prudente "prudent"; le corse da confermare nella prudente sono sempre guida. Per il
// conteggio del lavoro restano lavoro come ogni altro minuto.
struct ExcludedDriving {
    TimeCategory certain = TimeCategory::OtherWork;
    TimeCategory prudent = TimeCategory::Driving;
    RuleSource source;
    std::string note;
    Source src;
};

enum class ExclusionEffect { Exclude, ToConfirm };   // l'altro regime non vale, oppure vale solo come "da confermare"

struct RegimeOverride {
    Ref regime;
    std::vector<RuleFamily> families;
    ExclusionEffect effect = ExclusionEffect::Exclude;
    Source src;
};

// Casi della legge che il programma non tratta (es. la multipresenza del Reg. 561/2006): se compaiono, un avviso.
enum class UntreatedCase { MultiManning };

struct UntreatedCaseEntry {
    UntreatedCase kind = UntreatedCase::MultiManning;
    RuleSource source;
    std::string note;
    Source src;
};

enum class RegimeKind { Law, CollectiveAgreement };   // legge, contratto collettivo

// Un regime: un insieme di regole di una fonte (una legge, un contratto collettivo), con dove si applica.
struct Regime {
    Code code;
    std::string name;
    RegimeKind kind = RegimeKind::Law;
    RuleSource source;
    std::optional<Day> valid_from, valid_to;
    bool to_confirm = false;
    std::string note;
    // Dove si applica. Condizioni sulla persona: ruolo del turno, rapporto, contratto collettivo. Condizioni sulla corsa:
    // tipo di servizio, mezzo, percorso, esclusioni (anche prese da un altro regime: come_regime).
    Applies<ShiftRole> roles;
    Applies<EmploymentKind> employments;
    Applies<std::string> agreements;
    Applies<ServiceKind> service_kinds;
    std::optional<VehicleCondition> vehicle;
    std::optional<RouteCondition> route;
    std::vector<RegimeExclusion> exclusions;
    std::optional<ExcludedDriving> excluded_driving;   // guida delle corse escluse (es. art. 6 c. 5 del 561)
    std::optional<Ref> like;   // come_regime: le condizioni sulla corsa e le esclusioni di un altro regime
    // Con come_regime e l'estensione alla settimana dell'altro regime: nei giorni della settimana senza le corse che lo
    // fanno valere, questo regime vale solo da confermare (di base: la legge non lo dice, K-011 aggiunta 1) o con certezza.
    bool like_extension_to_confirm = true;
    // Servizi occasionali (gite, noleggi): per le interruzioni che valgono solo per loro. Dal K-012 (norme versione 3) tipo
    // per tipo certi o da confermare (es. l'NCC certo, "altro" da confermare); la versione 2 aveva un solo "da confermare"
    // per tutto l'elenco, che si legge mettendo tutti i tipi tra i certi o tutti tra quelli da confermare.
    Applies<ServiceKind> occasional;
    RuleSource occasional_source;
    std::string occasional_note;
    // Estensione: una corsa che lo fa scattare vale per tutta la settimana, per queste famiglie.
    bool week_extension = false;
    std::vector<RuleFamily> extension_families;
    std::vector<RegimeOverride> overrides;   // altri regimi esclusi nei giorni (o nelle settimane) in cui vale
    std::vector<UntreatedCaseEntry> untreated;
    std::vector<WorkRule> rules;
    Source src;
};

// Cambio dell'ora (ora legale): un dato del paese, con la fonte (in Europa la direttiva 2000/84/CE). L'orologio salta
// dalle "from" alle "to" di quel giorno (es. 03:00 -> 02:00 l'ultima domenica di ottobre in Italia).
struct ClockChange {
    Day day = 0;
    Minutes from = 0, to = 0;
    Source src;
};

// Scelte dell'azienda o della persona sugli orari (livello 3): facoltative, preferenza oppure limite rigido.
enum class ChoiceStrength { Preference, Limit };

struct WorkChoice {
    RuleType type = RuleType::MaxWork;   // lavoro_massimo, guida_massima, ampiezza_massima, riposo_giornaliero, giorni_consecutivi
    RulePeriod period = RulePeriod::Workday;
    int value = 0;                       // minuti (o giorni per giorni_consecutivi)
    ChoiceStrength strength = ChoiceStrength::Preference;
    std::string note;
    Source src;
};

// La chiave della pausa fuori dal posto (rientro), decisa dal supervisore il 20/10: fermo minimo, fattore della guida a vuoto
// in piu', km a vuoto in piu' al massimo (facoltativo), forza. Ogni campo della persona vince su quello dell'azienda.
struct ReturnKey {
    std::optional<int> min_stop;          // minuti
    std::optional<double> driving_factor;
    std::optional<double> max_empty_km;
    std::optional<ChoiceStrength> strength;
    Source src;
};

// Speciali esigenze dell'azienda per una regola (es. lo straordinario della L. 138/1958 art. 3), con il riferimento.
struct SpecialNeed {
    Ref rule;
    std::string reference;
    std::string note;
    Source src;
};

// Giornate di lavoro prima del periodo del pacchetto (rapporto a chiamata): quante, contate da quando.
struct PriorDays {
    int days = 0;
    Day counted_from = 0;
    Source src;
};

// --- Norme di un paese: abilitazioni, usi dei mezzi, requisiti per guidare ----------------------------------
// Tutto e' un dato con la fonte: nessuna patente, nessun uso e nessun requisito e' scritto nel codice.

// Una voce di un elenco delle norme: un'abilitazione (es. patente D1, CQC persone) o un uso dei mezzi (es. servizio
// di linea, noleggio con conducente, uso proprio).
struct NormEntry {
    Code code;
    std::string name;
    std::optional<RuleSource> source;
    std::string note;
    Source src;
};

// Un modo per soddisfare un requisito: tutte queste abilitazioni insieme, se il mezzo rispetta i limiti indicati
// (es. patente D1 solo fino a 16 posti e 8 m di lunghezza).
struct LicenceAlternative {
    std::vector<Ref> licences;
    std::optional<int> max_seats;          // posti per i passeggeri
    std::optional<double> max_length_m;    // un mezzo senza lunghezza non rispetta il limite (prudenza)
    Source src;
};

// Requisito per guidare i mezzi di alcune categorie: vale se il mezzo e' di una delle categorie, ha i posti
// indicati, il rimorchio indicato e (se ci sono usi) uno degli usi. L'autista deve soddisfare una delle alternative.
// Valgono tutti i requisiti che corrispondono al mezzo (es. la patente per le dimensioni e la CQC per l'uso).
struct DrivingRequirement {
    std::vector<VehicleCategory> categories;
    std::optional<int> min_seats, max_seats;
    std::optional<bool> trailer;
    std::vector<Ref> uses;
    std::vector<LicenceAlternative> alternatives;
    RuleSource source;
    bool to_confirm = false;
    std::string note;
    Source src;
};

struct ServiceUsesNorm {
    ServiceUses value;
    RuleSource source;
    bool to_confirm = false;
    std::string note;
    Source src;
};

struct Norms {
    Code code;
    std::string name;
    std::string country;   // ISO 3166-1, es. "IT"
    std::vector<NormEntry> licences;
    std::vector<NormEntry> uses;
    std::vector<DrivingRequirement> requirements;
    std::vector<ServiceUsesNorm> service_uses;
    // Versione 2 (K-011): leggi e contratti collettivi sugli orari, cambi dell'ora.
    std::vector<Regime> regimes;
    std::vector<ClockChange> clock_changes;
    RuleSource clock_source;
    std::string note;
    Source src;

    const NormEntry* licence(const Code& c) const {
        for (const auto& e : licences)
            if (e.code == c) return &e;
        return nullptr;
    }
    const NormEntry* use(const Code& c) const {
        for (const auto& e : uses)
            if (e.code == c) return &e;
        return nullptr;
    }
};

// --- Calendari ----------------------------------------------------------------------

struct DayEntry {
    Day day = 0;
    Source src;
};

struct Suspension {
    DateRange range;
    std::string reason;   // es. "chiusura del centro"
    Source src;
};

// Calendario delle festivita' di un paese o di un territorio (es. le feste nazionali, il santo patrono di un
// comune), con la fonte. Nessuna festivita' e' scritta nel codice, nemmeno quelle mobili come Pasqua: le date
// sono dati.
struct Holiday {
    Day day = 0;
    std::string name;          // es. "Natale"
    bool to_confirm = false;   // da confermare
    std::string note;
    Source src;
};

struct HolidayCalendar {
    Code code;
    std::string name;
    std::string country;     // codice ISO 3166-1 del paese, es. "IT"
    std::string territory;   // vuoto = tutto il paese
    RuleSource source;       // documento, punto, riferimenti
    std::vector<Holiday> days;
    std::string note;
    Source src;

    bool contains(Day d) const {
        for (const auto& h : days)
            if (h.day == d) return true;
        return false;
    }
};

// Un calendario puo' aggiungere o togliere i giorni di altri calendari o di calendari delle festivita'.
enum class CalendarLinkKind { Calendar, Holidays };

struct CalendarLink {
    CalendarLinkKind kind = CalendarLinkKind::Calendar;
    Ref ref;
};

// Un calendario vale in un giorno se (in quest'ordine, il primo che decide vince):
// - il giorno non e' una data tolta e non cade in una sospensione (altrimenti no);
// - il giorno e' una data aggiunta (allora si');
// - il giorno sta nel periodo (altrimenti no);
// - il giorno non e' un giorno di un calendario tolto (removes; altrimenti no);
// - il giorno e' un giorno di un calendario aggiunto (adds; allora si');
// - il giorno e' uno dei giorni della settimana e, se c'e' una base, vale anche la base.
struct Calendar {
    Code code;
    std::string name;
    std::vector<ExternalCode> external;
    std::optional<Ref> base;   // calendario di partenza: vale solo nei giorni in cui vale anche la base
    DateRange range;
    Weekdays days;
    std::vector<DayEntry> added, removed;
    std::vector<Suspension> suspensions;
    std::vector<CalendarLink> adds, removes;   // giorni di altri calendari aggiunti e tolti
    Source src;
};

// --- Scuole e centri (prima "Destination") ---------------------------------------------

// Attesa concordata con la scuola: minuti accettati prima e dopo le lezioni.
struct AgreedWait {
    int before_min = 0;
    int after_min = 0;
    std::string agreement;   // riferimento dell'accordo
    Source src;
};

enum class AuthorizedTimeKind { Arrival, Pickup };   // arrivo, ripresa

struct AuthorizedTime {
    AuthorizedTimeKind kind = AuthorizedTimeKind::Arrival;
    Minutes time = 0;
    std::string agreement;
};

// Orario di una scuola o di un centro in un periodo e in alcuni giorni della settimana.
struct Timetable {
    DateRange period;
    Weekdays days;
    Minutes start = 0;   // inizio delle lezioni o apertura
    Minutes end = 0;     // fine delle lezioni o chiusura
    std::optional<Minutes> arrive_by;   // ora di arrivo richiesta (es. centro: entro le 09:00)
    std::optional<AgreedWait> wait;
    std::vector<AuthorizedTime> authorized;
    Source src;
};

enum class InstituteKind { School, Center };

struct School {
    Code code;
    std::string name;
    Ref place;
    InstituteKind kind = InstituteKind::School;
    std::optional<Grade> grade;              // solo scuole
    std::optional<CenterType> center_type;   // solo centri
    std::string center_type_description;
    std::vector<ExternalCode> external;
    std::vector<Timetable> timetables;
    std::optional<int> max_ride_min;              // durata massima a bordo
    std::optional<Minutes> earliest_boarding;     // ora piu' presto di salita
    Source src;
};

// --- Committenti, contratti, gruppi di servizi -----------------------------------------

// Committente: un ente (scuola, Comune, agenzia...: dati comuni) oppure, dal K-012, un privato (una persona: nome e nota sono
// dati personali, nella cassaforte; legati a un luogo sensibile come quelli dei passeggeri, riservatezza.hpp).
struct Client {
    Code code;
    std::string name;   // per un privato puo' mancare (pacchetto "solo numeri", o un utente senza il permesso)
    std::vector<ExternalCode> external;
    bool is_private = false;   // committenti_privati (committenti.json versione 2)
    std::string note;          // la nota di un privato (dato personale); quella di un ente non serve al calcolo
    Source src;
};

// Regole del committente sul mezzo (nel contratto, oppure nel servizio se diverse).
struct VehicleRules {
    std::optional<int> min_seats;          // mezzo minimo: posti per i passeggeri
    std::optional<int> max_seats;          // mezzo massimo ammesso
    std::optional<bool> ramp_required;     // pedana obbligatoria
    std::optional<int> max_ride_min;       // durata massima a bordo
    std::string recognized_category;       // categoria riconosciuta: quella richiesta, anche con un mezzo piu' grande
    Source src;
};

// Chi fornisce e paga l'assistente: lo stabilisce il contratto.
enum class EscortProvider {
    Client,     // Provincia, Comune, associazione: per l'azienda occupa solo un posto
    Company     // l'azienda: va pianificato e pagato come l'autista
};

// Contratto con un committente: le regole che valgono per i suoi servizi.
struct Contract {
    Code code;
    std::string name;
    Ref client;
    Ref company;
    Ref regulation;   // regolamento che vale per i suoi servizi
    std::optional<Day> valid_from, valid_to;
    EscortProvider escort_provider = EscortProvider::Client;
    VehicleRules rules;
    std::vector<ExternalCode> external;
    Source src;
};

// Gruppo di servizi affidati insieme (es. un "lotto").
struct ServiceLot {
    Code code;
    std::string name;
    Ref contract;
    std::vector<Ref> services;
    Source src;
};

// --- Servizi e domanda ------------------------------------------------------------------

enum class Direction { Outbound, Return };            // andata verso la scuola o il centro; ritorno
enum class CountKind { Exact, Estimated, Unknown };    // conteggi esatti, stimati, non noti (linee)

// Persone che salgono e scendono a una fermata secondo la domanda.
struct DemandStop {
    Ref place;
    int boarding = 0;
    int alighting = 0;
    int wheelchairs = 0;   // persone che salgono qui e viaggiano sedute sulla carrozzina (dato particolare)
    int folded_aids = 0;   // ausili piegati da caricare qui, carrozzine piegate comprese (dato particolare)
    Source src;
};

enum class PassengerStatus { Active, Suspended, Unlocked };   // attivo, sospeso, sbloccato dal committente
enum class Assignment { Proposed, Locked };   // proposta dal programma; scelta a mano e bloccata

struct StopAssignment {
    Ref place;
    Assignment mode = Assignment::Proposed;
};

// Come viaggia una persona con la carrozzina: seduta sulla carrozzina (servono pedana e posto carrozzina)
// oppure su un posto normale, con la carrozzina piegata da caricare (un ausilio piegato).
enum class WheelchairUse { None, Seated, Folded };

struct Needs {   // dati particolari
    WheelchairUse wheelchair = WheelchairUse::None;
    int folded_aids = 0;     // altri ausili dichiarati che viaggiano piegati (es. deambulatore)
    bool assistant = false;
    std::string other;

    // Ausili piegati da caricare: la carrozzina piegata e gli altri ausili.
    int folded_items() const { return (wheelchair == WheelchairUse::Folded ? 1 : 0) + folded_aids; }
};

struct Home {   // dati personali
    std::string address;
    std::string municipality;
    std::optional<Coord> coord;
};

// Un passeggero della domanda (solo nel livello "completo").
struct Passenger {
    Code code;
    std::string name;   // dato personale
    Home home;
    PassengerStatus status = PassengerStatus::Active;
    std::string status_reference;
    std::optional<StopAssignment> boarding, alighting;
    std::string reason;   // motivo scritto a mano (dato personale)
    Needs needs;
    Source src;

    bool travels() const { return status != PassengerStatus::Suspended; }
};

// Chi deve andare da dove a dove ed entro quando: un gruppo della domanda di un servizio.
struct PassengerGroup {
    Code code;
    std::string name;
    GroupType type = GroupType::Students;
    std::string type_description;
    Direction direction = Direction::Outbound;
    std::optional<Ref> school;   // scuola o centro di arrivo (andata) o di partenza (ritorno)
    std::optional<TimeWindow> window;
    CountKind counts = CountKind::Exact;
    std::vector<DemandStop> stops;
    bool assistant_required = false;   // dato particolare
    std::vector<Passenger> passengers;
    Source src;

    int total_boarding() const {
        int n = 0;
        for (const auto& s : stops) n += s.boarding;
        return n;
    }
    int total_alighting() const {
        int n = 0;
        for (const auto& s : stops) n += s.alighting;
        return n;
    }
};

enum class TimeFreedom { Fixed, Free };   // orari del committente; liberi dentro il regolamento

// Il lavoro che un committente affida con un contratto: si paga, si perde, si rinuncia, si scambia.
struct Service {
    Code code;
    std::string name;
    std::vector<ExternalCode> external;
    ServiceKind kind = ServiceKind::School;
    std::string kind_description;
    Ref contract;
    Ref calendar;
    TimeFreedom freedom = TimeFreedom::Fixed;
    std::optional<int> agree_threshold_min;   // oltre questo spostamento un cambio d'orario e' "da concordare"
    VehicleRules rules;                       // proprie del servizio, se diverse dal contratto
    std::vector<PassengerGroup> groups;       // la domanda
    std::vector<Ref> competences;             // competenze richieste a chi lo fa (versione 2; dato particolare)
    Source src;
};

// --- Corse (prima "Route") e passaggi (prima "StopVisit") --------------------------------

enum class TransferAction { Deliver, Receive };   // consegna, ricezione

// Meta' di un trasbordo: la consegna su una corsa o la ricezione su un'altra, con lo stesso codice.
struct TransferHalf {
    Code code;
    TransferAction action = TransferAction::Deliver;
    Ref group;
    int persons = 0;
    std::optional<Ref> passenger;
    Source src;
};

// Persone di un gruppo che salgono e scendono in un passaggio (come "St. Car." e "St. Scar." sulle schede).
struct PassageCount {
    Ref group;
    int boarding = 0;
    int alighting = 0;
    Source src;
};

enum class AssistantMove { None, Boards, Alights };

struct Passage {
    Ref place;
    Minutes time = 0;                   // arrivo nel luogo
    std::optional<Minutes> departure;   // partenza, se il mezzo aspetta
    bool transit = false;               // passa senza fermarsi
    std::vector<PassageCount> persons;
    AssistantMove assistant = AssistantMove::None;
    std::vector<TransferHalf> transfers;
    std::optional<double> loaded_km, empty_km;   // km dal passaggio precedente, come li scrive il committente
    Source src;

    Minutes leaves() const { return departure ? *departure : time; }
};

enum class ConnectionKind { WaitArrival, GuaranteeDeparture };   // aspettare l'arrivo; garantire la partenza

// Coincidenza con un servizio esterno (es. "treno FTM n. 3").
struct Connection {
    Ref place;
    std::string external_service;
    Minutes time = 0;   // arrivo (aspettare l'arrivo) o partenza (garantire la partenza) del servizio esterno
    ConnectionKind kind = ConnectionKind::WaitArrival;
    Source src;
};

// Corsa: il viaggio vero di un mezzo. Puo' soddisfare piu' servizi (allora e' promiscua).
struct Trip {
    Code code;
    std::string name;
    std::vector<ExternalCode> external;
    std::vector<Ref> services;
    Ref calendar;
    std::vector<Passage> passages;
    std::optional<double> total_loaded_km, total_empty_km;   // totali della scheda del committente
    std::vector<Connection> connections;
    std::string display_text;   // testo da esporre sul mezzo
    Source src;

    Minutes start() const { return passages.empty() ? 0 : passages.front().time; }
    Minutes end() const { return passages.empty() ? 0 : passages.back().time; }
    // Km a pieno e a vuoto: i totali della scheda; senza totali, la somma dei tratti.
    double loaded_km() const {
        if (total_loaded_km) return *total_loaded_km;
        double t = 0;
        for (const auto& p : passages) t += p.loaded_km.value_or(0.0);
        return t;
    }
    double empty_km() const {
        if (total_empty_km) return *total_empty_km;
        double t = 0;
        for (const auto& p : passages) t += p.empty_km.value_or(0.0);
        return t;
    }
};

// --- Mezzi e rimesse ---------------------------------------------------------------------

enum class Fuel { Diesel, Petrol, Methane, Lpg, Hybrid, Electric, Hydrogen, Other };

struct Dimensions {
    std::optional<double> length_m, width_m, height_m, mass_t, rear_overhang_m;
};

struct Power {
    Fuel fuel = Fuel::Diesel;
    std::string description;
    std::optional<double> battery_kwh;          // elettrici: capacita' della batteria
    std::optional<double> consumption_kwh_km;   // elettrici: consumo medio
    std::optional<int> min_charge_percent;      // elettrici: carica minima
};

// Persona o mezzo prestato a un'altra azienda (subaffidamento, consorzio, noleggio con conducente): ammesso nei
// turni di quell'azienda solo se dichiarato, con il riferimento dell'accordo.
struct Loan {
    Ref company;             // azienda che lo riceve
    std::string agreement;   // riferimento dell'accordo
    std::optional<Day> from, to;
    Source src;

    bool covers(Day d) const { return (!from || d >= *from) && (!to || d <= *to); }
};

// Autorizzazione a un uso in piu' rispetto alla carta di circolazione (in Italia la "distrazione": es. un mezzo di
// linea che fa noleggio), per singolo mezzo e con le date. Chi la rilascia e' un dato.
struct UseAuthorization {
    Ref use;                  // l'uso autorizzato
    std::string authority;    // ente che la rilascia
    std::string reference;    // riferimento del documento
    DateRange period;
    std::string prescriptions;
    Source src;
};

// Vincolo di destinazione: il mezzo fa solo certi servizi (per tipo o per contratto), fino a una data.
struct DestinationConstraint {
    std::vector<ServiceKind> kinds;
    std::vector<Ref> contracts;
    std::optional<Day> until;
    std::string source;   // da dove viene il vincolo (es. il contributo pubblico)
    std::string note;
    Source src;
};

struct Vehicle {
    Code code;
    Ref company;
    std::string plate;
    std::vector<ExternalCode> external;
    VehicleCategory category = VehicleCategory::M2;
    int seats = 0;                      // posti per i passeggeri, conducente escluso (sul libretto S.1 lo comprende)
    int standing = 0;                   // posti in piedi, solo se previsti dal libretto (S.2)
    int wheelchair_places = 0;          // carrozzine trasportabili al massimo
    int seats_lost_per_wheelchair = 0;  // posti a sedere persi per ogni carrozzina caricata
    bool has_ramp = false;              // pedana o sollevatore
    int ramp_seconds = 0;               // tempo per azionare la pedana a ogni fermata
    int aids_in_trunk = 0;              // ausili piegati che entrano nel bagagliaio (0 = niente bagagliaio)
    // Posti occupati da un ausilio piegato fissato tra i sedili (cinghie o ancoraggi): scelta facoltativa
    // dell'azienda. Nessun valore = tra i sedili non e' ammesso.
    std::optional<int> seats_per_aid_between;
    bool trailer = false;
    Dimensions dims;
    Power power;
    std::optional<double> daily_fixed_cost;   // nella valuta del pacchetto: assicurazione, bollo, ammortamento
    std::optional<double> cost_per_km;
    bool hypothetical = false;          // non ancora in flotta (scenari)
    // Versione 2 di mezzi.json.
    std::optional<Ref> use;                       // uso della carta di circolazione (norme del paese)
    std::vector<UseAuthorization> use_authorizations;
    std::optional<DestinationConstraint> constraint;
    bool spare = false;                           // mezzo di scorta: non usarlo non e' un avviso
    std::vector<Loan> loans;
    Source src;
};

// Rimessa o ricovero di un'azienda (prima "ParkingSpot").
struct Depot {
    Code code;
    Ref place;
    Ref company;
    bool charging = false;            // ricarica per i mezzi elettrici
    std::optional<int> capacity;      // quanti mezzi ci stanno
    Source src;
};

// Chi e che cosa deve stare sul mezzo.
struct Load {
    int seated = 0;        // passeggeri sui posti a sedere (anche chi ha la carrozzina piegata)
    int wheelchairs = 0;   // passeggeri seduti sulla propria carrozzina
    int folded_aids = 0;   // ausili piegati da caricare: carrozzine piegate, deambulatori...
    bool escort = false;   // l'assistente occupa un posto
};

// Esito: ci sta o no, dove vanno gli ausili piegati, quanti posti a sedere restano liberi.
struct Fit {
    bool admitted = false;
    int aids_in_trunk = 0;
    int aids_between_seats = 0;
    int free_seats = 0;   // posti a sedere ancora liberi; negativo = mancano posti
    explicit operator bool() const { return admitted; }
};

// Ci sta questo carico sul mezzo?
// - chi viaggia seduto sulla carrozzina ha bisogno della pedana e di un posto carrozzina, e toglie posti a sedere;
// - l'assistente occupa un posto;
// - gli ausili piegati vanno prima nel bagagliaio; quelli che non ci stanno vanno tra i sedili, legati e
//   bloccati, solo se il mezzo lo ammette e se i posti che occupano sono liberi; altrimenti non e' ammesso.
// Si contano gli ausili, non il loro volume. Nessun valore nel codice: tutto viene dal mezzo.
inline Fit fits(const Vehicle& v, const Load& load) {
    Fit f;
    f.aids_in_trunk = std::min(load.folded_aids, std::max(v.aids_in_trunk, 0));
    const int other_aids = load.folded_aids - f.aids_in_trunk;
    const bool between_allowed = v.seats_per_aid_between.has_value();
    if (between_allowed) f.aids_between_seats = other_aids;
    const int seats_for_aids = between_allowed ? other_aids * *v.seats_per_aid_between : 0;
    f.free_seats = v.seats - load.wheelchairs * v.seats_lost_per_wheelchair - (load.escort ? 1 : 0) - seats_for_aids -
                   load.seated;
    const bool wheelchairs_ok = load.wheelchairs == 0 || (v.has_ramp && load.wheelchairs <= v.wheelchair_places);
    const bool aids_ok = other_aids == 0 || between_allowed;
    f.admitted = wheelchairs_ok && aids_ok && f.free_seats >= 0;
    return f;
}

// Costo di una giornata del mezzo: quota fissa + km percorsi. Nessun valore se manca uno dei due costi.
inline std::optional<double> day_cost(const Vehicle& v, double total_km) {
    if (!v.daily_fixed_cost || !v.cost_per_km) return std::nullopt;
    return *v.daily_fixed_cost + total_km * *v.cost_per_km;
}

// Quanti giri servono per portare tutti con un mezzo (solo posti a sedere).
inline int trips_needed(const Vehicle& v, int passengers) {
    return v.seats > 0 ? (passengers + v.seats - 1) / v.seats : 0;
}

// --- Tempi di sosta (facoltativi finche' non confermati) -----------------------------------

enum class DwellKind { Student, Kindergarten, Disabled };   // studente, infanzia, disabile

struct PassengerDwell {
    DwellKind kind = DwellKind::Student;
    int boarding_s = 0;    // secondi per ogni passeggero che sale
    int alighting_s = 0;   // secondi per ogni passeggero che scende
};

struct DwellTimes {
    std::optional<int> per_stop_s;   // parte fissa per fermata
    std::vector<PassengerDwell> per_passenger;
    std::optional<int> school_unload_s;   // scarico a scuola
    Source src;
};

// --- Persone (autisti e assistenti a bordo) -------------------------------------------------------------
// Codice, azienda, presa servizio, mezzo abituale, ipotetica e prestiti sono dati comuni; il resto e' personale.
// Minimizzazione: nessun campo per codice fiscale, telefoni, mail, data di nascita, idoneita' medica.

enum class PersonRole { Driver, Escort };   // autista, assistente a bordo


struct HoursLimits {   // ore (con decimali) per giorno, settimana, mese
    std::optional<double> day, week, month;
};

struct DatedHours {
    std::optional<Day> from, to;
    std::optional<HoursLimits> planned, max;
    Source src;
};

// Fascia di disponibilita': nei giorni indicati, dalle alle.
struct Availability {
    std::optional<Day> from, to;
    Weekdays days;
    Minutes start = 0, end = 0;
    Source src;

    bool covers_date(Day d) const { return (!from || d >= *from) && (!to || d <= *to); }
};

struct HourlyCost {
    std::optional<Day> from, to;
    double value = 0.0;
    Source src;
};

// Abilitazione (patente, CQC...): il tipo e' un codice delle norme del paese.
struct Licence {
    Ref type;
    std::optional<Day> expiry;   // ultimo giorno di validita'
    std::string reference;
    Source src;

    bool valid_on(Day d) const { return !expiry || d <= *expiry; }
};

// Autorizzazione del committente (es. cartellino di riconoscimento per i servizi speciali).
struct ClientAuthorization {
    Ref client;
    std::string reference;
    std::optional<Day> expiry;
    Source src;

    bool valid_on(Day d) const { return !expiry || d <= *expiry; }
};

// Preferenza della persona: dato morbido (avviso se non rispettata), distinto dalla disponibilita' (rigida).
enum class PreferenceKind { FreeDays, TimeWindow, AvoidShifts, MaxBreak, Other };

struct Preference {
    PreferenceKind kind = PreferenceKind::Other;
    std::optional<Day> from, to;
    Weekdays days;                 // giorni liberi
    Minutes start = 0, end = 0;    // fascia oraria
    std::vector<Ref> shifts;       // turni da evitare
    int max_break_min = 0;         // pausa massima dentro un turno
    std::string description;
    Source src;

    bool covers_date(Day d) const { return (!from || d >= *from) && (!to || d <= *to); }
};

struct Person {
    Code code;
    Ref company;
    std::vector<PersonRole> roles;
    std::string name;                 // personale
    std::optional<Home> home;         // personale
    std::optional<Ref> base_place;    // rimessa o luogo abituale di presa servizio
    std::optional<Ref> usual_vehicle; // mezzo abituale
    bool hypothetical = false;        // non ancora assunta (scenari)
    EmploymentKind employment = EmploymentKind::Other;
    std::string employment_description;
    std::string agreement_code, agreement_name;   // contratto collettivo
    std::vector<DatedHours> hours;
    std::vector<Availability> availability;
    std::vector<HourlyCost> hourly_cost;
    std::vector<Licence> licences;
    std::vector<ClientAuthorization> authorizations;
    std::vector<Ref> competences;
    std::vector<Preference> preferences;
    std::vector<Loan> loans;
    // Versione 2 (K-011): scelte sugli orari per la persona (vincono su quelle dell'azienda), chiave del rientro, giornate
    // di lavoro prima del pacchetto (dati personali).
    std::vector<WorkChoice> choices;
    std::optional<ReturnKey> return_key;
    std::optional<PriorDays> prior_days;
    std::string note;                 // personale
    Source src;

    bool has_role(PersonRole r) const { return std::find(roles.begin(), roles.end(), r) != roles.end(); }
    bool has_competence(const Code& c) const {
        for (const auto& k : competences)
            if (k.code == c) return true;
        return false;
    }
};

// Assenza programmata: solo il tipo (elenco dell'azienda), mai la causa.
struct Absence {
    Ref person;
    Ref type;
    DateRange days;
    std::optional<Minutes> start, end;   // solo una parte di ogni giorno
    Source src;
};

// Fermo programmato di un mezzo (es. revisione).
struct VehicleStop {
    Ref vehicle;
    Ref type;
    DateRange days;
    std::optional<Minutes> start, end;
    std::string note;
    Source src;
};

// --- Turni del giorno, rotazioni, servizi fissi, assegnazioni del singolo giorno, blocchi -----------------
// Il turno del giorno e' l'unita' di base: il lavoro di una persona in un giorno. Chi lo fa lo dicono tre modi, dal
// piu' generale al piu' preciso (vince il piu' preciso, turno per turno): rotazione, servizio fisso, assegnazione del
// singolo giorno. Se nessuno da' una persona il turno e' scoperto: il programma non inventa assegnazioni.


// Mezzo del turno: preciso, il mezzo abituale dell'autista, da assegnare, nessuno.
enum class ShiftVehicleMode { Specific, Usual, ToAssign, None };

// Passaggio di una corsa che segna l'inizio o la fine di un pezzo: luogo e, se la corsa ci passa piu' volte, orario.
struct TripPoint {
    Ref place;
    std::optional<Minutes> time;
    Source src;
};

// Una corsa del turno, intera o un pezzo (cambio dell'autista lungo la corsa in un punto di cambio).
struct ShiftTrip {
    Ref trip;
    std::optional<TripPoint> from, to;
    std::optional<EscortProvider> escort;   // chi fornisce l'assistente su questa corsa, se diverso dal contratto
};

// Attivita' senza passeggeri (tipo dall'elenco dell'azienda): luogo, orari, eventuali km a vuoto scritti a mano.
struct ShiftActivity {
    Ref type;
    Ref place;
    std::optional<Ref> to_place;
    Minutes start = 0, end = 0;
    std::optional<double> empty_km;
    std::optional<Ref> calendar;   // giorni in cui c'e' l'attivita' (dentro quelli del turno)
    std::string note;
};

struct ShiftItem {
    bool is_trip = true;
    ShiftTrip trip;
    ShiftActivity activity;
    Source src;
};

struct Shift {
    Code code;
    std::string name;   // nome dato dall'azienda (es. "Primo")
    Ref company;
    ShiftRole role = ShiftRole::Driver;
    Ref calendar;
    ShiftVehicleMode vehicle_mode = ShiftVehicleMode::None;
    std::optional<Ref> vehicle;   // solo con il mezzo preciso
    Source vehicle_src;           // dove e' scritto il mezzo del turno
    Ref start_place, end_place;
    std::vector<ShiftItem> items;
    std::vector<Ref> competences;   // competenze richieste (dato particolare)
    std::string note;
    Source src;
};

enum class RotationUnit { Day, Month };   // passi di un giorno o di un mese di calendario

struct RotationStep {
    bool rest = false;            // "riposo"
    std::vector<Ref> shifts;      // turni del passo
    Source src;
};

struct RotationRow {
    Ref person;
    std::vector<RotationStep> steps;
    Source src;
};

struct Rotation {
    Code code;
    std::string name;
    RotationUnit unit = RotationUnit::Day;
    int length = 1;                 // passi del ciclo
    Day start = 0;                  // giorno del primo passo
    std::optional<Day> from, to;    // validita' (senza dal: dalla partenza; senza al: fino alla fine del pacchetto)
    std::vector<RotationRow> rows;
    std::string note;
    Source src;

    // Passo del ciclo (da 0 a length - 1) nel giorno d: il ciclo si conta dalla partenza anche prima e dopo.
    int step_index(Day d) const {
        long long k = 0;
        if (unit == RotationUnit::Day) {
            k = static_cast<long long>(d) - start;
        } else {
            int y1 = 0, m1 = 0, d1 = 0, y2 = 0, m2 = 0, d2 = 0;
            ymd_from_day(start, y1, m1, d1);
            ymd_from_day(d, y2, m2, d2);
            k = (static_cast<long long>(y2) * 12 + m2) - (static_cast<long long>(y1) * 12 + m1);
        }
        const long long n = length > 0 ? length : 1;
        return static_cast<int>(((k % n) + n) % n);
    }
    bool valid_on(Day d, Day package_end) const { return d >= from.value_or(start) && d <= to.value_or(package_end); }
};

// Servizio fisso: un turno sempre alla stessa persona, nei giorni del suo calendario.
struct FixedAssignment {
    Code code;
    Ref shift;
    Ref person;
    Ref calendar;
    std::string note;   // dato personale
    Source src;
};

// Assegnazione del singolo giorno: la persona, il mezzo o tutti e due.
struct DayAssignment {
    Day date = 0;
    Ref shift;
    std::optional<Ref> person, vehicle;
    std::string note;   // dato personale
    Source src;
};

enum class BlockStrength { VehicleFixed, AllFixed };   // mezzo fisso, tutto fisso

// Blocco: vincolo per il calcolo futuro (le corse restano a quel mezzo).
struct Block {
    Code code;
    Ref vehicle;
    std::vector<Ref> trips;
    std::optional<Day> from, to;
    BlockStrength strength = BlockStrength::VehicleFixed;
    std::string note;
    Source src;

    bool valid_on(Day d) const { return (!from || d >= *from) && (!to || d <= *to); }
};

// --- Pacchetto dati ------------------------------------------------------------------------

struct Company {
    Code code;
    std::string name;
    Source src;
};

// --- Elenchi e impostazioni di un'azienda ---------------------------------------------------------------------
// L'organizzazione la sceglie l'azienda: i tipi di attivita', di assenza, di fermo dei mezzi e le competenze sono
// elenchi nei dati, con i nomi dell'azienda. Nel codice c'e' solo cio' che serve a un controllo.

enum class ShelterNearHome { Preferred, Allowed, Never };     // ricovero dei mezzi vicino a casa dell'autista
enum class DriverVehiclePairing { None, Preferred, Fixed };   // abbinamento autista-mezzo

struct ActivityType {
    Code code;
    std::string name;
    bool uses_vehicle = true;   // l'attivita' impegna il mezzo del turno (es. pulizia si', spostamento a piedi no)
    std::optional<TimeCategory> time;   // categoria di tempo (aziende.json versione 2); senza: presunta, con un avviso
    std::string note;
    Source src;
};

struct ListEntry {
    Code code;
    std::string name;
    std::string note;
    Source src;
};

// --- Modo delle corse: OUT o tachigrafo (K-011 aggiunta 1, supervisore 28/10) ---------------------------------------------
// L'azienda sceglie, per tipo di servizio, per servizio o per corsa, se una corsa si fa "out" (con le regole nazionali, dove
// la legge lo permette) oppure "tachigrafo" (seguendo le regole del regime che la legge esclude, es. il Reg. 561/2006, anche
// dove la legge non lo chiede). Il programma non impone un modo: la legge resta sopra e il programma dice cosa si rischia.
enum class TripModeKind { Out, Tachograph };
// Corsa con servizi di tipi (o servizi) con scelte diverse: decide l'azienda; di base la scelta piu' prudente (tachigrafo).
enum class MixedModes { MostPrudent, Out, Tachograph };

// Chi ha scritto una forzatura o una dichiarazione, quando e perche'. Il motivo (scritto a mano) e chi sono dati personali.
struct ModeJustification {
    std::string reason;
    std::string who;
    Day when = 0;
    std::optional<Day> until;   // forzatura: vale fino a questo giorno (incluso), facoltativo
    Source src;
};

struct ModeEntry {
    TripModeKind mode = TripModeKind::Out;
    std::optional<ServiceKind> kind;   // per tipo di servizio
    std::optional<Ref> service;        // per servizio
    std::optional<Ref> trip;           // per corsa
    std::optional<ChoiceStrength> strength;   // tachigrafo: preferenza (avviso) o limite (errore su un turno ricorrente)
    // Solo con "out" su un servizio o una corsa: forzatura (dove la legge, con i dati scritti, vuole il regime) oppure
    // dichiarazione (dove l'esclusione e' da confermare: da li' esclusa con certezza).
    std::optional<ModeJustification> forcing, declaration;
    std::string note;
    Source src;
};

struct TripModes {
    Ref regime;                          // il regime che il tachigrafo segue (es. R-561); vale anche per chi lo segue (come_regime)
    std::vector<ModeEntry> by_kind, by_service, by_trip;
    MixedModes mixed = MixedModes::MostPrudent;
    ChoiceStrength strength = ChoiceStrength::Preference;   // forza di base del tachigrafo
    std::string note;
    Source src;
};

struct CompanySettings {
    Ref company;
    std::optional<Ref> norms;   // norme del paese che valgono per l'azienda
    ShelterNearHome shelter = ShelterNearHome::Allowed;
    DriverVehiclePairing pairing = DriverVehiclePairing::None;
    std::vector<ActivityType> activities;
    std::vector<ListEntry> absence_types, stop_types, competences;
    // Versione 2 (K-011): orari di lavoro.
    std::optional<UnwrittenStops> unwritten;   // obbligatorio nella versione 2; senza (versione 1): presunto, con un avviso
    std::vector<WorkChoice> choices;           // scelte dell'azienda sugli orari
    std::optional<ReturnKey> return_key;       // chiave della pausa fuori dal posto (rientro)
    std::vector<SpecialNeed> special_needs;
    std::optional<int> days_margin;            // avviso quando mancano tante giornate al limite (giornate_massime)
    std::optional<TripModes> trip_modes;       // OUT o tachigrafo (K-011 aggiunta 1)
    std::string note;
    Source src;

    template <class T>
    static const T* find_in(const std::vector<T>& v, const Code& c) {
        for (const auto& e : v)
            if (e.code == c) return &e;
        return nullptr;
    }
};

enum class Level { NumbersOnly, Full };                      // "solo numeri", "completo"
enum class Origin { Manual, Computed, Imported, Scenario };  // rilevato a mano, calcolato, importato, scenario

// Manifesto del pacchetto: chi, quando, da dove viene, che livello di dati contiene.
struct Manifest {
    Code code;
    std::string name;
    bool invented = false;   // dati inventati (esempi, prove)
    std::vector<Company> companies;
    DateRange period;
    Origin origin = Origin::Manual;
    std::string derived_from;   // codice del pacchetto da cui deriva
    std::string origin_note;
    Level level = Level::NumbersOnly;
    std::string timezone;   // nome IANA, es. "Europe/Rome": gli orari sono orari locali di questo fuso
    std::string currency;   // codice ISO 4217, es. "EUR": i costi sono in questa valuta
    int transfer_margin_min = 0;   // margine dei trasbordi: la corsa che riceve arriva prima di questo
    // Manifesto versione 2 (K-010). Il porta a porta lascia capire un dato sulla salute (scelta di chi scrive i dati).
    bool door_to_door_sensitive = false;
    // La cassaforte dei dati personali: codice (32 cifre esadecimali) e generazione dei dati; vuoto = nessuna cassaforte.
    std::string vault_id;
    int vault_generation = 0;
    bool has_vault() const { return !vault_id.empty(); }
    Source src;
};

// Tutti i dati di un pacchetto. I luoghi privati (case) stanno insieme ai pubblici, segnati da is_private.
struct Package {
    Manifest manifest;
    std::vector<CompanySettings> company_settings;
    std::vector<Norms> norms;
    std::vector<Place> places;
    std::vector<RoadSegment> segments;
    std::vector<Regulation> regulations;
    std::vector<HolidayCalendar> holidays;
    std::vector<Calendar> calendars;
    std::vector<School> schools;
    std::vector<Client> clients;
    std::vector<Contract> contracts;
    std::vector<ServiceLot> lots;
    std::vector<Service> services;
    std::vector<Trip> trips;
    std::vector<Vehicle> vehicles;
    std::vector<Depot> depots;
    std::optional<DwellTimes> dwell;
    std::vector<Person> persons;
    std::vector<Absence> absences;
    std::vector<VehicleStop> stops;
    std::vector<Shift> shifts;
    std::vector<Rotation> rotations;
    std::vector<FixedAssignment> fixed;
    std::vector<DayAssignment> day_assignments;
    std::vector<Block> blocks;
};

// --- Regole del mestiere sulle corse --------------------------------------------------------

// Categoria della corsa ricavata da chi trasporta e dove lo porta:
// - piu' servizi sulla stessa corsa, anche se uno e' NCC    -> Promiscuo
// - un solo servizio NCC                                   -> NCC: i gruppi non cambiano la categoria (una gita
//                                                             scolastica porta studenti ma e' NCC; K-012)
// - gruppi di tipi diversi (studenti, disabili, utenti
//   di linea, altro; gli adulti autorizzati non contano)    -> Promiscuo
// - studenti verso un centro (non una scuola)              -> Promiscuo
// - solo utenti di linea / disabili / studenti / altro     -> Linea / Handicap / Scolastico / Altro
// I gruppi sono quelli che salgono o scendono nei passaggi; se la corsa non ne nomina nessuno valgono
// i gruppi del suo servizio (anche anonimi, anche con conteggi non noti), e senza gruppi il tipo dichiarato
// dal committente. Se la corsa non elenca i servizi (un errore nei dati, K-CTR-034) i servizi sono quelli dei suoi gruppi,
// solo per riconoscere l'NCC: i gruppi di un solo servizio NCC = NCC; di piu' servizi, uno NCC = Promiscuo; altrimenti i
// tipi dei gruppi, come sempre. Nessun valore se la corsa non ha ne' servizi ne' gruppi.
template <class FindService, class FindGroup, class FindSchool, class FindGroupService>
std::optional<ServiceType> derive_service(const Trip& t, FindService find_service, FindGroup find_group,
                                          FindSchool find_school, FindGroupService find_group_service) {
    std::set<Code> services;
    for (const auto& s : t.services) services.insert(s.code);
    if (services.size() > 1) return ServiceType::Mixed;

    std::set<Code> group_codes;
    for (const auto& p : t.passages) {
        for (const auto& c : p.persons) group_codes.insert(c.group.code);
        for (const auto& h : p.transfers) group_codes.insert(h.group.code);
    }
    const Service* service = services.empty() ? nullptr : find_service(*services.begin());
    if (service && service->kind == ServiceKind::Ncc) return ServiceType::Ncc;
    if (services.empty()) {
        std::set<Code> of_groups;
        bool ncc = false;
        for (const auto& g : group_codes)
            if (const Service* s = find_group_service(g)) {
                of_groups.insert(s->code);
                ncc = ncc || s->kind == ServiceKind::Ncc;
            }
        if (ncc) return of_groups.size() > 1 ? ServiceType::Mixed : ServiceType::Ncc;
    }
    std::vector<const PassengerGroup*> groups;
    for (const auto& g : group_codes)
        if (const PassengerGroup* pg = find_group(g)) groups.push_back(pg);
    if (groups.empty() && service)
        for (const auto& g : service->groups) groups.push_back(&g);

    bool students = false, disabled = false, line = false, other = false, students_to_center = false;
    for (const PassengerGroup* g : groups) {
        switch (g->type) {
            case GroupType::Students: {
                students = true;
                const School* s = g->school ? find_school(g->school->code) : nullptr;
                if (s && s->kind == InstituteKind::Center) students_to_center = true;
                break;
            }
            case GroupType::Disabled: disabled = true; break;
            case GroupType::LineUsers: line = true; break;
            case GroupType::Other: other = true; break;
            case GroupType::AuthorizedAdults: break;   // viaggiano con l'autorizzazione del servizio
        }
    }
    const int kinds = int(students) + int(disabled) + int(line) + int(other);
    if (kinds > 1 || students_to_center) return ServiceType::Mixed;
    if (disabled) return ServiceType::Disability;
    if (students) return ServiceType::School;
    if (line) return ServiceType::Line;
    if (other) return ServiceType::Other;
    if (!service) return std::nullopt;
    switch (service->kind) {
        case ServiceKind::Line: return ServiceType::Line;
        case ServiceKind::School: return ServiceType::School;
        case ServiceKind::Disability: return ServiceType::Disability;
        case ServiceKind::Ncc: return ServiceType::Ncc;
        case ServiceKind::Other: return ServiceType::Other;
    }
    return std::nullopt;
}

// Chi viaggia insieme su una corsa: serve alle regole dei regolamenti.
struct OnBoard {
    std::vector<Grade> grades;
    std::vector<GroupType> group_types;
    std::vector<Need> needs;
    std::vector<ServiceKind> service_kinds;
    std::vector<CenterType> center_types;
    bool assistant_requested = false;   // un gruppo o un passeggero chiede l'assistente
};

// Vale la condizione di un regolamento per chi e' a bordo di un mezzo con quei posti per i passeggeri?
inline bool condition_holds(const PassengerCondition& c, const OnBoard& b, int passenger_seats) {
    const auto any = [](const auto& wanted, const auto& present) {
        if (wanted.empty()) return true;
        for (const auto& w : wanted)
            if (std::find(present.begin(), present.end(), w) != present.end()) return true;
        return false;
    };
    return any(c.grades, b.grades) && any(c.group_types, b.group_types) && any(c.needs, b.needs) &&
           any(c.service_kinds, b.service_kinds) && any(c.center_types, b.center_types) &&
           (!c.seats_over || passenger_seats > *c.seats_over);
}

// Serve l'assistente a bordo? Si' se un gruppo o un passeggero lo chiede, oppure se vale una delle
// condizioni del regolamento (es. infanzia sui mezzi con piu' di 8 posti per i passeggeri).
inline bool needs_assistant(const ConditionsRule* rule, const OnBoard& b, int passenger_seats) {
    if (b.assistant_requested) return true;
    if (!rule) return false;
    for (const auto& c : rule->conditions)
        if (condition_holds(c, b, passenger_seats)) return true;
    return false;
}

// --- Pesi del calcolo ----------------------------------------------------------------------------------------

// Quanto pesa ogni voce nel calcolo. I valori sono DA DECIDERE con l'azienda (mandato del calcolo), non ancora usati.
// La pausa fuori dal posto non ha un peso: la giudica la chiave del rientro nei dati (K-011, orari.hpp); i pesi delle
// preferenze e il costo delle scelte arriveranno con il calcolo.
struct ObjectiveWeights {
    double per_empty_km = 1.0;              // ogni km a vuoto
    double per_idle_minute = 0.05;          // ogni minuto di mezzo fermo
    double per_commute_km = 0.5;            // ogni km casa-ricovero dell'autista
};

}  // namespace kzero
