// K-Zero API - catalogo dei controlli e testo dei messaggi (vedi controlli_catalogo.hpp e docs/controlli.md).
#include "controlli_catalogo.hpp"

#include <algorithm>
#include <cmath>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "pacchetto.hpp"
#include "riservatezza.hpp"
#include "tempo.hpp"

namespace kzero::api {

using kzero::Check;

const std::vector<DefinizioneControllo>& catalogo_controlli() {
    // Nuovi controlli: codice successivo all'ultimo. Un codice non si cambia e non si riusa mai.
    static const std::vector<DefinizioneControllo> catalogo = [] {
        const Livello E = Livello::Errore, A = Livello::Avviso;
        // Cosa rivela il problema in se' (codice, messaggio, che ci sia): nessun dato personale o particolare (N), un dato
        // personale (PE) o un dato particolare, sulla salute (PA). Vedi docs/controlli.md.
        const Classe N = Classe::Comune, PE = Classe::Personale, PA = Classe::Particolare;
        std::vector<DefinizioneControllo> c = {
            // Forma dei file
            {Check::MissingManifest, "K-CTR-001", E, N, "Manca il manifesto",
             {"Manca il file pacchetto.json, il manifesto del pacchetto: senza manifesto il pacchetto non si puo' controllare."}},
            {Check::UnreadableFile, "K-CTR-002", E, N, "File illeggibile",
             {"Il file non si riesce a leggere (permessi o disco): non e' stato letto.",
              "Il file e' un collegamento che non porta a nessun file (il file a cui portava e' stato spostato o cancellato): "
              "non e' stato letto."}},
            {Check::InvalidJson, "K-CTR-003", E, N, "JSON non valido",
             {"Il file non e' un JSON valido: errore di scrittura alla riga {riga}, colonna {colonna}, o poco prima "
              "(es. una virgola o una parentesi mancante). Il file non e' stato letto.",
              "Il file non e' un JSON valido: alla riga {riga}, colonna {colonna} c'e' un byte zero, che un file JSON non puo' "
              "contenere (di solito il file e' rovinato, es. dopo uno spegnimento improvviso). Il file non e' stato letto.",
              "Il file non e' un JSON valido: alla riga {riga}, colonna {colonna} c'e' un numero troppo grande per essere letto "
              "(es. 1e400). Il file non e' stato letto."}},
            {Check::DuplicateKey, "K-CTR-004", E, N, "Campo ripetuto",
             {"Lo stesso campo compare due volte nello stesso oggetto: va scritto una volta sola. Il file non e' stato letto."}},
            {Check::WrongSchema, "K-CTR-005", E, N, "Schema del file sbagliato o mancante",
             {"Manca il campo \"schema\": per questo file deve essere \"{atteso}\". Il file non e' stato letto.",
              "Il file dichiara lo schema \"{trovato}\", ma questo file deve avere lo schema \"{atteso}\". Il file non e' stato letto."}},
            {Check::UnsupportedVersion, "K-CTR-006", E, N, "Versione dello schema non supportata",
             {"Versione dello schema {versione} non supportata (versioni lette da questo programma: {supportate}). "
              "Il file non e' stato letto.",
              "Manca il numero intero \"schema_versione\" (versioni lette da questo programma: {supportate}). "
              "Il file non e' stato letto."}},
            {Check::SchemaViolation, "K-CTR-007", E, N, "File non conforme allo schema",
             {"Non rispetta lo schema: {dettaglio}. Il file non e' stato usato."}},
            {Check::UnknownFile, "K-CTR-008", A, N, "File non riconosciuto",
             {"Il file {file} non fa parte del formato e non e' stato letto (nome scritto male?).",
              "La cartella {file} non fa parte del formato e non e' stata letta."}},
            // Contenuto: errori
            {Check::DuplicateCode, "K-CTR-009", E, N, "Codice doppio",
             {"Il codice {codice} compare due volte tra {elementi}: la prima volta in {prima}."}},
            {Check::UnknownReference, "K-CTR-010", E, N, "Riferimento a un codice inesistente",
             {"Il codice {codice} non esiste tra {elementi}.", "Il codice {codice} non esiste tra {elementi} delle norme {norme}.",
              "Il codice {codice} non esiste tra {elementi} dell'azienda {azienda}."}},
            {Check::WrongPlaceType, "K-CTR-011", E, N, "Luogo del tipo sbagliato",
             {"Il luogo {luogo} non e' una scuola ne' un centro.", "Il luogo {luogo} non e' una rimessa ne' un ricovero esterno."}},
            {Check::InvertedPeriod, "K-CTR-012", E, N, "Fine prima dell'inizio",
             {"Il periodo finisce ({al}) prima di cominciare ({dal}).", "L'orario finisce ({alle}) prima di cominciare ({dalle})."}},
            {Check::DateOutsideCalendar, "K-CTR-013", E, N, "Data fuori dal periodo del calendario",
             {"La data aggiunta {data} e' fuori dal periodo del calendario {calendario} ({dal} - {al}).",
              "La data tolta {data} e' fuori dal periodo del calendario {calendario} ({dal} - {al}).",
              "La sospensione dal {inizio} al {fine} esce dal periodo del calendario {calendario} ({dal} - {al})."}},
            {Check::ContradictoryDate, "K-CTR-014", E, N, "Data contraddittoria nel calendario",
             {"Nel calendario {calendario} la data {data} e' sia aggiunta sia tolta.",
              "Nel calendario {calendario} la data aggiunta {data} cade in una sospensione."}},
            {Check::CalendarWithoutDays, "K-CTR-015", E, N, "Calendario senza nessun giorno",
             {"Il calendario {calendario} non vale in nessun giorno del periodo del pacchetto ({dal} - {al})."}},
            {Check::CalendarCycle, "K-CTR-016", E, N, "Calendario basato su se stesso",
             {"Il calendario {calendario} si basa su se stesso, direttamente o attraverso altri calendari."}},
            {Check::TimeGoesBack, "K-CTR-017", E, N, "Orario che torna indietro in una corsa",
             {"Nella corsa {corsa} l'orario {orario} non sta tra quello del passaggio precedente ({precedente}) "
              "e quello del successivo ({successivo}): l'orario torna indietro.",
              "Nella corsa {corsa} l'orario {orario} viene prima di quello del passaggio precedente ({precedente}): "
              "l'orario torna indietro.",
              "Nella corsa {corsa} la partenza ({partenza}) viene prima dell'arrivo ({arrivo}) nello stesso passaggio."}},
            {Check::AmbiguousStop, "K-CTR-018", E, N, "Nome o codice esterno di fermata che vale per due fermate",
             {"Il nome \"{nome}\" (scritto in forma normalizzata) appartiene a due fermate: {luogo} e {altro}.",
              "Il codice esterno \"{nome}\" (sistema/tipo/codice, in forma normalizzata) appartiene a due fermate: "
              "{luogo} e {altro}."}},
            {Check::NumbersOnlyPersonalData, "K-CTR-019", E, N, "Dati personali in un pacchetto \"solo numeri\"",
             {"Il pacchetto e' di livello \"solo numeri\": il luogo privato {luogo} (casa di una persona) non e' ammesso.",
              "Il pacchetto e' di livello \"solo numeri\": il gruppo {gruppo} elenca i passeggeri uno per uno "
              "({passeggeri}), cosa ammessa solo nel livello \"completo\".",
              "Il pacchetto e' di livello \"solo numeri\": la persona {persona} ha il nome, ammesso solo nel livello \"completo\".",
              "Il pacchetto e' di livello \"solo numeri\": la persona {persona} ha la casa, ammessa solo nel livello \"completo\".",
              "Il pacchetto e' di livello \"solo numeri\": la persona {persona} ha una nota scritta a mano, ammessa solo nel "
              "livello \"completo\".",
              "Il pacchetto e' di livello \"solo numeri\": la rotazione {rotazione} ha una nota scritta a mano (puo' parlare delle "
              "persone), ammessa solo nel livello \"completo\".",
              "Il pacchetto e' di livello \"solo numeri\": il servizio fisso {servizio_fisso} ha una nota scritta a mano (puo' "
              "parlare delle persone), ammessa solo nel livello \"completo\".",
              "Il pacchetto e' di livello \"solo numeri\": l'assegnazione del {data} del turno {turno} ha una nota scritta a mano "
              "(puo' dire il motivo di un cambio), ammessa solo nel livello \"completo\"."},
             {{2, PE}, {3, PE}, {4, PE}}},
            {Check::NegativeOnBoard, "K-CTR-020", E, N, "Persone a bordo sotto zero",
             {"Nella corsa {corsa} le persone del gruppo {gruppo} che scendono ({scendono}) sono piu' di quelle a bordo "
              "({a_bordo})."}},
            {Check::WaitOutsideRegulation, "K-CTR-021", E, N, "Attesa concordata fuori dai limiti del regolamento",
             {"L'attesa concordata con {scuola} prima delle lezioni supera il massimo del regolamento {regolamento}, "
              "che vale per il contratto {contratto} (minuti concordati: {minuti}; massimo: {massimo}).",
              "L'attesa concordata con {scuola} dopo le lezioni supera il massimo del regolamento {regolamento}, "
              "che vale per il contratto {contratto} (minuti concordati: {minuti}; massimo: {massimo})."}},
            {Check::MaxSeatsBelowMin, "K-CTR-022", E, N, "Mezzo massimo piu' piccolo del mezzo minimo",
             {"Il mezzo massimo ammesso e' piu' piccolo del mezzo minimo (posti del massimo: {massimo}; del minimo: {minimo}).",
              "Con le regole del contratto {contratto} il mezzo massimo ammesso e' piu' piccolo del mezzo minimo "
              "(posti del massimo: {massimo}; del minimo: {minimo})."}},
            {Check::TransferNotAuthorized, "K-CTR-023", E, N, "Trasbordo in un luogo non autorizzato",
             {"Il trasbordo {trasbordo} avviene in {luogo}, che non e' un punto di trasbordo autorizzato."}},
            {Check::TransferHalfMissing, "K-CTR-024", E, N, "Trasbordo senza la sua meta'",
             {"Il trasbordo {trasbordo} ha la consegna (corsa {corsa}), ma nessuna corsa lo riceve.",
              "Il trasbordo {trasbordo} ha la ricezione (corsa {corsa}), ma nessuna corsa lo consegna."}},
            {Check::TransferLateReceiver, "K-CTR-025", E, N, "Corsa che riceve un trasbordo arrivando dopo la corsa che consegna",
             {"Trasbordo {trasbordo}: la corsa {corsa} arriva a {luogo} alle {arrivo}, ma deve esserci entro le {limite}, "
              "perche' la corsa {corsa_consegna} consegna alle {consegna} (margine del pacchetto in minuti: {margine})."}},
            {Check::TransferHalvesDiffer, "K-CTR-026", E, N, "Meta' del trasbordo che non corrispondono",
             {"Il trasbordo {trasbordo} ha piu' di una consegna.", "Il trasbordo {trasbordo} ha piu' di una ricezione.",
              "Il trasbordo {trasbordo} consegna e riceve sulla stessa corsa {corsa}.",
              "Le due meta' del trasbordo {trasbordo} sono in luoghi diversi: {luogo} e {altro}.",
              "Le due meta' del trasbordo {trasbordo} non hanno lo stesso gruppo, lo stesso numero di persone "
              "o lo stesso passeggero."}},
            {Check::TransferReceiverLeaves, "K-CTR-027", E, N, "Corsa che riceve un trasbordo e riparte prima della consegna",
             {"Trasbordo {trasbordo}: la corsa {corsa} riparte alle {partenza}, prima che la corsa {corsa_consegna} "
              "arrivi alle {consegna}."}},
            {Check::ConnectionWaitArrival, "K-CTR-028", E, N, "Coincidenza da aspettare: partenza prima dell'arrivo",
             {"La corsa {corsa} parte da {luogo} alle {partenza}, prima dell'arrivo di {servizio} alle {arrivo}, "
              "che deve aspettare."}},
            {Check::ConnectionGuaranteeDeparture, "K-CTR-029", E, N, "Coincidenza da garantire: arrivo dopo la partenza",
             {"La corsa {corsa} arriva a {luogo} alle {arrivo}, dopo la partenza di {servizio} alle {partenza}, "
              "che deve garantire."}},
            {Check::ConnectionNotOnTrip, "K-CTR-030", E, N, "Coincidenza in un luogo dove la corsa non passa",
             {"La coincidenza con {servizio} e' a {luogo}, dove la corsa {corsa} non passa."}},
            {Check::CountsDifferFromPassengers, "K-CTR-031", E, N, "Conteggi del gruppo diversi dai passeggeri",
             {"Gruppo {gruppo}, fermata {luogo}: le persone che salgono ({conteggio}) non corrispondono ai passeggeri "
              "che salgono qui ({passeggeri}).",
              "Gruppo {gruppo}, fermata {luogo}: le persone che scendono ({conteggio}) non corrispondono ai passeggeri "
              "che scendono qui ({passeggeri}).",
              "Gruppo {gruppo}, fermata {luogo}: le persone sedute sulla carrozzina non corrispondono ai passeggeri che "
              "salgono qui seduti sulla carrozzina.",
              "Il passeggero {passeggero} sale o scende a {luogo}, che non e' tra le fermate del gruppo {gruppo}.",
              "Gruppo {gruppo}, fermata {luogo}: gli ausili piegati da caricare (carrozzine piegate comprese) non "
              "corrispondono a quelli dei passeggeri che salgono qui."},
             {{2, PA}, {4, PA}}},
            {Check::LotServiceOtherContract, "K-CTR-032", E, N, "Servizio di un altro contratto in un gruppo di servizi",
             {"Il servizio {servizio} ha il contratto {contratto}, ma il gruppo di servizi {gruppo_servizi} e' del "
              "contratto {contratto_gruppo}."}},
            {Check::ServiceInTwoLots, "K-CTR-033", E, N, "Servizio in due gruppi di servizi",
             {"Il servizio {servizio} e' sia nel gruppo di servizi {altro} sia in {gruppo_servizi}."}},
            {Check::GroupNotOnTripServices, "K-CTR-034", E, N, "Corsa che trasporta un gruppo di un servizio che non soddisfa",
             {"La corsa {corsa} trasporta il gruppo {gruppo} del servizio {servizio}, ma non soddisfa quel servizio."}},
            {Check::AssistantNotRequested, "K-CTR-047", E, PA, "Passeggero con un'esigenza che il suo gruppo non riporta",
             {"Il passeggero {passeggero} ha bisogno dell'assistente a bordo, ma il gruppo {gruppo} non richiede "
              "l'assistente."}},
            {Check::NotUtf8, "K-CTR-048", E, N, "File non salvato in UTF-8",
             {"Il file non e' salvato in UTF-8: alla riga {riga}, colonna {colonna} c'e' un carattere scritto in un'altra "
              "codifica (succede con i file salvati come \"ANSI\" da programmi vecchi). Salvarlo di nuovo scegliendo la "
              "codifica UTF-8. Il file non e' stato letto.",
              "Il file e' salvato in UTF-16 (il \"Unicode\" del Blocco note), ma il formato accetta solo UTF-8. Salvarlo di "
              "nuovo scegliendo la codifica UTF-8. Il file non e' stato letto."}},
            {Check::CalendarCombinationCycle, "K-CTR-049", E, N, "Calendario combinato con se stesso",
             {"Il calendario {calendario} aggiunge o toglie i giorni di se stesso, direttamente o attraverso altri "
              "calendari: non si puo' sapere in quali giorni vale."}},
            {Check::ExpiresInPeriod, "K-CTR-050", A, N, "Scadenza dentro il periodo del pacchetto",
             {"L'autorizzazione del mezzo {mezzo} all'uso {uso} (distrazione) scade il {data}, dentro il periodo del pacchetto: "
              "dopo quella data il mezzo non puo' fare i servizi che chiedono quell'uso.",
              "L'abilitazione {tipo} della persona {persona} scade il {data}, dentro il periodo del pacchetto: dopo quella data "
              "non puo' guidare i mezzi che la richiedono.",
              "L'autorizzazione del committente {committente} della persona {persona} scade il {data}, dentro il periodo del "
              "pacchetto: dopo quella data non puo' guidare nei servizi che la richiedono."},
             {{1, PE}, {2, PE}},
             {{1, "Un'abilitazione o un'autorizzazione della persona {persona} scade dentro il periodo del pacchetto (dati "
                  "personali: riservati)."},
              {2, "Un'abilitazione o un'autorizzazione della persona {persona} scade dentro il periodo del pacchetto (dati "
                  "personali: riservati)."}}},
            {Check::FileTooBig, "K-CTR-053", E, N, "File troppo grande",
             {"Il file e' piu' grande di {massimo} MB: e' certamente un errore (un file sbagliato o di un altro programma) e "
              "non e' stato letto."}},
            {Check::VehicleWithoutUse, "K-CTR-051", A, N, "Mezzo senza l'uso della carta di circolazione",
             {"Il mezzo {mezzo} non ha l'uso della carta di circolazione: con le norme {norme} l'uso richiesto dai servizi e i "
              "requisiti di guida che dipendono dall'uso non si possono controllare."}},
            {Check::CompanyWithoutNorms, "K-CTR-052", A, N, "Azienda senza le norme del paese",
             {"L'azienda {azienda} non indica le norme del paese (aziende.json): usi dei mezzi, abilitazioni e requisiti di guida "
              "dei suoi dati non si possono controllare."}},
            // Turni (K-008)
            {Check::ShiftPartInvalid, "K-CTR-054", E, N, "Pezzo di corsa che non si trova nella corsa",
             {"Nel turno {turno} il pezzo della corsa {corsa} comincia o finisce a {luogo}, dove la corsa non passa.",
              "Nel turno {turno} il pezzo della corsa {corsa} comincia o finisce a {luogo}, dove la corsa passa piu' volte: "
              "scrivere anche l'orario del passaggio.",
              "Nel turno {turno} il pezzo della corsa {corsa} finisce a {luogo} prima di cominciare (o nello stesso passaggio)."}},
            {Check::ShiftItemsOrder, "K-CTR-055", E, N, "Corse e attivita' di un turno fuori ordine o sovrapposte",
             {"Nel turno {turno} un elemento comincia alle {orario}, prima dell'elemento precedente ({precedente}): corse e "
              "attivita' vanno scritte in ordine di orario.",
              "Nel turno {turno} un elemento comincia alle {orario}, prima che finisca il precedente ({fine}), {giorni}."}},
            {Check::ShiftVehicleInconsistent, "K-CTR-056", E, N, "Mezzo del turno che non va con il suo ruolo",
             {"Il turno {turno} e' da autista e ha delle corse, ma il mezzo e' \"nessuno\".",
              "Il turno {turno} e' da assistente o da secondo autista: il mezzo deve essere \"nessuno\" (e' quello del turno "
              "dell'autista).",
              "Il turno {turno} usa il mezzo abituale della persona, ma {persona} non ha un mezzo abituale, {giorni}."}},
            {Check::RotationMalformed, "K-CTR-057", E, N, "Rotazione scritta in modo incoerente",
             {"Nella rotazione {rotazione} la riga di {persona} ha {passi} passi, ma il ciclo ne ha {lunghezza}.",
              "Nella rotazione {rotazione} la persona {persona} compare in piu' righe.",
              "La rotazione {rotazione} ha i passi di un mese, ma parte il {partenza}: deve partire il primo giorno di un mese."}},
            {Check::AssignmentOnDayWithoutShift, "K-CTR-058", E, N, "Assegnazione in un giorno in cui il turno non c'e'",
             {"L'assegnazione del turno {turno} il {data} e' fuori dal periodo del pacchetto.",
              "L'assegnazione del turno {turno} il {data} e' in un giorno in cui il turno non c'e' (calendario del turno, o nessuna "
              "corsa e attivita' quel giorno)."}},
            {Check::ShiftAtShelterNever, "K-CTR-059", E, N, "Turno in un ricovero esterno con l'impostazione \"mai\"",
             {"Il turno {turno} comincia o finisce a {luogo}, un ricovero esterno, ma l'azienda {azienda} vuole i mezzi sempre in "
              "rimessa (ricovero vicino a casa: mai)."}},
            {Check::PeriodTooLongForShifts, "K-CTR-060", E, N, "Periodo del pacchetto troppo lungo per i turni",
             {"Il periodo del pacchetto dura {giorni} giorni: con i turni al massimo {massimo} (tre anni). I controlli giorno per "
              "giorno sui turni non sono stati fatti."}},
            {Check::TripPartsGap, "K-CTR-061", E, N, "Corsa divisa in pezzi con un buco",
             {"La corsa {corsa} e' divisa in pezzi tra i turni, ma nessun turno la fa da {da} a {a}, {giorni}."}},
            {Check::TripInTwoShifts, "K-CTR-062", E, N, "Stessa corsa o pezzo di corsa in due turni nello stesso giorno",
             {"La corsa {corsa} (o una sua parte) e' sia nel turno {turno} sia nel turno {altro}, {giorni}."}},
            {Check::PersonOverlap, "K-CTR-063", E, N, "Persona in due turni che si sovrappongono",
             {"La persona {persona} e' nel turno {turno} e nel turno {altro}, che si sovrappongono, {giorni}.",
              "La persona {persona} e' nel turno {turno}, che si sovrappone al turno {altro} del giorno prima, "
              "a cavallo della mezzanotte, {giorni}."}},
            {Check::VehicleOverlap, "K-CTR-064", E, N, "Mezzo in due turni che si sovrappongono",
             {"Il mezzo {mezzo} e' nel turno {turno} e nel turno {altro}, che si sovrappongono, {giorni}.",
              "Il mezzo {mezzo} e' nel turno {turno}, che si sovrappone al turno {altro} del giorno prima, a "
              "cavallo della mezzanotte, {giorni}."}},
            {Check::RotationTwoPersons, "K-CTR-065", E, N, "Due persone nello stesso turno nello stesso giorno",
             {"Le rotazioni danno il turno {turno} sia a {persona} sia a {altra}, {giorni}."}},
            {Check::TwoAssignmentsSameShift, "K-CTR-066", E, N, "Due servizi fissi o due assegnazioni del giorno sullo stesso turno",
             {"Il turno {turno} ha due servizi fissi nello stesso giorno (l'altro e' {altro}), {giorni}.",
              "Il turno {turno} ha due assegnazioni il {data}: va scritta una sola, con la persona, il mezzo o tutti e due."}},
            {Check::DayAssignmentAgainstFact, "K-CTR-067", E, N, "Assegnazione del singolo giorno contro un fatto",
             {"L'assegnazione del {data} da' il turno {turno} a {persona}, che quel giorno e' assente ({tipo}).",
              "L'assegnazione del {data} da' il turno {turno} a {persona}, che non e' disponibile in quegli orari ({orari}).",
              "L'assegnazione del {data} da' il turno {turno} a {persona}, ma il suo prestito all'azienda {azienda} non vale quel "
              "giorno.",
              "L'assegnazione del {data} da' il turno {turno} a {persona}, ma quel giorno l'abilitazione {tipo} e' scaduta.",
              "L'assegnazione del {data} da' il turno {turno} a {persona}, ma quel giorno l'autorizzazione del committente {tipo} "
              "e' scaduta.",
              "L'assegnazione del {data} mette il mezzo {mezzo} nel turno {turno}, ma quel giorno il mezzo e' in fermo ({tipo}).",
              "L'assegnazione del {data} mette il mezzo {mezzo} nel turno {turno}, ma quel giorno non vale la sua autorizzazione "
              "all'uso {tipo} (distrazione) che i servizi del turno chiedono.",
              "L'assegnazione del {data} mette il mezzo {mezzo} nel turno {turno}, ma quel giorno non vale il suo prestito "
              "all'azienda {tipo}."},
             {{0, PE}, {1, PE}, {3, PE}, {4, PE}},
             {{0, "L'assegnazione del {data} da' il turno {turno} a {persona}, che quel giorno non puo' farlo (dati personali: "
                  "riservati)."},
              {1, "L'assegnazione del {data} da' il turno {turno} a {persona}, che quel giorno non puo' farlo (dati personali: "
                  "riservati)."},
              {3, "L'assegnazione del {data} da' il turno {turno} a {persona}, che quel giorno non puo' farlo (dati personali: "
                  "riservati)."},
              {4, "L'assegnazione del {data} da' il turno {turno} a {persona}, che quel giorno non puo' farlo (dati personali: "
                  "riservati)."}}},
            {Check::DriverWithoutLicence, "K-CTR-068", E, PE, "Autista senza l'abilitazione o l'autorizzazione richiesta",
             {"Nel turno {turno} la persona {persona} guida il mezzo {mezzo}, ma non ha le abilitazioni richieste dalle norme "
              "(serve una di: {alternative}), {giorni}.",
              "Nel turno {turno} la persona {persona} guida per il servizio {servizio}, che chiede un'autorizzazione del "
              "committente {committente}, ma non ce l'ha, {giorni}."},
             {},
             {{0, "Nel turno {turno} la persona {persona} guida il mezzo {mezzo} senza un'abilitazione richiesta dalle norme "
                  "(dati personali: riservati), {giorni}."},
              {1, "Nel turno {turno} la persona {persona} guida per il servizio {servizio} senza l'autorizzazione del committente "
                  "{committente}, {giorni}."}}},
            {Check::VehicleWithoutUseForService, "K-CTR-069", E, N, "Mezzo senza l'uso richiesto dal servizio",
             {"Nel turno {turno} il mezzo {mezzo} (uso {uso}) fa il servizio {servizio}, che chiede l'uso {usi}, e non ha "
              "un'autorizzazione a quell'uso (distrazione), {giorni}."}},
            {Check::PairingFixedViolated, "K-CTR-070", E, N, "Turno ricorrente contro l'abbinamento autista-mezzo fisso",
             {"Nel turno {turno} la persona {persona} guida il mezzo {mezzo}, ma l'azienda ha l'abbinamento fisso e il suo mezzo "
              "e' un altro, {giorni}.",
              "Nel turno {turno} la persona {persona} guida il mezzo {mezzo}, ma l'azienda ha l'abbinamento fisso e la persona non "
              "ha un mezzo abituale, {giorni}."}},
            {Check::PersonWithoutCompetence, "K-CTR-071", E, PA, "Persona senza una competenza richiesta",
             {"Nel turno {turno} la persona {persona} non ha la competenza {competenza}, richiesta dal turno o dal servizio, {giorni}."},
             {},
             {{0, "Nel turno {turno} la persona {persona} non ha una competenza richiesta dal turno o dal servizio (dati riservati), "
                  "{giorni}."}}},
            {Check::VehicleAgainstConstraint, "K-CTR-072", E, N, "Mezzo usato contro il suo vincolo di destinazione",
             {"Nel turno {turno} il mezzo {mezzo} fa il servizio {servizio}, che il suo vincolo di destinazione non ammette, {giorni}."}},
            {Check::OtherCompanyWithoutLoan, "K-CTR-073", E, N, "Persona o mezzo di un'altra azienda senza prestito dichiarato",
             {"Il turno {turno} e' dell'azienda {azienda}, ma la persona {persona} e' di un'altra azienda e non ha un prestito a "
              "{azienda}, {giorni}.",
              "Il turno {turno} e' dell'azienda {azienda}, ma il mezzo {mezzo} e' di un'altra azienda e non ha un prestito a "
              "{azienda}, {giorni}."}},
            {Check::BlockedTripOtherVehicle, "K-CTR-074", E, N, "Corsa bloccata su un mezzo e fatta con un altro",
             {"La corsa {corsa} e' bloccata sul mezzo {mezzo} (blocco {blocco}), ma il turno {turno} la fa con il mezzo {altro}, "
              "{giorni}."}},
            {Check::IncompatibleBlocks, "K-CTR-075", E, N, "Due blocchi incompatibili sulla stessa corsa",
             {"La corsa {corsa} e' bloccata sul mezzo {mezzo} (blocco {blocco}) e sul mezzo {altro_mezzo} (blocco {altro}) in "
              "periodi che si sovrappongono."}},
            {Check::PersonRoleMismatch, "K-CTR-076", E, PE, "Persona con un ruolo diverso da quello del turno",
             {"Il turno {turno} e' da autista, ma la persona {persona} non e' autista, {giorni}.",
              "Il turno {turno} e' da assistente, ma la persona {persona} non e' assistente, {giorni}."},
             {},
             {{0, "Il turno {turno} chiede un ruolo che la persona {persona} non ha (dati personali: riservati), {giorni}."},
              {1, "Il turno {turno} chiede un ruolo che la persona {persona} non ha (dati personali: riservati), {giorni}."}}},
            {Check::ShiftUncovered, "K-CTR-077", A, N, "Turno scoperto",
             {"Il turno {turno} e' scoperto {giorni}: nessuna rotazione, servizio fisso o assegnazione del singolo giorno lo "
              "assegna a una persona."}},
            {Check::RecurringShiftPersonFact, "K-CTR-078", A, N, "Turno ricorrente scoperto per un fatto con le date",
             {"Il turno {turno} e' scoperto {giorni}: la persona prevista {persona} e' assente ({tipo}). Rimedio: un'assegnazione "
              "del singolo giorno.",
              "Il turno {turno} e' scoperto {giorni}: la persona prevista {persona} non e' disponibile in quegli orari ({orari}). "
              "Rimedio: un'assegnazione del singolo giorno.",
              "Il turno {turno} e' scoperto {giorni}: il prestito della persona prevista {persona} all'azienda {azienda} non vale. "
              "Rimedio: un'assegnazione del singolo giorno.",
              "Il turno {turno} e' scoperto {giorni}: l'abilitazione {tipo} della persona prevista {persona} e' scaduta. Rimedio: "
              "un'assegnazione del singolo giorno.",
              "Il turno {turno} e' scoperto {giorni}: l'autorizzazione del committente {tipo} della persona prevista {persona} e' "
              "scaduta. Rimedio: un'assegnazione del singolo giorno."},
             {{0, PE}, {1, PE}, {3, PE}, {4, PE}},
             {{0, "Il turno {turno} e' scoperto {giorni}: la persona prevista {persona} non puo' farlo (dati personali: riservati). "
                  "Rimedio: un'assegnazione del singolo giorno."},
              {1, "Il turno {turno} e' scoperto {giorni}: la persona prevista {persona} non puo' farlo (dati personali: riservati). "
                  "Rimedio: un'assegnazione del singolo giorno."},
              {3, "Il turno {turno} e' scoperto {giorni}: la persona prevista {persona} non puo' farlo (dati personali: riservati). "
                  "Rimedio: un'assegnazione del singolo giorno."},
              {4, "Il turno {turno} e' scoperto {giorni}: la persona prevista {persona} non puo' farlo (dati personali: riservati). "
                  "Rimedio: un'assegnazione del singolo giorno."}}},
            {Check::RecurringShiftVehicleFact, "K-CTR-079", A, N, "Turno ricorrente senza mezzo per un fatto con le date",
             {"Il turno {turno} e' senza mezzo {giorni}: il mezzo {mezzo} e' in fermo ({tipo}). Rimedio: un'assegnazione del "
              "singolo giorno con un altro mezzo.",
              "Il turno {turno} e' senza mezzo {giorni}: l'autorizzazione del mezzo {mezzo} all'uso {tipo} (distrazione) non vale. "
              "Rimedio: un'assegnazione del singolo giorno con un altro mezzo.",
              "Il turno {turno} e' senza mezzo {giorni}: il prestito del mezzo {mezzo} all'azienda {tipo} non vale. Rimedio: "
              "un'assegnazione del singolo giorno con un altro mezzo."}},
            {Check::TripWithoutShift, "K-CTR-080", A, N, "Corsa senza turno",
             {"La corsa {corsa} non e' in nessun turno da autista {giorni}."}},
            {Check::PreferenceNotRespected, "K-CTR-081", A, PE, "Preferenza di una persona non rispettata",
             {"Il turno {turno} non rispetta una preferenza della persona {persona}, {giorni}.",
              "L'assegnazione del singolo giorno del turno {turno} non rispetta una preferenza della persona {persona}, {giorni} "
              "(la scelta dell'utente vince)."}},
            {Check::PairingNotRespected, "K-CTR-082", A, N, "Abbinamento autista-mezzo non rispettato",
             {"Nel turno {turno} la persona {persona} guida il mezzo {mezzo}, non il suo mezzo abituale (abbinamento preferito), "
              "{giorni}.",
              "Nel turno {turno} la persona {persona} guida il mezzo {mezzo} e non ha un mezzo abituale (abbinamento preferito), "
              "{giorni}.",
              "Un'assegnazione del singolo giorno mette la persona {persona} sul mezzo {mezzo} nel turno {turno}, contro "
              "l'abbinamento fisso dell'azienda (la scelta dell'utente vince), {giorni}.",
              "Un'assegnazione del singolo giorno mette la persona {persona}, che non ha un mezzo abituale, sul mezzo {mezzo} nel "
              "turno {turno}, contro l'abbinamento fisso dell'azienda (la scelta dell'utente vince), {giorni}."}},
            {Check::ShiftVehicleToAssign, "K-CTR-083", A, N, "Turno con il mezzo da assegnare",
             {"Il turno {turno} ha il mezzo da assegnare {giorni}."}},
            {Check::PersonWithoutShifts, "K-CTR-084", A, N, "Persona senza turni",
             {"La persona {persona} non fa nessun turno nel periodo del pacchetto."}},
            {Check::VehicleNeverUsed, "K-CTR-085", A, N, "Mezzo mai usato che non e' di scorta",
             {"Il mezzo {mezzo} non e' in nessun turno nel periodo del pacchetto e non e' segnato come mezzo di scorta."}},
            {Check::EscortWithoutPerson, "K-CTR-086", A, N, "Corsa con l'assistente dell'azienda senza persona assegnata",
             {"Nel turno {turno} la corsa {corsa} ha l'assistente fornito dall'azienda, ma nessun turno da assistente con una "
              "persona la copre, {giorni}."}},
            {Check::EscortProviderDiffers, "K-CTR-087", A, N, "Assistente fornito da chi il contratto non dice",
             {"Nel turno {turno} l'assistente della corsa {corsa} e' dell'azienda, ma il contratto {contratto} dice che lo fornisce "
              "il committente, {giorni}.",
              "Nel turno {turno} l'assistente della corsa {corsa} e' del committente, ma il contratto {contratto} dice che lo "
              "fornisce l'azienda, {giorni}.",
              "Il turno da assistente {turno} copre la corsa {corsa}, ma il contratto {contratto} dice che l'assistente lo fornisce "
              "il committente, {giorni}."}},
            // Lettura blindata (K-009)
            {Check::NotRegularFile, "K-CTR-088", E, N, "Non e' un file normale",
             {"Al posto del file c'e' qualcosa che non e' un file normale (un dispositivo del sistema, una \"pipe\" o "
              "simili): non e' stato letto. Mettere al suo posto il file di dati."}},
            {Check::LinkOutsideFolder, "K-CTR-089", E, N, "Collegamento a un file fuori dalla cartella del pacchetto",
             {"Il file e' un collegamento a un file che sta fuori dalla cartella del pacchetto: non e' stato letto. Copiare il "
              "file dentro la cartella del pacchetto."}},
            {Check::DirectoryInsteadOfFile, "K-CTR-090", E, N, "Cartella al posto di un file del pacchetto",
             {"C'e' una cartella che si chiama {file}, come un file del pacchetto: il file non e' stato letto. Togliere o "
              "rinominare la cartella e mettere al suo posto il file."}},
            // Cassaforte dei dati personali (K-010)
            {Check::VaultMissing, "K-CTR-091", E, N, "Manca la cassaforte",
             {"Il manifesto dice che il pacchetto ha una cassaforte, ma il file cassaforte.kzc non c'e': i dati personali e "
              "particolari mancano (es. cartella copiata a meta'). Rimettere il file della cassaforte preso dalla stessa copia "
              "del pacchetto."}},
            {Check::VaultUndeclared, "K-CTR-092", E, N, "Cassaforte non dichiarata nel manifesto",
             {"Nella cartella c'e' il file della cassaforte (cassaforte.kzc), ma il manifesto non la dichiara (\"cassaforte\": "
              "false o manifesto della versione 1): il manifesto e' di un'altra copia del pacchetto, e i dati personali e "
              "particolari non si leggono. Rimettere il manifesto di questa copia."}},
            {Check::VaultGenerationMismatch, "K-CTR-093", E, N, "Cassaforte e file normali di chiusure diverse",
             {"Il manifesto e' della chiusura numero {manifesto}, la cassaforte della chiusura numero {cassaforte}: i file "
              "normali e la cassaforte vengono da copie diverse del pacchetto (es. una copia vecchia rimessa al suo posto, o "
              "una richiusura interrotta). Rimettere la copia giusta, o rifare la richiusura (kzero richiudi)."}},
            {Check::VaultValueOrphan, "K-CTR-094", E, N, "Valore della cassaforte senza il suo elemento",
             {"Un valore personale della cassaforte per {file} non trova il suo elemento ({elemento}): l'elemento e' stato tolto "
              "o cambiato nei file normali dopo la chiusura. Il valore non e' stato messo da nessuna parte. Rimettere "
              "l'elemento com'era, oppure correggere i dati con un'uscita in chiaro e una richiusura (kzero esporta, kzero "
              "richiudi).",
              "Un valore particolare della cassaforte per {file} non trova il suo elemento ({elemento}): l'elemento e' stato "
              "tolto o cambiato nei file normali dopo la chiusura. Il valore non e' stato messo da nessuna parte. Rimettere "
              "l'elemento com'era, oppure correggere i dati con un'uscita in chiaro e una richiusura (kzero esporta, kzero "
              "richiudi).",
              "Un valore personale della cassaforte per {file} trova piu' elementi uguali ({elemento}): non si sa a quale "
              "va. Il valore non e' stato messo da nessuna parte. Correggere i dati con un'uscita in chiaro e una "
              "richiusura (kzero esporta, kzero richiudi).",
              "Un valore particolare della cassaforte per {file} trova piu' elementi uguali ({elemento}): non si sa a quale "
              "va. Il valore non e' stato messo da nessuna parte. Correggere i dati con un'uscita in chiaro e una "
              "richiusura (kzero esporta, kzero richiudi)."},
             {{1, PA}, {3, PA}}},
            {Check::VaultValueConflict, "K-CTR-095", E, N, "Valore sia nella cassaforte sia in chiaro",
             {"Un valore personale di {file} c'e' sia nella cassaforte sia in chiaro nel file normale: non si sa quale vale. "
              "Togliere il valore in chiaro, oppure correggere i dati con un'uscita in chiaro e una richiusura (kzero "
              "esporta, kzero richiudi).",
              "Un valore particolare di {file} c'e' sia nella cassaforte sia in chiaro nel file normale: non si sa quale vale. "
              "Togliere il valore in chiaro, oppure correggere i dati con un'uscita in chiaro e una richiusura (kzero "
              "esporta, kzero richiudi)."},
             {{1, PA}}},
            {Check::ClearPersonalData, "K-CTR-096", A, N, "Dati personali o particolari in chiaro",
             {"Il pacchetto contiene {personali} valori personali e {particolari} particolari in chiaro e il manifesto non dice "
              "\"dati_inventati\": se sono dati veri, vanno chiusi nella cassaforte (kzero chiudi), e poi la cartella in chiaro "
              "va cancellata.",
              "Nei file normali, accanto alla cassaforte, ci sono {personali} valori personali e {particolari} particolari in "
              "chiaro (es. scritti dopo la chiusura): vanno messi nella cassaforte con un'uscita in chiaro e una richiusura "
              "(kzero esporta, kzero richiudi)."}},
            {Check::RetentionExpired, "K-CTR-097", A, N, "Data di conservazione dei dati passata",
             {"La data di conservazione dei dati personali e particolari della cassaforte ({data}) e' passata: se i dati non "
              "servono piu', cancellare la cassaforte (kzero cancella); se c'e' un motivo per tenerli, scrivere una data nuova "
              "(kzero conservazione)."}},
            // Contenuto: avvisi
            {Check::PlaceWithoutCoordinates, "K-CTR-035", A, N, "Luogo senza coordinate",
             {"Il luogo {luogo} non ha le coordinate: serviranno al calcolo delle distanze."}},
            {Check::DwellTimesUndefined, "K-CTR-036", A, N, "Tempi di sosta non definiti",
             {"Il pacchetto non ha i tempi di sosta (file tempi_sosta.json): serviranno al calcolo.",
              "I tempi di sosta sono incompleti (parte fissa, salita e discesa per studenti, infanzia e disabili, "
              "scarico a scuola): serviranno al calcolo."}},
            {Check::MaxRideUndefined, "K-CTR-037", A, N, "Durata massima a bordo non definita",
             {"Per il gruppo {gruppo} del servizio {servizio} nessuno fissa la durata massima a bordo (scuola, servizio, "
              "contratto o regolamento): servira' al calcolo."}},
            {Check::StopNeverUsed, "K-CTR-038", A, N, "Fermata mai usata",
             {"La fermata {luogo} non e' usata da nessuna corsa e da nessuna domanda."}},
            {Check::ServiceWithoutTrips, "K-CTR-039", A, N, "Servizio senza corse", {"Nessuna corsa soddisfa il servizio {servizio}."}},
            {Check::TripWithoutService, "K-CTR-040", A, N, "Corsa che non soddisfa nessun servizio",
             {"La corsa {corsa} non soddisfa nessun servizio."}},
            {Check::GroupWithoutPassengers, "K-CTR-041", A, N, "Gruppo con zero passeggeri",
             {"Il gruppo {gruppo} del servizio {servizio} non ha passeggeri."}},
            {Check::DemandDiffersFromTrips, "K-CTR-042", A, N, "Domanda diversa dalle persone delle corse",
             {"Gruppo {gruppo}, fermata {luogo}: le persone che salgono secondo la domanda ({domanda}) non corrispondono "
              "a quelle caricate dalle corse del calendario {calendario} ({corse}).",
              "Gruppo {gruppo}, fermata {luogo}: le persone che scendono secondo la domanda ({domanda}) non corrispondono "
              "a quelle scaricate dalle corse del calendario {calendario} ({corse}).",
              "Gruppo {gruppo}: le corse del calendario {calendario} fanno salire o scendere persone a {luogo} ({corse}), "
              "che non e' tra le fermate della domanda."}},
            {Check::TransferVerbalOnly, "K-CTR-043", A, N, "Punto di trasbordo autorizzato a voce, senza documento",
             {"Il punto di trasbordo {luogo} e' autorizzato solo a voce: manca il documento."}},
            {Check::VehicleWithoutCosts, "K-CTR-044", A, N, "Mezzo senza costo fisso giornaliero o senza costo al km",
             {"Il mezzo {mezzo} non ha il costo fisso giornaliero.", "Il mezzo {mezzo} non ha il costo al km.",
              "Il mezzo {mezzo} non ha ne' il costo fisso giornaliero ne' il costo al km."}},
            {Check::CompanyWithoutVehicles, "K-CTR-045", A, N, "Azienda senza mezzi", {"L'azienda {azienda} non ha mezzi nel pacchetto."}},
            {Check::ContractWithoutRegulation, "K-CTR-046", A, N, "Contratto senza regolamento valido per una parte del periodo",
             {"Il regolamento {regolamento} non vale per una parte del periodo del contratto {contratto}: dal {dal} al {al}."}},
        };
        std::sort(c.begin(), c.end(), [](const DefinizioneControllo& a, const DefinizioneControllo& b) { return a.codice < b.codice; });
        return c;
    }();
    return catalogo;
}

const DefinizioneControllo& definizione_controllo(Check c) {
    for (const auto& d : catalogo_controlli())
        if (d.controllo == c) return d;
    throw std::logic_error("controllo senza voce nel catalogo");
}

const char* nome_livello(Livello l) { return l == Livello::Errore ? "errore" : "avviso"; }

// --- Presentazione dei messaggi -----------------------------------------------------------------------

namespace {

const char* nome_elementi(kzero::Element e) {
    switch (e) {
        case kzero::Element::Company: return "le aziende";
        case kzero::Element::Place: return "i luoghi";
        case kzero::Element::Segment: return "i tratti di strada";
        case kzero::Element::Regulation: return "i regolamenti";
        case kzero::Element::Calendar: return "i calendari";
        case kzero::Element::School: return "le scuole e i centri";
        case kzero::Element::Client: return "i committenti";
        case kzero::Element::Contract: return "i contratti";
        case kzero::Element::Lot: return "i gruppi di servizi";
        case kzero::Element::Service: return "i servizi";
        case kzero::Element::Group: return "i gruppi di passeggeri";
        case kzero::Element::Passenger: return "i passeggeri";
        case kzero::Element::Trip: return "le corse";
        case kzero::Element::Vehicle: return "i mezzi";
        case kzero::Element::Depot: return "le rimesse";
        case kzero::Element::Transfer: return "i trasbordi";
        case kzero::Element::Holidays: return "i calendari delle festivita'";
        case kzero::Element::Norms: return "le norme";
        case kzero::Element::CompanySettings: return "le impostazioni delle aziende";
        case kzero::Element::Licence: return "le abilitazioni";
        case kzero::Element::VehicleUse: return "gli usi dei mezzi";
        case kzero::Element::ActivityType: return "i tipi di attivita'";
        case kzero::Element::AbsenceType: return "i tipi di assenza";
        case kzero::Element::StopType: return "i tipi di fermo";
        case kzero::Element::Competence: return "le competenze";
        case kzero::Element::Person: return "le persone";
        case kzero::Element::Shift: return "i turni";
        case kzero::Element::Rotation: return "le rotazioni";
        case kzero::Element::FixedAssignment: return "i servizi fissi";
        case kzero::Element::Block: return "i blocchi";
    }
    return "gli elementi";
}

// 12.5 -> "12,5"; al massimo 3 decimali.
std::string decimale(double x) {
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o.setf(std::ios::fixed);
    o.precision(3);
    o << x;
    std::string s = o.str();
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    for (char& c : s)
        if (c == '.') c = ',';
    return s;
}

}  // namespace

std::vector<std::pair<kzero::Day, kzero::Day>> periodi_di_giorni(const std::vector<kzero::Day>& giorni) {
    std::vector<std::pair<kzero::Day, kzero::Day>> out;
    for (const kzero::Day d : giorni) {
        if (!out.empty() && d == out.back().second + 1) out.back().second = d;
        else if (out.empty() || d > out.back().second) out.push_back({d, d});
    }
    return out;
}

std::string testo_giorni(const std::vector<kzero::Day>& giorni) {
    const auto periodi = periodi_di_giorni(giorni);
    const auto data = [](kzero::Day d) { return kzero::text_from_day(d); };
    const auto uno = [&](const std::pair<kzero::Day, kzero::Day>& p) {
        return p.first == p.second ? data(p.first) : "dal " + data(p.first) + " al " + data(p.second);
    };
    if (periodi.empty()) return "";
    const std::size_t n = giorni.size();
    if (n == 1) return "il " + data(giorni.front());
    if (periodi.size() == 1) return "dal " + data(periodi[0].first) + " al " + data(periodi[0].second) + " (" + std::to_string(n) + " giorni)";
    constexpr std::size_t kMassimo = 6;   // oltre, si mostrano i primi periodi; l'elenco completo e' nel JSON
    std::string elenco;
    for (std::size_t i = 0; i < periodi.size() && i < kMassimo; ++i) elenco += (i ? ", " : "") + uno(periodi[i]);
    if (periodi.size() <= kMassimo) return "in " + std::to_string(n) + " giorni (" + elenco + ")";
    return "in " + std::to_string(n) + " giorni tra il " + data(giorni.front()) + " e il " + data(giorni.back()) + " (i primi: " +
           elenco + "...)";
}

struct Presentatore::Dati {
    const kzero::Package* pacchetto = nullptr;
    const std::map<std::string, int>* versioni = nullptr;
    Visibilita visibilita;
    std::optional<kzero::Index> indice;
    std::optional<LegameSensibile> legame;   // passeggeri e case legati a un luogo sensibile (riservatezza.hpp)
};

Presentatore::Presentatore(const kzero::Package* pacchetto, const std::map<std::string, int>* versioni, Visibilita visibilita)
    : dati_(std::make_unique<Dati>()) {
    dati_->pacchetto = pacchetto;
    dati_->versioni = versioni;
    dati_->visibilita = visibilita;
    if (pacchetto) {
        dati_->indice.emplace(*pacchetto);
        dati_->legame.emplace(&*dati_->indice);
    }
}

Presentatore::~Presentatore() = default;

std::string Presentatore::riferimento(kzero::Element e, const std::string& codice) const {
    if (!dati_->indice) return codice;
    const kzero::Index& idx = *dati_->indice;
    const kzero::Source* src = nullptr;
    std::string nome, campo = "nome";
    const auto da = [&](const auto* x) {
        if (x) {
            src = &x->src;
            nome = x->name;
        }
    };
    switch (e) {
        case kzero::Element::Company: da(idx.company(codice)); break;
        case kzero::Element::Place: da(idx.place(codice)); break;
        case kzero::Element::Segment: da(idx.segment(codice)); break;
        case kzero::Element::Calendar: da(idx.calendar(codice)); break;
        case kzero::Element::Holidays: da(idx.holidays(codice)); break;
        case kzero::Element::Norms: da(idx.norms(codice)); break;
        case kzero::Element::Person: da(idx.person(codice)); break;
        case kzero::Element::Shift: da(idx.shift(codice)); break;
        case kzero::Element::School: da(idx.school(codice)); break;
        case kzero::Element::Client: da(idx.client(codice)); break;
        case kzero::Element::Contract: da(idx.contract(codice)); break;
        case kzero::Element::Lot: da(idx.lot(codice)); break;
        case kzero::Element::Service: da(idx.service(codice)); break;
        case kzero::Element::Group: da(idx.group(codice)); break;
        case kzero::Element::Passenger: da(idx.passenger(codice)); break;
        case kzero::Element::Trip: da(idx.trip(codice)); break;
        case kzero::Element::Regulation:
            if (const auto* r = idx.regulation(codice)) {
                src = &r->src;
                nome = r->title;
                campo = "titolo";
            }
            break;
        case kzero::Element::Vehicle:
            if (const auto* v = idx.vehicle(codice)) {
                src = &v->src;
                nome = v->plate;
                campo = "targa";
            }
            break;
        case kzero::Element::Depot:
        case kzero::Element::Transfer:
        case kzero::Element::CompanySettings:
        case kzero::Element::Licence:
        case kzero::Element::VehicleUse:
        case kzero::Element::ActivityType:
        case kzero::Element::AbsenceType:
        case kzero::Element::StopType:
        case kzero::Element::Competence:
        case kzero::Element::Rotation:
        case kzero::Element::FixedAssignment:
        case kzero::Element::Block: break;
    }
    if (!src || nome.empty() || !dati_->versioni) return codice;
    // La classe del nome viene dalla marcatura dello schema del file da cui viene l'elemento.
    const auto versione = dati_->versioni->find(src->file);
    const std::string schema = src->file.substr(0, src->file.rfind(".json"));
    if (versione == dati_->versioni->end()) return codice;
    const Classe classe = SchemiDati::incorporati().classe(schema, versione->second, src->pointer + "/" + campo);
    // Il nome di un passeggero o di una casa legati a un luogo sensibile vale come dato particolare (riservatezza.hpp).
    const bool legato = classe == Classe::Personale && dati_->legame &&
                        ((e == kzero::Element::Passenger && dati_->legame->passeggero(codice)) ||
                         (e == kzero::Element::Place && dati_->legame->casa(codice)));
    const bool mostra = visibile(classe, dati_->visibilita) && (!legato || visibile(Classe::Particolare, dati_->visibilita));
    return mostra ? codice + " (" + nome + ")" : codice;
}

std::string Presentatore::messaggio(const kzero::Problem& p) const {
    const DefinizioneControllo& d = definizione_controllo(p.check);
    if (p.variant < 0 || static_cast<std::size_t>(p.variant) >= d.messaggi.size())
        throw std::logic_error(d.codice + ": variante del messaggio inesistente");
    const auto personale = d.personali.find(p.variant);
    const std::string& modello = personale != d.personali.end() && !visibile(Classe::Personale, dati_->visibilita)
                                     ? personale->second
                                     : d.messaggi[static_cast<std::size_t>(p.variant)];
    std::string out;
    for (std::size_t i = 0; i < modello.size(); ++i) {
        if (modello[i] != '{') {
            out += modello[i];
            continue;
        }
        const std::size_t fine = modello.find('}', i);
        const std::string nome = modello.substr(i + 1, fine - i - 1);
        const auto it = std::find_if(p.params.begin(), p.params.end(), [&](const kzero::Param& x) { return x.name == nome; });
        if (it == p.params.end()) throw std::logic_error(d.codice + ": manca il valore {" + nome + "} del messaggio");
        const bool nascosto =
            (it->sensitivity == kzero::Param::Sensitivity::Personal && !visibile(Classe::Personale, dati_->visibilita)) ||
            (it->sensitivity == kzero::Param::Sensitivity::Particular && !visibile(Classe::Particolare, dati_->visibilita));
        std::string valore;
        if (nascosto) {
            valore = it->hidden.empty() ? std::string(kRiservato) : it->hidden;
        } else {
            switch (it->kind) {
                case kzero::Param::Kind::Text: valore = it->text; break;
                case kzero::Param::Kind::Number: valore = std::to_string(it->number); break;
                case kzero::Param::Kind::Decimal: valore = decimale(it->decimal); break;
                case kzero::Param::Kind::Time: valore = kzero::text_from_minutes(static_cast<kzero::Minutes>(it->number)); break;
                case kzero::Param::Kind::Date: valore = kzero::text_from_day(static_cast<kzero::Day>(it->number)); break;
                case kzero::Param::Kind::Ref: valore = riferimento(it->element, it->text); break;
                case kzero::Param::Kind::Elements: valore = nome_elementi(it->element); break;
                case kzero::Param::Kind::Days: valore = testo_giorni(it->days); break;
            }
        }
        // Un valore che finisce con il punto, seguito dal punto della frase: un punto solo.
        if (!valore.empty() && valore.back() == '.' && fine + 1 < modello.size() && modello[fine + 1] == '.') valore.pop_back();
        out += valore;
        i = fine;
    }
    return out;
}

}  // namespace kzero::api
