// K-Zero API - comando "descrivi": descrizione del programma per persone (testo) e per programmi (JSON).
// Nasce tutta dalla tabella dei comandi, dal catalogo degli errori e dall'elenco degli schemi:
// nulla e' scritto a mano una seconda volta.
#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

#include "controlli_catalogo.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "formato_dati.hpp"
#include "versione.hpp"   // generato da CMake

namespace kzero::api {

namespace {

const char* const kDescrizioneProgramma =
    "K-Zero: pianificazione della flotta per servizi di linea, disabili, scolastici e ibridi, per aziende, consorzi "
    "ed enti di ogni dimensione e di ogni paese.";

const char* nome_tipo(TipoParametro t) {
    switch (t) {
        case TipoParametro::Interruttore: return "interruttore";
        case TipoParametro::Scelta: return "scelta";
        case TipoParametro::Valore: return "valore";
    }
    return "?";
}

const char* nome_forma(FormaValore f) {
    switch (f) {
        case FormaValore::Data: return "data";
        case FormaValore::Codice: return "codice";
        case FormaValore::Percorso: return "percorso";
    }
    return "?";
}

std::string unisci(const std::vector<std::string>& v, const std::string& separatore) {
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) s += (i ? separatore : "") + v[i];
    return s;
}

// "--formato <json|testo>", "--data <AAAA-MM-GG>", "--persona <codice>" oppure "--includi-dati-personali"
std::string forma(const DefinizioneParametro& p) {
    switch (p.tipo) {
        case TipoParametro::Scelta: return p.nome + " <" + unisci(p.valori, "|") + ">";
        case TipoParametro::Valore:
            return p.nome + (p.forma == FormaValore::Data ? " <AAAA-MM-GG>" : p.forma == FormaValore::Codice ? " <codice>" : " <cartella>");
        case TipoParametro::Interruttore: break;
    }
    return p.nome;
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
        if (!p.obbligatorio) j["predefinito"] = p.predefinito;
    }
    if (p.tipo == TipoParametro::Valore) j["forma"] = nome_forma(p.forma);
    j["obbligatorio"] = p.obbligatorio;
    j["incompatibile_con"] = p.incompatibili;
    j["descrizione"] = p.descrizione;
    return j;
}

// Tutti gli schemi di docs/schema: uscite in uso, uscite ritirate, file di dati. In ordine di nome e versione.
struct VoceSchema {
    Schema schema;
    const char* tipo;   // "uscita" o "dati"
    bool in_uso;
};

std::vector<VoceSchema> tutti_gli_schemi() {
    std::vector<VoceSchema> v;
    for (const auto& s : schemi()) v.push_back({s, "uscita", true});
    for (const auto& s : schemi_ritirati()) v.push_back({s, "uscita", false});
    for (const auto& f : file_dati())
        for (int versione : f.versioni) v.push_back({{f.schema, versione}, "dati", true});
    std::sort(v.begin(), v.end(), [](const VoceSchema& a, const VoceSchema& b) {
        return std::tie(a.schema.nome, a.schema.versione) < std::tie(b.schema.nome, b.schema.versione);
    });
    return v;
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
        Json argomenti = Json::array();
        for (const auto& a : c.argomenti) {
            Json ja = Json::object();
            ja["nome"] = a.nome;
            ja["descrizione"] = a.descrizione;
            argomenti.push_back(ja);
        }
        jc["argomenti"] = argomenti;
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
    for (const auto& v : tutti_gli_schemi()) {
        Json js = Json::object();
        js["nome"] = v.schema.nome;
        js["versione"] = v.schema.versione;
        js["file"] = percorso_schema(v.schema);
        js["tipo"] = v.tipo;
        js["in_uso"] = v.in_uso;
        elenco_schemi.push_back(js);
    }
    j["schemi"] = elenco_schemi;

    Json elenco_file = Json::array();
    for (const auto& f : file_dati()) {
        Json jf = Json::object();
        jf["file"] = f.file;
        jf["schema"] = f.schema;
        jf["versioni"] = f.versioni;
        jf["obbligatorio"] = f.obbligatorio;
        jf["contenuto"] = f.contenuto;
        elenco_file.push_back(jf);
    }
    j["formato_dati"] = elenco_file;

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

    Json elenco_controlli = Json::array();
    for (const auto& c : catalogo_controlli()) {
        Json jc = Json::object();
        jc["codice"] = c.codice;
        jc["livello"] = nome_livello(c.livello);
        jc["natura"] = c.piano ? "piano" : "dati";
        jc["descrizione"] = c.descrizione;
        elenco_controlli.push_back(jc);
    }
    j["controlli"] = elenco_controlli;
    return testo_json(j);
}

std::string descrizione_testo() {
    std::string t;
    t += std::string(kNomeProgramma) + " " + kVersione + "\n";
    t += std::string(kDescrizioneProgramma) + "\n\n";
    t += "Uso: " + std::string(kUso) + "\n";
    t += "Il risultato va sull'uscita standard, gli errori sull'uscita di errore.\n";
    t += "I dati personali e particolari si vedono solo con --includi-dati-personali e --includi-dati-particolari.\n";

    std::size_t larghezza_comando = 0;
    for (const auto& c : comandi()) larghezza_comando = std::max(larghezza_comando, c.nome.size());
    const std::string rientro(2 + larghezza_comando + 2, ' ');
    t += "\nComandi:\n";
    for (const auto& c : comandi()) {
        t += "  " + riempi(c.nome, larghezza_comando) + "  " + c.descrizione + "\n";
        t += rientro + "Uso: " + forma_comando(c) + "\n";
        std::size_t larghezza_parametro = 0;
        for (const auto& a : c.argomenti) larghezza_parametro = std::max(larghezza_parametro, a.nome.size() + 2);
        for (const auto& p : c.parametri) larghezza_parametro = std::max(larghezza_parametro, forma(p).size());
        for (const auto& a : c.argomenti) t += rientro + riempi("<" + a.nome + ">", larghezza_parametro) + "  " + a.descrizione + "\n";
        for (const auto& p : c.parametri) {
            t += rientro + riempi(forma(p), larghezza_parametro) + "  " + p.descrizione;
            if (p.tipo == TipoParametro::Scelta && !p.obbligatorio) t += " Predefinito: " + p.predefinito + ".";
            if (p.obbligatorio) t += " Obbligatorio.";
            if (!p.incompatibili.empty()) t += " Non insieme a " + unisci(p.incompatibili, ", ") + ".";
            t += "\n";
        }
        t += rientro + "Uscita JSON: schema " + c.schema.nome + ", versione " + std::to_string(c.schema.versione) +
             ".\n";
    }

    std::size_t larghezza_schema = 0;
    for (const auto& v : tutti_gli_schemi()) larghezza_schema = std::max(larghezza_schema, v.schema.nome.size());
    t += "\nSchemi JSON (JSON Schema draft-07), per le uscite e per i file di dati:\n";
    for (const auto& v : tutti_gli_schemi())
        t += "  " + riempi(v.schema.nome, larghezza_schema) + "  versione " + std::to_string(v.schema.versione) + "  " +
             percorso_schema(v.schema) + "  (" + v.tipo + (v.in_uso ? "" : ", non piu' prodotto") + ")\n";

    std::size_t larghezza_file = 0;
    for (const auto& f : file_dati()) larghezza_file = std::max(larghezza_file, f.file.size());
    t += "\nFile di un pacchetto dati (spiegati in docs/formato-dati.md):\n";
    for (const auto& f : file_dati()) {
        std::string versioni;
        for (int v : f.versioni) versioni += (versioni.empty() ? "" : ", ") + std::to_string(v);
        t += "  " + riempi(f.file, larghezza_file) + "  " + (f.obbligatorio ? "obbligatorio" : "facoltativo ") +
             "  versioni " + versioni + "  " + f.contenuto + "\n";
    }

    t += "\nCodici di uscita:\n";
    for (const auto& u : codici_uscita())
        t += "  " + std::to_string(static_cast<int>(u.codice)) + "  " + u.significato + "\n";

    t += "\nCodici di errore (spiegati in docs/errori.md):\n";
    for (const auto& e : catalogo_errori())
        t += "  " + e.codice + "  uscita " + std::to_string(static_cast<int>(e.uscita)) + "  " + e.descrizione + "\n";

    t += "\nCodici dei controlli sui pacchetti dati (spiegati in docs/controlli.md):\n";
    for (const auto& c : catalogo_controlli())
        t += "  " + c.codice + "  " + riempi(nome_livello(c.livello), 6) + "  " + c.descrizione + "\n";
    return t;
}

}  // namespace

Risultato esegui_descrivi(const DefinizioneComando& comando, const Parametri& parametri) {
    return {parametri.formato == Formato::Json ? descrizione_json(comando.schema) : descrizione_testo(), 0};
}

}  // namespace kzero::api
