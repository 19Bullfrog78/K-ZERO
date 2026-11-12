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
    NotRegularFile,           // K-009: lettura blindata
    LinkOutsideFolder,
    DirectoryInsteadOfFile,
    // Cassaforte dei dati personali (K-010): li segnala chi legge il pacchetto e la cassaforte (l'API).
    VaultMissing,              // il manifesto dichiara la cassaforte, il file manca
    VaultUndeclared,           // c'e' il file della cassaforte, il manifesto non la dichiara
    VaultGenerationMismatch,   // cassaforte e file normali di chiusure diverse
    VaultValueOrphan,          // un valore della cassaforte non trova il suo elemento
    VaultValueConflict,        // un valore c'e' sia nella cassaforte sia in chiaro
    ClearPersonalData,         // dati personali o particolari in chiaro (senza "dati_inventati", o accanto a una cassaforte)
    RetentionExpired,          // data di conservazione passata
    // Password da cambiare (K-010 aggiunta 1): avvisi dell'apertura e dei comandi della cassaforte.
    KeysFromEnvelope,          // chi apre ha ricevuto chiavi nuove in una busta (un utente e' stato tolto)
    UserRemovedPasswords,      // dopo aver tolto un utente: le password che conosceva vanno cambiate
    PasswordWrittenByOther,    // password scritta da chi ha aggiunto l'utente o gliel'ha data
    // Testi comuni (K-018): li segnala l'API (src/api/testi_liberi.cpp), che conosce le classi degli schemi.
    NameInCommonText,          // il nome di una persona o di una casa dei dati in un testo comune
    HealthWordInCommonText,    // una parola sulla salute (elenchi delle norme e delle aziende) in un testo comune
    PlaceMaybeSensitive,       // un luogo pubblico non segnato sensibile con una parola sulla salute nel nome
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
    // Orari di lavoro (K-011): avvisi sui dati e sul piano.
    TimeCategoryMissing,          // tipi di attivita' senza categoria di tempo, soste non scritte non definite (versione 1)
    RegimeUndeterminable,         // corsa senza km o turno senza mezzo: non si sa se vale un regime
    ClockChangeCrossed,           // un turno attraversa il cambio dell'ora
    PartialVerification,          // una regola si verifica solo in parte (finestre fuori dal periodo del pacchetto)
    ReturnStopBelowCountedStops,  // fermo minimo del rientro sotto le soste che il regime conta come lavoro
    ChoiceLooserThanLaw,          // scelta dell'azienda o della persona piu' larga della legge
    EmployeeWithoutLaw,           // un dipendente lavora in giorni senza nessuna legge sugli orari nei dati
    UntreatedLawCase,             // un caso della legge che il programma non tratta (es. multipresenza)
    DaysNearLimit,                // giornate di lavoro vicine al limite (es. rapporto a chiamata)
    // Orari di lavoro (K-011): violazioni (errori del piano, non dei dati: non bloccano kzero giornata e kzero ore).
    LawWorkViolated,              // lavoro: errore (legge o contratto collettivo, lettura certa)
    LawWorkToConfirm,             // lavoro: avviso da confermare
    LawOvertime,                  // straordinario: avviso
    LawDrivingViolated,
    LawDrivingToConfirm,
    LawDailyRestViolated,
    LawDailyRestToConfirm,
    LawWeeklyRestViolated,
    LawWeeklyRestToConfirm,
    ContractHoursExceeded,        // ore massime del contratto della persona
    ContractHoursToConfirm,
    ChoiceLimitViolated,          // limite rigido dell'azienda o della persona su un turno ricorrente
    ChoiceNotRespected,           // preferenza, o limite su un'assegnazione del singolo giorno
    ReturnNotJustifiedLimit,      // rientro non giustificato, chiave rigida su un turno ricorrente
    ReturnNotJustified,           // rientro non giustificato: avviso
    ReturnNotJudged,              // rientro non giudicato: servono le distanze o i km
    WorkRuleIncoherent,           // regola sugli orari scritta in modo incoerente (dato)
    // Modo delle corse: OUT o tachigrafo (K-011 aggiunta 1).
    ChosenRegimeLimit,            // regola del regime seguito per scelta (tachigrafo), limite rigido su un turno ricorrente
    ChosenRegimeNotRespected,     // ... preferenza, assegnazione del singolo giorno, o solo con la lettura prudente
    ModeForced,                   // forzatura dell'azienda: OUT dove la legge, con i dati scritti, vuole il regime (sempre visibile)
    ModeForcedRisk,               // rischio della forzatura: le violazioni della legge come la calcolano i dati
    ModeDeclared,                 // dichiarazione dell'azienda: servizio o corsa esclusi dove la legge e' da confermare
    ModeOutNotValid,              // OUT dove la legge vuole il regime, senza forzatura: vale la legge
    ModeMixed,                    // corsa con servizi di tipi (o servizi) con scelte diverse: decide l'impostazione
    ModeForcedUseless,            // forzatura che non cambia niente nel periodo del pacchetto (K-011 aggiunta 2)
    ModeDeclaredUseless,          // dichiarazione che non cambia niente nel periodo del pacchetto (K-011 aggiunta 2)
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
    // K-011
    Regime, WorkRule,
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
        Duration,   // durata in minuti (K-011): "4 ore e 30 minuti"
        Reasons,    // motivi di un avviso "da confermare" (K-011): codici separati da virgole, che il catalogo scrive in
                    // italiano filtrando ognuno secondo la sua classe (es. il rapporto e' personale, il tipo di servizio
                    // particolare)
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
Param duration_param(std::string name, long long minutes);
Param reasons_param(std::string name, std::vector<std::string> codes);

struct Problem {
    Check check;
    int variant = 0;   // forma del messaggio, quando lo stesso problema si presenta in modi diversi
    Source where;
    std::vector<Param> params;
    // K-013: il problema dipende dal tipo di servizio delle corse (orari di lavoro: un regime che dipende dal tipo, il lavoro
    // contato da quei regimi). Che ci sia, e il suo messaggio, rivelano allora un dato particolare: il catalogo dell'API dice
    // per quali controlli puo' succedere.
    bool by_service_kind = false;
    // K-017: il problema dipende da un risultato del piano dei turni non verificato (il tipo di un servizio non e' noto a chi non
    // puo' leggerlo: turni.hpp, ShiftDay). Per quell'utente non si mostra: si conta soltanto, tra i non verificati. Con tutti i
    // tipi noti mai.
    bool unverified = false;
};

// Controlli sul contenuto del pacchetto (dati gia' conformi agli schemi). Ordine dei problemi: quello dei dati.
// I problemi uguali che succedono in giorni diversi (stesso controllo, stesso punto, stessi valori) diventano uno
// solo, con tutti i giorni.
struct HoursRequest;   // orari.hpp
struct HoursReport;

// Con request e report (kzero ore, K-011) riempie anche il resoconto degli orari, nella stessa passata.
std::vector<Problem> check_package(const Package& p, const HoursRequest* request = nullptr, HoursReport* report = nullptr);

class PlanCollector;   // orari.hpp

// Controlli sui turni, giorno per giorno (controlli_turni.cpp): aggiunge i problemi a out. Con collector raccoglie anche le
// giornate delle persone per i controlli sugli orari (K-011), nella stessa passata sul periodo.
void check_shifts(const Package& p, std::vector<Problem>& out, PlanCollector* collector = nullptr);

// Raggruppa i problemi uguali di giorni diversi in uno solo (il primo, con tutti i giorni).
std::vector<Problem> group_by_days(std::vector<Problem> problems);

}  // namespace kzero
