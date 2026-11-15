// K-Zero - prove dei comandi delle schede dei servizi (K-021): kzero leggi-schede e kzero togli-scheda con la cassaforte (permessi,
// anteprima, scrittura, registro con i soli codici), le uscite del resoconto e le versioni. Le regole del nucleo: test_schede.cpp.
// Dati inventati (tests/esempi/k021-schede).
#include <gtest/gtest.h>

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "cassaforte_prove.hpp"
#include "errori.hpp"
#include "schede_prove.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove;
using namespace kzero::prove::schede;
namespace fs = std::filesystem;

namespace {

const std::string kPw = "cavallo pesca lampione verde";
const std::string kPwB = "barca gialla sul lago verde";

struct Prova {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    fs::path chiuso;
    explicit Prova(PacchettoInMemoria m = pacchetto_di_prova()) {
        fs::create_directories(lavoro / "chiaro");
        for (const auto& [f, t] : m.file) std::ofstream(lavoro / "chiaro" / f, std::ios::binary) << t;
        amb.password = {kPw};
        comando({"chiudi", (lavoro / "chiaro").string(), "--scrivi", (lavoro / "chiuso").string(), "--utente", "A", "--conservazione", "2099-12-31"});
        chiuso = lavoro / "chiuso";
        amb.password = {kPw, kPwB};
        comando({"aggiungi-utente", "--scrivi", chiuso.string(), "--utente", "A", "--nuovo-utente", "B", "--permessi", "personali"});
    }
    void file(const std::string& nome, const Fogli& f) { std::ofstream(lavoro / nome, std::ios::binary) << file_schede(f); }
    Risultato leggi(const std::string& nome, std::vector<std::string> altri, bool scrivere = true, const std::string& utente = "A",
                    const std::string& pw = kPw, const std::string& data = "2026-11-16") {
        amb.password = {pw};
        std::vector<std::string> a = {"leggi-schede", (lavoro / nome).string(), chiuso.string(), "--profilo", "PS-CVA", "--contratto", "CT-SCU",
                                      "--data", data, "--riferimento", "mail del committente", "--utente", utente};
        if (scrivere) a.insert(a.end(), {"--scrivi", chiuso.string()});
        a.insert(a.end(), altri.begin(), altri.end());
        return comando(a);
    }
    Risultato togli(const std::string& scheda, std::vector<std::string> altri, bool scrivere = true) {
        amb.password = {kPw};
        std::vector<std::string> a = {"togli-scheda", chiuso.string(), "--scheda", scheda, "--riferimento", "scheda copiata per errore",
                                      "--data", "2026-11-17", "--utente", "A"};
        if (scrivere) a.insert(a.end(), {"--scrivi", chiuso.string()});
        a.insert(a.end(), altri.begin(), altri.end());
        return comando(a);
    }
    std::string normale(const std::string& f) const { return elenco_a_mano::leggi_testo(chiuso / f); }
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

// Le uscite del resoconto (testo): tre schede (una normale, una con due scuole e i totali giusti, una con un totale sbagliato), una
// versione nuova della stessa scheda con la scelta della data, l'uscita quando l'attivazione e' prima della presa in carico.
TEST(SchedeComandi, UsciteDelResoconto) {
    Prova p;
    p.file("tre.xlsx", fogli_di_prova());
    const Risultato senza = p.leggi("tre.xlsx", {}, false);
    const Risultato tre = p.leggi("tre.xlsx", {"--validita", "attivazione", "--includi-dati-particolari"});
    // La versione nuova della prima scheda: modifica, un orario cambiato, valida dal 23/11 (scelta per quella scheda sola).
    Fogli v = fogli_di_prova();
    v.schede.resize(2);
    v.schede[1][21] = "Modifica";
    v.fermate.resize(4);
    v.fermate[1][1] = "07:25";
    p.file("versione.xlsx", v);
    const Risultato ver = p.leggi("versione.xlsx", {"--validita-scheda", "4000123/3=2026-11-23", "--includi-dati-particolari"}, true, "A", kPw,
                                  "2026-11-20");
    std::cout << "--- attivazione prima della presa in carico, senza scelta ---\n" << senza.uscita << "--- tre schede ---\n" << tre.uscita
              << "--- versione nuova ---\n" << ver.uscita;
    EXPECT_NE(senza.uscita.find("K-CTR-162"), std::string::npos) << senza.uscita;
    EXPECT_NE(tre.uscita.find("2 nuove, 0 versioni nuove, 0 uguali (niente da fare), 1 ferma"), std::string::npos) << tre.uscita;
    EXPECT_NE(ver.uscita.find("versione nuova 2 della scheda SCH-4000123-3"), std::string::npos) << ver.uscita;
    EXPECT_NE(ver.uscita.find("La versione 1 vale fino al 2026-11-22"), std::string::npos) << ver.uscita;
}

// Permessi (B ha solo i dati personali: K-ERR-023), contratto di un altro committente o che non c'e' (K-ERR-052), scelta della data
// scritta male (K-ERR-053), --scrivi su un'altra cartella (K-ERR-046), scheda da togliere che non c'e' (K-ERR-054). Niente cambia.
TEST(SchedeComandi, Errori) {
    Prova p;
    p.file("tre.xlsx", fogli_di_prova());
    const std::string prima = p.normale("servizi.json");
    EXPECT_EQ(errore_di([&] { p.leggi("tre.xlsx", {"--validita", "attivazione"}, true, "B", kPwB); }), "K-ERR-023");
    EXPECT_EQ(errore_di([&] {
                  p.amb.password = {kPw};
                  comando({"leggi-schede", (p.lavoro / "tre.xlsx").string(), p.chiuso.string(), "--profilo", "PS-CVA", "--contratto", "CT-LIN",
                           "--data", "2026-11-16", "--riferimento", "x", "--utente", "A", "--validita", "attivazione"});
              }),
              "K-ERR-052");
    EXPECT_EQ(errore_di([&] { p.leggi("tre.xlsx", {"--validita", "domani"}); }), "K-ERR-053");
    EXPECT_EQ(errore_di([&] { p.leggi("tre.xlsx", {"--validita-scheda", "4000123/3"}); }), "K-ERR-053");
    EXPECT_EQ(errore_di([&] { p.leggi("tre.xlsx", {"--validita-scheda", "4000123/3=attivazione;4000123 / 3=2026-11-20"}); }), "K-ERR-053");
    EXPECT_EQ(errore_di([&] {
                  p.amb.password = {kPw};
                  comando({"leggi-schede", (p.lavoro / "tre.xlsx").string(), p.chiuso.string(), "--profilo", "PS-CVA", "--contratto", "CT-SCU",
                           "--data", "2026-11-16", "--riferimento", "x", "--utente", "A", "--scrivi", (p.lavoro / "altra").string()});
              }),
              "K-ERR-046");
    EXPECT_EQ(errore_di([&] { p.togli("SCH-NON-CE", {}); }), "K-ERR-054");
    EXPECT_EQ(p.normale("servizi.json"), prima);
}
