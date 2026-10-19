// K-Zero API - comando "controlla": controlla un pacchetto dati e ne scrive il resoconto, in testo o in JSON.
// Il resoconto va sempre sull'uscita standard: e' il risultato del comando anche quando trova errori nei dati
// (allora il codice di uscita e' 1). Un errore del programma (es. cartella inesistente) va invece sull'uscita
// di errore con un codice K-ERR, e il resoconto resta vuoto.
#include <filesystem>
#include <string>

#include "controllo.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

std::string quanti(std::size_t n, const char* uno, const char* molti) {
    return std::to_string(n) + " " + (n == 1 ? uno : molti);
}

// Numero di elementi, oppure "aziende non lette" se il file non e' stato letto.
std::string quanti_opz(const std::optional<std::size_t>& n, const char* uno, const char* molti, const char* non_letti) {
    return n ? quanti(*n, uno, molti) : std::string(molti) + " " + non_letti;
}

const char* nome_livello_dati(kzero::Level l) { return l == kzero::Level::Full ? "completo" : "solo_numeri"; }

// "persone.json, /persone/2", con la nota se la posizione completa e' riservata.
std::string posizione_testo(const ProblemaPresentato& p) {
    const kzero::Source& s = p.posizione;
    std::string t = s.file;
    if (!s.pointer.empty()) t += ", " + s.pointer;
    if (s.line > 0) t += ", riga " + std::to_string(s.line) + ", colonna " + std::to_string(s.column);
    if (p.posizione_ridotta)
        t += std::string(t.empty() ? "" : "; ") + "il punto preciso con " + parametro_per(*p.posizione_ridotta);
    return t;
}

// "1 errore e 2 avvisi", "nessun errore e 1 avviso".
std::string errori_e_avvisi(int errori, int avvisi) {
    const auto n = [](int x, const char* uno, const char* molti) {
        return x == 0 ? std::string("nessun ") + uno : std::to_string(x) + " " + (x == 1 ? uno : molti);
    };
    return n(errori, "errore", "errori") + " e " + n(avvisi, "avviso", "avvisi");
}

Json posizione_json(const kzero::Source& s) {
    Json j = Json::object();
    j["file"] = s.file;
    if (!s.pointer.empty()) j["puntatore"] = s.pointer;
    if (s.line > 0) {
        j["riga"] = s.line;
        j["colonna"] = s.column;
    }
    return j;
}

struct Conteggi {
    std::optional<std::size_t> aziende, luoghi, servizi, corse, mezzi, persone, turni;
};

Conteggi conteggi(const Resoconto& r) {
    const kzero::Package& p = r.lettura.pacchetto;
    return {r.quanti("pacchetto.json", p.manifest.companies.size()), r.quanti("luoghi.json", p.places.size()),
            r.quanti("servizi.json", p.services.size()),          r.quanti("corse.json", p.trips.size()),
            r.quanti("mezzi.json", p.vehicles.size()),            r.quanti("persone.json", p.persons.size()),
            r.quanti("turni.json", p.shifts.size())};
}

std::string in_testo(const Resoconto& r) {
    std::string t;
    const kzero::Manifest& m = r.lettura.pacchetto.manifest;
    if (r.lettura.usato("pacchetto.json")) {
        t += "Controllo del pacchetto " + m.code + (m.name.empty() ? "" : " \"" + m.name + "\"") + " (livello " +
             (m.level == kzero::Level::Full ? "completo" : "solo numeri") + ")\n";
    } else {
        t += "Controllo del pacchetto (manifesto non letto)\n";
    }
    // Una riga sola per i problemi che rivelano un dato particolare: senza codice ne' posizione, solo quanti sono.
    if (r.errori_riservati + r.avvisi_riservati > 0)
        t += "Problemi sui dati particolari (salute), non elencati uno per uno: " +
             errori_e_avvisi(r.errori_riservati, r.avvisi_riservati) + ". Per vederli: --includi-dati-particolari.\n";
    for (const ProblemaPresentato& p : r.problemi) {
        t += std::string(p.livello == Livello::Errore ? "Errore " : "Avviso ") + p.codice;
        const std::string pos = posizione_testo(p);
        if (!pos.empty()) t += " (" + pos + ")";
        t += ": " + p.messaggio + "\n";
    }
    if (r.problemi.empty() && r.errori_riservati + r.avvisi_riservati == 0) t += "Nessun problema.\n";
    if (!r.contenuto_controllato)
        t += "Controlli sul contenuto non eseguiti: prima vanno corretti gli errori nella forma dei file.\n";
    const Conteggi c = conteggi(r);
    t += "Riepilogo: " + quanti_opz(c.aziende, "azienda", "aziende", "non lette") + ", " +
         quanti_opz(c.luoghi, "luogo", "luoghi", "non letti") + ", " + quanti_opz(c.servizi, "servizio", "servizi", "non letti") +
         ", " + quanti_opz(c.corse, "corsa", "corse", "non lette") + ", " + quanti_opz(c.mezzi, "mezzo", "mezzi", "non letti") +
         ", " + quanti_opz(c.persone, "persona", "persone", "non lette") + ", " + quanti_opz(c.turni, "turno", "turni", "non letti") +
         "; " +
         quanti(static_cast<std::size_t>(r.errori), "errore", "errori") + ", " +
         quanti(static_cast<std::size_t>(r.avvisi), "avviso", "avvisi") + ".\n";
    return t;
}

std::string in_json(const DefinizioneComando& comando, const Resoconto& r, const Visibilita& v) {
    Json j = documento(comando.schema);
    if (r.lettura.usato("pacchetto.json")) {
        const kzero::Manifest& m = r.lettura.pacchetto.manifest;
        Json p = Json::object();
        p["codice"] = m.code;
        if (!m.name.empty()) p["nome"] = m.name;
        p["livello"] = nome_livello_dati(m.level);
        j["pacchetto"] = p;
    } else {
        j["pacchetto"] = nullptr;
    }
    j["dati_personali_inclusi"] = v.personali;
    j["dati_particolari_inclusi"] = v.particolari;
    j["controlli_sul_contenuto"] = r.contenuto_controllato;
    j["problemi_riservati"] = {{"errori", r.errori_riservati}, {"avvisi", r.avvisi_riservati}};
    Json problemi = Json::array();
    for (const ProblemaPresentato& p : r.problemi) {
        Json x = Json::object();
        x["livello"] = nome_livello(p.livello);
        x["codice"] = p.codice;
        x["messaggio"] = p.messaggio;
        if (!p.posizione.file.empty()) x["posizione"] = posizione_json(p.posizione);
        if (p.posizione_ridotta) x["posizione_completa_con"] = parametro_per(*p.posizione_ridotta);
        if (!p.giorni.empty()) {
            Json giorni = Json::array();
            for (const auto& [dal, al] : p.giorni)
                giorni.push_back({{"dal", kzero::text_from_day(dal)}, {"al", kzero::text_from_day(al)}});
            x["giorni"] = giorni;
        }
        problemi.push_back(x);
    }
    j["problemi"] = problemi;
    const Conteggi c = conteggi(r);
    const auto numero = [](const std::optional<std::size_t>& n) { return n ? Json(*n) : Json(nullptr); };
    Json riepilogo = Json::object();
    riepilogo["aziende"] = numero(c.aziende);
    riepilogo["luoghi"] = numero(c.luoghi);
    riepilogo["servizi"] = numero(c.servizi);
    riepilogo["corse"] = numero(c.corse);
    riepilogo["mezzi"] = numero(c.mezzi);
    riepilogo["persone"] = numero(c.persone);
    riepilogo["turni"] = numero(c.turni);
    riepilogo["errori"] = r.errori;
    riepilogo["avvisi"] = r.avvisi;
    j["riepilogo"] = riepilogo;
    return testo_json(j);
}

}  // namespace

Risultato esegui_controlla(const DefinizioneComando& comando, const Parametri& parametri) {
    const std::string& cartella = parametri.argomenti.at(0);
    std::error_code ec;
    if (cartella.empty() || !std::filesystem::is_directory(std::filesystem::path(cartella), ec))
        throw ErroreKZero(Errore::CartellaNonTrovata,
                          "Cartella del pacchetto non trovata: '" + cartella +
                              "'. Indicare la cartella che contiene il manifesto pacchetto.json.");
    const Visibilita visibilita{parametri.includi_dati_personali, parametri.includi_dati_particolari};
    const Resoconto r = controlla_pacchetto(CartellaSuDisco(cartella), visibilita);
    Risultato risultato;
    risultato.uscita = parametri.formato == Formato::Json ? in_json(comando, r, visibilita) : in_testo(r);
    risultato.codice = r.errori > 0 ? static_cast<int>(CodiceUscita::ErroreDati) : static_cast<int>(CodiceUscita::Riuscito);
    return risultato;
}

}  // namespace kzero::api
