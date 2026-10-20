// K-Zero - bersagli del fuzzing (vedi bersagli.hpp).
#include "bersagli.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>

#include "api.hpp"
#include "cassaforte.hpp"
#include "cassaforte_formato.hpp"
#include "chiusura.hpp"
#include "classi.hpp"
#include "password.hpp"
#include "registro.hpp"
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
    // Di base, con i soli dati personali (la regola dei luoghi sensibili) e con tutti i dati.
    for (const Visibilita v : {Visibilita{}, Visibilita{true, false}, Visibilita{true, true}}) {
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

// --- Cassaforte (K-010) -------------------------------------------------------------------------------------------------

namespace {

const cripto::PoliticaKdf& politica_fuzz() {
    static const cripto::PoliticaKdf p{{1, 8}, {1, 64}, {1, 8}};
    return p;
}

const std::string kIdFuzz = "0123456789abcdef0123456789abcdef";
const char* const kUtenti[3] = {"TUTTI", "PERSONALI", "PARTICOLARI"};
const char* const kPassword[3] = {"password di prova del fuzzing uno", "password di prova del fuzzing due",
                                  "password di prova del fuzzing tre"};

cripto::Segreto normalizzata(const std::string& t) {
    password::Problema p = password::Problema::Nessuno;
    return password::normalizza(cripto::Segreto(reinterpret_cast<const unsigned char*>(t.data()), t.size()), p);
}

// La cassaforte del demo, preparata una volta: tre utenti (tutti i permessi, i soli personali, i soli particolari) e un
// cambio delle chiavi (un quarto utente tolto), cosi' ci sono buste e storie.
struct Preparata {
    std::string file;
    std::string registro;
    std::string sezione_personali, sezione_particolari, testo_registro;
    std::map<std::string, JsonDati> normali;   // i file normali del demo
};

const Preparata& preparata() {
    static const Preparata p = [] {
        Preparata x;
        const Lettura l = leggi_pacchetto(PacchettoIncorporato("esempi/demo"));
        chiusura::Separazione s = chiusura::separa(l);
        x.normali = s.normali;
        x.sezione_personali = cassaforte::formato::testo_sezione(s.contenuto.personali);
        x.sezione_particolari = cassaforte::formato::testo_sezione(s.contenuto.particolari);
        cassaforte::Cassaforte c = cassaforte::Cassaforte::crea(kIdFuzz, "2099-12-31", kUtenti[0], normalizzata(kPassword[0]),
                                                                politica_fuzz(), std::move(s.contenuto));
        c.aggiungi_utente(kUtenti[1], cassaforte::Permessi{true, false}, normalizzata(kPassword[1]), politica_fuzz());
        c.aggiungi_utente(kUtenti[2], cassaforte::Permessi{false, true}, normalizzata(kPassword[2]), politica_fuzz());
        c.aggiungi_utente("VIA", cassaforte::Permessi{true, true}, normalizzata("un utente da togliere subito"), politica_fuzz());
        c.togli_utente("VIA");
        registro::Registro r = registro::nuovo(kIdFuzz);
        for (const char* comando : {"chiudi", "aggiungi-utente", "controlla"}) {
            registro::Voce v;
            v.ora = "2026-10-20T10:00:00Z";
            v.utente = kUtenti[0];
            v.comando = comando;
            v.aperte = {"personali", "particolari"};
            registro::aggiungi(r, v);
        }
        c.lega_registro(2, registro::impronta(r.voci[1]));
        x.file = c.file();
        x.registro = registro::scrivi(r, c);
        JsonDati voci = JsonDati::array();
        for (const registro::Voce& v : r.voci) voci.push_back(v.in_json());
        x.testo_registro = voci.dump();
        return x;
    }();
    return p;
}

const cassaforte::Cassaforte& aperta() {
    static const cassaforte::Cassaforte c =
        cassaforte::Cassaforte::apri(preparata().file, kIdFuzz, kUtenti[0], normalizzata(kPassword[0]), politica_fuzz());
    return c;
}

}  // namespace

void prova_cassaforte(const std::uint8_t* dati, std::size_t n) {
    const std::string_view file(reinterpret_cast<const char*>(dati), n);
    try {
        (void)cassaforte::leggi_intestazione(file);
    } catch (const cassaforte::ErroreCassaforte&) {
    }
    for (int u = 0; u < 3; ++u) {
        try {
            const cassaforte::Cassaforte c = cassaforte::Cassaforte::apri(file, kIdFuzz, kUtenti[u], normalizzata(kPassword[u]),
                                                                          politica_fuzz());
            // Aperta: deve essere coerente con i permessi dell'utente e riscriversi in un file che si riapre.
            const cassaforte::Permessi p = c.permessi();
            if ((!p.personali && !c.voci(Classe::Personale).empty()) || (!p.particolari && !c.voci(Classe::Particolare).empty()))
                guasto("cassaforte: voci decifrate senza il permesso");
            const std::string di_nuovo = c.file();
            (void)cassaforte::Cassaforte::apri(di_nuovo, kIdFuzz, kUtenti[u], normalizzata(kPassword[u]), politica_fuzz());
        } catch (const cassaforte::ErroreCassaforte&) {
        }
    }
}

void prova_cassaforte_voci(const std::uint8_t* dati, std::size_t n) {
    const std::string_view testo(reinterpret_cast<const char*>(dati), n);
    std::vector<Voce> voci;
    for (const Classe classe : {Classe::Personale, Classe::Particolare}) {
        if (!cassaforte::formato::voci_di(testo, classe, voci)) continue;
        std::map<std::string, std::vector<const Voce*>> per_file;
        for (const Voce& v : voci) per_file[v.file].push_back(&v);
        for (const auto& [file, x] : per_file) {
            const auto it = preparata().normali.find(file);
            if (it == preparata().normali.end()) continue;
            JsonDati d = it->second;
            std::vector<kzero::Problem> problemi;
            const bool unito = unisci(file, d, x, problemi);
            if (!unito && problemi.empty()) guasto("unione fallita senza problemi");
            if (!unito && d != it->second) guasto("unione fallita che ha cambiato il documento");
            if (unito) json_valido(d.dump(-1, ' ', false, JsonDati::error_handler_t::replace), "unione");
        }
    }
}

void prova_registro(const std::uint8_t* dati, std::size_t n) {
    try {
        (void)registro::leggi(std::string_view(reinterpret_cast<const char*>(dati), n), aperta());
    } catch (const registro::ErroreRegistro&) {
    }
}

void prova_registro_voci(const std::uint8_t* dati, std::size_t n) {
    cripto::Bytes id;
    cripto::da_esadecimale(kIdFuzz, id);
    try {
        const registro::Registro r = registro::voci_dal_testo(std::string_view(reinterpret_cast<const char*>(dati), n), id, 0, {});
        // Letto: riscritto come voci e riletto da' lo stesso.
        JsonDati voci = JsonDati::array();
        for (const registro::Voce& v : r.voci) voci.push_back(v.in_json());
        const std::string t = voci.dump(-1, ' ', false, JsonDati::error_handler_t::replace);
        if (registro::voci_dal_testo(t, id, 0, {}).voci.size() != r.voci.size()) guasto("registro riletto diverso");
    } catch (const registro::ErroreRegistro&) {
    }
}

void prova_password(const std::uint8_t* dati, std::size_t n) {
    const cripto::Segreto scritta(dati, n);
    password::Problema p1 = password::Problema::Nessuno, p2 = password::Problema::Nessuno;
    const cripto::Segreto a = password::normalizza(scritta, p1);
    std::size_t caratteri = 0;
    const cripto::Segreto b = password::nuova(scritta, {"UTENTE", "PACCHETTO"}, p2, caratteri);
    if (p2 == password::Problema::Nessuno) {
        if (p1 != password::Problema::Nessuno) guasto("password nuova accettata ma non valida per aprire");
        if (caratteri < password::kMinimoCaratteri || caratteri > password::kMassimoCaratteri) guasto("password fuori misura accettata");
        if (a.dimensione() != b.dimensione() || !cripto::uguali(a.dati(), b.dati(), a.dimensione()))
            guasto("password normalizzata in due modi diversi");
    }
    (void)password::spiegazione(p2, caratteri);
}

void scrivi_semi_cassaforte(const std::string& cartella) {
    namespace fs = std::filesystem;
    const Preparata& p = preparata();
    const auto scrivi = [&](const std::string& bersaglio, const std::string& nome, const std::string& testo) {
        fs::create_directories(fs::path(cartella) / bersaglio);
        std::ofstream(fs::path(cartella) / bersaglio / nome, std::ios::binary) << testo;
    };
    scrivi("cassaforte", "seme-demo", p.file);
    scrivi("cassaforte-voci", "seme-personali", p.sezione_personali);
    scrivi("cassaforte-voci", "seme-particolari", p.sezione_particolari);
    scrivi("registro", "seme-demo", p.registro);
    scrivi("registro-voci", "seme-demo", p.testo_registro);
    scrivi("password", "seme-frase", "una frase lunga come password");
    scrivi("password", "seme-accenti", "un caff\xC3\xA8 lungo per tutti");
    scrivi("password", "seme-corta", "corta");
}

int prova_semi_cassaforte() {
    const Preparata& p = preparata();
    const auto u = [](const std::string& t) { return reinterpret_cast<const std::uint8_t*>(t.data()); };
    prova_cassaforte(u(p.file), p.file.size());
    prova_cassaforte_voci(u(p.sezione_personali), p.sezione_personali.size());
    prova_cassaforte_voci(u(p.sezione_particolari), p.sezione_particolari.size());
    prova_registro(u(p.registro), p.registro.size());
    prova_registro_voci(u(p.testo_registro), p.testo_registro.size());
    const std::string frase = "una frase lunga come password";
    prova_password(u(frase), frase.size());
    // I semi devono passare davvero: la cassaforte si apre per tutti e tre, il registro si legge.
    for (int k = 0; k < 3; ++k)
        (void)cassaforte::Cassaforte::apri(p.file, kIdFuzz, kUtenti[k], normalizzata(kPassword[k]), politica_fuzz());
    (void)registro::leggi(p.registro, aperta());
    return 7;
}

bool prova(const std::string& bersaglio, const std::uint8_t* dati, std::size_t n) {
    if (bersaglio == "cassaforte") prova_cassaforte(dati, n);
    else if (bersaglio == "cassaforte-voci") prova_cassaforte_voci(dati, n);
    else if (bersaglio == "registro") prova_registro(dati, n);
    else if (bersaglio == "registro-voci") prova_registro_voci(dati, n);
    else if (bersaglio == "password") prova_password(dati, n);
    else if (bersaglio == "parametri") prova_parametri(dati, n);
    else if (bersaglio == "date-e-orari") prova_date_e_orari(dati, n);
    else if (bersaglio.rfind("pacchetto-", 0) == 0 && file_dati_per_nome(bersaglio.substr(10)))
        prova_file_del_pacchetto(bersaglio.substr(10), dati, n);
    else return false;
    return true;
}

}  // namespace kzero::fuzz
