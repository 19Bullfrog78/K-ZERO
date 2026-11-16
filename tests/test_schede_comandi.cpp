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
    void file(const std::string& nome, const Fogli& f, bool date1904 = false) {
        std::ofstream(lavoro / nome, std::ios::binary) << file_schede(f, date1904);
    }
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

// Tipo, pedana, servizio individualizzato e accompagnatore richiesto (dati particolari) solo con --includi-dati-particolari, in testo e
// in JSON, anche tra le cose che cambiano tra due versioni; nel file normale del pacchetto chiuso mai (stanno nella cassaforte). Il
// movimento dell'accompagnatore sulla corsa resta comune. Sabotaggio: pedana o "individualizzato" visibili senza i parametri.
TEST(SchedeComandi, ParticolariSoloConIlParametro) {
    Prova p;
    Fogli f = fogli_di_prova();
    f.schede.resize(2);
    f.fermate.resize(4);
    f.schede[1][14] = "X";
    f.schede[1][15] = "X";
    p.file("una.xlsx", f);
    const auto controlla = [](const Risultato& r, bool json, bool particolari) {
        if (json) {
            const JsonDati j = JsonDati::parse(r.uscita);
            for (const JsonDati& s : j.at("schede")) {
                EXPECT_EQ(s.at("particolari").is_null(), !particolari) << r.uscita;
                for (const JsonDati& c : s.at("cambia"))
                    EXPECT_EQ(c.at("riservato").get<bool>(), c.at("cosa") == "dati_particolari") << r.uscita;
            }
        }
        const std::vector<std::string> nascosti = json ? std::vector<std::string>{"\"pedana_obbligatoria\"", "\"individualizzato\"",
                                                                                   "\"tipo\": \"scolastico\"", "\"assistente_richiesto\""}
                                                       : std::vector<std::string>{"pedana obbligatoria", "individualizzato si'", "individualizzato no",
                                                                                  "Tipo scolastico", "accompagnatore richiesto no.", "attrezzato"};
        for (const std::string& x : nascosti) {
            if (!particolari) {
                EXPECT_EQ(r.uscita.find(x), std::string::npos) << x << "\n" << r.uscita;
            }
        }
        if (particolari && r.uscita.find(json ? "\"esito\": \"nuova\"" : "scheda nuova") != std::string::npos) {
            for (const std::string& x : json ? std::vector<std::string>{"\"pedana_obbligatoria\": true", "\"individualizzato\": true"}
                                             : std::vector<std::string>{"pedana obbligatoria si'", "servizio individualizzato si'"})
                EXPECT_NE(r.uscita.find(x), std::string::npos) << x << "\n" << r.uscita;
        }
    };
    for (const bool json : {false, true})
        for (const bool particolari : {false, true}) {
            std::vector<std::string> a = {"--validita", "attivazione"};
            if (json) a.insert(a.end(), {"--formato", "json"});
            if (particolari) a.push_back("--includi-dati-particolari");
            controlla(p.leggi("una.xlsx", a, false), json, particolari);
        }
    p.leggi("una.xlsx", {"--validita", "attivazione"});
    for (const char* x : {"pedana", "individualizzato", "attrezzato", "\"tipo\""}) {
        EXPECT_EQ(p.normale("servizi.json").find(std::string("\"") + x), std::string::npos) << x;
        EXPECT_EQ(p.normale("schede_servizi.json").find(std::string("\"") + x), std::string::npos) << x;
    }
    // La versione nuova toglie le due caselle: tra le cose che cambiano, riservate senza il parametro.
    f.schede[1][14] = "";
    f.schede[1][15] = "";
    f.schede[1][21] = "Modifica";
    p.file("due.xlsx", f);
    for (const bool json : {false, true}) {
        std::vector<std::string> a = {"--validita", "presa-in-carico"};
        if (json) a.insert(a.end(), {"--formato", "json"});
        const Risultato r = p.leggi("due.xlsx", a, false);
        EXPECT_NE(r.uscita.find(json ? "\"cosa\": \"dati_particolari\"" : "Cambia anche un dato particolare (riservato: --includi-dati-particolari)."),
                  std::string::npos)
            << r.uscita;
        EXPECT_EQ(r.uscita.find("prima si'"), std::string::npos) << r.uscita;
        controlla(r, json, false);
    }
}

// kzero togli-scheda: anteprima e scrittura; l'ultima versione se ne va e quella di prima riprende la sua fine; il registro degli accessi
// ha solo codici; riferimento e data restano nel registro delle schede. Non toglie una corsa usata da un turno (K-ERR-055).
TEST(SchedeComandi, TogliScheda) {
    Prova p;
    p.file("tre.xlsx", fogli_di_prova());
    p.leggi("tre.xlsx", {"--validita", "attivazione"});
    Fogli v = fogli_di_prova();
    v.schede.resize(2);
    v.schede[1][21] = "Modifica";
    v.fermate.resize(4);
    v.fermate[1][1] = "07:25";
    p.file("versione.xlsx", v);
    p.leggi("versione.xlsx", {"--validita", "presa-in-carico"}, true, "A", kPw, "2026-11-20");
    const std::string prima = p.normale("schede_servizi.json");
    const Risultato a = p.togli("SCH-4000123-3", {}, false);
    EXPECT_NE(a.uscita.find("Anteprima"), std::string::npos) << a.uscita;
    EXPECT_EQ(p.normale("schede_servizi.json"), prima);
    const Risultato t = p.togli("5c1e2f40-0a1b-4c2d-8e3f-000000000001", {});   // anche con l'ID
    std::cout << "--- togli-scheda ---\n" << t.uscita;
    EXPECT_NE(t.uscita.find("tolta la versione 2"), std::string::npos) << t.uscita;
    EXPECT_NE(t.uscita.find("La versione 1 riprende la sua fine: vale fino al 2027-06-12."), std::string::npos) << t.uscita;
    const std::string dopo = p.normale("schede_servizi.json");
    EXPECT_NE(dopo.find("\"tolte\""), std::string::npos);
    EXPECT_NE(dopo.find("scheda copiata per errore"), std::string::npos);
    EXPECT_EQ(p.normale("servizi.json").find("S-4000123-3-V2"), std::string::npos);
    // Il registro degli accessi: codici e conteggi, nient'altro.
    p.amb.password = {kPw};
    const Risultato r = comando({"registro", p.chiuso.string(), "--utente", "A"});
    EXPECT_NE(r.uscita.find("scheda dei servizi tolta: SCH-4000123-3, versione 2"), std::string::npos) << r.uscita;
    EXPECT_NE(r.uscita.find("lettura delle schede dei servizi, profilo PS-CVA, contratto CT-SCU"), std::string::npos) << r.uscita;
    // Una corsa usata da un turno non si toglie.
    PacchettoInMemoria m = pacchetto_con_schede();
    JsonDati turni = JsonDati::parse(m.file.at("turni.json"));
    turni.at("turni").at(0).at("elementi").push_back({{"corsa", "C-4000124-1"}});
    m.file["turni.json"] = turni.dump(2);
    Prova q(m);
    EXPECT_EQ(errore_di([&] { q.togli("SCH-4000124-1", {}); }), "K-ERR-055");
}

// S11 (K-022): il segno "date dal 1904" della cartella di lavoro si legge dal file, fino alle date lette: le stesse date scritte come
// numeri di Excel nel sistema del 1904 danno gli stessi giorni; senza il segno, gli stessi numeri sono altri giorni (sistema 1900).
// Sabotaggi: il segno del file ignorato (dal lettore Excel o dal comando).
TEST(SchedeComandi, DateDal1904LetteDalFile) {
    Prova p;
    const Fogli tutte = fogli_di_prova();
    Fogli f;
    f.schede = {tutte.schede[0], tutte.schede[2]};   // 4000124/1: attivazione e disattivazione come numeri di Excel
    f.fermate = {tutte.fermate[0], tutte.fermate[4], tutte.fermate[5], tutte.fermate[6], tutte.fermate[7]};
    f.schede[1][4] = "#44817";   // 2026-09-14 nel sistema del 1904 (1462 giorni in meno)
    f.schede[1][5] = "#45088";   // 2027-06-12
    p.file("1904.xlsx", f, true);
    p.file("1900.xlsx", f, false);
    const auto periodo = [&](const std::string& nome) {
        const JsonDati j = JsonDati::parse(p.leggi(nome, {"--validita", "attivazione", "--formato", "json"}, false).uscita);
        const JsonDati& s = j.at("schede").at(0);
        EXPECT_EQ(s.at("esito"), "nuova") << j.dump(2);
        return s.at("dal").get<std::string>() + " " + s.at("al").get<std::string>();
    };
    EXPECT_EQ(periodo("1904.xlsx"), "2026-09-14 2027-06-12");
    EXPECT_EQ(periodo("1900.xlsx"), "2022-09-13 2023-06-11");
}
