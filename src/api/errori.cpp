// K-Zero API - catalogo degli errori e loro presentazione in testo e in JSON.
#include "errori.hpp"

#include <algorithm>
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

std::string testo_errori(const std::vector<ErroreKZero>& errori, Formato formato) {
    if (formato == Formato::Json) {
        Json j = documento(schema_errore());
        j["codice_uscita"] = codice_uscita(errori);
        Json elenco = Json::array();
        for (const auto& e : errori) {
            Json voce = Json::object();
            voce["codice"] = e.definizione().codice;
            voce["messaggio"] = e.messaggio();
            if (e.posizione()) {
                Json pos = posizione_in_json(*e.posizione());
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
        if (e.posizione()) {
            const std::string pos = posizione_in_testo(*e.posizione());
            if (!pos.empty()) testo += " (" + pos + ")";
        }
        testo += ": " + e.messaggio() + "\n";
    }
    return testo;
}

}  // namespace kzero::api
