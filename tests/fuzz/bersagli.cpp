// K-Zero - bersagli del fuzzing (vedi bersagli.hpp).
#include "bersagli.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "api.hpp"
#include "comandi.hpp"
#include "controllo.hpp"
#include "errori.hpp"
#include "formato_dati.hpp"
#include "lettore.hpp"
#include "parametri.hpp"
#include "riservatezza.hpp"
#include "schemi_dati.hpp"
#include "tempo.hpp"
#include "uscita.hpp"

namespace kzero::fuzz {

using namespace kzero::api;

namespace {

// Un risultato incoerente e' un guasto come un errore di memoria: si ferma tutto, e libFuzzer salva il caso.
[[noreturn]] void guasto(const std::string& cosa) {
    std::fprintf(stderr, "GUASTO: %s\n", cosa.c_str());
    std::abort();
}

// Il pacchetto demo senza un file.
class DemoSenza : public Sorgente {
public:
    explicit DemoSenza(std::string file) : demo_("esempi/demo"), file_(std::move(file)) {}
    std::vector<Voce> voci() const override {
        std::vector<Voce> v = demo_.voci();
        v.erase(std::remove_if(v.begin(), v.end(), [this](const Voce& x) { return x.nome == file_; }), v.end());
        return v;
    }
    FileLetto leggi(const std::string& nome, std::uintmax_t massimo) const override { return demo_.leggi(nome, massimo); }

private:
    PacchettoIncorporato demo_;
    std::string file_;
};

// Lettura del demo senza un file, fatta una volta sola: a ogni prova si legge solo il file da provare.
const Lettura& base_senza(const std::string& file) {
    static std::map<std::string, Lettura> basi;
    auto it = basi.find(file);
    if (it == basi.end()) {
        Lettura l = leggi_pacchetto(DemoSenza(file));
        l.problemi.erase(std::remove_if(l.problemi.begin(), l.problemi.end(),
                                        [](const kzero::Problem& p) { return p.check == kzero::Check::MissingManifest; }),
                         l.problemi.end());
        it = basi.emplace(file, std::move(l)).first;
    }
    return it->second;
}

// Il JSON scritto dal programma deve essere sempre valido (anche con byte qualsiasi nei messaggi).
void json_valido(const std::string& testo, const std::string& dove) {
    if (!JsonDati::accept(testo)) guasto(dove + ": il programma ha scritto un JSON non valido");
}

}  // namespace

std::vector<std::string> file_del_formato() {
    std::vector<std::string> v;
    for (const FileDati& f : file_dati()) v.push_back(f.file);
    return v;
}

void prova_file_del_pacchetto(const std::string& file, const std::uint8_t* dati, std::size_t n) {
    Lettura lettura = base_senza(file);
    const std::string testo(reinterpret_cast<const char*>(dati), n);
    leggi_file(file, testo, lettura);
    // Un file usato deve essere stato letto tutto: nessun byte zero (la libreria JSON lo prende per la fine del file).
    if (lettura.usato(file) && testo.find('\0') != std::string::npos) guasto(file + ": file con un byte zero usato lo stesso");
    for (const Visibilita v : {Visibilita{}, Visibilita{true, true}}) {
        const Resoconto r = controlla_lettura(lettura, v);
        if (r.errori + r.avvisi != static_cast<int>(r.problemi.size()) + r.errori_riservati + r.avvisi_riservati)
            guasto("problemi contati e problemi elencati non coincidono");
        Json elenco = Json::array();
        for (const ProblemaPresentato& p : r.problemi) {
            // Di base nessuna posizione mostrata entra in un dato personale o particolare.
            if (!p.posizione.file.empty() && !visibile(classe_della_posizione(p.posizione, r.lettura), v))
                guasto(p.codice + ": posizione mostrata dentro un dato riservato: " + p.posizione.file + " " + p.posizione.pointer);
            if (p.messaggio.empty()) guasto(p.codice + ": messaggio vuoto");
            elenco.push_back({{"codice", p.codice}, {"messaggio", p.messaggio}, {"file", p.posizione.file},
                              {"puntatore", p.posizione.pointer}});
        }
        json_valido(testo_json(elenco), "controllo di " + file);
    }
}

void prova_parametri(const std::uint8_t* dati, std::size_t n) {
    std::vector<std::string> argomenti(1);
    for (std::size_t i = 0; i < n && argomenti.size() <= 64; ++i) {
        if (dati[i] == 0) argomenti.emplace_back();
        else argomenti.back() += static_cast<char>(dati[i]);
    }
    if (n == 0) argomenti.clear();
    (void)formato_richiesto(argomenti);
    for (const DefinizioneComando& c : comandi()) {
        try {
            const Parametri p = leggi_parametri(c, argomenti);
            if (p.formato != Formato::Testo && p.formato != Formato::Json) guasto("formato sconosciuto");
        } catch (const ErroreKZero& e) {
            if (e.definizione().uscita != CodiceUscita::ErroreParametri) guasto("errore dei parametri con un'uscita diversa da 2");
            json_valido(testo_errori({e}, Formato::Json), "errore dei parametri");
        }
    }
    // L'intera esecuzione, ma non dei comandi che leggono il disco o il pacchetto demo (lenti, gia' provati sopra).
    if (!argomenti.empty() && (argomenti[0] == "controlla" || argomenti[0] == "giornata" || argomenti[0] == "dimostrazione")) return;
    const Esito e = esegui(argomenti);
    if (e.codice < 0 || e.codice > 3) guasto("codice di uscita fuori da 0-3");
    if (e.uscita.empty() == e.errori.empty()) guasto("o il risultato o l'errore, mai tutti e due o nessuno");
    if (formato_richiesto(argomenti) == Formato::Json) json_valido(e.uscita.empty() ? e.errori : e.uscita, "esegui");
    std::ostringstream uscita, errori;
    if (consegna(e, uscita, errori) != e.codice) guasto("consegna cambia il codice di uscita");
}

void prova_date_e_orari(const std::uint8_t* dati, std::size_t n) {
    const std::string t(reinterpret_cast<const char*>(dati), n);
    if (const auto d = kzero::day_from_text(t)) {
        if (kzero::text_from_day(*d) != t) guasto("data che non torna uguale: " + t);
        int a = 0, m = 0, g = 0;
        kzero::ymd_from_day(*d, a, m, g);
        if (kzero::day_from_ymd(a, m, g) != *d) guasto("anno, mese e giorno che non tornano: " + t);
        (void)kzero::weekday_of(*d);
    }
    if (const auto m = kzero::minutes_from_text(t)) {
        if (*m < 0) guasto("orario negativo: " + t);
        if (kzero::minutes_from_text(kzero::text_from_minutes(*m)) != m) guasto("orario che non torna uguale: " + t);
    }
    // Stessa regola delle date nel programma e negli schemi (che controllano i file): una data scritta e' valida per
    // tutti e due o per nessuno.
    static const std::regex data_dello_schema = [] {
        const JsonDati* s = SchemiDati::incorporati().schema("assenze", 1);
        return std::regex(s->at("definitions").at("data").at("pattern").get<std::string>());
    }();
    static const std::regex orario_dello_schema = [] {
        const JsonDati* s = SchemiDati::incorporati().schema("assenze", 1);
        return std::regex(s->at("definitions").at("orario").at("pattern").get<std::string>());
    }();
    if (t.size() <= 16) {
        if (std::regex_match(t, data_dello_schema) != kzero::day_from_text(t).has_value())
            guasto("data valida per lo schema e non per il programma, o il contrario: " + t);
        if (std::regex_match(t, orario_dello_schema) && !kzero::minutes_from_text(t))
            guasto("orario valido per lo schema ma non per il programma: " + t);
    }
}

bool prova(const std::string& bersaglio, const std::uint8_t* dati, std::size_t n) {
    if (bersaglio == "parametri") prova_parametri(dati, n);
    else if (bersaglio == "date-e-orari") prova_date_e_orari(dati, n);
    else if (bersaglio.rfind("pacchetto-", 0) == 0 && file_dati_per_nome(bersaglio.substr(10)))
        prova_file_del_pacchetto(bersaglio.substr(10), dati, n);
    else return false;
    return true;
}

}  // namespace kzero::fuzz
