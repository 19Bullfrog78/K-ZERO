#pragma once
// K-Zero - controlli sul pacchetto dati.
// Ogni problema ha un tipo (Check), la posizione nei dati e i valori da mettere nel messaggio.
// Codici K-CTR, livello (errore o avviso) e testi dei messaggi stanno nel catalogo dell'API
// (src/api/controlli_catalogo.cpp e docs/controlli.md): qui non c'e' nessun testo da mostrare.

#include <string>
#include <vector>

#include "domain_models.hpp"

namespace kzero {

enum class Check {
    // Forma dei file: li segnala chi legge i file (l'API), prima dei controlli sul contenuto.
    MissingManifest,
    UnreadableFile,
    InvalidJson,
    DuplicateKey,
    WrongSchema,
    UnsupportedVersion,
    SchemaViolation,
    UnknownFile,
    NotUtf8,
    FileTooBig,
    // Turni (K-008): errori.
    ShiftPartInvalid,
    ShiftItemsOrder,
    ShiftVehicleInconsistent,
    RotationMalformed,
    AssignmentOnDayWithoutShift,
    ShiftAtShelterNever,
    PeriodTooLongForShifts,
    TripPartsGap,
    TripInTwoShifts,
    PersonOverlap,
    VehicleOverlap,
    RotationTwoPersons,
    TwoAssignmentsSameShift,
    DayAssignmentAgainstFact,
    DriverWithoutLicence,
    VehicleWithoutUseForService,
    PairingFixedViolated,
    PersonWithoutCompetence,
    VehicleAgainstConstraint,
    OtherCompanyWithoutLoan,
    BlockedTripOtherVehicle,
    IncompatibleBlocks,
    PersonRoleMismatch,
    // Turni (K-008): avvisi.
    ShiftUncovered,
    RecurringShiftPersonFact,
    RecurringShiftVehicleFact,
    TripWithoutShift,
    PreferenceNotRespected,
    PairingNotRespected,
    ShiftVehicleToAssign,
    PersonWithoutShifts,
    VehicleNeverUsed,
    EscortWithoutPerson,
    EscortProviderDiffers,
    // Contenuto: errori.
    DuplicateCode,
    UnknownReference,
    WrongPlaceType,
    InvertedPeriod,
    DateOutsideCalendar,
    ContradictoryDate,
    CalendarWithoutDays,
    CalendarCycle,
    TimeGoesBack,
    AmbiguousStop,
    NumbersOnlyPersonalData,
    NegativeOnBoard,
    WaitOutsideRegulation,
    MaxSeatsBelowMin,
    TransferNotAuthorized,
    TransferHalfMissing,
    TransferLateReceiver,
    TransferHalvesDiffer,
    TransferReceiverLeaves,
    ConnectionWaitArrival,
    ConnectionGuaranteeDeparture,
    ConnectionNotOnTrip,
    CountsDifferFromPassengers,
    LotServiceOtherContract,
    ServiceInTwoLots,
    GroupNotOnTripServices,
    AssistantNotRequested,
    CalendarCombinationCycle,
    // Contenuto: avvisi (K-008).
    ExpiresInPeriod,
    VehicleWithoutUse,
    CompanyWithoutNorms,
    // Contenuto: avvisi.
    PlaceWithoutCoordinates,
    DwellTimesUndefined,
    MaxRideUndefined,
    StopNeverUsed,
    ServiceWithoutTrips,
    TripWithoutService,
    GroupWithoutPassengers,
    DemandDiffersFromTrips,
    TransferVerbalOnly,
    VehicleWithoutCosts,
    CompanyWithoutVehicles,
    ContractWithoutRegulation,
};

// Tipi di elemento del pacchetto: per i riferimenti nei messaggi.
enum class Element {
    Company, Place, Segment, Regulation, Calendar, School, Client, Contract, Lot, Service, Group, Passenger, Trip,
    Vehicle, Depot, Transfer, Holidays,
    // K-008
    Norms, CompanySettings, Licence, VehicleUse, ActivityType, AbsenceType, StopType, Competence, Person,
    Shift, Rotation, FixedAssignment, Block,
};

// Un valore da mettere nel messaggio, al posto di {nome}.
struct Param {
    enum class Kind {
        Text,       // testo dei dati o codice di un valore del formato (es. "fermata")
        Number,     // numero intero
        Decimal,    // numero con decimali (es. km)
        Time,       // orario: minuti dalla mezzanotte
        Date,       // data
        Ref,        // un elemento: codice, e il nome se le classi dei dati lo permettono
        Elements,   // il nome di un tipo di elemento (es. "luoghi")
        Days,       // i giorni in cui succede il problema (i problemi uguali di molti giorni si raggruppano)
    };
    // Classe del valore (docs/formato-dati.md). Un valore personale o particolare compare nel messaggio solo con
    // il parametro che lo permette (--includi-dati-personali, --includi-dati-particolari); altrimenti al suo posto
    // va hidden (es. la stessa frase senza il valore), o "riservato" se hidden e' vuoto.
    enum class Sensitivity { Common, Personal, Particular };

    std::string name;
    Kind kind = Kind::Text;
    std::string text;      // Text; codice per Ref
    long long number = 0;  // Number, Time, Date
    double decimal = 0.0;  // Decimal
    Element element = Element::Place;   // Ref, Elements
    Sensitivity sensitivity = Sensitivity::Common;
    std::string hidden;
    std::vector<Day> days;   // Days: in ordine, senza ripetizioni
};

Param text_param(std::string name, std::string value);
Param number_param(std::string name, long long value);
Param decimal_param(std::string name, double value);
Param time_param(std::string name, Minutes value);
Param date_param(std::string name, Day value);
Param ref_param(std::string name, Element element, std::string code);
Param elements_param(std::string name, Element element);
Param days_param(std::string name, Day day);

struct Problem {
    Check check;
    int variant = 0;   // forma del messaggio, quando lo stesso problema si presenta in modi diversi
    Source where;
    std::vector<Param> params;
};

// Controlli sul contenuto del pacchetto (dati gia' conformi agli schemi). Ordine dei problemi: quello dei dati.
// I problemi uguali che succedono in giorni diversi (stesso controllo, stesso punto, stessi valori) diventano uno
// solo, con tutti i giorni.
std::vector<Problem> check_package(const Package& p);

// Controlli sui turni, giorno per giorno (controlli_turni.cpp): aggiunge i problemi a out.
void check_shifts(const Package& p, std::vector<Problem>& out);

// Raggruppa i problemi uguali di giorni diversi in uno solo (il primo, con tutti i giorni).
std::vector<Problem> group_by_days(std::vector<Problem> problems);

}  // namespace kzero
