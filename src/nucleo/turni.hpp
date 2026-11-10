#pragma once
// K-Zero - turni del giorno: in una data, chi fa ogni turno, con quali corse e attivita', con quale mezzo e da quale
// dei tre modi viene l'assegnazione (assegnazione del singolo giorno > servizio fisso > rotazione: vince il piu'
// preciso, turno per turno). Tiene conto di calendari, festivita', assenze, disponibilita', scadenze e fermi.
// Il programma non inventa assegnazioni: se nessun modo da' una persona, il turno e' scoperto.
// "Giornata della persona" e "giornata del mezzo" sono due viste degli stessi turni.

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "pacchetto.hpp"

namespace kzero {

// Da dove viene la persona del turno, e da dove viene il mezzo.
enum class PersonSource { None, Rotation, Fixed, Day };
enum class VehicleSource { None, Shift, Usual, Day };

// Fatti con le date che impediscono alla persona prevista di fare il turno quel giorno.
enum class PersonImpediment { None, Absent, OutsideAvailability, LoanNotValid, LicenceExpired, AuthorizationExpired };

// Fatti con le date che impediscono al mezzo previsto di fare il turno quel giorno.
enum class VehicleImpediment { None, Stopped, UseNotAuthorized, LoanNotValid };

// Una corsa (intera o un pezzo) o un'attivita' del turno, in un giorno in cui c'e'.
struct PlannedItem {
    const ShiftItem* item = nullptr;
    const Trip* trip = nullptr;     // nullptr per un'attivita'
    std::size_t from = 0, to = 0;   // primo e ultimo passaggio del pezzo
    bool whole = true;              // corsa intera
    Minutes start = 0, end = 0;
    double loaded_km = 0.0, empty_km = 0.0;   // km scritti nei dati (corsa o pezzo; attivita': km a vuoto)
    bool uses_vehicle = true;       // impegna il mezzo del turno
};

// Un candidato di una rotazione per un turno in un giorno.
struct RotationCandidate {
    const Rotation* rotation = nullptr;
    std::size_t row = 0;
    int step = 0;
    const Person* person = nullptr;
};

// Il turno in un giorno.
struct ShiftDay {
    const Shift* shift = nullptr;
    Day day = 0;
    std::vector<PlannedItem> items;   // solo corse e attivita' che valgono quel giorno, nell'ordine del turno
    Minutes start = 0, end = 0;       // ricavati dalla prima e dall'ultima corsa o attivita' (anche oltre le 24:00)
    std::optional<Minutes> vehicle_start, vehicle_end;   // quando il turno impegna il mezzo

    // Persona: il modo piu' preciso che la da'.
    PersonSource person_source = PersonSource::None;
    const Person* planned = nullptr;   // persona prevista (anche se poi un fatto glielo impedisce)
    const Person* person = nullptr;    // chi fa il turno: nullptr = scoperto
    RotationCandidate rotation;        // con PersonSource::Rotation
    const FixedAssignment* fixed = nullptr;
    const DayAssignment* person_assignment = nullptr;
    PersonImpediment person_impediment = PersonImpediment::None;
    const Absence* absence = nullptr;
    const Licence* licence = nullptr;
    const ClientAuthorization* authorization = nullptr;
    const Loan* person_loan = nullptr;
    // Tutti i candidati di ogni modo (piu' di uno: errore K-CTR-065 o K-CTR-066).
    std::vector<RotationCandidate> rotation_candidates;
    std::vector<const FixedAssignment*> fixed_candidates;
    std::vector<const DayAssignment*> day_candidates;   // tutte le assegnazioni del giorno per questo turno

    // Mezzo.
    VehicleSource vehicle_source = VehicleSource::None;
    const Vehicle* planned_vehicle = nullptr;
    const Vehicle* vehicle = nullptr;   // nullptr = senza mezzo
    const DayAssignment* vehicle_assignment = nullptr;
    VehicleImpediment vehicle_impediment = VehicleImpediment::None;
    const VehicleStop* stop = nullptr;
    const UseAuthorization* use_authorization = nullptr;
    const Loan* vehicle_loan = nullptr;
    // Turni da assistente o da secondo autista: il mezzo su cui la persona e' a bordo (dal turno dell'autista).
    const Vehicle* aboard = nullptr;

    bool uncovered() const { return person == nullptr; }
    // Punto dei dati in cui e' scritta l'assegnazione della persona (la riga della rotazione, il servizio fisso,
    // l'assegnazione del giorno); il turno se nessun modo da' una persona. Resta lo stesso nei giorni diversi, cosi' i
    // problemi uguali di molti giorni si raggruppano.
    const Source& person_position() const;
    // Punto dei dati in cui e' scritto il mezzo (assegnazione del giorno o turno).
    const Source& vehicle_position() const;
};

// Inizio assoluto di un orario di un giorno: minuti dall'inizio del 1970-01-01 (per confrontare giorni diversi,
// anche a cavallo della mezzanotte).
inline long long absolute(Day d, Minutes m) { return static_cast<long long>(d) * 1440 + m; }

// Dove sta un pezzo di corsa: i passaggi di inizio e di fine, oppure perche' non si trova.
enum class PartProblem { None, PlaceNotOnTrip, Ambiguous, Reversed };
struct PartResolution {
    PartProblem problem = PartProblem::None;
    std::size_t from = 0, to = 0;
    const TripPoint* bad = nullptr;   // il punto che non va
};
PartResolution resolve_part(const ShiftTrip& part, const Trip& trip);

// Usi dei mezzi ammessi per un servizio: il regolamento del contratto, se lo dice; altrimenti le norme dell'azienda
// del contratto. Nessun valore = nessuna regola (nessun uso richiesto).
std::optional<std::vector<Code>> allowed_uses(const Service& s, const Index& idx);

// K-014: i requisiti dei dati che dipendono dal tipo di servizio (dato particolare: rivela il servizio per disabili), fuori
// dagli orari (per gli orari: kind_dependence, orari.hpp). Lo dicono i dati, mai il nome:
// - gli usi dei mezzi per tipo di servizio: regolamenti (usi_dei_mezzi) e norme (usi_per_servizio), allowed_uses;
// - l'autorizzazione del committente per tipo di servizio: regolamenti (autorizzazione_del_committente),
//   required_authorizations;
// - il vincolo di destinazione di un mezzo che nomina tipi di servizio (ammessi.tipi_servizio);
// - le condizioni dei regolamenti con tipi di servizio (assistente a bordo, adulto alla fermata). Oggi nessuna uscita le usa:
//   kzero dimostrazione guarda solo le condizioni sul grado e sui posti.
// Senza i dati particolari i problemi che nascono da questi requisiti si contano soltanto (Problem::by_service_kind) e, se il
// pacchetto ne ha almeno uno, i turni scoperti o senza mezzo per un fatto con le date si mostrano tutti con lo stesso testo,
// senza la causa (DECISIONI.md, K-014).
struct KindRequirements {
    std::vector<const Regulation*> vehicle_uses;           // regolamenti con usi dei mezzi per tipo
    std::vector<const Norms*> service_uses;                // norme con usi per tipo di servizio
    std::vector<const Regulation*> client_authorization;   // regolamenti con l'autorizzazione del committente per tipo
    std::vector<const Vehicle*> constraints;               // mezzi con un vincolo di destinazione per tipo
    std::vector<const Regulation*> conditions;             // regolamenti con condizioni per tipo
    bool any() const {
        return !vehicle_uses.empty() || !service_uses.empty() || !client_authorization.empty() || !constraints.empty() ||
               !conditions.empty();
    }
};
KindRequirements kind_requirements(const Package& p);

// Requisiti di guida: la persona puo' guidare il mezzo secondo le norme? Missing = manca un'abilitazione (sempre
// sbagliato); Expired = le avrebbe, ma quel giorno una e' scaduta (fatto con la data).
enum class LicenceStatus { Ok, Missing, Expired };
struct LicenceResult {
    LicenceStatus status = LicenceStatus::Ok;
    const DrivingRequirement* requirement = nullptr;   // il requisito non soddisfatto
    const Licence* expired = nullptr;                  // con Expired: un'abilitazione scaduta
};
LicenceResult check_licences(const Person& p, const Vehicle& v, const Norms& n, Day d);

// Autorizzazioni del committente che servono all'autista per le corse di un turno in un giorno.
struct RequiredAuthorization {
    const Contract* contract = nullptr;   // il committente e' quello del contratto
    const Service* service = nullptr;
};
std::vector<RequiredAuthorization> required_authorizations(const ShiftDay& sd, const Index& idx);

// Totali di un turno in un giorno: km a pieno e a vuoto scritti nei dati, minuti fermi tra un elemento e l'altro.
double loaded_km(const ShiftDay& sd);
double empty_km(const ShiftDay& sd);
Minutes idle_minutes(const ShiftDay& sd);

// Luogo in cui finisce e in cui comincia un elemento del turno (per le pause tra un elemento e l'altro).
const Code& end_place(const PlannedItem& it);
const Code& start_place(const PlannedItem& it);

// Tragitto casa -> ricovero del mezzo (andata e ritorno), in km in linea d'aria; nessun valore senza coordinate.
inline std::optional<double> commute_km(const Person& p, const Coord& depot) {
    if (!p.home || !p.home->coord) return std::nullopt;
    return 2.0 * air_km(*p.home->coord, depot);
}

// Chi fa ogni turno, giorno per giorno. Prepara una volta sola gli indici (calendari, assegnazioni, assenze, fermi).
class Planner {
public:
    explicit Planner(const Package& p);
    Planner(const Planner&) = delete;
    Planner& operator=(const Planner&) = delete;

    const Package& package() const { return p_; }
    const Index& index() const { return idx_; }
    const CalendarDays& days() const { return days_; }

    // Tutti i turni che ci sono nel giorno d (dentro il periodo del pacchetto), in ordine di inizio e di codice.
    std::vector<ShiftDay> day(Day d) const;

    // L'elemento del turno c'e' nel giorno d? Una corsa se vale il suo calendario (e il pezzo si trova nella corsa),
    // un'attivita' se vale il suo calendario (o non ne ha). Il calendario del turno si guarda a parte.
    bool item_on(const ShiftItem& item, Day d) const;
    // In quanti giorni del periodo del pacchetto c'e' il turno: vale il suo calendario e quel giorno ha almeno una corsa
    // o un'attivita'.
    int shift_days(const Shift& s) const;

    // Usi dei mezzi ammessi per un servizio del pacchetto (allowed_uses), calcolati una volta sola.
    const std::optional<std::vector<Code>>& uses_of(const Service& s) const;

private:
    void resolve_person(ShiftDay& sd) const;
    void resolve_vehicle(ShiftDay& sd) const;
    void person_facts(ShiftDay& sd, bool with_vehicle) const;
    void vehicle_facts(ShiftDay& sd) const;

    // Corsa e pezzo di ogni elemento-corsa dei turni: non cambiano da un giorno all'altro, si calcolano una volta sola.
    struct TripItem {
        const Trip* trip = nullptr;
        const std::vector<char>* days = nullptr;   // giorni del calendario della corsa
        PartResolution part;
    };

    const Package& p_;
    Index idx_;
    CalendarDays days_;
    std::unordered_map<const ShiftItem*, TripItem> trip_items_;
    std::unordered_map<const Service*, std::optional<std::vector<Code>>> service_uses_;
    // Per ogni rotazione, riga e passo: i turni del passo (posizioni in Package::shifts) e la persona della riga.
    struct RotationRowPlan {
        const Person* person = nullptr;
        std::vector<std::vector<std::size_t>> steps;   // vuoto = riposo
    };
    std::vector<std::vector<RotationRowPlan>> rotation_plan_;
    std::map<std::pair<Day, Code>, std::vector<const DayAssignment*>> day_assignments_;
    std::map<Code, std::vector<const FixedAssignment*>> fixed_;
    std::map<Code, std::vector<const Absence*>> absences_;
    std::map<Code, std::vector<const VehicleStop*>> stops_;
};

}  // namespace kzero
