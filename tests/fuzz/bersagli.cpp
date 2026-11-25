// K-Zero - bersagli del fuzzing (vedi bersagli.hpp).
#include "bersagli.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <map>
#include <regex>
#include <set>
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
#include "schede.hpp"
#include "schede_scrittura.hpp"
#include "schemi_dati.hpp"
#include "ricerca.hpp"
#include "tempo.hpp"
#include "uscita.hpp"
#include "elenco.hpp"
#include "excel.hpp"
#include "campi_ingresso.hpp"
#include "punti.hpp"
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

void prova_elenco_campi(const std::uint8_t* dati, std::size_t n) {
    const campi::Righe r = campi::leggi_testo(std::string(reinterpret_cast<const char*>(dati), n));
    if (r.motivo != campi::Motivo::Nessuno) {
        if (r.riga < 1) guasto("riga fuori dai limiti senza numero");
        return;
    }
    if (r.righe.size() > campi::kRighe) guasto("troppe righe accettate");
    for (const std::string& x : r.righe)
        if (x.size() > campi::kRigaByte || x.find('\n') != std::string::npos || x.empty()) guasto("riga fuori dai limiti accettata");
    const Lettura& l = lettura_elenco();
    const kzero::Index idx(l.pacchetto);
    for (const kzero::ReadingProfile& p : l.pacchetto.reading_profiles) {
        // Un elenco con un utente (casa a Pradello), su cui provare l'aggiunta e il cambio.
        kzero::ClientList lista;
        lista.client = kzero::Ref{p.client.code, {}};
        lista.profile = kzero::Ref{p.code, {}};
        lista.first_year = 2026;
        kzero::ListUser u;
        u.code = "U-000001";
        u.name = "PROVA UNO";
        u.school = kzero::Ref{l.pacchetto.schools.front().code, {}};
        u.home = kzero::ListUserHome{"Pradello", "", "via Roma", "3"};
        lista.users.push_back(u);
        for (const bool aggiungere : {true, false}) {
            const kzero::ManualParse in = kzero::parse_manual(r.righe, p, idx, 2026, aggiungere ? nullptr : &u, aggiungere);
            if (in.error.fault != kzero::ManualFault::None) {
                if (!in.fields.empty()) guasto("campi letti a meta' con un errore");
                continue;
            }
            for (const kzero::ListUserTime& t : in.values.times)
                if (t.dates.to < t.dates.from || t.time < 0 || t.time >= 24 * 60 || t.column.empty()) guasto("orario incoerente");
            if (in.values.class_number && (*in.values.class_number < 0 || *in.values.class_number > 20)) guasto("classe fuori dai limiti");
            kzero::ClientList x = lista;
            const kzero::ManualMark segno{"prova", kzero::day_from_ymd(2026, 11, 12)};
            const kzero::ManualOutcome o =
                aggiungere ? kzero::manual_add(l.pacchetto, x, in, segno) : kzero::manual_change(x, "U-000001", in, segno);
            if (o.refusal != kzero::ManualRefusal::None) {
                if (x.users.size() != lista.users.size()) guasto("elenco cambiato con un rifiuto");
                continue;
            }
            std::set<std::string> codici;
            for (const kzero::ListUser& y : x.users)
                if (!codici.insert(y.code).second) guasto("codice ripetuto dopo una modifica a mano");
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
    // K-020: campi di una modifica a mano, buoni (gli stessi testi del file) e uno con i limiti.
    scrivi("elenco-campi", "seme-aggiunta",
           "nome: ROSA DEMO\nscuola: Scuola primaria Valfonda\nstato: SOSPESO - nota\npercorsi: AM/ALFA RM/DITTA GAMMA RP/LINEA C SCOL.\n"
           "comune: Pradello\nfrazione:\nvia: via dei Mille\ncivico: 7\nclasse: 3\nsezione: A\nfermata: PRADELLO PIAZZA\n"
           "orari ORARI MATTINA: AM: 08:00 LU-MA-ME-GI-VE | RM: 12:00 LU-MA-ME-GI-VE\n"
           "orari ORARI POMERIGGIO: //LU o 15.50 [15/09-09/06] //ME f1 15.50 [17/09-23/01] - f2 15.50 [28/01-05/06] //VE rm 12.00\n");
    scrivi("elenco-campi", "seme-cambio", "Via: via Nuova\nCOMUNE: Pradello\nfrazione: Croce\ncivico: 21/C\n\nnome: dopo la riga vuota\n");
    scrivi("elenco-campi", "seme-altro-profilo", "Cognome Nome: x\nnome: TPV PROVA\nscuola: IST. SUPERIORE VALFONDA\nstato: Regolare\n"
                                                "percorsi: ANDATA/ALFA\ncomune: Valfonda\nvia: via Alta\n");
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
    for (const char* b : {"elenco-archivio", "elenco-foglio", "elenco-righe", "elenco-campi"})
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

// --- Schede dei servizi (K-021) --------------------------------------------------------------------------------------------

namespace {

const Lettura& lettura_schede() {
    static const Lettura l = [] {
        PacchettoInMemoria m;
        const PacchettoIncorporato demo("esempi/demo");
        const std::string k019 = std::string(KZERO_SORGENTI) + "/tests/esempi/k019-elenco/";
        const std::string k021 = std::string(KZERO_SORGENTI) + "/tests/esempi/k021-schede/";
        const std::string k023 = std::string(KZERO_SORGENTI) + "/tests/esempi/k023-schede/";
        const std::string k024 = std::string(KZERO_SORGENTI) + "/tests/esempi/k024-schede/";
        const JsonDati c19 = JsonDati::parse(testo_di(k019 + "codici_esterni.json"));
        const JsonDati c21 = JsonDati::parse(testo_di(k021 + "codici_esterni.json"));
        const JsonDati c23 = JsonDati::parse(testo_di(k023 + "codici_esterni.json"));
        const JsonDati c24 = JsonDati::parse(testo_di(k024 + "codici_esterni.json"));
        for (const auto& v : demo.voci()) {
            std::string t = demo.leggi(v.nome, kMassimoByteFile).testo;
            for (const char* f : {"scuole", "luoghi", "calendari"})
                if (v.nome == std::string(f) + ".json") {
                    JsonDati d = JsonDati::parse(t);
                    for (JsonDati& x : d.at(f)) {
                        const std::string c = x.at("codice").get<std::string>();
                        for (const JsonDati* codici : {&c19, &c21, &c23, &c24}) {
                            if (!codici->contains(f) || !codici->at(f).contains(c)) continue;
                            if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
                            for (const JsonDati& e : codici->at(f).at(c)) x["codici_esterni"].push_back(e);
                        }
                    }
                    t = d.dump();
                }
            m.file[v.nome] = t;
        }
        // K-023: i profili alla versione 2 (PS-CVA con il foglio delle interruzioni, PS-CVA-H per i servizi per disabili). K-024: alla
        // versione 3 (la contiene) e, in piu', il profilo del K-024 (regola del tipo sul calendario e colonna dei disabili) e uno con
        // regole su tutte e quattro le colonne, anche con tipi diversi sulla stessa scheda (si deve fermare).
        JsonDati pr = JsonDati::parse(testo_di(k023 + "profili_schede.json"));
        pr["schema_versione"] = 4;   // K-026: la 4 contiene le altre
        JsonDati p24 = JsonDati::parse(testo_di(k024 + "profili_schede.json")).at("profili").at(0);
        p24["codice"] = "PS-CVA-T";
        JsonDati p4 = p24;
        p4["codice"] = "PS-CVA-R";
        p4.at("colonne").at("schede").erase("disabili");
        const auto regola = [](const char* c, const char* col, std::vector<std::string> v, const char* tipo, const char* gruppo) {
            return JsonDati{{"codice", c}, {"colonna", col}, {"valori", v}, {"servizio", {{"tipo", tipo}, {"tipo_gruppo", gruppo}}}};
        };
        p4["regole_tipo"] = JsonDati::array({regola("RT-1", "calendario", {"SPECIALE-A", "FERIALE"}, "disabili", "disabili"),
                                            regola("RT-2", "servizio", {"Andata Mattino"}, "linea", "utenti_linea"),
                                            regola("RT-3", "scuole", {"Centro diurno Il Faro"}, "disabili", "disabili"),
                                            regola("RT-4", "cadenza", {"Ogni 15 giorni"}, "ncc", "adulti_autorizzati")});
        pr.at("profili").push_back(p24);
        pr.at("profili").push_back(p4);
        // K-026: un profilo alla versione 4 con la colonna del mezzo (il K-024 con "Mezzo" in fondo, come tests/esempi/k026-schede).
        JsonDati p26 = p24;
        p26["codice"] = "PS-CVA-M";
        p26.at("colonne").at("schede")["mezzo"] = "Mezzo";
        pr.at("profili").push_back(p26);
        m.file["profili_schede.json"] = pr.dump(2);
        // K-026: i mezzi del demo con un prestito di BETA-01 all'azienda ALFA del contratto CT-SCU per una parte dell'anno e una targa su due
        // mezzi (BETA-03 con la targa di ALFA-02), cosi' il riconoscimento passa da tutte le sue strade.
        JsonDati mz = JsonDati::parse(m.file.at("mezzi.json"));
        for (JsonDati& x : mz.at("mezzi"))
            if (x.at("codice") == "BETA-01")
                x["prestiti"] = JsonDati::array({{{"azienda", "ALFA"}, {"accordo", "prova"}, {"dal", "2026-09-21"}, {"al", "2027-01-31"}}});
        JsonDati doppio = mz.at("mezzi").at(0);
        doppio["codice"] = "BETA-03";
        doppio["azienda"] = "BETA";
        doppio["targa"] = "AA 102 AA";
        mz.at("mezzi").push_back(doppio);
        m.file["mezzi.json"] = mz.dump(2);
        Lettura x = leggi_pacchetto(m);
        if (!x.forma_valida()) guasto("pacchetto di prova delle schede con errori di forma");
        return x;
    }();
    return l;
}

// Le quattro tabelle dal testo: i fogli separati da una riga "@@" (schede, fermate, date e, dal K-023, interruzioni).
kzero::CardTables tabelle_da_testo(const std::string& t) {
    std::vector<std::string> parti(1);
    std::size_t a = 0;
    while (a <= t.size()) {
        const std::size_t b = std::min(t.find('\n', a), t.size());
        const std::string riga = t.substr(a, b - a);
        if (riga == "@@" && parti.size() < 4) parti.emplace_back();
        else parti.back() += riga + "\n";
        if (b == t.size()) break;
        a = b + 1;
    }
    parti.resize(4);
    return {tabella_da_testo(parti[0]), tabella_da_testo(parti[1]), tabella_da_testo(parti[2]), tabella_da_testo(parti[3])};
}

}  // namespace

void prova_schede_righe(const std::uint8_t* dati, std::size_t n) {
    const Lettura& l = lettura_schede();
    const kzero::Index idx(l.pacchetto);
    const kzero::CardTables tab = tabelle_da_testo(std::string(reinterpret_cast<const char*>(dati), n));
    // K-023: tutti i profili di prova (con il foglio delle interruzioni); K-024: con le regole del tipo e la colonna dei disabili.
    for (const kzero::CardProfile& p : l.pacchetto.card_profiles) {
        for (const bool date1904 : {false, true}) {
            const kzero::CardsRead r = kzero::read_cards(tab, p, idx, date1904);
            if (r.fault != kzero::TableFault::None) continue;
            // Una scheda letta torna sempre con i suoi totali: mai una scheda che non torna accettata.
            for (const kzero::CardRead& c : r.cards) {
                if (c.fault) continue;
                const kzero::Card& k = c.card;
                int su = 0, giu = 0, a_bordo = 0;
                double pieni = 0, vuoti = 0;
                if (k.stops.size() < 2 || k.schools.empty() || k.to < k.from || k.base_calendar.empty()) guasto("scheda incompleta accettata");
                for (std::size_t i = 0; i < k.stops.size(); ++i) {
                    const kzero::CardStop& s = k.stops[i];
                    if (i && s.time < k.stops[i - 1].time) guasto("orario all'indietro accettato");
                    a_bordo += s.boarding - s.alighting;
                    if (a_bordo < 0) guasto("piu' persone scese di quelle a bordo");
                    su += s.boarding;
                    giu += s.alighting;
                    pieni += s.loaded_km.value_or(0.0);
                    vuoti += s.empty_km.value_or(0.0);
                }
                if (su != k.pupils || giu != k.pupils) guasto("totali che non tornano accettati");
                if (std::llround(pieni * 100) != std::llround(k.total_loaded_km * 100) || std::llround(vuoti * 100) != std::llround(k.total_empty_km * 100))
                    guasto("km che non tornano accettati");
                if (k.cadence_form == kzero::CadenceForm::ListedDates && k.dates.empty()) guasto("date elencate vuote accettate");
                // K-023: interruzioni dentro il periodo, in ordine, senza sovrapposizioni, mai su una data elencata.
                for (std::size_t i = 0; i < k.breaks.size(); ++i) {
                    const kzero::DateRange& b = k.breaks[i];
                    if (b.to < b.from || b.from < k.from || b.to > k.to) guasto("interruzione rovesciata o fuori dal periodo accettata");
                    if (i && b.from <= k.breaks[i - 1].to) guasto("interruzioni sovrapposte o fuori ordine accettate");
                    for (kzero::Day d : k.dates)
                        if (d >= b.from && d <= b.to) guasto("data elencata dentro un'interruzione accettata");
                }
                // K-023: una scuola senza la sua riga solo nella coincidenza (una scuola sola, la corsa che finisce o comincia in un punto
                // di trasbordo autorizzato, con qualcuno che prosegue).
                const std::optional<kzero::CardTransferEnd> coincidenza = kzero::card_transfer_end(k, idx);
                for (const kzero::Code& sc : k.schools) {
                    const kzero::School* x = idx.school(sc);
                    const bool riga = x && std::any_of(k.stops.begin(), k.stops.end(), [&](const kzero::CardStop& f) {
                                          return f.match == kzero::CardStopMatch::School && f.place == x->place.code;
                                      });
                    if (!riga && !(coincidenza && coincidenza->school == sc && k.schools.size() == 1))
                        guasto("scuola senza la sua riga accettata fuori dalla coincidenza");
                }
                // K-024: il numero dei disabili mai piu' delle persone a bordo (accompagnatore escluso); il tipo sempre scelto (il profilo
                // lo conosce), e quello della regola che dice, o del profilo; mai due regole che scattano con tipi diversi (si ricostruisce
                // per calendario, cadenza e verso e momento, che la scheda letta ha come parola; le scuole sono codici).
                if (k.disabled && (*k.disabled < 0 || *k.disabled > kzero::card_passengers(k))) guasto("piu' disabili delle persone a bordo accettati");
                if (!k.chosen) guasto("scheda letta senza il tipo");
                const kzero::CardTypeRule* da = nullptr;
                for (const kzero::CardTypeRule& g : p.type_rules)
                    if (g.code == k.chosen->rule) da = &g;
                if (k.chosen->from_rule ? (!da || !da->kind || *da->kind != k.chosen->kind) : (!p.kind || *p.kind != k.chosen->kind))
                    guasto("tipo diverso da quello della regola o del profilo");
                for (const kzero::CardTypeRule& g : p.type_rules) {
                    std::string cella;
                    if (g.column == kzero::CardRuleColumn::Calendar) cella = k.calendar_word;
                    else if (g.column == kzero::CardRuleColumn::Cadence) cella = k.cadence_word;
                    else if (g.column == kzero::CardRuleColumn::Service) cella = k.service_text;
                    else continue;
                    const bool scatta = std::any_of(g.values.begin(), g.values.end(),
                                                    [&](const std::string& v) { return kzero::normalize_name(v) == kzero::normalize_name(cella); });
                    if (scatta && (!g.kind || *g.kind != k.chosen->kind)) guasto("due regole con tipi diversi accettate");
                    if (scatta && !k.chosen->from_rule) guasto("regola che scatta ignorata");
                }
                // K-026: il mezzo letto ha sempre lettere o cifre (una cella senza e' "non scritto") e non e' ancora riconosciuto (si
                // riconosce con il registro, che sa il contratto e il periodo).
                if (k.vehicle && (kzero::plate_key(k.vehicle->written).empty() || k.vehicle->code || !k.vehicle->additions.empty()))
                    guasto("mezzo letto senza targa o gia' riconosciuto");
                if (k.vehicle && p.card_cols.vehicle.empty()) guasto("mezzo letto con un profilo senza la colonna");
                if (coincidenza) {
                    const kzero::CardStop& capo = coincidenza->at_start ? k.stops.front() : k.stops.back();
                    const kzero::Place* y = idx.place(capo.place);
                    if (capo.place != coincidenza->place || !y || y->is_private || !y->transfer)
                        guasto("coincidenza fuori da un punto di trasbordo autorizzato alla fine o all'inizio");
                }
            }
            // Il confronto (con le tre scelte) e i documenti fatti dal programma: sempre conformi agli schemi, codici mai ripetuti.
            for (const kzero::DateChoice::Kind scelta : {kzero::DateChoice::Kind::None, kzero::DateChoice::Kind::Activation,
                                                        kzero::DateChoice::Kind::TakenInCharge}) {
                kzero::DateChoices sc;
                sc.file.kind = scelta;
                const kzero::CardsMerge m = kzero::merge_cards(l.pacchetto, p, "CT-SCU", kzero::day_from_ymd(2026, 11, 16), "prova", sc, r);
                if (m.outcomes.size() != r.cards.size()) guasto("schede perse o inventate");
                // K-023 (variante 47): mai una versione scritta senza nessun giorno di servizio.
                for (const auto& [codice, numero] : m.added)
                    for (const kzero::ServiceCard& s : m.cards)
                        for (const kzero::CardVersion& v : s.versions)
                            if (s.code == codice && v.number == numero &&
                                !kzero::first_active_day(kzero::card_calendar(v.card, "", v.valid), v.valid, idx))
                                guasto("versione senza giorni scritta");
                // K-026: un mezzo riconosciuto e' sempre il solo (non ipotetico) con quella targa, dell'azienda ALFA del contratto CT-SCU o
                // in prestito a lei; un mezzo solo con quella targa e dell'azienda e' sempre riconosciuto. Mai un mezzo aggiunto alla prima
                // lettura (il registro di prova e' vuoto).
                if (!m.completed.empty()) guasto("mezzo aggiunto senza registro");
                for (const kzero::CardOutcome& o : m.outcomes) {
                    if (o.kind != kzero::CardOutcome::Kind::New && o.kind != kzero::CardOutcome::Kind::NewVersion) continue;
                    if (!o.vehicle) continue;
                    const std::string chiave = kzero::plate_key(o.vehicle->written);
                    std::vector<const kzero::Vehicle*> stessa;
                    for (const kzero::Vehicle& v : l.pacchetto.vehicles)
                        if (!v.hypothetical && kzero::plate_key(v.plate) == chiave) stessa.push_back(&v);
                    if (o.vehicle->code) {
                        const kzero::Vehicle* v = idx.vehicle(*o.vehicle->code);
                        if (!v || v->hypothetical || stessa.size() != 1 || stessa.front() != v) guasto("mezzo riconosciuto che non e' il solo con quella targa");
                        const bool in_prestito = std::any_of(v->loans.begin(), v->loans.end(), [](const kzero::Loan& x) { return x.company.code == "ALFA"; });
                        if (v->company.code != "ALFA" && !in_prestito) guasto("mezzo di un'altra azienda senza prestito riconosciuto");
                    } else if (stessa.size() == 1 && stessa.front()->company.code == "ALFA") {
                        guasto("mezzo dell'azienda del contratto non riconosciuto");
                    }
                }
                if (scelta == kzero::DateChoice::Kind::None)
                    for (const kzero::CardOutcome& o : m.outcomes)
                        if ((o.kind == kzero::CardOutcome::Kind::New || o.kind == kzero::CardOutcome::Kind::NewVersion) && o.from != kzero::ChoiceFrom::None)
                            guasto("data scelta dal programma");
                // I documenti (lenti: rileggono tutto il pacchetto) con una scelta sola.
                if (m.added.empty() || scelta != kzero::DateChoice::Kind::Activation) continue;
                try {
                    const Lettura d = schede::con_documenti(l, schede::documenti_dopo(l, m.cards, l.pacchetto.removed_cards, m.added, m.removed,
                                                                                       m.cuts, m.new_homes));
                    if (!schede::controlla_elementi(d).empty()) guasto("elementi fatti diversi da quelli della scheda");
                    // K-025: lo stesso file riletto subito e un mese dopo, senza nessuna scelta: ogni scheda scritta e' uguale all'ultima
                    // versione (mai ferma per la data, mai una versione nuova), nessun avviso della lettura per lei, niente da scrivere.
                    const kzero::Index idx2(d.pacchetto);
                    const kzero::CardProfile* p2 = idx2.card_profile(p.code);
                    if (!p2) guasto("profilo perso nella scrittura");
                    const kzero::CardsRead r2 = kzero::read_cards(tab, *p2, idx2, date1904);
                    if (r2.fault != kzero::TableFault::None || r2.cards.size() != r.cards.size()) guasto("rilettura diversa dalla lettura");
                    for (const kzero::Day presa : {kzero::day_from_ymd(2026, 11, 16), kzero::day_from_ymd(2026, 12, 16)}) {
                        const kzero::CardsMerge m2 = kzero::merge_cards(d.pacchetto, *p2, "CT-SCU", presa, "prova", {}, r2);
                        if (m2.outcomes.size() != m.outcomes.size()) guasto("schede perse o inventate nella rilettura");
                        std::set<std::string> uguali;
                        for (std::size_t i = 0; i < m.outcomes.size(); ++i) {
                            if (m.outcomes[i].kind != kzero::CardOutcome::Kind::New) continue;
                            if (m2.outcomes[i].kind != kzero::CardOutcome::Kind::Same) guasto("scheda scritta che riletta non e' uguale");
                            uguali.insert(m2.outcomes[i].nr);
                        }
                        if (!m2.added.empty() || !m2.removed.empty() || !m2.cuts.empty()) guasto("rilettura che scrive qualcosa");
                        // K-026 (D1): con lo stesso pacchetto la rilettura non aggiunge mai il mezzo alla copia.
                        if (!m2.completed.empty()) guasto("rilettura che aggiunge il mezzo con lo stesso pacchetto");
                        for (const kzero::Problem& x : m2.problems)
                            for (const kzero::Param& q : x.params)
                                if (q.name == "scheda" && q.kind == kzero::Param::Kind::Text && uguali.count(q.text))
                                    guasto("avviso della lettura su una scheda uguale");
                    }
                    // K-023: le sospensioni (interruzioni) dentro il periodo del calendario (fuori sarebbe K-CTR-013).
                    for (const kzero::Calendar& c : d.pacchetto.calendars)
                        for (const kzero::Suspension& x : c.suspensions)
                            if (x.range.from < c.range.from || x.range.to > c.range.to || x.range.to < x.range.from)
                                guasto("interruzione non tagliata al periodo della versione");
                } catch (const std::logic_error&) {
                    guasto("documenti fatti dal programma non conformi agli schemi");
                }
            }
        }
    }
}

void scrivi_semi_schede(const std::string& cartella) {
    namespace fs = std::filesystem;
    const std::string k021 = std::string(KZERO_SORGENTI) + "/tests/esempi/k021-schede/";
    fs::create_directories(fs::path(cartella) / "schede-righe");
    std::ofstream(fs::path(cartella) / "schede-righe" / "seme-tre-schede", std::ios::binary)
        << testo_di(k021 + "schede.tsv") << "@@\n" << testo_di(k021 + "fermate.tsv") << "@@\n" << testo_di(k021 + "date.tsv");
    // K-023: le schede per disabili con le interruzioni e la coppia in coincidenza (quattro fogli).
    const std::string k023 = std::string(KZERO_SORGENTI) + "/tests/esempi/k023-schede/";
    std::ofstream(fs::path(cartella) / "schede-righe" / "seme-k023", std::ios::binary)
        << testo_di(k023 + "schede.tsv") << "@@\n" << testo_di(k023 + "fermate.tsv") << "@@\n" << testo_di(k023 + "date.tsv") << "@@\n"
        << testo_di(k023 + "interruzioni.tsv");
    // K-024: le quattro schede del tipo per scheda (con la colonna dei disabili).
    const std::string k024 = std::string(KZERO_SORGENTI) + "/tests/esempi/k024-schede/";
    std::ofstream(fs::path(cartella) / "schede-righe" / "seme-k024", std::ios::binary)
        << testo_di(k024 + "schede.tsv") << "@@\n" << testo_di(k024 + "fermate.tsv") << "@@\n" << testo_di(k024 + "date.tsv") << "@@\n"
        << testo_di(k024 + "interruzioni.tsv");
    // K-025: le quattro schede della rilettura (una gia' finita alla seconda lettura, una in coincidenza, una con Tot Giorni sbagliato).
    const std::string k025 = std::string(KZERO_SORGENTI) + "/tests/esempi/k025-schede/";
    std::ofstream(fs::path(cartella) / "schede-righe" / "seme-k025", std::ios::binary)
        << testo_di(k025 + "schede.tsv") << "@@\n" << testo_di(k025 + "fermate.tsv") << "@@\n" << testo_di(k025 + "date.tsv") << "@@\n"
        << testo_di(k025 + "interruzioni.tsv");
    // K-026: le tre schede del mezzo della scheda (la colonna "Mezzo"), e le stesse con le targhe scritte in altri modi: un mezzo dell'altra
    // azienda in prestito per una parte dell'anno, una targa su due mezzi, la targa con la descrizione.
    const std::string k026 = std::string(KZERO_SORGENTI) + "/tests/esempi/k026-schede/";
    const std::string resto = "@@\n" + testo_di(k026 + "fermate.tsv") + "@@\n" + testo_di(k026 + "date.tsv") + "@@\n" + testo_di(k026 + "interruzioni.tsv");
    std::ofstream(fs::path(cartella) / "schede-righe" / "seme-k026", std::ios::binary) << testo_di(k026 + "schede.tsv") << resto;
    std::string targhe = testo_di(k026 + "schede.tsv");
    for (const auto& [da, a] : {std::make_pair(std::string("AA 101-AA"), std::string("bb.201.bb")),
                                std::make_pair(std::string("ZZ 999 ZZ"), std::string("AA 102 AA"))})
        targhe.replace(targhe.find(da), da.size(), a);
    const std::string vuota = "Scheda nuova\t\t\n";
    if (targhe.find(vuota) != std::string::npos) targhe.replace(targhe.find(vuota), vuota.size(), "Scheda nuova\t\tAA 103 AA - MARCA 00\n");
    std::ofstream(fs::path(cartella) / "schede-righe" / "seme-k026-targhe", std::ios::binary) << targhe << resto;
}

int prova_semi_schede() {
    namespace fs = std::filesystem;
    const fs::path cartella = fs::temp_directory_path() / ("kzero-semi-schede-" + std::to_string(::getpid()));
    scrivi_semi_schede(cartella.string());
    int n = 0;
    for (const auto& e : fs::directory_iterator(cartella / "schede-righe")) {
        const std::string t = testo_di(e.path().string());
        prova("schede-righe", reinterpret_cast<const std::uint8_t*>(t.data()), t.size());
        ++n;
    }
    std::error_code ec;
    fs::remove_all(cartella, ec);
    // Il seme deve essere buono: due schede si leggono, la terza ha un totale sbagliato.
    const std::string k021 = std::string(KZERO_SORGENTI) + "/tests/esempi/k021-schede/";
    const kzero::CardsRead r = kzero::read_cards(tabelle_da_testo(testo_di(k021 + "schede.tsv") + "@@\n" + testo_di(k021 + "fermate.tsv")),
                                                 lettura_schede().pacchetto.card_profiles.at(0), kzero::Index(lettura_schede().pacchetto), false);
    if (r.cards.size() != 3 || r.cards[0].fault || r.cards[1].fault || !r.cards[2].fault) guasto("il seme delle schede non si legge come atteso");
    // K-023: le tre schede per disabili si leggono tutte, la prima con tre interruzioni, la seconda in coincidenza.
    const std::string k023 = std::string(KZERO_SORGENTI) + "/tests/esempi/k023-schede/";
    const kzero::Index idx(lettura_schede().pacchetto);
    const kzero::CardProfile* h = idx.card_profile("PS-CVA-H");
    if (!h) guasto("manca il profilo PS-CVA-H");
    const kzero::CardsRead q = kzero::read_cards(tabelle_da_testo(testo_di(k023 + "schede.tsv") + "@@\n" + testo_di(k023 + "fermate.tsv") + "@@\n" +
                                                                  testo_di(k023 + "date.tsv") + "@@\n" + testo_di(k023 + "interruzioni.tsv")),
                                                 *h, idx, false);
    if (q.cards.size() != 3 || q.cards[0].fault || q.cards[1].fault || q.cards[2].fault || q.cards[0].card.breaks.size() != 3 ||
        !kzero::card_transfer_end(q.cards[1].card, idx))
        guasto("il seme delle schede del K-023 non si legge come atteso");
    // K-024: le quattro schede si leggono con il profilo del K-024 (due con il tipo della regola); con PS-CVA-R le due per disabili si
    // fermano (RT-2 e RT-3 o RT-1 con tipi diversi).
    const std::string k024 = std::string(KZERO_SORGENTI) + "/tests/esempi/k024-schede/";
    const kzero::CardTables t24 = tabelle_da_testo(testo_di(k024 + "schede.tsv") + "@@\n" + testo_di(k024 + "fermate.tsv") + "@@\n" +
                                                   testo_di(k024 + "date.tsv") + "@@\n" + testo_di(k024 + "interruzioni.tsv"));
    const kzero::CardsRead s = kzero::read_cards(t24, *idx.card_profile("PS-CVA-T"), idx, false);
    if (s.cards.size() != 4 || s.cards[0].fault || s.cards[1].fault || s.cards[2].fault || s.cards[3].fault || !s.cards[1].card.chosen ||
        !s.cards[1].card.chosen->from_rule || s.cards[0].card.chosen->from_rule)
        guasto("il seme delle schede del K-024 non si legge come atteso");
    // K-026: le tre schede si leggono con il profilo con la colonna del mezzo; la prima ha la targa, la terza la cella vuota.
    const std::string k026 = std::string(KZERO_SORGENTI) + "/tests/esempi/k026-schede/";
    const kzero::CardsRead w = kzero::read_cards(tabelle_da_testo(testo_di(k026 + "schede.tsv") + "@@\n" + testo_di(k026 + "fermate.tsv") + "@@\n" +
                                                                  testo_di(k026 + "date.tsv") + "@@\n" + testo_di(k026 + "interruzioni.tsv")),
                                                 *idx.card_profile("PS-CVA-M"), idx, false);
    if (w.cards.size() != 3 || w.cards[0].fault || w.cards[1].fault || w.cards[2].fault || !w.cards[0].card.vehicle ||
        w.cards[0].card.vehicle->written != "AA 101-AA" || w.cards[2].card.vehicle)
        guasto("il seme delle schede del K-026 non si legge come atteso");
    return n;
}

// --- K-027: i punti degli utenti dell'elenco -------------------------------------------------------------------------------

namespace {

// Una voce della storia dei punti come testo (dal, regole, giorni): per confrontare i giorni prima e dopo una voce nuova.
std::string firma_voce(const kzero::PointEntry* e) {
    if (!e) return "nessuna voce";
    std::string out = std::to_string(e->from) + (e->rules.empty() ? " nessun punto" : "");
    for (const kzero::PointRule& r : e->rules) {
        out += " | " + r.card_id + " " + r.place.code + " " + std::to_string(r.occurrence) + " v" + std::to_string(r.version) + " ";
        for (int g = 0; g < 7; ++g) out += r.covers(static_cast<kzero::Weekday>(g)) ? '1' : '0';
    }
    return out;
}

}  // namespace

void prova_punti_campi(const std::uint8_t* dati, std::size_t n) {
    const campi::Righe r = campi::leggi_testo(std::string(reinterpret_cast<const char*>(dati), n));
    if (r.motivo != campi::Motivo::Nessuno) {
        if (r.riga < 1) guasto("riga fuori dai limiti senza numero");
        return;
    }
    const int righe = static_cast<int>(r.righe.size());
    using Modo = kzero::PointLinesMode;
    using Guasto = kzero::PointLinesFault;
    for (const Modo modo : {Modo::Assign, Modo::Confirm, Modo::NotTraveling, Modo::Voice}) {
        const kzero::PointLines a = kzero::parse_point_lines(r.righe, modo);
        const kzero::PointLines b = kzero::parse_point_lines(r.righe, modo);
        if (a.fault != b.fault || a.line != b.line || a.points.size() != b.points.size() || a.consent != b.consent || a.who != b.who ||
            a.note != b.note || a.reason != b.reason || a.nobody != b.nobody)
            guasto("le stesse righe dei punti lette in due modi diversi");
        if (a.line < 0 || a.line > righe) guasto("riga del problema fuori dalle righe");
        if (a.fault != Guasto::None) continue;
        for (const kzero::PointLine& x : a.points) {
            if (x.card.empty() || x.card.size() > 300 || x.place.empty()) guasto("punto senza scheda o senza luogo accettato");
            if (x.occurrence && (*x.occurrence < 1 || *x.occurrence > 99)) guasto("volta fuori dai limiti accettata");
            if (x.all_days != x.days.empty()) guasto("giorni del punto incoerenti");
            if (x.line < 1 || x.line > righe) guasto("riga del punto fuori dalle righe");
            if (modo == Modo::Confirm && (!x.occurrence || x.user.empty() || x.leg.empty() || x.leg.size() > 100))
                guasto("riga pronta della conferma senza utente, tratta o volta accettata");
            if (modo != Modo::Confirm && (!x.user.empty() || !x.leg.empty())) guasto("utente o tratta fuori dalla conferma");
        }
        if (modo != Modo::Assign && (a.nobody || !a.consent.empty())) guasto("\"nessuno\" o consenso fuori da assegna-punto");
        if (modo != Modo::NotTraveling && (a.reason || !a.note.empty())) guasto("motivo o nota fuori da non-viaggia");
        if (modo != Modo::Voice && !a.who.empty()) guasto("\"chi\" fuori dallo sblocco a voce");
        if (modo == Modo::Assign && a.nobody == !a.points.empty()) guasto("\"nessuno\" e i punti insieme, o nessuno dei due");
        if (modo == Modo::Assign && a.nobody && !a.consent.empty()) guasto("consenso con \"nessuno\"");
        if (modo == Modo::Confirm && a.points.empty()) guasto("conferma senza righe accettata");
        if (modo == Modo::NotTraveling && (!a.reason || a.note.size() > 2000)) guasto("non-viaggia senza motivo o con la nota troppo lunga");
        if (modo == Modo::Voice && (a.who.empty() || a.who.size() > 300)) guasto("sblocco a voce senza \"chi\" o troppo lungo");
        if (a.consent.size() > 300) guasto("consenso troppo lungo accettato");
    }
    for (const std::string& x : r.righe) {
        const std::optional<kzero::Weekdays> g = kzero::parse_weekdays(x);
        if (g && g->empty()) guasto("giorni della settimana vuoti accettati");
    }
    // La storia: una voce nuova (i punti letti per assegna-punto, dal giorno ricavato dai byte) non cambia mai i giorni prima della sua
    // data, vale da li' in poi e non toglie nessuna voce dalla storia (le voci dopo restano, segnate come sostituite).
    const kzero::PointLines a = kzero::parse_point_lines(r.righe, Modo::Assign);
    if (a.fault != Guasto::None) return;
    const kzero::Day inizio = kzero::day_from_ymd(2026, 9, 1);
    unsigned somma = 0;
    for (std::size_t i = 0; i < n; ++i) somma = somma * 31u + dati[i];
    kzero::PointEntry e;
    e.from = inizio + static_cast<kzero::Day>(somma % 300u);
    e.mark = kzero::ManualMark{"prova", e.from};
    e.consent = a.consent;
    for (const kzero::PointLine& x : a.points) {
        kzero::PointRule pr;
        pr.card_id = x.card;
        pr.place = kzero::Ref{x.place, {}};
        pr.occurrence = x.occurrence.value_or(1);
        pr.all_days = x.all_days;
        pr.days = x.days;
        e.rules.push_back(pr);
    }
    kzero::UserPoints up{"AM", {}, {}};
    for (const auto& [dal, luogo] : {std::make_pair(14, "F-UNO"), std::make_pair(62, "F-DUE"), std::make_pair(200, "F-TRE")}) {
        kzero::PointEntry v;
        v.from = inizio + dal;
        v.mark = kzero::ManualMark{"prima", v.from};
        kzero::PointRule pr;
        pr.card_id = "SCHEDA";
        pr.place = kzero::Ref{luogo, {}};
        v.rules.push_back(pr);
        (void)kzero::add_point_entry(up, v);
    }
    std::vector<std::string> prima;
    for (kzero::Day d = inizio - 5; d < inizio + 320; ++d) prima.push_back(firma_voce(kzero::point_entry_on(up, d)));
    const std::size_t voci = up.history.size();
    const std::vector<kzero::Day> sostituite = kzero::add_point_entry(up, e);
    if (up.history.size() != voci + 1) guasto("la voce nuova ha tolto o aggiunto altre voci della storia");
    for (kzero::Day s : sostituite)
        if (s < e.from) guasto("sostituita una voce che comincia prima della nuova");
    const std::string nuova = firma_voce(&e);
    for (kzero::Day d = inizio - 5; d < inizio + 320; ++d) {
        const std::string ora = firma_voce(kzero::point_entry_on(up, d));
        if (d < e.from && ora != prima[static_cast<std::size_t>(d - (inizio - 5))]) guasto("una voce nuova ha cambiato un giorno prima della sua data");
        if (d >= e.from && ora != nuova) guasto("la voce nuova non vale dalla sua data in poi");
    }
}

void scrivi_semi_punti(const std::string& cartella) {
    namespace fs = std::filesystem;
    const auto scrivi = [&](const std::string& nome, const std::string& testo) {
        fs::create_directories(fs::path(cartella) / "punti-campi");
        std::ofstream(fs::path(cartella) / "punti-campi" / nome, std::ios::binary) << testo;
    };
    // Le righe dei quattro comandi, come nelle prove (dati inventati): assegna-punto con le due volte e i giorni, la doppia tratta con il
    // consenso, "nessuno", le righe pronte di kzero proponi-punti, non-viaggia e lo sblocco a voce.
    scrivi("seme-assegna", "punto: SCH-4000901-1 / F-LAGOBELLO / 2 / lunedi, mercoledi\npunto: SCH-4000901-1 / F-LAGOBELLO / 1 / martedi,giovedi,venerdi\n");
    scrivi("seme-doppia", "punto: 7d2a9c10-27aa-4b1e-9c00-000000000903 / F-PRADELLO\npunto: SCH-4000903-1 / F-CASTELROVO / 1 / venerdi\n"
                          "consenso: mail del committente del 3/11 (prova)\n");
    scrivi("seme-nessuno", "nessuno\n");
    scrivi("seme-conferma", "U-000001 AM: SCH-4000901-1 / F-LAGOBELLO / 1\nU-000002 AM: SCH-4000901-1 / F-PRADELLO / 1 / martedi, mercoledi\n");
    scrivi("seme-non-viaggia", "motivo: ritirato\nnota: la famiglia lo accompagna (prova)\n");
    scrivi("seme-a-voce", "chi: la mamma al telefono (prova)\n");
}

int prova_semi_punti() {
    namespace fs = std::filesystem;
    const fs::path cartella = fs::temp_directory_path() / ("kzero-semi-punti-" + std::to_string(::getpid()));
    scrivi_semi_punti(cartella.string());
    int n = 0;
    for (const auto& x : fs::directory_iterator(cartella / "punti-campi")) {
        const std::string t = testo_di(x.path().string());
        prova("punti-campi", reinterpret_cast<const std::uint8_t*>(t.data()), t.size());
        ++n;
    }
    std::error_code ec;
    fs::remove_all(cartella, ec);
    // I semi devono essere buoni: ognuno si legge senza problemi nel suo comando.
    const auto righe = [](const std::string& t) { return campi::leggi_testo(t).righe; };
    using Modo = kzero::PointLinesMode;
    const kzero::PointLines as = kzero::parse_point_lines(righe("punto: SCH-4000901-1 / F-LAGOBELLO / 2 / lunedi, mercoledi\n"), Modo::Assign);
    const kzero::PointLines co = kzero::parse_point_lines(righe("U-000001 AM: SCH-4000901-1 / F-LAGOBELLO / 1\n"), Modo::Confirm);
    const kzero::PointLines nv = kzero::parse_point_lines(righe("motivo: ritirato\n"), Modo::NotTraveling);
    const kzero::PointLines vo = kzero::parse_point_lines(righe("chi: la mamma al telefono (prova)\n"), Modo::Voice);
    if (as.fault != kzero::PointLinesFault::None || as.points.size() != 1 || as.points[0].occurrence != 2 || as.points[0].all_days ||
        co.fault != kzero::PointLinesFault::None || co.points.size() != 1 || co.points[0].user != "U-000001" ||
        nv.fault != kzero::PointLinesFault::None || nv.reason != kzero::NotTravelingReason::Withdrawn || vo.fault != kzero::PointLinesFault::None)
        guasto("i semi dei punti non si leggono come atteso");
    // I semi dei file del K-027 nei bersagli del pacchetto: l'elenco alla versione 3 (con i punti, chi non viaggia e gli sblocchi per
    // giorni) e lo stradario si leggono, i punti restano.
    const std::string semi = std::string(KZERO_SORGENTI) + "/tests/fuzz/semi/pacchetto-elenco_utenti.json/";
    for (const auto& x : fs::directory_iterator(semi)) {
        const std::string t = testo_di(x.path().string());
        prova("pacchetto-elenco_utenti.json", reinterpret_cast<const std::uint8_t*>(t.data()), t.size());
        ++n;
        Lettura l = base_senza("elenco_utenti.json");
        leggi_file("elenco_utenti.json", t, l);
        std::size_t punti = 0, fermi = 0, sblocchi = 0;
        for (const kzero::ClientList& c : l.pacchetto.client_lists)
            for (const kzero::ListUser& u : c.users) {
                punti += u.points.size();
                fermi += u.not_traveling.size();
                for (const kzero::ListUserUnlock& s : u.unlocks) sblocchi += s.all_days ? 0 : 1;
            }
        if (!l.usato("elenco_utenti.json") || punti == 0 || fermi == 0 || sblocchi == 0)
            guasto("il seme dell'elenco alla versione 3 non si legge come atteso: " + x.path().filename().string());
    }
    const std::string stradario = testo_di(std::string(KZERO_SORGENTI) + "/tests/esempi/k027-punti/stradari.json");
    prova("pacchetto-stradari.json", reinterpret_cast<const std::uint8_t*>(stradario.data()), stradario.size());
    Lettura l = base_senza("stradari.json");
    leggi_file("stradari.json", stradario, l);
    if (l.pacchetto.street_tables.size() != 1 || l.pacchetto.street_tables[0].rows.size() != 4) guasto("lo stradario di prova non si legge come atteso");
    return n + 1;
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
    else if (bersaglio == "elenco-campi") prova_elenco_campi(dati, n);
    else if (bersaglio == "schede-righe") prova_schede_righe(dati, n);
    else if (bersaglio == "punti-campi") prova_punti_campi(dati, n);
    else if (bersaglio.rfind("pacchetto-", 0) == 0 && file_dati_per_nome(bersaglio.substr(10)))
        prova_file_del_pacchetto(bersaglio.substr(10), dati, n);
    else return false;
    return true;
}

}  // namespace kzero::fuzz
