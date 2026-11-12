// K-Zero API - catalogo degli errori e loro presentazione in testo e in JSON.
#include "errori.hpp"

#include <algorithm>

#include "riservatezza.hpp"
#include <stdexcept>
#include <utility>

namespace kzero::api {

const std::vector<SignificatoUscita>& codici_uscita() {
    static const std::vector<SignificatoUscita> codici = {
        {CodiceUscita::Riuscito, "riuscito"},
        {CodiceUscita::ErroreDati, "errore nei dati"},
        {CodiceUscita::ErroreParametri, "errore nei parametri"},
        {CodiceUscita::ErroreInterno, "errore interno"},
    };
    return codici;
}

const std::vector<DefinizioneErrore>& catalogo_errori() {
    // Nuovi errori: codice successivo all'ultimo. Un codice non si cambia e non si riusa mai.
    static const std::vector<DefinizioneErrore> catalogo = [] {
        std::vector<DefinizioneErrore> c = {
            {Errore::MancaComando, "K-ERR-001", CodiceUscita::ErroreParametri, "Manca il comando"},
            {Errore::ComandoSconosciuto, "K-ERR-002", CodiceUscita::ErroreParametri, "Comando sconosciuto"},
            {Errore::ParametroSconosciuto, "K-ERR-003", CodiceUscita::ErroreParametri, "Parametro sconosciuto"},
            {Errore::ArgomentoNonPrevisto, "K-ERR-004", CodiceUscita::ErroreParametri, "Argomento non previsto"},
            {Errore::ValoreMancante, "K-ERR-005", CodiceUscita::ErroreParametri, "Manca il valore del parametro"},
            {Errore::ValoreNonAmmesso, "K-ERR-006", CodiceUscita::ErroreParametri, "Valore non ammesso"},
            {Errore::ParametroRipetuto, "K-ERR-007", CodiceUscita::ErroreParametri, "Parametro ripetuto"},
            {Errore::ParametroSenzaValore, "K-ERR-008", CodiceUscita::ErroreParametri,
             "Il parametro non accetta valori"},
            {Errore::ErroreInterno, "K-ERR-009", CodiceUscita::ErroreInterno, "Errore interno"},
            {Errore::UscitaNonScrivibile, "K-ERR-010", CodiceUscita::ErroreInterno,
             "Impossibile scrivere il risultato"},
            {Errore::MancaArgomento, "K-ERR-011", CodiceUscita::ErroreParametri, "Manca un argomento"},
            {Errore::CartellaNonTrovata, "K-ERR-012", CodiceUscita::ErroreDati, "Cartella del pacchetto non trovata"},
            {Errore::ParametroObbligatorio, "K-ERR-013", CodiceUscita::ErroreParametri, "Manca un parametro obbligatorio"},
            {Errore::ValoreScrittoMale, "K-ERR-014", CodiceUscita::ErroreParametri, "Valore scritto nella forma sbagliata"},
            {Errore::ParametriIncompatibili, "K-ERR-015", CodiceUscita::ErroreParametri,
             "Parametri che non si possono usare insieme"},
            {Errore::DataFuoriPeriodo, "K-ERR-016", CodiceUscita::ErroreParametri, "Data fuori dal periodo del pacchetto"},
            {Errore::ElementoNonTrovato, "K-ERR-017", CodiceUscita::ErroreParametri, "Persona o mezzo non presente nel pacchetto"},
            {Errore::PacchettoConErrori, "K-ERR-018", CodiceUscita::ErroreDati, "Il pacchetto ha errori nei dati"},
            // Cassaforte (K-010)
            {Errore::PasswordNonDisponibile, "K-ERR-019", CodiceUscita::ErroreParametri,
             "Password non disponibile (nessun terminale e niente --password-da-ingresso)"},
            {Errore::PasswordNonAmmessa, "K-ERR-020", CodiceUscita::ErroreParametri, "Password nuova non ammessa"},
            {Errore::UtenteSconosciuto, "K-ERR-021", CodiceUscita::ErroreParametri, "Utente sconosciuto nella cassaforte"},
            {Errore::PasswordSbagliata, "K-ERR-022", CodiceUscita::ErroreParametri, "Password sbagliata"},
            {Errore::PermessoMancante, "K-ERR-023", CodiceUscita::ErroreParametri, "Permesso mancante"},
            {Errore::CassaforteIlleggibile, "K-ERR-024", CodiceUscita::ErroreDati, "Cassaforte vuota, troncata o rovinata"},
            {Errore::CassaforteVersioneSconosciuta, "K-ERR-025", CodiceUscita::ErroreDati,
             "Cassaforte di una versione del formato sconosciuta"},
            {Errore::CassaforteDiUnAltroPacchetto, "K-ERR-026", CodiceUscita::ErroreDati, "Cassaforte di un altro pacchetto"},
            {Errore::ParametriCassaforteNonAmmessi, "K-ERR-027", CodiceUscita::ErroreDati,
             "Parametri della password assurdi o sotto il minimo"},
            {Errore::CassaforteManomessa, "K-ERR-028", CodiceUscita::ErroreDati, "Cassaforte cambiata da fuori di K-Zero"},
            {Errore::RegistroIlleggibile, "K-ERR-029", CodiceUscita::ErroreDati,
             "Registro degli accessi mancante, rovinato o cambiato"},
            {Errore::ScritturaNonRiuscita, "K-ERR-030", CodiceUscita::ErroreDati,
             "Scrittura sul disco non riuscita (registro, cassaforte o file)"},
            {Errore::PacchettoSenzaCassaforte, "K-ERR-031", CodiceUscita::ErroreParametri, "Il pacchetto non ha una cassaforte"},
            {Errore::ServeUtente, "K-ERR-032", CodiceUscita::ErroreParametri, "Il pacchetto ha una cassaforte: serve --utente"},
            {Errore::CartellaGiaPresente, "K-ERR-033", CodiceUscita::ErroreParametri, "La cartella da scrivere c'e' gia'"},
            {Errore::ManifestoDaAggiornare, "K-ERR-034", CodiceUscita::ErroreDati,
             "Manifesto da aggiornare alla versione 2 prima di chiudere"},
            {Errore::ChiusuraNonVerificata, "K-ERR-035", CodiceUscita::ErroreDati,
             "Dati riaperti diversi dall'originale: niente scritto"},
            {Errore::UtentiNonAmmessi, "K-ERR-036", CodiceUscita::ErroreParametri, "Modifica degli utenti non ammessa"},
            {Errore::PacchettoDiverso, "K-ERR-037", CodiceUscita::ErroreDati, "Pacchetto in chiaro diverso da quello chiuso"},
            {Errore::CassaforteMancante, "K-ERR-038", CodiceUscita::ErroreDati,
             "Il manifesto dichiara una cassaforte che manca"},
            {Errore::PeriodoRovesciato, "K-ERR-039", CodiceUscita::ErroreParametri, "Periodo che finisce prima di cominciare"},
            {Errore::PeriodoIncompleto, "K-ERR-040", CodiceUscita::ErroreParametri,
             "Periodo non scritto o scritto a meta' (--data, oppure --dal e --al)"},
            {Errore::FileDaLeggereIlleggibile, "K-ERR-041", CodiceUscita::ErroreDati, "File da leggere non trovato o illeggibile"},
            {Errore::FileExcelNonValido, "K-ERR-042", CodiceUscita::ErroreDati, "File Excel rovinato, cifrato o fuori dai limiti"},
            {Errore::FormatoXls, "K-ERR-043", CodiceUscita::ErroreDati, "File nel vecchio formato .xls"},
            {Errore::ColonnaMancante, "K-ERR-044", CodiceUscita::ErroreDati, "Colonna del profilo mancante o ripetuta nel file"},
            {Errore::ProfiloNonTrovato, "K-ERR-045", CodiceUscita::ErroreParametri, "Profilo di lettura non presente nel pacchetto"},
            {Errore::ScriviDiversa, "K-ERR-046", CodiceUscita::ErroreParametri,
             "La cartella di --scrivi non e' quella del pacchetto"},
            {Errore::ElencoSoloNumeri, "K-ERR-047", CodiceUscita::ErroreDati, "Elenco utenti in un pacchetto \"solo numeri\""},
            {Errore::UtenteElencoNonTrovato, "K-ERR-048", CodiceUscita::ErroreParametri, "Utente non presente nell'elenco"},
            {Errore::SbloccoNonAmmesso, "K-ERR-049", CodiceUscita::ErroreParametri, "Sblocco non ammesso"},
        };
        std::sort(c.begin(), c.end(), [](const DefinizioneErrore& a, const DefinizioneErrore& b) {
            return a.codice < b.codice;
        });
        return c;
    }();
    return catalogo;
}

const DefinizioneErrore& definizione_errore(Errore e) {
    for (const auto& d : catalogo_errori())
        if (d.errore == e) return d;
    throw std::logic_error("errore senza voce nel catalogo");
}

const Schema& schema_errore() {
    static const Schema s{"errore", 1};
    return s;
}

ErroreKZero::ErroreKZero(Errore errore, std::string messaggio, std::optional<Posizione> posizione)
    : definizione_(&definizione_errore(errore)), messaggio_(std::move(messaggio)), posizione_(std::move(posizione)) {}

int codice_uscita(const std::vector<ErroreKZero>& errori) {
    int codice = static_cast<int>(CodiceUscita::Riuscito);
    for (const auto& e : errori) codice = std::max(codice, static_cast<int>(e.definizione().uscita));
    return codice;
}

namespace {

// "dati.csv, riga 3, colonna 5": solo le parti note.
std::string posizione_in_testo(const Posizione& p) {
    std::string s = p.file;
    auto aggiungi = [&s](const std::string& parte) { s += (s.empty() ? "" : ", ") + parte; };
    if (p.riga > 0) aggiungi("riga " + std::to_string(p.riga));
    if (p.colonna > 0) aggiungi("colonna " + std::to_string(p.colonna));
    return s;
}

Json posizione_in_json(const Posizione& p) {
    Json j = Json::object();
    if (!p.file.empty()) j["file"] = p.file;
    if (p.riga > 0) j["riga"] = p.riga;
    if (p.colonna > 0) j["colonna"] = p.colonna;
    return j;
}

}  // namespace

namespace {

// Gli errori del programma non sanno quali parametri sono stati scritti: la loro posizione non nomina mai un file di
// dati che, tutto intero, e' un dato personale o particolare (es. assenze.json). Vedi riservatezza.hpp.
std::optional<Posizione> posizione_mostrata(const ErroreKZero& e) {
    if (!e.posizione() || file_riservato(e.posizione()->file)) return std::nullopt;
    return e.posizione();
}

}  // namespace

std::string testo_errori(const std::vector<ErroreKZero>& errori, Formato formato) {
    if (formato == Formato::Json) {
        Json j = documento(schema_errore());
        j["codice_uscita"] = codice_uscita(errori);
        Json elenco = Json::array();
        for (const auto& e : errori) {
            Json voce = Json::object();
            voce["codice"] = e.definizione().codice;
            voce["messaggio"] = e.messaggio();
            if (const auto p = posizione_mostrata(e)) {
                Json pos = posizione_in_json(*p);
                if (!pos.empty()) voce["posizione"] = pos;
            }
            elenco.push_back(voce);
        }
        j["errori"] = elenco;
        return testo_json(j);
    }
    std::string testo;
    for (const auto& e : errori) {
        testo += "Errore " + e.definizione().codice;
        if (const auto p = posizione_mostrata(e)) {
            const std::string pos = posizione_in_testo(*p);
            if (!pos.empty()) testo += " (" + pos + ")";
        }
        testo += ": " + e.messaggio() + "\n";
    }
    return testo;
}

}  // namespace kzero::api
