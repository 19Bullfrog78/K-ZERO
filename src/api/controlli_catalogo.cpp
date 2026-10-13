// K-Zero API - catalogo dei controlli e testo dei messaggi (vedi controlli_catalogo.hpp e docs/controlli.md).
#include "controlli_catalogo.hpp"

#include <algorithm>
#include <cmath>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "pacchetto.hpp"
#include "tempo.hpp"

namespace kzero::api {

using kzero::Check;

const std::vector<DefinizioneControllo>& catalogo_controlli() {
    // Nuovi controlli: codice successivo all'ultimo. Un codice non si cambia e non si riusa mai.
    static const std::vector<DefinizioneControllo> catalogo = [] {
        const Livello E = Livello::Errore, A = Livello::Avviso;
        std::vector<DefinizioneControllo> c = {
            // Forma dei file
            {Check::MissingManifest, "K-CTR-001", E, "Manca il manifesto",
             {"Manca il file pacchetto.json, il manifesto del pacchetto: senza manifesto il pacchetto non si puo' controllare."}},
            {Check::UnreadableFile, "K-CTR-002", E, "File illeggibile",
             {"Il file non si riesce a leggere (permessi o disco): non e' stato letto."}},
            {Check::InvalidJson, "K-CTR-003", E, "JSON non valido",
             {"Il file non e' un JSON valido: errore di scrittura alla riga {riga}, colonna {colonna}, o poco prima "
              "(es. una virgola o una parentesi mancante). Il file non e' stato letto."}},
            {Check::DuplicateKey, "K-CTR-004", E, "Campo ripetuto",
             {"Lo stesso campo compare due volte nello stesso oggetto: va scritto una volta sola. Il file non e' stato letto."}},
            {Check::WrongSchema, "K-CTR-005", E, "Schema del file sbagliato o mancante",
             {"Manca il campo \"schema\": per questo file deve essere \"{atteso}\". Il file non e' stato letto.",
              "Il file dichiara lo schema \"{trovato}\", ma questo file deve avere lo schema \"{atteso}\". Il file non e' stato letto."}},
            {Check::UnsupportedVersion, "K-CTR-006", E, "Versione dello schema non supportata",
             {"Versione dello schema {versione} non supportata (versioni lette da questo programma: {supportate}). "
              "Il file non e' stato letto.",
              "Manca il numero intero \"schema_versione\" (versioni lette da questo programma: {supportate}). "
              "Il file non e' stato letto."}},
            {Check::SchemaViolation, "K-CTR-007", E, "File non conforme allo schema",
             {"Non rispetta lo schema: {dettaglio}. Il file non e' stato usato."}},
            {Check::UnknownFile, "K-CTR-008", A, "File non riconosciuto",
             {"Il file {file} non fa parte del formato e non e' stato letto (nome scritto male?).",
              "La cartella {file} non fa parte del formato e non e' stata letta."}},
            // Contenuto: errori
            {Check::DuplicateCode, "K-CTR-009", E, "Codice doppio",
             {"Il codice {codice} compare due volte tra {elementi}: la prima volta in {prima}."}},
            {Check::UnknownReference, "K-CTR-010", E, "Riferimento a un codice inesistente",
             {"Il codice {codice} non esiste tra {elementi}."}},
            {Check::WrongPlaceType, "K-CTR-011", E, "Luogo del tipo sbagliato",
             {"Il luogo {luogo} non e' una scuola ne' un centro.", "Il luogo {luogo} non e' una rimessa ne' un ricovero esterno."}},
            {Check::InvertedPeriod, "K-CTR-012", E, "Fine prima dell'inizio",
             {"Il periodo finisce ({al}) prima di cominciare ({dal}).", "L'orario finisce ({alle}) prima di cominciare ({dalle})."}},
            {Check::DateOutsideCalendar, "K-CTR-013", E, "Data fuori dal periodo del calendario",
             {"La data aggiunta {data} e' fuori dal periodo del calendario {calendario} ({dal} - {al}).",
              "La data tolta {data} e' fuori dal periodo del calendario {calendario} ({dal} - {al}).",
              "La sospensione dal {inizio} al {fine} esce dal periodo del calendario {calendario} ({dal} - {al})."}},
            {Check::ContradictoryDate, "K-CTR-014", E, "Data contraddittoria nel calendario",
             {"Nel calendario {calendario} la data {data} e' sia aggiunta sia tolta.",
              "Nel calendario {calendario} la data aggiunta {data} cade in una sospensione."}},
            {Check::CalendarWithoutDays, "K-CTR-015", E, "Calendario senza nessun giorno",
             {"Il calendario {calendario} non vale in nessun giorno del periodo del pacchetto ({dal} - {al})."}},
            {Check::CalendarCycle, "K-CTR-016", E, "Calendario basato su se stesso",
             {"Il calendario {calendario} si basa su se stesso, direttamente o attraverso altri calendari."}},
            {Check::TimeGoesBack, "K-CTR-017", E, "Orario che torna indietro in una corsa",
             {"Nella corsa {corsa} l'orario {orario} non sta tra quello del passaggio precedente ({precedente}) "
              "e quello del successivo ({successivo}): l'orario torna indietro.",
              "Nella corsa {corsa} l'orario {orario} viene prima di quello del passaggio precedente ({precedente}): "
              "l'orario torna indietro.",
              "Nella corsa {corsa} la partenza ({partenza}) viene prima dell'arrivo ({arrivo}) nello stesso passaggio."}},
            {Check::AmbiguousStop, "K-CTR-018", E, "Nome o codice esterno di fermata che vale per due fermate",
             {"Il nome \"{nome}\" (scritto in forma normalizzata) appartiene a due fermate: {luogo} e {altro}.",
              "Il codice esterno \"{nome}\" (sistema/tipo/codice, in forma normalizzata) appartiene a due fermate: "
              "{luogo} e {altro}."}},
            {Check::NumbersOnlyPersonalData, "K-CTR-019", E, "Dati personali in un pacchetto \"solo numeri\"",
             {"Il pacchetto e' di livello \"solo numeri\": il luogo privato {luogo} (casa di una persona) non e' ammesso.",
              "Il pacchetto e' di livello \"solo numeri\": il gruppo {gruppo} elenca i passeggeri uno per uno "
              "({passeggeri}), cosa ammessa solo nel livello \"completo\"."}},
            {Check::NegativeOnBoard, "K-CTR-020", E, "Persone a bordo sotto zero",
             {"Nella corsa {corsa} le persone del gruppo {gruppo} che scendono ({scendono}) sono piu' di quelle a bordo "
              "({a_bordo})."}},
            {Check::WaitOutsideRegulation, "K-CTR-021", E, "Attesa concordata fuori dai limiti del regolamento",
             {"L'attesa concordata con {scuola} prima delle lezioni supera il massimo del regolamento {regolamento}, "
              "che vale per il contratto {contratto} (minuti concordati: {minuti}; massimo: {massimo}).",
              "L'attesa concordata con {scuola} dopo le lezioni supera il massimo del regolamento {regolamento}, "
              "che vale per il contratto {contratto} (minuti concordati: {minuti}; massimo: {massimo})."}},
            {Check::MaxSeatsBelowMin, "K-CTR-022", E, "Mezzo massimo piu' piccolo del mezzo minimo",
             {"Il mezzo massimo ammesso e' piu' piccolo del mezzo minimo (posti del massimo: {massimo}; del minimo: {minimo}).",
              "Con le regole del contratto {contratto} il mezzo massimo ammesso e' piu' piccolo del mezzo minimo "
              "(posti del massimo: {massimo}; del minimo: {minimo})."}},
            {Check::TransferNotAuthorized, "K-CTR-023", E, "Trasbordo in un luogo non autorizzato",
             {"Il trasbordo {trasbordo} avviene in {luogo}, che non e' un punto di trasbordo autorizzato."}},
            {Check::TransferHalfMissing, "K-CTR-024", E, "Trasbordo senza la sua meta'",
             {"Il trasbordo {trasbordo} ha la consegna (corsa {corsa}), ma nessuna corsa lo riceve.",
              "Il trasbordo {trasbordo} ha la ricezione (corsa {corsa}), ma nessuna corsa lo consegna."}},
            {Check::TransferLateReceiver, "K-CTR-025", E, "Corsa che riceve un trasbordo arrivando dopo la corsa che consegna",
             {"Trasbordo {trasbordo}: la corsa {corsa} arriva a {luogo} alle {arrivo}, ma deve esserci entro le {limite}, "
              "perche' la corsa {corsa_consegna} consegna alle {consegna} (margine del pacchetto in minuti: {margine})."}},
            {Check::TransferHalvesDiffer, "K-CTR-026", E, "Meta' del trasbordo che non corrispondono",
             {"Il trasbordo {trasbordo} ha piu' di una consegna.", "Il trasbordo {trasbordo} ha piu' di una ricezione.",
              "Il trasbordo {trasbordo} consegna e riceve sulla stessa corsa {corsa}.",
              "Le due meta' del trasbordo {trasbordo} sono in luoghi diversi: {luogo} e {altro}.",
              "Le due meta' del trasbordo {trasbordo} non hanno lo stesso gruppo, lo stesso numero di persone "
              "o lo stesso passeggero."}},
            {Check::TransferReceiverLeaves, "K-CTR-027", E, "Corsa che riceve un trasbordo e riparte prima della consegna",
             {"Trasbordo {trasbordo}: la corsa {corsa} riparte alle {partenza}, prima che la corsa {corsa_consegna} "
              "arrivi alle {consegna}."}},
            {Check::ConnectionWaitArrival, "K-CTR-028", E, "Coincidenza da aspettare: partenza prima dell'arrivo",
             {"La corsa {corsa} parte da {luogo} alle {partenza}, prima dell'arrivo di {servizio} alle {arrivo}, "
              "che deve aspettare."}},
            {Check::ConnectionGuaranteeDeparture, "K-CTR-029", E, "Coincidenza da garantire: arrivo dopo la partenza",
             {"La corsa {corsa} arriva a {luogo} alle {arrivo}, dopo la partenza di {servizio} alle {partenza}, "
              "che deve garantire."}},
            {Check::ConnectionNotOnTrip, "K-CTR-030", E, "Coincidenza in un luogo dove la corsa non passa",
             {"La coincidenza con {servizio} e' a {luogo}, dove la corsa {corsa} non passa."}},
            {Check::CountsDifferFromPassengers, "K-CTR-031", E, "Conteggi del gruppo diversi dai passeggeri",
             {"Gruppo {gruppo}, fermata {luogo}: le persone che salgono ({conteggio}) non corrispondono ai passeggeri "
              "che salgono qui ({passeggeri}).",
              "Gruppo {gruppo}, fermata {luogo}: le persone che scendono ({conteggio}) non corrispondono ai passeggeri "
              "che scendono qui ({passeggeri}).",
              "Gruppo {gruppo}, fermata {luogo}: le carrozzine non corrispondono ai passeggeri in carrozzina che salgono qui.",
              "Il passeggero {passeggero} sale o scende a {luogo}, che non e' tra le fermate del gruppo {gruppo}."},
             {{2, "Gruppo {gruppo}, fermata {luogo}: un dato particolare (riservato) non corrisponde ai passeggeri."}}},
            {Check::LotServiceOtherContract, "K-CTR-032", E, "Servizio di un altro contratto in un gruppo di servizi",
             {"Il servizio {servizio} ha il contratto {contratto}, ma il gruppo di servizi {gruppo_servizi} e' del "
              "contratto {contratto_gruppo}."}},
            {Check::ServiceInTwoLots, "K-CTR-033", E, "Servizio in due gruppi di servizi",
             {"Il servizio {servizio} e' sia nel gruppo di servizi {altro} sia in {gruppo_servizi}."}},
            {Check::GroupNotOnTripServices, "K-CTR-034", E, "Corsa che trasporta un gruppo di un servizio che non soddisfa",
             {"La corsa {corsa} trasporta il gruppo {gruppo} del servizio {servizio}, ma non soddisfa quel servizio."}},
            // Contenuto: avvisi
            {Check::PlaceWithoutCoordinates, "K-CTR-035", A, "Luogo senza coordinate",
             {"Il luogo {luogo} non ha le coordinate: serviranno al calcolo delle distanze."}},
            {Check::DwellTimesUndefined, "K-CTR-036", A, "Tempi di sosta non definiti",
             {"Il pacchetto non ha i tempi di sosta (file tempi_sosta.json): serviranno al calcolo.",
              "I tempi di sosta sono incompleti (parte fissa, salita e discesa per studenti, infanzia e disabili, "
              "scarico a scuola): serviranno al calcolo."}},
            {Check::MaxRideUndefined, "K-CTR-037", A, "Durata massima a bordo non definita",
             {"Per il gruppo {gruppo} del servizio {servizio} nessuno fissa la durata massima a bordo (scuola, servizio, "
              "contratto o regolamento): servira' al calcolo."}},
            {Check::StopNeverUsed, "K-CTR-038", A, "Fermata mai usata",
             {"La fermata {luogo} non e' usata da nessuna corsa e da nessuna domanda."}},
            {Check::ServiceWithoutTrips, "K-CTR-039", A, "Servizio senza corse", {"Nessuna corsa soddisfa il servizio {servizio}."}},
            {Check::TripWithoutService, "K-CTR-040", A, "Corsa che non soddisfa nessun servizio",
             {"La corsa {corsa} non soddisfa nessun servizio."}},
            {Check::GroupWithoutPassengers, "K-CTR-041", A, "Gruppo con zero passeggeri",
             {"Il gruppo {gruppo} del servizio {servizio} non ha passeggeri."}},
            {Check::DemandDiffersFromTrips, "K-CTR-042", A, "Domanda diversa dalle persone delle corse",
             {"Gruppo {gruppo}, fermata {luogo}: le persone che salgono secondo la domanda ({domanda}) non corrispondono "
              "a quelle caricate dalle corse del calendario {calendario} ({corse}).",
              "Gruppo {gruppo}, fermata {luogo}: le persone che scendono secondo la domanda ({domanda}) non corrispondono "
              "a quelle scaricate dalle corse del calendario {calendario} ({corse}).",
              "Gruppo {gruppo}: le corse del calendario {calendario} fanno salire o scendere persone a {luogo} ({corse}), "
              "che non e' tra le fermate della domanda."}},
            {Check::TransferVerbalOnly, "K-CTR-043", A, "Punto di trasbordo autorizzato a voce, senza documento",
             {"Il punto di trasbordo {luogo} e' autorizzato solo a voce: manca il documento."}},
            {Check::VehicleWithoutCosts, "K-CTR-044", A, "Mezzo senza costo fisso giornaliero o senza costo al km",
             {"Il mezzo {mezzo} non ha il costo fisso giornaliero.", "Il mezzo {mezzo} non ha il costo al km.",
              "Il mezzo {mezzo} non ha ne' il costo fisso giornaliero ne' il costo al km."}},
            {Check::CompanyWithoutVehicles, "K-CTR-045", A, "Azienda senza mezzi", {"L'azienda {azienda} non ha mezzi nel pacchetto."}},
            {Check::ContractWithoutRegulation, "K-CTR-046", A, "Contratto senza regolamento valido per una parte del periodo",
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

struct Presentatore::Dati {
    const kzero::Package* pacchetto = nullptr;
    const std::map<std::string, int>* versioni = nullptr;
    Visibilita visibilita;
    std::optional<kzero::Index> indice;
};

Presentatore::Presentatore(const kzero::Package* pacchetto, const std::map<std::string, int>* versioni, Visibilita visibilita)
    : dati_(std::make_unique<Dati>()) {
    dati_->pacchetto = pacchetto;
    dati_->versioni = versioni;
    dati_->visibilita = visibilita;
    if (pacchetto) dati_->indice.emplace(*pacchetto);
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
        case kzero::Element::Transfer: break;
    }
    if (!src || nome.empty() || !dati_->versioni) return codice;
    // La classe del nome viene dalla marcatura dello schema del file da cui viene l'elemento.
    const auto versione = dati_->versioni->find(src->file);
    const std::string schema = src->file.substr(0, src->file.rfind(".json"));
    if (versione == dati_->versioni->end()) return codice;
    const Classe classe = SchemiDati::incorporati().classe(schema, versione->second, src->pointer + "/" + campo);
    return visibile(classe, dati_->visibilita) ? codice + " (" + nome + ")" : codice;
}

std::string Presentatore::messaggio(const kzero::Problem& p) const {
    const DefinizioneControllo& d = definizione_controllo(p.check);
    if (p.variant < 0 || static_cast<std::size_t>(p.variant) >= d.messaggi.size())
        throw std::logic_error(d.codice + ": variante del messaggio inesistente");
    const auto riservato = d.riservati.find(p.variant);
    const std::string& modello = riservato != d.riservati.end() && !visibile(Classe::Particolare, dati_->visibilita)
                                     ? riservato->second
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
        if (nascosto) {
            out += it->hidden.empty() ? std::string(kRiservato) : it->hidden;
            i = fine;
            continue;
        }
        switch (it->kind) {
            case kzero::Param::Kind::Text: out += it->text; break;
            case kzero::Param::Kind::Number: out += std::to_string(it->number); break;
            case kzero::Param::Kind::Decimal: out += decimale(it->decimal); break;
            case kzero::Param::Kind::Time: out += kzero::text_from_minutes(static_cast<kzero::Minutes>(it->number)); break;
            case kzero::Param::Kind::Date: out += kzero::text_from_day(static_cast<kzero::Day>(it->number)); break;
            case kzero::Param::Kind::Ref: out += riferimento(it->element, it->text); break;
            case kzero::Param::Kind::Elements: out += nome_elementi(it->element); break;
        }
        i = fine;
    }
    return out;
}

}  // namespace kzero::api
