#pragma once
// K-Zero - orari di lavoro (K-011): i tempi di ogni persona, giorno per giorno, ricavati dai turni; le leggi sugli orari e i
// contratti collettivi come dati (regimi con i loro tipi di regola); il contratto della persona; le scelte dell'azienda e
// della persona; la chiave della pausa fuori dal posto (rientro).
//
// Principi (mandato K-011, arbitro): dove la legge e' certa vale come errore; dove una lettura e' incerta il programma lo
// dice e non decide (avviso "da confermare"); dove mancano i dati il programma non dice mai "regolare" ("verifica parziale",
// "non giudicato"). Ogni regola si valuta due volte: nel mondo "certo" (lettura certa, solo i regimi che valgono con
// certezza, le categorie presunte nel modo piu' favorevole) e nel mondo "prudente" (lettura prudente, anche i regimi da
// confermare, le categorie presunte nel modo prudente). Una violazione nel mondo certo e' un errore; una violazione solo nel
// mondo prudente e' un avviso con i motivi da confermare.
//
// Il nucleo non contiene testi: i problemi hanno il loro tipo (Check) e i valori; i testi stanno nel catalogo dell'API.

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
#include "turni.hpp"

namespace kzero {

// --- La giornata della persona ----------------------------------------------------------------------------------------

// Un elemento (corsa intera, pezzo di corsa o attivita') di un turno fatto dalla persona in un giorno.
struct WorkItem {
    const Shift* shift = nullptr;
    const ShiftItem* item = nullptr;
    const Trip* trip = nullptr;        // nullptr per un'attivita'
    std::size_t from = 0, to = 0;      // passaggi del pezzo di corsa
    Minutes start = 0, end = 0;        // orari del giorno della giornata (anche oltre le 24:00)
    ShiftRole role = ShiftRole::Driver;
    const Vehicle* vehicle = nullptr;  // autista: il mezzo del turno; assistente e secondo autista: il mezzo a bordo
    PersonSource source = PersonSource::None;
    bool uses_vehicle = true;
};

// La giornata della persona: tutti i turni che comincia in quel giorno di calendario (anche di aziende diverse, prestiti),
// in ordine di orario. Un turno oltre la mezzanotte resta del giorno in cui comincia.
struct PersonDay {
    Day day = 0;
    std::vector<WorkItem> items;
    std::vector<const Shift*> shifts;               // in ordine di inizio
    std::vector<PersonSource> shift_sources;        // come la persona ha ogni turno
    Minutes start = 0, end = 0;
    bool day_assignment = false;                    // almeno un turno da un'assegnazione del singolo giorno
};

struct PersonPlan {
    const Person* person = nullptr;
    std::vector<PersonDay> days;   // in ordine di giorno
};

// Un turno in un giorno vicino a un cambio dell'ora (il giorno stesso o quello prima): con o senza persona.
struct ShiftSpan {
    const Shift* shift = nullptr;
    Day day = 0;
    Minutes start = 0, end = 0;
};

// Il piano di tutto il periodo per i controlli sugli orari.
struct PlanData {
    std::vector<PersonPlan> persons;          // in ordine di posizione nel pacchetto (solo quelle con almeno una giornata)
    std::vector<ShiftSpan> near_clock_change;
};

// Raccoglie le giornate delle persone dal piano di ogni giorno (Planner::day), un giorno alla volta: cosi' i controlli sui
// turni e quelli sugli orari si fanno con una passata sola sul periodo.
class PlanCollector {
public:
    explicit PlanCollector(const Package& p);
    void add(Day d, const std::vector<ShiftDay>& plan);
    PlanData take(const Package& p);

private:
    std::map<const Person*, std::vector<PersonDay>> days_;
    std::set<Day> clock_days_;   // i giorni dei cambi dell'ora e quelli prima
    std::vector<ShiftSpan> near_clock_;
};

// --- Esiti per kzero ore ----------------------------------------------------------------------------------------------

enum class Certainty { No, ToConfirm, Certain };

// Perche' un regime vale (o vale solo da confermare, o non vale) in un giorno per la persona. Dati strutturati: l'API li
// scrive seguendo le classi dei dati (il tipo di servizio e' un dato particolare, il rapporto e il contratto personali).
struct ApplyReason {
    enum class What {
        Trip,             // una corsa del giorno fa scattare il regime (o non lo fa scattare con certezza)
        Shift,            // un turno (regimi senza condizioni sulla corsa)
        NoTrips,          // una giornata senza corse, per un regime con condizioni sulla corsa: da confermare
        Week,             // il regime vale per tutta la settimana (estensione) per una corsa di un altro giorno
        OverriddenBy,     // un altro regime lo esclude (o lo rende da confermare) per alcune famiglie
        RegimeToConfirm,  // il regime e' segnato da confermare nei dati
    };
    What what = What::Trip;
    Certainty level = Certainty::Certain;
    const Trip* trip = nullptr;
    const Shift* shift = nullptr;
    ShiftRole role = ShiftRole::Driver;
    Certainty role_level = Certainty::Certain;
    std::vector<ServiceKind> kinds;          // tipi di servizio della corsa (dato particolare)
    Certainty kinds_level = Certainty::Certain;   // cosa dicono i tipi di servizio (inclusione ed esclusioni)
    const Vehicle* vehicle = nullptr;        // nullptr: mezzo non noto
    int persons = 0;                         // persone trasportabili del mezzo, compreso il conducente
    Certainty vehicle_level = Certainty::Certain;
    bool km_known = false;
    double km = 0.0;                         // percorso della corsa (con km_known), oppure i km noti (almeno)
    Certainty route_level = Certainty::Certain;
    Certainty employment_level = Certainty::Certain;   // rapporto della persona (dato personale)
    Certainty agreement_level = Certainty::Certain;    // contratto collettivo della persona (dato personale)
    Day week_from = 0, week_to = 0;
    Day trigger_day = 0;
    const Regime* other = nullptr;           // OverriddenBy
    std::vector<RuleFamily> families;        // OverriddenBy
    ExclusionEffect effect = ExclusionEffect::Exclude;
    // Nel resoconto (HoursReport) un motivo Trip vale per tutte le corse del giorno con lo stesso esito e le stesse
    // condizioni: sono qui, i loro dati una volta sola in DayHours::trips (un anno di 1.000 persone resta leggero).
    std::vector<const Trip*> trips;
};

// Una corsa del giorno con i dati che decidono quali regimi valgono (nel resoconto, una volta sola per giornata).
struct TripOnDay {
    const Trip* trip = nullptr;
    const Shift* shift = nullptr;
    ShiftRole role = ShiftRole::Driver;
    std::vector<ServiceKind> kinds;          // dato particolare
    const Vehicle* vehicle = nullptr;        // nullptr: mezzo non noto
    int persons = 0;                         // persone trasportabili, compreso il conducente
    bool km_known = false;
    double km = 0.0;
};

struct RegimeOnDay {
    const Regime* regime = nullptr;
    std::array<Certainty, 5> family{};   // per famiglia (RuleFamily)
    std::vector<ApplyReason> reasons;
    bool applies() const {
        for (Certainty c : family)
            if (c != Certainty::No) return true;
        return false;
    }
};

// Lavoro contato da un regime (es. lavoro effettivo, orario di lavoro), in centesimi di minuto (il 12% di un minuto).
struct WorkMeasure {
    const Regime* regime = nullptr;          // nullptr: senza regime (guida, altre mansioni e disponibilita')
    std::string name;                        // dai dati (es. "lavoro effettivo")
    long long certain_x100 = 0, prudent_x100 = 0;
    long long stops_counted = 0;             // minuti di soste contati per intero (lettura certa)
    long long idle_counted_x100 = 0;         // tempo inoperoso contato in parte (lettura certa)
};

struct NightCount {
    const Regime* regime = nullptr;
    const WorkRule* rule = nullptr;
    long long certain = 0, prudent = 0;      // minuti di lavoro nella fascia (letture certa e prudente)
};

// Un rientro (pausa fuori dal posto) e il suo giudizio con la chiave dell'azienda o della persona.
struct ReturnJudgment {
    const Trip* before = nullptr;
    const Trip* after = nullptr;
    Code place;                       // rimessa o ricovero
    Minutes arrival = 0, departure = 0;
    int stop = 0;                     // minuti fermo li'
    int transfers_driving = 0;        // minuti di guida a vuoto dei trasferimenti (verso il luogo e indietro)
    bool same_place = false;          // l'elemento dopo comincia dove finisce quello prima: il diretto e' zero
    bool km_known = true;
    double transfers_km = 0.0;
    // Chiave usata (campo per campo: la persona vince sull'azienda) e da dove viene.
    int min_stop = 0;
    double factor = 0.0;
    std::optional<double> max_km;
    ChoiceStrength strength = ChoiceStrength::Preference;
    bool personal_key = false;        // almeno un valore della persona (dato personale)
    enum class Outcome { Justified, NotJustified, NotJudged } outcome = Outcome::NotJudged;
    enum class Why { None, StopBelowMin, StopBelowFactor, KmOverMax, NeedDistances, NeedKm } why = Why::None;
    Source where;                     // la chiave (aziende.json o persone.json)
};

// Il riposo prima della giornata.
struct RestBefore {
    bool known = false;          // false: comincia prima del periodo del pacchetto
    long long minutes = 0;       // minuti (almeno, se non noto)
    Day from_day = 0;            // dove comincia (se noto)
    Minutes from_time = 0;
    bool after_on_call = false;  // comincia alla fine di una reperibilita' (che qui non conta come riposo)
};

// Una regola che non si e' potuta verificare del tutto (finestre che escono dal periodo del pacchetto).
struct PartialCheck {
    const Regime* regime = nullptr;
    const WorkRule* rule = nullptr;
    Day until = 0;               // verifica parziale fino al (incluso)
};

struct DayHours {
    const PersonDay* pd = nullptr;
    std::array<long long, kTimeCategories> minutes{};   // per categoria (lettura certa)
    long long assumed = 0;                              // minuti con la categoria presunta (aziende.json versione 1)
    long long unwritten = 0;                            // minuti di soste non scritte
    std::vector<WorkMeasure> work;                      // per regime (il primo senza regime se nessuno vale)
    long long contract_x100 = 0, contract_prudent_x100 = 0;   // lavoro contato per il contratto della persona
    std::vector<NightCount> night;
    RestBefore rest_before;
    std::vector<TripOnDay> trips;                       // le corse del giorno che decidono quali regimi valgono
    std::vector<RegimeOnDay> regimes;
    bool no_law = false;                                // nessun regime dei dati vale per la persona quel giorno
    std::vector<ReturnJudgment> returns;
    bool clock_change = false;                          // un turno attraversa il cambio dell'ora
    std::vector<PartialCheck> partial;
};

struct WeekHours {
    Day monday = 0;
    long long driving = 0;                              // minuti di guida
    std::vector<WorkMeasure> work;                      // per regime
    long long contract_x100 = 0;                        // lavoro contato per il contratto (giorni del pacchetto)
    std::optional<long long> planned;                   // ore previste alla settimana dal contratto (minuti)
    long long longest_rest = 0;                         // riposo piu' lungo che tocca la settimana (minuti)
    bool rest_known = true;
};

struct PersonHours {
    const Person* person = nullptr;
    std::vector<DayHours> days;
    std::vector<WeekHours> weeks;
    // Contratto della persona: ore previste (minuti) e lavoro contato nel periodo mostrato.
    std::optional<long long> planned_week;   // ore previste alla settimana, se ci sono (minuti)
    long long total_driving = 0;
    std::array<long long, kTimeCategories> total_minutes{};
    long long total_work_x100 = 0;           // lavoro (misura del contratto, lettura certa) nel periodo mostrato
    int workdays = 0;
};

struct HoursReport {
    Day from = 0, to = 0;
    std::vector<PersonHours> persons;
    PlanData plan;   // le giornate a cui puntano persons (DayHours::pd): restano vive con il resoconto
};

// --- Valutazione ------------------------------------------------------------------------------------------------------

struct HoursRequest {
    // Con il resoconto per kzero ore: i giorni mostrati e le persone (vuoto = tutte).
    bool report = false;
    Day from = 0, to = 0;
    std::set<Code> persons;
};

// Controlli sugli orari (K-CTR-101 ...): aggiunge i problemi a out; con request.report riempie il resoconto. plans = le
// giornate di tutte le persone nel periodo del pacchetto (PlanCollector). Controlla tutto il periodo anche quando il
// resoconto ne mostra una parte: le finestre (settimane, quattro settimane, mesi, anni) guardano anche fuori.
void check_hours(const Package& p, const Index& idx, const PlanData& plans, std::vector<Problem>& out,
                 const HoursRequest& request = {}, HoursReport* report = nullptr);

// Controlli sui dati degli orari che non dipendono dal piano (categorie mancanti, km mancanti, scelte piu' larghe della
// legge, fermo minimo sotto le soste contate come lavoro, regole incoerenti).
void check_hours_data(const Package& p, const Index& idx, std::vector<Problem>& out);

// --- Strumenti provati da soli -----------------------------------------------------------------------------------------

// Persone trasportabili compreso il conducente: posti + posti in piedi + 1 (Reg. 561/2006 art. 2: "piu' di nove persone
// compreso il conducente"; i posti in piedi si contano per prudenza).
inline int persons_with_driver(const Vehicle& v) { return v.seats + v.standing + 1; }

// Percorso della corsa: i km dal primo all'ultimo passaggio (pieni e vuoti dentro la corsa), oppure i totali della scheda.
// Senza km su qualche passaggio il percorso non e' noto: known = false e km = i km noti (il percorso e' almeno questo).
struct RouteKm {
    bool known = false;
    double km = 0.0;
};
RouteKm route_km(const Trip& t);

// Il giudizio di un rientro con la chiave (fermo minimo, fattore della guida, km massimi). Senza distanze il diretto non si
// conosce ma non e' mai negativo: giustificato con certezza se le condizioni valgono anche contando tutti i trasferimenti
// come "in piu'"; non giustificato con certezza se manca il fermo minimo (o, con il diretto zero, le altre condizioni);
// negli altri casi non giudicato.
void judge_return(ReturnJudgment& r);

// Lunedi' della settimana di un giorno.
Day monday_of(Day d);

}  // namespace kzero
