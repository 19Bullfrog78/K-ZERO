// K-Zero - prove dei comandi delle modifiche a mano dell'elenco utenti (K-020): i campi dall'ingresso con i loro limiti, e i comandi
// aggiungi-in-elenco, cambia-in-elenco, togli-da-elenco, rimetti-in-elenco e cancella-da-elenco con i tre utenti della cassaforte.
// Le regole del nucleo: test_elenco_a_mano.cpp. Dati inventati.
#include <gtest/gtest.h>

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "campi_ingresso.hpp"
#include "cassaforte_prove.hpp"
#include "elenco_a_mano_prove.hpp"
#include "errori.hpp"
#include "xlsx_prove.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove;
using namespace kzero::prove::elenco_a_mano;
namespace fs = std::filesystem;

// --- I campi dall'ingresso: limiti ----------------------------------------------------------------------------------------------

TEST(ElencoAMano, LimitiDellIngresso) {
    const campi::Righe buone = campi::leggi_testo("nome: A B\r\nvia: via Roma\n\nquesto non si legge\n");
    EXPECT_EQ(buone.motivo, campi::Motivo::Nessuno);
    EXPECT_EQ(buone.righe, (std::vector<std::string>{"nome: A B", "via: via Roma"}));   // fino alla riga vuota
    EXPECT_EQ(campi::leggi_testo("nome: " + std::string(campi::kRigaByte, 'x') + "\n").motivo, campi::Motivo::RigaTroppoLunga);
    std::string tante;
    for (std::size_t i = 0; i <= campi::kRighe; ++i) tante += "nome: x\n";
    EXPECT_EQ(campi::leggi_testo(tante).motivo, campi::Motivo::TroppeRighe);
    EXPECT_EQ(campi::leggi_testo("nome: \xC3\x28\n").motivo, campi::Motivo::NonUtf8);
    EXPECT_EQ(campi::leggi_testo("nome: \xE0\x80\xAF\n").motivo, campi::Motivo::NonUtf8);       // forma troppo lunga
    EXPECT_EQ(campi::leggi_testo("nome: \xED\xA0\x80\n").motivo, campi::Motivo::NonUtf8);       // meta' di una coppia
    EXPECT_EQ(campi::leggi_testo(std::string("nome: a\0b\n", 10)).motivo, campi::Motivo::Controllo);
    EXPECT_EQ(campi::leggi_testo("nome: a\x1b[31mb\n").motivo, campi::Motivo::Controllo);
    // K-021 (punto 0, H): i controlli C1 (U+0080 ... U+009F, es. U+0085 "a capo" e U+009B, l'inizio delle sequenze del terminale);
    // U+00A0 (spazio indivisibile) e' un carattere normale.
    for (const char* c1 : {"\xC2\x80", "\xC2\x85", "\xC2\x9B", "\xC2\x9F"})
        EXPECT_EQ(campi::leggi_testo(std::string("nome: a") + c1 + "b\n").motivo, campi::Motivo::Controllo) << c1;
    EXPECT_EQ(campi::leggi_testo("nome: a\xC2\xA0" "b\n").motivo, campi::Motivo::Nessuno);
    EXPECT_EQ(campi::leggi_testo("nome: Nicol\xC3\xB2\tD'Angelo").motivo, campi::Motivo::Nessuno);   // senza a capo finale
    // La riga troppo lunga si scarta senza tenerla tutta in memoria, e conta il numero giusto.
    const campi::Righe lunga = campi::leggi_testo("nome: a\nvia: " + std::string(3 * campi::kRigaByte, 'y') + "\n");
    EXPECT_EQ(lunga.riga, 2);
}

// --- I comandi, con i tre utenti della cassaforte -------------------------------------------------------------------------------

namespace {

const std::string kPw = "cavallo pesca lampione verde";
const std::string kPwB = "barca gialla sul lago verde";
const std::string kPwC = "il treno delle sette e mezza";

struct Prova {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    fs::path chiuso;
    std::string valori;   // i campi sull'ingresso del prossimo comando
    std::size_t letti = 0;
    Prova() {
        const PacchettoInMemoria m = pacchetto_di_prova();
        fs::create_directories(lavoro / "chiaro");
        for (const auto& [f, t] : m.file) std::ofstream(lavoro / "chiaro" / f, std::ios::binary) << t;
        amb.password = {kPw};
        comando({"chiudi", (lavoro / "chiaro").string(), "--scrivi", (lavoro / "chiuso").string(), "--utente", "A", "--conservazione", "2099-12-31"});
        chiuso = lavoro / "chiuso";
        amb.password = {kPw, kPwB};
        comando({"aggiungi-utente", "--scrivi", chiuso.string(), "--utente", "A", "--nuovo-utente", "B", "--permessi", "personali"});
        amb.password = {kPw, kPwC};
        comando({"aggiungi-utente", "--scrivi", chiuso.string(), "--utente", "A", "--nuovo-utente", "C", "--permessi", "particolari"});
        const_cast<accesso::Ambiente&>(amb.ambiente()).valori = [this]() -> int {
            return letti < valori.size() ? static_cast<unsigned char>(valori[letti++]) : -1;
        };
        // La prima lettura del file: ROSSI ANNA ... GIALLI PAOLO (U-000001 ... U-000005).
        std::vector<std::vector<std::string>> righe;
        std::istringstream in(leggi_testo(kCartella + "/prima-lettura.tsv"));
        for (std::string r; std::getline(in, r);) {
            std::vector<std::string> celle;
            std::size_t a = 0;
            for (;;) {
                const std::size_t b = r.find('\t', a);
                celle.push_back(r.substr(a, b == std::string::npos ? std::string::npos : b - a));
                if (b == std::string::npos) break;
                a = b + 1;
            }
            righe.push_back(celle);
        }
        std::ofstream(lavoro / "prima.xlsx", std::ios::binary) << xlsx::file_righe(righe);
        leggi_file("prima.xlsx", "A", kPw, true);
    }
    Risultato leggi_file(const std::string& file, const std::string& utente, const std::string& pw, bool scrivere,
                         std::vector<std::string> altri = {}) {
        amb.password = {pw};
        std::vector<std::string> a = {"leggi-elenco", (lavoro / file).string(), chiuso.string(), "--profilo", "PR-CVA", "--anno-scolastico",
                                      "2026-2027", "--elenco", "completo", "--utente", utente};
        if (scrivere) a.insert(a.end(), {"--scrivi", chiuso.string()});
        a.insert(a.end(), altri.begin(), altri.end());
        return comando(a);
    }
    Risultato a_mano(const std::string& cmd, const std::vector<std::string>& righe, const std::string& utente, const std::string& pw,
                     std::vector<std::string> altri, bool scrivere = true, const std::string& data = "2026-11-12") {
        amb.password = {pw};
        valori.clear();
        for (const std::string& r : righe) valori += r + "\n";
        letti = 0;
        std::vector<std::string> a = {cmd, chiuso.string(), "--utente", utente, "--riferimento", "mail del committente", "--data", data};
        if (scrivere) a.insert(a.end(), {"--scrivi", chiuso.string()});
        a.insert(a.end(), altri.begin(), altri.end());
        return comando(a);
    }
    std::string normale() const { return leggi_testo(chiuso / "elenco_utenti.json"); }
};

std::string errore_di(const std::function<void()>& f) {
    try {
        f();
    } catch (const ErroreKZero& e) {
        return e.definizione().codice;
    }
    return "nessuno";
}

}  // namespace

// Chi ha un permesso solo non cambia l'elenco; il titolare si'. Nessun nome nell'uscita senza il parametro (sabotaggio); nessun
// valore personale, nemmeno quello di prima, nel file normale (sabotaggio); la voce del registro ha solo codici.
TEST(ElencoAManoComandi, TreUtentiDellaCassaforte) {
    Prova p;
    const std::vector<std::string> nuovo = {"nome: ROSA DEMO", "scuola: " + kPrim, "stato: ATTIVO", "percorsi: AM/ALFA"};
    EXPECT_EQ(errore_di([&] { p.a_mano("aggiungi-in-elenco", nuovo, "B", kPwB, {"--profilo", "PR-CVA"}); }), "K-ERR-023");
    EXPECT_EQ(errore_di([&] { p.a_mano("togli-da-elenco", {}, "C", kPwC, {"--utente-elenco", "U-000001"}); }), "K-ERR-023");
    // Anteprima: niente scritto.
    const std::string prima = p.normale();
    const Risultato a = p.a_mano("aggiungi-in-elenco", nuovo, "A", kPw, {"--profilo", "PR-CVA"}, false);
    EXPECT_NE(a.uscita.find("Anteprima"), std::string::npos) << a.uscita;
    EXPECT_EQ(a.uscita.find("ROSA"), std::string::npos) << a.uscita;
    EXPECT_EQ(p.normale(), prima);
    const Risultato s = p.a_mano("aggiungi-in-elenco", nuovo, "A", kPw, {"--profilo", "PR-CVA", "--includi-dati-personali"});
    EXPECT_NE(s.uscita.find("U-000006"), std::string::npos) << s.uscita;
    EXPECT_NE(s.uscita.find("nome: ROSA DEMO"), std::string::npos) << s.uscita;
    // Cambio della casa di ROSSI ANNA (U-000001): nel file normale ne' la casa nuova ne' quella di prima.
    const Risultato c = p.a_mano("cambia-in-elenco", kCasaNuova, "A", kPw, {"--utente-elenco", "U-000001"});
    EXPECT_NE(c.uscita.find("casa: prima (riservato"), std::string::npos) << c.uscita;
    const std::string normale = p.normale();
    for (const char* x : {"ROSA", "ROSSI", "via Roma", "Larici", "Croce", "\"prima\": {"}) EXPECT_EQ(normale.find(x), std::string::npos) << x;
    EXPECT_NE(normale.find("\"a_mano\""), std::string::npos);
    // Con il parametro: il valore di prima e quello nuovo.
    const Risultato v = p.a_mano("cambia-in-elenco", {"sezione: B"}, "A", kPw, {"--utente-elenco", "U-000001", "--includi-dati-personali"}, false);
    EXPECT_NE(v.uscita.find("sezione: prima A; dopo B"), std::string::npos) << v.uscita;
    // Il registro: solo codici.
    p.amb.password = {kPw};
    const Risultato r = comando({"registro", p.chiuso.string(), "--utente", "A"});
    EXPECT_NE(r.uscita.find("modifica a mano dell'elenco (cambiato): utente U-000001, committente CVA, campi: casa"), std::string::npos) << r.uscita;
    EXPECT_EQ(r.uscita.find("Larici"), std::string::npos);
    // B legge lo stato con kzero controlla (dati comuni), non i valori.
    p.amb.password = {kPwB};
    const Risultato b = comando({"controlla", p.chiuso.string(), "--utente", "B", "--includi-dati-personali"});
    EXPECT_NE(b.uscita.find("K-CTR-153"), std::string::npos) << b.uscita;
    EXPECT_NE(b.uscita.find("K-CTR-154"), std::string::npos) << b.uscita;
    EXPECT_EQ(b.uscita.find("Larici"), std::string::npos);
}

// Errori dei comandi: campi scritti male (K-ERR-050), modifica non ammessa (K-ERR-051), utente che non c'e' (K-ERR-048), --scrivi su
// un'altra cartella (K-ERR-046). Niente cambia.
TEST(ElencoAManoComandi, Errori) {
    Prova p;
    const std::string prima = p.normale();
    EXPECT_EQ(errore_di([&] { p.a_mano("cambia-in-elenco", {"telefono: 000"}, "A", kPw, {"--utente-elenco", "U-000001"}); }), "K-ERR-050");
    EXPECT_EQ(errore_di([&] { p.a_mano("cambia-in-elenco", {"via: via Nuova"}, "A", kPw, {"--utente-elenco", "U-000001"}); }), "K-ERR-050");
    EXPECT_EQ(errore_di([&] { p.a_mano("cambia-in-elenco", {"nome: \x01"}, "A", kPw, {"--utente-elenco", "U-000001"}); }), "K-ERR-050");
    EXPECT_EQ(errore_di([&] { p.a_mano("cambia-in-elenco", {"nome: NERI MARCO"}, "A", kPw, {"--utente-elenco", "U-000001"}); }), "K-ERR-051");
    EXPECT_EQ(errore_di([&] { p.a_mano("cambia-in-elenco", {"sezione: A"}, "A", kPw, {"--utente-elenco", "U-000001"}); }), "K-ERR-051");
    EXPECT_EQ(errore_di([&] { p.a_mano("rimetti-in-elenco", {}, "A", kPw, {"--utente-elenco", "U-000001"}); }), "K-ERR-051");
    EXPECT_EQ(errore_di([&] { p.a_mano("togli-da-elenco", {}, "A", kPw, {"--utente-elenco", "U-000099"}); }), "K-ERR-048");
    EXPECT_EQ(errore_di([&] {
                  p.amb.password = {kPw};
                  comando({"togli-da-elenco", p.chiuso.string(), "--utente", "A", "--riferimento", "x", "--data", "2026-11-12", "--utente-elenco",
                           "U-000001", "--scrivi", (p.lavoro / "altra").string()});
              }),
              "K-ERR-046");
    EXPECT_EQ(p.normale(), prima);
}

// Uscite (testo) di un'aggiunta, di un cambio di casa e di un utente tolto; poi una lettura con i tre casi e un nome corretto a
// mano. Le scrive tutte (con --includi-dati-personali) per il resoconto, e controlla le righe che contano.
TEST(ElencoAManoComandi, UsciteDelResoconto) {
    Prova p;
    const Risultato a = p.a_mano("aggiungi-in-elenco",
                                 {"nome: ROSA DEMO", "scuola: " + kPrim, "stato: ATTIVO", "percorsi: AM/ALFA RM/ALFA", "comune: Pradello", "frazione:",
                                  "via: via dei Mille", "civico: 7", "orari ORARI MATTINA: AM: 08:00 LU-MA-ME-GI-VE | RM: 12:00 LU-MA-ME-GI-VE"},
                                 "A", kPw, {"--profilo", "PR-CVA", "--includi-dati-personali"});
    const Risultato c = p.a_mano("cambia-in-elenco", kCasaNuova, "A", kPw, {"--utente-elenco", "U-000001", "--includi-dati-personali"});
    // Cambio dello stato di VERDI LUCA (il file lo avra' uguale, attivo e senza la nota: confermato), della sezione di BIANCHI SOFIA (il file avra' un terzo
    // valore) e del nome di NERI MARCO (il file avra' ancora quello di prima).
    p.a_mano("cambia-in-elenco", {"stato: ATTIVO"}, "A", kPw, {"--utente-elenco", "U-000002"});
    p.a_mano("cambia-in-elenco", {"sezione: C"}, "A", kPw, {"--utente-elenco", "U-000003"});
    p.a_mano("cambia-in-elenco", {"nome: NERI MARCO ANTONIO"}, "A", kPw, {"--utente-elenco", "U-000004"});
    const Risultato t = p.a_mano("togli-da-elenco", {}, "A", kPw, {"--utente-elenco", "U-000005", "--includi-dati-personali"});
    // Il file: la seconda lettura (ROSSI con la casa di prima ma civico 3/B, VERDI attivo, NERI con un orario nuovo) con BIANCHI in
    // sezione D e senza GIALLI (tolto a mano: se ne va davvero).
    std::vector<std::vector<std::string>> righe;
    std::istringstream in(leggi_testo(kCartella + "/seconda-lettura.tsv"));
    for (std::string r; std::getline(in, r);) {
        std::vector<std::string> celle;
        std::size_t x = 0;
        for (;;) {
            const std::size_t b = r.find('\t', x);
            celle.push_back(r.substr(x, b == std::string::npos ? std::string::npos : b - x));
            if (b == std::string::npos) break;
            x = b + 1;
        }
        righe.push_back(celle);
    }
    righe[3][8] = "D";
    std::ofstream(p.lavoro / "seconda.xlsx", std::ios::binary) << xlsx::file_righe(righe);
    const Risultato l = p.leggi_file("seconda.xlsx", "A", kPw, false, {"--includi-dati-personali"});
    std::cout << "--- aggiunta ---\n" << a.uscita << "--- cambio di casa ---\n" << c.uscita << "--- utente tolto ---\n" << t.uscita
              << "--- lettura ---\n" << l.uscita;
    EXPECT_NE(a.uscita.find("Utente aggiunto a mano all'elenco: U-000006"), std::string::npos);
    EXPECT_NE(c.uscita.find("casa: prima via Roma 3, Pradello; dopo via dei Larici 23, Croce, Pradello"), std::string::npos) << c.uscita;
    EXPECT_NE(t.uscita.find("Utente tolto a mano dall'elenco: U-000005"), std::string::npos);
    EXPECT_NE(l.uscita.find("K-CTR-157: Riga 2: per l'utente U-000001"), std::string::npos) << l.uscita;   // ROSSI: casa con un terzo valore
    // VERDI: lo stato scritto a mano ("ATTIVO", senza la nota) e' quello del file: confermato.
    EXPECT_NE(l.uscita.find("U-000002 \"VERDI LUCA\" (riga 3): invariato; il file conferma le modifiche a mano (stato)."), std::string::npos)
        << l.uscita;
    EXPECT_NE(l.uscita.find("K-CTR-157: Riga 4: per l'utente U-000003"), std::string::npos) << l.uscita;   // BIANCHI: sezione terza
    EXPECT_NE(l.uscita.find("K-CTR-156: Riga 5: per l'utente U-000004"), std::string::npos) << l.uscita;   // NERI: nome di prima
    EXPECT_NE(l.uscita.find("U-000005 \"GIALLI PAOLO\": tolto a mano e non nel file: se ne va davvero"), std::string::npos) << l.uscita;
    EXPECT_EQ(l.uscita.find(": nuovo."), std::string::npos) << l.uscita;   // nessun "uscito + nuovo"
}

// K-021 (punto 0, prove mancanti trovate dall'arbitro nel K-020). A: la nota dello stato non si vede senza --includi-dati-personali.
// G: il testo di una fermata non riconosciuta non si vede senza il parametro. D: togliere a mano con una data prima dell'aggiunta e'
// un errore (K-ERR-051). Testo e JSON.
TEST(ElencoAManoComandi, NotaETestoDellaFermataRiservati) {
    Prova p;
    // VERDI LUCA (U-000002) e' sospeso con una nota; la nota nuova e una fermata che non e' un luogo del pacchetto.
    const std::vector<std::string> righe = {"stato: SOSPESO - nota nuova riservata", "fermata: Bivio del castagno nascosto"};
    for (const bool json : {false, true}) {
        std::vector<std::string> altri = {"--utente-elenco", "U-000002"};
        if (json) altri.insert(altri.end(), {"--formato", "json"});
        const Risultato r = p.a_mano("cambia-in-elenco", righe, "A", kPw, altri, false);
        for (const char* x : {"nota nuova riservata", "da attivare solo per ritorno", "castagno"})
            EXPECT_EQ(r.uscita.find(x), std::string::npos) << x << "\n" << r.uscita;
        altri.push_back("--includi-dati-personali");
        const Risultato v = p.a_mano("cambia-in-elenco", righe, "A", kPw, altri, false);
        for (const char* x : {"nota nuova riservata", "da attivare solo per ritorno", "castagno"})
            EXPECT_NE(v.uscita.find(x), std::string::npos) << x << "\n" << v.uscita;
    }
    // D: aggiunto il 12/11, tolto con la data dell'11/11: errore, niente cambia; il 12/11 va bene.
    p.a_mano("aggiungi-in-elenco", {"nome: ROSA DEMO", "scuola: " + kPrim, "stato: ATTIVO", "percorsi: AM/ALFA"}, "A", kPw, {"--profilo", "PR-CVA"});
    const std::string prima = p.normale();
    EXPECT_EQ(errore_di([&] { p.a_mano("togli-da-elenco", {}, "A", kPw, {"--utente-elenco", "U-000006"}, true, "2026-11-11"); }), "K-ERR-051");
    EXPECT_EQ(p.normale(), prima);
    const Risultato t = p.a_mano("togli-da-elenco", {}, "A", kPw, {"--utente-elenco", "U-000006"}, true, "2026-11-12");
    EXPECT_NE(t.uscita.find("Utente tolto a mano dall'elenco: U-000006"), std::string::npos) << t.uscita;
}
