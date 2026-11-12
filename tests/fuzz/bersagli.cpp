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
#include <unistd.h>

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
#include "orari.hpp"
#include "formato_dati.hpp"
#include "lettore.hpp"
#include "parametri.hpp"
#include "riservatezza.hpp"
#include "schemi_dati.hpp"
#include "tempo.hpp"
#include "uscita.hpp"
#include "elenco.hpp"
#include "excel.hpp"
#include "../xlsx_prove.hpp"

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
    // K-011: il resoconto degli orari (kzero ore) di una settimana, con tutti i dati: si costruisce senza guasti e conta solo
    // giorni dentro la settimana chiesta.
    if (lettura.forma_valida()) {
        kzero::HoursRequest richiesta;
        richiesta.report = true;
        richiesta.from = lettura.pacchetto.manifest.period.from;
        richiesta.to = richiesta.from + 6;
        kzero::HoursReport orari;
        (void)controlla_lettura(lettura, Visibilita{true, true}, &richiesta, &orari);
        for (const kzero::PersonHours& ph : orari.persons)
            for (const kzero::DayHours& dh : ph.days)
                if (!dh.pd || dh.pd->day < richiesta.from || dh.pd->day > richiesta.to) guasto("orari: un giorno fuori dal periodo chiesto");
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

// La cassaforte preparata una volta: tre utenti (tutti i permessi, i soli personali, i soli particolari) e un cambio delle
// chiavi (un quarto utente tolto), cosi' ci sono buste e storie. Dal K-011 aggiunta 2 i dati sono quelli del pacchetto di
// prova degli orari (il demo con in piu' la forzatura e la dichiarazione dell'azienda, con motivo e chi tra i legati); dal
// K-012 quelli del pacchetto di prova dei servizi, che lo comprende alle versioni nuove con i committenti privati (il nome di
// chi noleggia per andare al centro diurno tra i legati): i semi coprono ogni sezione e ogni file nuovo o cambiato.
struct Preparata {
    std::string file;
    std::string registro;
    std::string sezione_personali, sezione_particolari, sezione_legati, testo_registro;
    std::map<std::string, JsonDati> normali;   // i file normali del pacchetto
};

const Preparata& preparata() {
    static const Preparata p = [] {
        Preparata x;
        const Lettura l = leggi_pacchetto(CartellaSuDisco(std::string(KZERO_SORGENTI) + "/tests/esempi/k012-servizi"));
        chiusura::Separazione s = chiusura::separa(l);
        x.normali = s.normali;
        x.sezione_personali = cassaforte::formato::testo_sezione(s.contenuto.personali);
        x.sezione_particolari = cassaforte::formato::testo_sezione(s.contenuto.particolari);
        x.sezione_legati = cassaforte::formato::testo_sezione(s.contenuto.legati);
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
            if ((!p.personali && !c.voci(Classe::Personale).empty()) || (!p.particolari && !c.voci(Classe::Particolare).empty()) ||
                (!p.tutti() && !c.voci_legate().empty()))
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
    scrivi("cassaforte-voci", "seme-legati", p.sezione_legati);
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
    prova_cassaforte_voci(u(p.sezione_legati), p.sezione_legati.size());
    prova_registro(u(p.registro), p.registro.size());
    prova_registro_voci(u(p.testo_registro), p.testo_registro.size());
    const std::string frase = "una frase lunga come password";
    prova_password(u(frase), frase.size());
    // I semi devono passare davvero: la cassaforte si apre per tutti e tre, il registro si legge.
    for (int k = 0; k < 3; ++k)
        (void)cassaforte::Cassaforte::apri(p.file, kIdFuzz, kUtenti[k], normalizzata(kPassword[k]), politica_fuzz());
    (void)registro::leggi(p.registro, aperta());
    return 8;
}

// --- Elenco utenti del committente (K-019) ----------------------------------------------------------------------------

namespace {

std::string testo_di(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// Il pacchetto di prova dell'elenco: il demo con i profili e i codici esterni di tests/esempi/k019-elenco, letto una volta.
const Lettura& lettura_elenco() {
    static const Lettura l = [] {
        PacchettoInMemoria m;
        const PacchettoIncorporato demo("esempi/demo");
        const std::string k019 = std::string(KZERO_SORGENTI) + "/tests/esempi/k019-elenco/";
        const JsonDati codici = JsonDati::parse(testo_di(k019 + "codici_esterni.json"));
        for (const auto& v : demo.voci()) {
            std::string t = demo.leggi(v.nome, kMassimoByteFile).testo;
            for (const char* f : {"scuole", "luoghi"})
                if (v.nome == std::string(f) + ".json") {
                    JsonDati d = JsonDati::parse(t);
                    for (JsonDati& x : d.at(f)) {
                        const std::string c = x.at("codice").get<std::string>();
                        if (!codici.at(f).contains(c)) continue;
                        if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
                        for (const JsonDati& e : codici.at(f).at(c)) x["codici_esterni"].push_back(e);
                    }
                    t = d.dump();
                }
            m.file[v.nome] = t;
        }
        m.file["profili_elenchi.json"] = testo_di(k019 + "profili_elenchi.json");
        Lettura x = leggi_pacchetto(m);
        if (!x.forma_valida()) guasto("pacchetto di prova dell'elenco con errori di forma");
        return x;
    }();
    return l;
}

kzero::Table tabella_da_testo(const std::string& t) {
    kzero::Table out;
    std::size_t a = 0;
    int numero = 0;
    while (a <= t.size() && numero < 2000) {
        std::size_t b = t.find('\n', a);
        if (b == std::string::npos) b = t.size();
        const std::string riga = t.substr(a, b - a);
        kzero::TableRow r;
        r.number = ++numero;
        std::size_t c = 0;
        int colonna = 0;
        for (;;) {
            const std::size_t d = std::min(riga.find('\t', c), riga.size());
            std::string cella = riga.substr(c, d - c);
            if (!cella.empty() && cella[0] == '#') cella.erase(0, 1);   // come nei testi delle prove: "#3" e' il numero 3
            ++colonna;
            if (!cella.empty()) r.cells.push_back({colonna, cella, cella == "!"});
            if (d >= riga.size()) break;
            c = d + 1;
        }
        if (!r.cells.empty()) out.rows.push_back(std::move(r));
        if (b == t.size()) break;
        a = b + 1;
    }
    return out;
}

}  // namespace

void prova_elenco_archivio(const std::uint8_t* dati, std::size_t n) {
    const std::string_view f(reinterpret_cast<const char*>(dati), n);
    try {
        const excel::Foglio s = excel::leggi_foglio(f, "");
        for (const excel::Riga& r : s.righe) {
            if (r.numero == 0 || r.numero > excel::limiti().righe) guasto("riga fuori dai limiti");
            for (const excel::Cella& c : r.celle)
                if (c.colonna == 0 || c.colonna > excel::limiti().colonne || c.testo.size() > excel::limiti().cella)
                    guasto("cella fuori dai limiti");
        }
    } catch (const excel::ErroreExcel&) {
    }
    try {
        (void)excel::nomi_delle_parti(f);
    } catch (const excel::ErroreExcel&) {
    }
}

void prova_elenco_foglio(const std::uint8_t* dati, std::size_t n) {
    const std::string_view x(reinterpret_cast<const char*>(dati), n);
    try {
        (void)excel::testi_condivisi_da_xml(x);
    } catch (const excel::ErroreExcel&) {
    }
    try {
        const excel::Foglio s = excel::foglio_da_xml(x, {"uno", "due", "tre"});
        for (std::size_t i = 1; i < s.righe.size(); ++i)
            if (s.righe[i].numero <= s.righe[i - 1].numero) guasto("righe fuori ordine");
    } catch (const excel::ErroreExcel&) {
    }
}

void prova_elenco_righe(const std::uint8_t* dati, std::size_t n) {
    const Lettura& l = lettura_elenco();
    const kzero::Index idx(l.pacchetto);
    const std::string t(reinterpret_cast<const char*>(dati), n);
    const kzero::Table tabella = tabella_da_testo(t);
    for (const kzero::ReadingProfile& p : l.pacchetto.reading_profiles) {
        // Le celle una per una, con ogni forma del profilo.
        for (const kzero::TableRow& r : tabella.rows)
            for (const kzero::TableCell& c : r.cells) {
                (void)kzero::parse_routes(c.text, p);
                (void)kzero::parse_status(c.text, p);
                for (const kzero::ProfileTimes& o : p.times) {
                    const kzero::TimesParse x = kzero::parse_times(c.text, o, p, 2026);
                    if (x.fault && !x.times.empty()) guasto("orari letti a meta'");
                    for (const kzero::ListUserTime& v : x.times)
                        if (v.dates.to < v.dates.from || v.time < 0 || v.time >= 24 * 60) guasto("orario incoerente");
                }
            }
        // Le righe e il confronto: righe lette + illeggibili = righe dei dati; codici nuovi in ordine e unici.
        const kzero::ListRead letto = kzero::read_list(tabella, p, idx, 2026, nullptr);
        if (letto.fault != kzero::TableFault::None) continue;
        std::set<int> righe;
        for (const kzero::ParsedUser& u : letto.users) righe.insert(u.row);
        for (int r : letto.unreadable_rows)
            if (righe.count(r)) guasto("riga sia letta sia illeggibile");
        for (const bool completo : {true, false}) {
            const kzero::ListMerge m = kzero::merge_list(l.pacchetto, p, 2026, letto, completo, std::string(64, '0'));
            std::set<std::string> codici;
            for (const kzero::ListUser& u : m.list.users)
                if (!codici.insert(u.code).second) guasto("codice ripetuto nell'elenco");
            if (m.list.users.size() != letto.users.size()) guasto("utenti persi o inventati");
        }
    }
}

void scrivi_semi_elenco(const std::string& cartella) {
    namespace fs = std::filesystem;
    const std::string k019 = std::string(KZERO_SORGENTI) + "/tests/esempi/k019-elenco/";
    const auto scrivi = [&](const std::string& bersaglio, const std::string& nome, const std::string& testo) {
        fs::create_directories(fs::path(cartella) / bersaglio);
        std::ofstream(fs::path(cartella) / bersaglio / nome, std::ios::binary) << testo;
    };
    for (const char* nome : {"prima-lettura", "seconda-lettura"}) {
        const std::string tsv = testo_di(k019 + nome + ".tsv");
        scrivi("elenco-righe", std::string("seme-") + nome, tsv);
        std::vector<std::vector<std::string>> righe;
        const kzero::Table t = tabella_da_testo(tsv);
        for (const kzero::TableRow& r : t.rows) {
            std::vector<std::string> x;
            for (const kzero::TableCell& c : r.cells) {
                if (x.size() < static_cast<std::size_t>(c.column - 1)) x.resize(static_cast<std::size_t>(c.column - 1));
                const bool numero = !c.text.empty() && std::all_of(c.text.begin(), c.text.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
                x.push_back(numero ? "#" + c.text : c.text);
            }
            righe.push_back(x);
        }
        for (int metodo : {0, 8}) {
            const std::vector<kzero::prove::xlsx::Parte> parti = kzero::prove::xlsx::parti_xlsx({kzero::prove::xlsx::Foglio{"Foglio1", righe, false}}, metodo);
            scrivi("elenco-archivio", std::string("seme-") + nome + "-" + std::to_string(metodo),
                   kzero::prove::xlsx::Archivio{parti, 0}.scrivi());
            for (const kzero::prove::xlsx::Parte& p : parti)
                if (p.nome == "xl/worksheets/sheet1.xml" && metodo == 0) scrivi("elenco-foglio", std::string("seme-") + nome + "-foglio", p.dati);
                else if (p.nome == "xl/sharedStrings.xml" && metodo == 0) scrivi("elenco-foglio", std::string("seme-") + nome + "-testi", p.dati);
        }
    }
}

int prova_semi_elenco() {
    namespace fs = std::filesystem;
    const fs::path cartella = fs::temp_directory_path() / ("kzero-semi-elenco-" + std::to_string(::getpid()));
    scrivi_semi_elenco(cartella.string());
    int n = 0;
    for (const char* b : {"elenco-archivio", "elenco-foglio", "elenco-righe"})
        for (const auto& e : fs::directory_iterator(cartella / b)) {
            const std::string t = testo_di(e.path().string());
            prova(b, reinterpret_cast<const std::uint8_t*>(t.data()), t.size());
            ++n;
        }
    std::error_code ec;
    fs::remove_all(cartella, ec);
    // I semi devono essere buoni: i file si leggono e le righe sono tutte lette.
    const std::string tsv = testo_di(std::string(KZERO_SORGENTI) + "/tests/esempi/k019-elenco/prima-lettura.tsv");
    const kzero::ListRead l = kzero::read_list(tabella_da_testo(tsv), lettura_elenco().pacchetto.reading_profiles.at(0),
                                               kzero::Index(lettura_elenco().pacchetto), 2026, nullptr);
    if (l.users.size() != 5) guasto("il seme delle righe non si legge tutto");
    return n;
}

bool prova(const std::string& bersaglio, const std::uint8_t* dati, std::size_t n) {
    if (bersaglio == "cassaforte") prova_cassaforte(dati, n);
    else if (bersaglio == "cassaforte-voci") prova_cassaforte_voci(dati, n);
    else if (bersaglio == "registro") prova_registro(dati, n);
    else if (bersaglio == "registro-voci") prova_registro_voci(dati, n);
    else if (bersaglio == "password") prova_password(dati, n);
    else if (bersaglio == "parametri") prova_parametri(dati, n);
    else if (bersaglio == "date-e-orari") prova_date_e_orari(dati, n);
    else if (bersaglio == "elenco-archivio") prova_elenco_archivio(dati, n);
    else if (bersaglio == "elenco-foglio") prova_elenco_foglio(dati, n);
    else if (bersaglio == "elenco-righe") prova_elenco_righe(dati, n);
    else if (bersaglio.rfind("pacchetto-", 0) == 0 && file_dati_per_nome(bersaglio.substr(10)))
        prova_file_del_pacchetto(bersaglio.substr(10), dati, n);
    else return false;
    return true;
}

}  // namespace kzero::fuzz
