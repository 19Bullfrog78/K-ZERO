// K-Zero API - comando "descrivi": descrizione del programma per persone (testo) e per programmi (JSON).
// Nasce tutta dalla tabella dei comandi, dal catalogo degli errori e dall'elenco degli schemi:
// nulla e' scritto a mano una seconda volta.
#include <algorithm>
#include <string>
#include <vector>

#include "errori.hpp"
#include "esecutori.hpp"
#include "versione.hpp"   // generato da CMake

namespace kzero::api {

namespace {

const char* const kDescrizioneProgramma =
    "K-Zero: pianificazione della flotta per servizi di linea, handicap, scolastici e ibridi.";

const char* nome_tipo(TipoParametro t) { return t == TipoParametro::Scelta ? "scelta" : "interruttore"; }

std::string unisci(const std::vector<std::string>& v, const std::string& separatore) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) s += (i ? separatore : "") + v[i];
    return s;
}

// "--formato <json|testo>" oppure "--includi-dati-personali"
std::string forma(const DefinizioneParametro& p) {
    return p.tipo == TipoParametro::Scelta ? p.nome + " <" + unisci(p.valori, "|") + ">" : p.nome;
}

std::string riempi(const std::string& s, std::size_t larghezza) {
    return s.size() >= larghezza ? s : s + std::string(larghezza - s.size(), ' ');
}

Json in_json(const DefinizioneParametro& p) {
    Json j = Json::object();
    j["nome"] = p.nome;
    j["tipo"] = nome_tipo(p.tipo);
    if (p.tipo == TipoParametro::Scelta) {
        j["valori"] = p.valori;
        j["predefinito"] = p.predefinito;
    }
    j["descrizione"] = p.descrizione;
    return j;
}

std::string descrizione_json(const Schema& schema) {
    Json j = documento(schema);
    Json programma = Json::object();
    programma["nome"] = kNomeProgramma;
    programma["versione"] = kVersione;
    programma["descrizione"] = kDescrizioneProgramma;
    j["programma"] = programma;
    j["uso"] = kUso;

    Json elenco_comandi = Json::array();
    for (const auto& c : comandi()) {
        Json jc = Json::object();
        jc["nome"] = c.nome;
        jc["descrizione"] = c.descrizione;
        Json parametri = Json::array();
        for (const auto& p : c.parametri) parametri.push_back(in_json(p));
        jc["parametri"] = parametri;
        Json schema_uscita = Json::object();
        schema_uscita["nome"] = c.schema.nome;
        schema_uscita["versione"] = c.schema.versione;
        jc["schema"] = schema_uscita;
        elenco_comandi.push_back(jc);
    }
    j["comandi"] = elenco_comandi;

    Json elenco_schemi = Json::array();
    for (const auto& s : schemi()) {
        Json js = Json::object();
        js["nome"] = s.nome;
        js["versione"] = s.versione;
        js["file"] = percorso_schema(s);
        elenco_schemi.push_back(js);
    }
    j["schemi"] = elenco_schemi;

    Json elenco_uscite = Json::array();
    for (const auto& u : codici_uscita()) {
        Json ju = Json::object();
        ju["codice"] = static_cast<int>(u.codice);
        ju["significato"] = u.significato;
        elenco_uscite.push_back(ju);
    }
    j["codici_uscita"] = elenco_uscite;

    Json elenco_errori = Json::array();
    for (const auto& e : catalogo_errori()) {
        Json je = Json::object();
        je["codice"] = e.codice;
        je["codice_uscita"] = static_cast<int>(e.uscita);
        je["descrizione"] = e.descrizione;
        elenco_errori.push_back(je);
    }
    j["errori"] = elenco_errori;
    return testo_json(j);
}

std::string descrizione_testo() {
    std::string t;
    t += std::string(kNomeProgramma) + " " + kVersione + "\n";
    t += std::string(kDescrizioneProgramma) + "\n\n";
    t += "Uso: " + std::string(kUso) + "\n";
    t += "Il risultato va sull'uscita standard, gli errori sull'uscita di errore.\n";

    std::size_t larghezza_comando = 0;
    for (const auto& c : comandi()) larghezza_comando = std::max(larghezza_comando, c.nome.size());
    const std::string rientro(2 + larghezza_comando + 2, ' ');
    t += "\nComandi:\n";
    for (const auto& c : comandi()) {
        t += "  " + riempi(c.nome, larghezza_comando) + "  " + c.descrizione + "\n";
        std::size_t larghezza_parametro = 0;
        for (const auto& p : c.parametri) larghezza_parametro = std::max(larghezza_parametro, forma(p).size());
        for (const auto& p : c.parametri) {
            t += rientro + riempi(forma(p), larghezza_parametro) + "  " + p.descrizione;
            if (p.tipo == TipoParametro::Scelta) t += " Predefinito: " + p.predefinito + ".";
            t += "\n";
        }
        t += rientro + "Uscita JSON: schema " + c.schema.nome + ", versione " + std::to_string(c.schema.versione) +
             ".\n";
    }

    std::size_t larghezza_schema = 0;
    for (const auto& s : schemi()) larghezza_schema = std::max(larghezza_schema, s.nome.size());
    t += "\nSchemi delle uscite JSON (JSON Schema draft-07):\n";
    for (const auto& s : schemi())
        t += "  " + riempi(s.nome, larghezza_schema) + "  versione " + std::to_string(s.versione) + "  " +
             percorso_schema(s) + "\n";

    t += "\nCodici di uscita:\n";
    for (const auto& u : codici_uscita())
        t += "  " + std::to_string(static_cast<int>(u.codice)) + "  " + u.significato + "\n";

    t += "\nCodici di errore (spiegati in docs/errori.md):\n";
    for (const auto& e : catalogo_errori())
        t += "  " + e.codice + "  uscita " + std::to_string(static_cast<int>(e.uscita)) + "  " + e.descrizione + "\n";
    return t;
}

}  // namespace

std::string esegui_descrivi(const DefinizioneComando& comando, const Parametri& parametri) {
    return parametri.formato == Formato::Json ? descrizione_json(comando.schema) : descrizione_testo();
}

}  // namespace kzero::api
