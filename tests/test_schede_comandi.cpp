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
// numeri di Excel nel sistema del 1904 danno gli stessi giorni; senza il segno, gli stessi numeri sono altri giorni (sistema 1900: dal
// 2022-09-13 al 2023-06-11, fuori dal calendario SCOLASTICO; dal K-023 la scheda si ferma, variante 47, perche' non avrebbe nessun giorno).
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
    const auto lettura = [&](const std::string& nome) {
        return JsonDati::parse(p.leggi(nome, {"--validita", "attivazione", "--formato", "json"}, false).uscita);
    };
    const JsonDati j = lettura("1904.xlsx");
    const JsonDati& s = j.at("schede").at(0);
    EXPECT_EQ(s.at("esito"), "nuova") << j.dump(2);
    EXPECT_EQ(s.at("dal").get<std::string>() + " " + s.at("al").get<std::string>(), "2026-09-14 2027-06-12");
    // Sistema 1900: le stesse cifre sono il 2022-2023, fuori dal calendario della scheda.
    const JsonDati k = lettura("1900.xlsx");
    EXPECT_EQ(k.at("schede").at(0).at("esito"), "ferma") << k.dump(2);
    bool senza_giorni = false;
    for (const JsonDati& x : k.at("problemi"))
        senza_giorni = senza_giorni || (x.at("codice") == "K-CTR-160" && x.at("messaggio").get<std::string>().find("nessun giorno di servizio") !=
                                                                                 std::string::npos);
    EXPECT_TRUE(senza_giorni) << k.dump(2);
}

// K-024, punto 0 (S10, sabotaggio dell'arbitro sul K-023 non preso): il segno "date dal 1904" vale anche per il foglio delle interruzioni.
// La scheda 4000123/3 con il profilo PS-CVA alla versione 2 (con il foglio delle interruzioni), attivazione e disattivazione e due delle tre
// interruzioni scritte come numeri di Excel nel sistema del 1904 (la terza come testo): le interruzioni si leggono nei giorni giusti
// (21/12/2026 - 08/01/2027, 15 - 19/02/2027, 03/05/2027) e il Tot Giorni 165 torna. Lette come date del 1900 le stesse cifre cadrebbero
// nel 2022, fuori dal periodo della scheda (variante 44). Sabotaggio: le interruzioni lette sempre nel sistema del 1900.
TEST(SchedeComandi, InterruzioniDal1904LetteDalFile) {
    Prova p(pacchetto_k023());
    const Fogli tutte = fogli_di_prova();
    Fogli f;
    f.schede = {tutte.schede[0], tutte.schede[1]};
    f.fermate = {tutte.fermate[0], tutte.fermate[1], tutte.fermate[2], tutte.fermate[3]};
    f.schede[1][4] = "#44817";    // 2026-09-14 nel sistema del 1904
    f.schede[1][5] = "#45088";    // 2027-06-12
    f.schede[1][16] = "#165";     // 175 giorni meno le interruzioni
    f.interruzioni = {{"Nr", "Dal", "Al"},
                      {"4000123 / 3", "#44915", "#44933"},          // 2026-12-21 - 2027-01-08
                      {"4000123 / 3", "15/02/2027", "19/02/2027"},  // come testo: non cambia con il sistema delle date
                      {"4000123 / 3", "#45048", "#45048"}};         // 2027-05-03
    p.file("1904.xlsx", f, true);
    const JsonDati j = JsonDati::parse(p.leggi("1904.xlsx", {"--validita", "attivazione", "--formato", "json"}, false).uscita);
    const JsonDati& s = j.at("schede").at(0);
    ASSERT_EQ(s.at("esito"), "nuova") << j.dump(2);
    EXPECT_EQ(j.at("righe").at("interruzioni"), 3);
    EXPECT_EQ(s.at("dal").get<std::string>() + " " + s.at("al").get<std::string>(), "2026-09-14 2027-06-12");
    const JsonDati attese = JsonDati::array({{{"dal", "2026-12-21"}, {"al", "2027-01-08"}},
                                             {{"dal", "2027-02-15"}, {"al", "2027-02-19"}},
                                             {{"dal", "2027-05-03"}, {"al", "2027-05-03"}}});
    EXPECT_EQ(s.at("interruzioni"), attese) << j.dump(2);
    for (const JsonDati& x : j.at("problemi")) EXPECT_NE(x.at("codice"), "K-CTR-163") << j.dump(2);
}

namespace {

// Il pacchetto di prova (in chiaro) con la scheda d'infanzia a due giri (km solo sull'ultima riga) e dei turni con i pezzi dati della
// sua corsa C-4000300-1: ogni pezzo e' (da, a) con il luogo e l'orario del passaggio; vuoto = l'inizio o la fine della corsa.
struct Pezzo {
    std::string da_luogo, da_ora, a_luogo, a_ora;
};
// Ogni turno (T-PEZZO-1, T-PEZZO-2...) ha i suoi pezzi, nell'ordine.
fs::path giornata_con_turni(CartellaDiLavoro& lavoro, const std::string& nome, const std::vector<std::vector<Pezzo>>& per_turno) {
    PacchettoInMemoria m = con_schede(pacchetto_di_prova(), due_giri());
    JsonDati turni = JsonDati::parse(m.file.at("turni.json"));
    for (std::size_t i = 0; i < per_turno.size(); ++i) {
        JsonDati elementi = JsonDati::array();
        for (const Pezzo& pz : per_turno[i]) {
            JsonDati e = {{"corsa", "C-4000300-1"}};
            if (!pz.da_luogo.empty()) e["da"] = {{"luogo", pz.da_luogo}, {"orario", pz.da_ora}};
            if (!pz.a_luogo.empty()) e["a"] = {{"luogo", pz.a_luogo}, {"orario", pz.a_ora}};
            elementi.push_back(e);
        }
        turni.at("turni").push_back({{"codice", "T-PEZZO-" + std::to_string(i + 1)}, {"azienda", "ALFA"}, {"ruolo", "autista"},
                                     {"calendario", "CAL-4000300-1"}, {"mezzo", {{"modo", "da_assegnare"}}}, {"inizio", "L-RIMESSA-ALFA"},
                                     {"fine", "L-RIMESSA-ALFA"}, {"elementi", elementi}});
    }
    m.file["turni.json"] = turni.dump(2);
    const fs::path c = lavoro / nome;
    fs::create_directories(c);
    for (const auto& [f, t] : m.file) std::ofstream(c / f, std::ios::binary) << t;
    return c;
}
// Un turno per pezzo.
fs::path giornata_con_pezzi(CartellaDiLavoro& lavoro, const std::string& nome, const std::vector<Pezzo>& pezzi) {
    std::vector<std::vector<Pezzo>> per_turno;
    for (const Pezzo& pz : pezzi) per_turno.push_back({pz});
    return giornata_con_turni(lavoro, nome, per_turno);
}
const JsonDati& turno(const JsonDati& g, const std::string& codice) {
    for (const JsonDati& t : g.at("turni"))
        if (t.at("codice") == codice) return t;
    throw std::logic_error("turno che non c'e': " + codice);
}

}  // namespace

// Punto 3 (K-022): i km di un pezzo di corsa da scheda i cui capi non hanno km non si sanno. La corsa d'infanzia a due giri (km solo
// sull'ultima riga: 25) divisa in tre turni: fino a Pradello (manca il capo finale), da Pradello a Castelrovo (mancano tutti e due),
// da Castelrovo alla fine (manca il capo iniziale; l'ultima riga ha i 25 km di tutta la corsa). In giornata (versione 6): km del pezzo
// e dei suoi passaggi null ("km non noti" nel testo, mai 0 e mai i 25 km della corsa su un passaggio del pezzo), totali del turno e
// della giornata con la somma dei km noti, "almeno", e quanti pezzi non sono noti; K-CTR-172 al singolare o al plurale. La corsa intera
// in un turno solo: km come prima. Sabotaggi: 0 scritto come prima; i km di tutta la corsa scritti su un passaggio del pezzo.
TEST(SchedeComandi, GiornataConIPezziConIKmNonNoti) {
    CartellaDiLavoro lavoro;
    const fs::path c = giornata_con_pezzi(lavoro, "tre-pezzi", {{"", "", "F-PRADELLO", "07:20"}, {"F-PRADELLO", "07:20", "F-CASTELROVO", "07:40"},
                                                                {"F-CASTELROVO", "07:40", "", ""}});
    const Risultato rj = comando({"giornata", c.string(), "--data", "2026-11-24", "--formato", "json"});
    const JsonDati g = JsonDati::parse(rj.uscita);
    EXPECT_EQ(g.at("schema_versione"), 6);
    for (const char* t : {"T-PEZZO-1", "T-PEZZO-2", "T-PEZZO-3"}) {
        const JsonDati x = turno(g, t);
        const JsonDati& e = x.at("elementi").at(0);
        EXPECT_TRUE(e.at("km_pieni").is_null()) << t;
        EXPECT_TRUE(e.at("km_vuoti").is_null()) << t;
        for (const JsonDati& p : e.at("passaggi")) {
            EXPECT_TRUE(p.at("km_pieni").is_null()) << t;
            EXPECT_TRUE(p.at("km_vuoti").is_null()) << t;
        }
        EXPECT_EQ(x.at("totali").at("km_pieni"), 0.0) << t;
        EXPECT_EQ(x.at("totali").at("pezzi_km_non_noti"), 1) << t;
    }
    EXPECT_EQ(g.at("totali").at("pezzi_km_non_noti"), 3);
    const Risultato rt = comando({"giornata", c.string(), "--data", "2026-11-24"});
    std::cout << "--- giornata con tre pezzi della corsa C-4000300-1 ---\n" << rt.uscita;
    EXPECT_EQ(rt.uscita.find("km a pieno 25"), std::string::npos) << rt.uscita;
    EXPECT_NE(rt.uscita.find("km non noti (corsa da scheda: i capi del pezzo non hanno tutti dei km, K-CTR-172)"), std::string::npos);
    EXPECT_NE(rt.uscita.find("Totale del turno: km a pieno almeno 0, a vuoto almeno 0 (1 pezzo con i km non noti)"), std::string::npos)
        << rt.uscita;
    EXPECT_NE(rt.uscita.find("(dei mezzi guidati; 3 pezzi con i km non noti)"), std::string::npos) << rt.uscita;
    // K-CTR-172: un capo senza km (variante 0) o tutti e due (variante 1).
    const Lettura l = leggi_pacchetto(CartellaSuDisco(c));
    std::vector<Problem> v;
    check_cards(l.pacchetto, Index(l.pacchetto), v);
    std::vector<int> varianti;
    for (const Problem& p : v)
        if (p.check == Check::CardTripPartKmUnknown) varianti.push_back(p.variant);
    EXPECT_EQ(varianti, (std::vector<int>{0, 1, 0}));
    // La corsa intera in un turno: i km del piede, come prima.
    const fs::path d = giornata_con_pezzi(lavoro, "intera", {{"", "", "", ""}});
    const JsonDati h = JsonDati::parse(comando({"giornata", d.string(), "--data", "2026-11-24", "--formato", "json"}).uscita);
    const JsonDati x = turno(h, "T-PEZZO-1");
    EXPECT_EQ(x.at("elementi").at(0).at("km_pieni"), 25.0);
    EXPECT_EQ(x.at("totali").at("km_pieni"), 25.0);
    EXPECT_EQ(x.at("totali").at("pezzi_km_non_noti"), 0);
    EXPECT_EQ(h.at("totali").at("pezzi_km_non_noti"), 0);
}

// K-023, prova mancante del K-022 (N5): un turno con due pezzi della corsa da scheda dai km non noti (fino a Pradello, poi da Pradello
// alla fine: la corsa resta coperta tutta, altrimenti K-CTR-061). "2 pezzi" al plurale nel totale del turno e in quello della giornata, in testo e in JSON (con un pezzo il singolare:
// GiornataConIPezziConIKmNonNoti). Sabotaggi: i pezzi di un turno contati al massimo uno; il plurale solo da 3 in su.
TEST(SchedeComandi, GiornataConDuePezziNelloStessoTurno) {
    CartellaDiLavoro lavoro;
    const fs::path c = giornata_con_turni(lavoro, "due-pezzi", {{{"", "", "F-PRADELLO", "07:20"}, {"F-PRADELLO", "07:20", "", ""}}});
    const Risultato rj = comando({"giornata", c.string(), "--data", "2026-11-24", "--formato", "json"});
    const JsonDati g = JsonDati::parse(rj.uscita);
    const JsonDati x = turno(g, "T-PEZZO-1");
    ASSERT_EQ(x.at("elementi").size(), 2u);
    for (const JsonDati& e : x.at("elementi")) EXPECT_TRUE(e.at("km_pieni").is_null());
    EXPECT_EQ(x.at("totali").at("pezzi_km_non_noti"), 2);
    EXPECT_EQ(g.at("totali").at("pezzi_km_non_noti"), 2);
    const Risultato rt = comando({"giornata", c.string(), "--data", "2026-11-24"});
    std::cout << "--- giornata con due pezzi della corsa C-4000300-1 nello stesso turno ---\n" << rt.uscita;
    EXPECT_NE(rt.uscita.find("Totale del turno: km a pieno almeno 0, a vuoto almeno 0 (2 pezzi con i km non noti)"), std::string::npos)
        << rt.uscita;
    EXPECT_NE(rt.uscita.find("(dei mezzi guidati; 2 pezzi con i km non noti)"), std::string::npos) << rt.uscita;
    EXPECT_EQ(rt.uscita.find("2 pezzo"), std::string::npos) << rt.uscita;
}

// K-023, prova mancante del K-022 (N8): una versione nuova che toglie una sola versione dice "Versione tolta (cominciava ...)" al
// singolare; con due, "Versioni tolte (cominciavano ...)". La scheda 4000123/3 ha la versione 2 dal 23/11 e la 3 dal 25/11; una versione
// nuova dal 24/11 toglie la 3 sola, una dal 21/11 toglie la 2 e la 4. Sabotaggi: sempre il plurale; sempre il singolare.
TEST(SchedeComandi, VersioniTolteAlSingolareEAlPlurale) {
    Prova p;
    Fogli f = fogli_di_prova();
    f.schede.resize(2);
    f.fermate.resize(4);
    p.file("v1.xlsx", f);
    p.leggi("v1.xlsx", {"--validita", "attivazione"});
    f.schede[1][21] = "Modifica";
    const auto versione = [&](const std::string& orario, const std::string& dal) {
        f.fermate[1][1] = orario;   // ogni versione con un orario diverso: altrimenti e' "uguale"
        p.file("v.xlsx", f);
        return p.leggi("v.xlsx", {"--validita-scheda", "4000123/3=" + dal}, true, "A", kPw, "2026-11-20").uscita;
    };
    EXPECT_NE(versione("07:25", "2026-11-23").find("versione nuova 2 della scheda SCH-4000123-3"), std::string::npos);
    EXPECT_NE(versione("07:26", "2026-11-25").find("versione nuova 3 della scheda SCH-4000123-3"), std::string::npos);
    const std::string una = versione("07:27", "2026-11-24");
    std::cout << "--- versione nuova che toglie una versione ---\n" << una;
    EXPECT_NE(una.find("versione nuova 4 della scheda SCH-4000123-3"), std::string::npos) << una;
    EXPECT_NE(una.find("     Versione tolta (cominciava dal 2026-11-24 in poi): 3.\n"), std::string::npos) << una;
    EXPECT_EQ(una.find("Versioni tolte"), std::string::npos) << una;
    const std::string due = versione("07:28", "2026-11-21");
    std::cout << "--- versione nuova che toglie due versioni ---\n" << due;
    EXPECT_NE(due.find("versione nuova 5 della scheda SCH-4000123-3"), std::string::npos) << due;
    EXPECT_NE(due.find("     Versioni tolte (cominciavano dal 2026-11-21 in poi): 2, 4.\n"), std::string::npos) << due;
    EXPECT_EQ(due.find("Versione tolta"), std::string::npos) << due;
}

namespace {
// K-023: la scheda 4000123/3 con tre interruzioni (Tot Giorni 165, come in test_schede.cpp).
Fogli scheda_con_tre_interruzioni() {
    const Fogli tutte = fogli_di_prova();
    Fogli f;
    f.schede = {tutte.schede[0], tutte.schede[1]};
    f.fermate = {tutte.fermate[0], tutte.fermate[1], tutte.fermate[2], tutte.fermate[3]};
    f.date = tutte.date;
    f.schede[1][16] = "#165";
    f.interruzioni = {{"Nr", "Dal", "Al"},
                      {"4000123 / 3", "15/02/2027", "19/02/2027"},
                      {"4000123 / 3", "21/12/2026", "08/01/2027"},
                      {"4000123 / 3", "03/05/2027", "03/05/2027"}};
    return f;
}
std::string riga_che_comincia(const std::string& testo, const std::string& inizio) {
    const std::size_t a = testo.find(inizio);
    return a == std::string::npos ? std::string() : testo.substr(a, testo.find('\n', a) - a);
}
}  // namespace

// Punto 1 (K-023, D3): leggi-schede dice le interruzioni lette. Con il profilo alla versione 2: "3 interruzioni" nella riga delle righe e
// una riga con i periodi sotto la scheda; nel JSON (schede versione 2) le righe del foglio e i periodi; nessun K-CTR-163; il registro del
// pacchetto chiuso alla versione 2 con le interruzioni (dati comuni, nel file normale). Con il profilo di oggi (versione 1) il testo resta
// quello di prima (la riga delle righe senza interruzioni) e il JSON ha i campi nuovi vuoti.
TEST(SchedeComandi, InterruzioniNelleUscite) {
    Prova p(pacchetto_k023());
    p.file("interruzioni.xlsx", scheda_con_tre_interruzioni());
    const Risultato t = p.leggi("interruzioni.xlsx", {"--validita", "attivazione"});
    std::cout << "--- una scheda con tre interruzioni ---\n" << t.uscita;
    EXPECT_EQ(riga_che_comincia(t.uscita, "Righe:"), "Righe: 1 scheda, 3 fermate, 0 date, 3 interruzioni.");
    EXPECT_NE(t.uscita.find("     Interruzioni (il servizio non si fa): dal 2026-12-21 al 2027-01-08, dal 2027-02-15 al 2027-02-19, dal "
                            "2027-05-03 al 2027-05-03.\n"),
              std::string::npos)
        << t.uscita;
    EXPECT_EQ(t.uscita.find("K-CTR-163"), std::string::npos) << t.uscita;
    const std::string registro = p.normale("schede_servizi.json");
    EXPECT_NE(registro.find("\"schema_versione\": 3"), std::string::npos);   // K-024: il registro si scrive sempre alla 3
    EXPECT_NE(registro.find("\"interruzioni\""), std::string::npos);
    EXPECT_NE(p.normale("calendari.json").find("Interruzione della scheda"), std::string::npos);
    Prova q(pacchetto_k023());
    q.file("interruzioni.xlsx", scheda_con_tre_interruzioni());
    const JsonDati j = JsonDati::parse(q.leggi("interruzioni.xlsx", {"--validita", "attivazione", "--formato", "json"}, false).uscita);
    EXPECT_EQ(j.at("schema_versione"), 4);   // K-025
    EXPECT_EQ(j.at("righe").at("interruzioni"), 3);
    ASSERT_EQ(j.at("schede").at(0).at("interruzioni").size(), 3u);
    EXPECT_EQ(j.at("schede").at(0).at("interruzioni").at(0), JsonDati({{"dal", "2026-12-21"}, {"al", "2027-01-08"}}));
    // Il profilo di oggi (versione 1): il quarto foglio non si legge, il testo resta quello di prima.
    Prova r;
    r.file("interruzioni.xlsx", scheda_con_tre_interruzioni());
    const Risultato u = r.leggi("interruzioni.xlsx", {"--validita", "attivazione"}, false);
    EXPECT_EQ(riga_che_comincia(u.uscita, "Righe:"), "Righe: 1 scheda, 3 fermate, 0 date.");
    EXPECT_EQ(u.uscita.find("nterruzion"), std::string::npos) << u.uscita;
    const JsonDati k = JsonDati::parse(r.leggi("interruzioni.xlsx", {"--validita", "attivazione", "--formato", "json"}, false).uscita);
    EXPECT_EQ(k.at("schema_versione"), 4);   // K-025
    EXPECT_EQ(k.at("righe").at("interruzioni"), 0);
    EXPECT_EQ(k.at("schede").at(0).at("interruzioni"), JsonDati::array());
}

// --- K-024: tipo di servizio per scheda -------------------------------------------------------------------------------------------

namespace {
std::string riga_con(const std::string& testo, const std::string& pezzo) {
    std::istringstream in(testo);
    for (std::string r; std::getline(in, r);)
        if (r.find(pezzo) != std::string::npos) return r;
    return std::string();
}
}  // namespace

// K-024, P6: le uscite di leggi-schede con le quattro schede di prova. Con --includi-dati-particolari la riga del tipo dice il tipo del
// gruppo, da dove viene il tipo (la regola RT-1 o il profilo) e il numero dei disabili; il JSON (schede, versione 3) lo stesso nel blocco
// "particolari". Senza il parametro la riga e' quella di oggi e nessun segno della regola: niente tipo, niente regola, niente numero, i
// due avvisi K-CTR-177 contati soltanto. Sabotaggio: tipo scelto mostrato nell'uscita a chi non ha i dati particolari.
TEST(SchedeComandi, TipoPerSchedaNelleUscite) {
    Prova p(pacchetto_k024());
    p.file("k024.xlsx", fogli_k024());
    const Risultato con = p.leggi("k024.xlsx", {"--validita", "attivazione", "--includi-dati-particolari"}, false);
    const Risultato senza = p.leggi("k024.xlsx", {"--validita", "attivazione"}, false);
    std::cout << "--- con i dati particolari ---\n" << con.uscita << "--- senza ---\n" << senza.uscita;
    EXPECT_EQ(riga_con(con.uscita, "Tipo disabili"),
              "     Tipo disabili, gruppo disabili (regola RT-1 del profilo), disabili a bordo 1, pedana obbligatoria si', servizio "
              "individualizzato si', accompagnatore richiesto no.");
    EXPECT_EQ(riga_con(con.uscita, "Tipo scolastico"),
              "     Tipo scolastico, gruppo studenti (tipo del profilo), disabili a bordo 0, pedana obbligatoria no, servizio "
              "individualizzato no, accompagnatore richiesto no.");
    EXPECT_NE(con.uscita.find("Avviso K-CTR-177: Scheda 4000603/1: a bordo ci sono 1 disabile su 8 persone"), std::string::npos);
    EXPECT_NE(con.uscita.find("Avviso K-CTR-177: Scheda 4000604/1: il servizio e' per disabili, ma la scheda scrive 0"), std::string::npos);
    for (const char* x : {"Tipo disabili", "Tipo scolastico", "RT-1", "regola", "disabili a bordo", "K-CTR-177", "gruppo disabili", "gruppo studenti"})
        EXPECT_EQ(senza.uscita.find(x), std::string::npos) << x << "\n" << senza.uscita;
    EXPECT_NE(senza.uscita.find("2 problemi sui dati particolari (salute), non elencati"), std::string::npos) << senza.uscita;
    EXPECT_NE(senza.uscita.find("Tipo, pedana, servizio individualizzato e accompagnatore richiesto non mostrati"), std::string::npos);
    const JsonDati j = JsonDati::parse(p.leggi("k024.xlsx", {"--validita", "attivazione", "--includi-dati-particolari", "--formato", "json"}, false).uscita);
    EXPECT_EQ(j.at("schema_versione"), 4);   // K-025
    EXPECT_EQ(j.at("schede").at(1).at("particolari"),
              JsonDati({{"tipo", "disabili"}, {"tipo_gruppo", "disabili"}, {"da", "regola"}, {"regola", "RT-1"}, {"disabili", 1},
                        {"pedana_obbligatoria", true}, {"individualizzato", true}, {"assistente_richiesto", false}}));
    EXPECT_EQ(j.at("schede").at(0).at("particolari").at("da"), "profilo");
    EXPECT_TRUE(j.at("schede").at(0).at("particolari").at("regola").is_null());
    const JsonDati k = JsonDati::parse(p.leggi("k024.xlsx", {"--validita", "attivazione", "--formato", "json"}, false).uscita);
    for (const JsonDati& s : k.at("schede")) EXPECT_TRUE(s.at("particolari").is_null()) << s.dump();
    EXPECT_EQ(k.at("problemi_riservati"), 2);
    const std::string tutto = k.dump();
    for (const char* x : {"RT-1", "regola", "K-CTR-177"}) EXPECT_EQ(tutto.find(x), std::string::npos) << x;
}

// K-024: con i profili di oggi (versione 1 e 2) l'uscita resta quella di prima: la riga del tipo senza gruppo, origine e numero; nel JSON
// il tipo viene dal profilo, senza regola e senza numero. Sabotaggio: profilo di oggi letto in modo diverso.
TEST(SchedeComandi, ProfiliDiOggiStessaUscita) {
    for (const bool k023 : {false, true}) {
        Prova p(k023 ? pacchetto_k023() : pacchetto_di_prova());
        p.file("tre.xlsx", fogli_di_prova());
        const Risultato t = p.leggi("tre.xlsx", {"--validita", "attivazione", "--includi-dati-particolari"}, false);
        EXPECT_EQ(riga_con(t.uscita, "Tipo "),
                  "     Tipo scolastico, pedana obbligatoria no, servizio individualizzato no, accompagnatore richiesto no.")
            << t.uscita;
        const JsonDati j = JsonDati::parse(p.leggi("tre.xlsx", {"--validita", "attivazione", "--includi-dati-particolari", "--formato", "json"}, false).uscita);
        EXPECT_EQ(j.at("schede").at(0).at("particolari"),
                  JsonDati({{"tipo", "scolastico"}, {"tipo_gruppo", "studenti"}, {"da", "profilo"}, {"regola", nullptr}, {"disabili", nullptr},
                            {"pedana_obbligatoria", false}, {"individualizzato", false}, {"assistente_richiesto", false}}));
    }
}

// K-024: una scheda ferma per due regole con tipi diversi (variante 48, particolare): senza il parametro la riga della scheda non rimanda a
// un problema elencato; con il parametro si'. Una versione nuova per il tipo: senza il parametro "cambia anche un dato particolare", mai
// quale.
TEST(SchedeComandi, RegoleETipoCheCambiaSenzaIDatiParticolari) {
    PacchettoInMemoria m = pacchetto_k024();
    JsonDati d = JsonDati::parse(m.file.at("profili_schede.json"));
    d.at("profili").at(0).at("regole_tipo").push_back(
        {{"codice", "RT-2"}, {"colonna", "servizio"}, {"valori", {"Andata Mattino"}}, {"servizio", {{"tipo", "linea"}, {"tipo_gruppo", "utenti_linea"}}}});
    m.file["profili_schede.json"] = d.dump(2);
    Prova p(m);
    Fogli f = fogli_k024();
    f.schede.resize(3);
    p.file("due.xlsx", f);
    const Risultato senza = p.leggi("due.xlsx", {"--validita", "attivazione"}, false);
    const Risultato con = p.leggi("due.xlsx", {"--validita", "attivazione", "--includi-dati-particolari"}, false);
    EXPECT_NE(senza.uscita.find("4000602/1 (riga 3): ferma: non si legge (il motivo riguarda i dati particolari: --includi-dati-particolari)."),
              std::string::npos)
        << senza.uscita;
    EXPECT_EQ(senza.uscita.find("RT-"), std::string::npos) << senza.uscita;
    EXPECT_NE(con.uscita.find("4000602/1 (riga 3): ferma (K-CTR-160, sotto): non si legge."), std::string::npos) << con.uscita;
    EXPECT_NE(con.uscita.find("le regole del tipo RT-1, RT-2 del profilo scattano tutte"), std::string::npos) << con.uscita;
    // Il tipo che cambia: letta senza la regola RT-1, poi con.
    PacchettoInMemoria n = pacchetto_k024();
    JsonDati e = JsonDati::parse(n.file.at("profili_schede.json"));
    e.at("profili").at(0).erase("regole_tipo");
    n.file["profili_schede.json"] = e.dump(2);
    Prova q(n);
    q.file("k024.xlsx", fogli_k024());
    q.leggi("k024.xlsx", {"--validita", "attivazione"});
    // La regola nel profilo del pacchetto chiuso: il profilo e' un file normale con il tipo nella cassaforte; si rifa' con esporta/richiudi.
    q.amb.password = {kPw};
    comando({"esporta", q.chiuso.string(), "--utente", "A", "--scrivi", (q.lavoro / "esportato").string()});
    std::ofstream(q.lavoro / "esportato" / "profili_schede.json", std::ios::binary) << elenco_a_mano::leggi_testo(kCartella24 + "/profili_schede.json");
    q.amb.password = {kPw};
    comando({"richiudi", (q.lavoro / "esportato").string(), "--utente", "A", "--scrivi", q.chiuso.string()});
    const Risultato v_senza = q.leggi("k024.xlsx", {"--validita", "2026-11-23"}, false, "A", kPw, "2026-11-20");
    const Risultato v_con = q.leggi("k024.xlsx", {"--validita", "2026-11-23", "--includi-dati-particolari"}, false, "A", kPw, "2026-11-20");
    std::cout << "--- tipo che cambia, senza ---\n" << v_senza.uscita << "--- con ---\n" << v_con.uscita;
    EXPECT_NE(v_senza.uscita.find("versione nuova 2 della scheda SCH-4000602-1"), std::string::npos) << v_senza.uscita;
    EXPECT_NE(v_senza.uscita.find("Cambia anche un dato particolare (riservato: --includi-dati-particolari)."), std::string::npos);
    EXPECT_EQ(v_senza.uscita.find("Cambia tipo"), std::string::npos);
    EXPECT_NE(v_con.uscita.find("Cambia tipo: prima scolastico (gruppo studenti); dopo disabili (gruppo disabili)."), std::string::npos)
        << v_con.uscita;
}

// K-024: kzero controlla dopo la scrittura delle quattro schede. Con tutti e due i permessi e il parametro: i due K-CTR-177 (il tipo del
// servizio nel pacchetto) e nessun K-CTR-178 (profilo alla versione 3). L'utente B (solo dati personali) non legge il tipo: K-CTR-177 e'
// tra i controlli non verificati, mai un avviso fatto senza i dati.
TEST(SchedeComandi, ControllaDopoLaScrittura) {
    Prova p(pacchetto_k024());
    p.file("k024.xlsx", fogli_k024());
    p.leggi("k024.xlsx", {"--validita", "attivazione"});
    p.amb.password = {kPw};
    const Risultato a = comando({"controlla", p.chiuso.string(), "--utente", "A", "--includi-dati-particolari"});
    p.amb.password = {kPwB};
    const Risultato b = comando({"controlla", p.chiuso.string(), "--utente", "B", "--includi-dati-personali"});
    std::cout << "--- A ---\n" << a.uscita << "--- B ---\n" << b.uscita;
    EXPECT_NE(a.uscita.find("K-CTR-177"), std::string::npos) << a.uscita;
    EXPECT_NE(a.uscita.find("Scheda SCH-4000603-1 (versione 1): a bordo ci sono 1 disabile su 8 persone"), std::string::npos) << a.uscita;
    EXPECT_EQ(a.uscita.find("K-CTR-178"), std::string::npos) << a.uscita;
    EXPECT_EQ(riga_con(b.uscita, "K-CTR-177:"), std::string()) << b.uscita;
    EXPECT_NE(riga_con(b.uscita, "Non verificato (servono i dati particolari").find("K-CTR-177"), std::string::npos) << b.uscita;
    for (const char* x : {"RT-1", "disabile su"}) EXPECT_EQ(b.uscita.find(x), std::string::npos) << x;
}

// --- K-025: rilettura delle schede -----------------------------------------------------------------------------------------------

// K-025, punto 1: lo stesso file riletto un mese dopo, senza --validita: quattro uguali, nessun problema, la riga nuova del riepilogo; JSON
// alla versione 4 con scelta_scheda_non_usata (falso). Con --validita-scheda per una scheda uguale: la riga della scheda lo dice, e nel
// JSON il campo e' vero solo per lei (D1 A). Sabotaggi: avvisi ripetuti sulle uguali; la riga del riepilogo che manca.
TEST(SchedeComandi, RiletturaSenzaRumore) {
    Prova p(pacchetto_k024());
    p.file("k025.xlsx", fogli_k025());
    const Risultato prima = p.leggi("k025.xlsx", {"--validita", "attivazione"});
    EXPECT_NE(prima.uscita.find("Avviso K-CTR-163: Scheda 4000703/1"), std::string::npos) << prima.uscita;
    EXPECT_NE(prima.uscita.find("Avviso K-CTR-176: Scheda 4000702/1"), std::string::npos) << prima.uscita;
    EXPECT_EQ(prima.uscita.find("Schede uguali all'ultima versione"), std::string::npos) << prima.uscita;
    const std::string registro = p.normale("schede_servizi.json");
    const Risultato dopo = p.leggi("k025.xlsx", {}, true, "A", kPw, "2026-12-16");
    std::cout << dopo.uscita;
    EXPECT_NE(dopo.uscita.find("Schede: 0 nuove, 0 versioni nuove, 4 uguali (niente da fare), 0 ferme, 0 senza la scelta della data.\n"
                               "Schede uguali all'ultima versione: 4. Niente da fare, e i loro avvisi non si ripetono a ogni lettura: quelli "
                               "che descrivono il pacchetto (Tot Giorni, date elencate, coincidenze, numero dei disabili) li da' kzero "
                               "controlla.\n"),
              std::string::npos)
        << dopo.uscita;
    EXPECT_NE(dopo.uscita.find("4000701/1 (riga 2): uguale alla versione 1 della scheda SCH-4000701-1: niente da fare.\n"), std::string::npos)
        << dopo.uscita;
    for (const char* x : {"Avviso K-CTR", "Errore K-CTR", "problema"}) EXPECT_EQ(dopo.uscita.find(x), std::string::npos) << x;
    EXPECT_EQ(p.normale("schede_servizi.json"), registro);
    const Risultato j = p.leggi("k025.xlsx", {"--validita-scheda", "4000703 / 1=2027-01-11", "--formato", "json"}, false, "A", kPw, "2026-12-16");
    const JsonDati d = JsonDati::parse(j.uscita);
    EXPECT_EQ(d.at("schema_versione"), 4);
    EXPECT_EQ(d.at("conteggi").at("uguali"), 4);
    EXPECT_TRUE(d.at("problemi").empty());
    EXPECT_FALSE(d.contains("uguali"));   // D7: niente oggetto con valori fissi
    for (const JsonDati& s : d.at("schede")) EXPECT_EQ(s.at("scelta_scheda_non_usata"), s.at("nr") == "4000703/1") << s.at("nr");
    const Risultato t = p.leggi("k025.xlsx", {"--validita-scheda", "4000703/1=2027-01-11"}, false, "A", kPw, "2026-12-16");
    EXPECT_NE(t.uscita.find("4000703/1 (riga 4): uguale alla versione 1 della scheda SCH-4000703-1: niente da fare (la scelta "
                            "--validita-scheda non serve: la scheda non e' cambiata).\n"),
              std::string::npos)
        << t.uscita;
}

// K-025, D1-bis A: --validita-scheda con un Nr che non e' nel file: K-ERR-056, niente letto e niente scritto, prima della password (il
// comando non ha nessuna password da dare). Una voce giusta e una sbagliata nello stesso comando: il messaggio dice solo quella sbagliata.
// Con le voci giuste il comando va. Sabotaggio: il Nr sbagliato perso in silenzio.
TEST(SchedeComandi, SceltaPerUnaSchedaCheNonCe) {
    Prova p(pacchetto_k024());
    p.file("k025.xlsx", fogli_k025());
    const std::string servizi = p.normale("servizi.json");
    p.amb.password = {};
    std::string messaggio;
    try {
        comando({"leggi-schede", (p.lavoro / "k025.xlsx").string(), p.chiuso.string(), "--profilo", "PS-CVA", "--contratto", "CT-SCU", "--data",
                 "2026-11-16", "--riferimento", "x", "--utente", "A", "--validita", "attivazione", "--validita-scheda",
                 "4000703/1=2026-11-23;4000730/1=2026-11-23", "--scrivi", p.chiuso.string()});
    } catch (const ErroreKZero& e) {
        EXPECT_EQ(e.definizione().codice, "K-ERR-056");
        messaggio = e.what();
    }
    EXPECT_EQ(messaggio, "--validita-scheda nomina la scheda 4000730/1, che non e' nel foglio \"Schede\" del file: forse il Nr e' scritto "
                         "male. Niente e' stato letto.");
    EXPECT_EQ(p.normale("servizi.json"), servizi);
    EXPECT_EQ(errore_di([&] { p.leggi("k025.xlsx", {"--validita-scheda", "4000731/1=attivazione;4000730/1=attivazione"}); }), "K-ERR-056");
    EXPECT_EQ(p.normale("servizi.json"), servizi);
    EXPECT_EQ(errore_di([&] { p.leggi("k025.xlsx", {"--validita", "attivazione", "--validita-scheda", "4000703 / 1=2026-11-23"}); }), "nessuno");
}

// K-025, D3 A: gli avvisi sul registro (K-CTR-169 sovrapposizione, K-CTR-171 accompagnatore) solo per le schede scritte nella lettura, non
// per le uguali; una sovrapposizione con una scheda scritta si vede ancora. Tutti restano in kzero controlla. Sabotaggio: 169/171 ripetuti
// sulle uguali.
TEST(SchedeComandi, AvvisiDelRegistroSoloPerLeSchedeScritte) {
    Prova p;
    Fogli f = fogli_di_prova();
    f.schede.resize(2);   // 4000123/3
    f.fermate.resize(4);
    // Una seconda scheda dello stesso servizio (4000123), con un altro ID, negli stessi giorni: K-CTR-169.
    std::vector<std::string> altra = f.schede[1];
    altra[0] = "4000123 / 4";
    altra[1] = "5c1e2f40-0a1b-4c2d-8e3f-000000000004";
    f.schede.push_back(altra);
    for (std::size_t i = 1; i < 4; ++i) {
        std::vector<std::string> r = f.fermate[i];
        r[0] = "4000123 / 4";
        f.fermate.push_back(r);
    }
    // Una scheda dell'infanzia con i bambini a bordo prima dell'accompagnatore: K-CTR-171 (variante 1, dipende dal mezzo).
    const std::vector<std::vector<std::string>> inf = {
        {"4000200 / 1", "5c1e2f40-0a1b-4c2d-8e3f-000000000200", "Andata Mattino", "Scuola infanzia Lagobello", "14/09/2026", "30/06/2027",
         "X", "X", "X", "X", "X", "", "", "#6", "", "", "#0", "9", "2", "MATERNA", "Sempre", "Scheda nuova"}};
    f.schede.push_back(inf[0]);
    f.fermate.push_back({"4000200/1", "07:40", "Lagobello, Lago", "", "#2", "", "3", "2", ""});
    f.fermate.push_back({"4000200/1", "07:50", "PRADELLO PIAZZA", "", "#4", "", "", "", "SALE"});
    f.fermate.push_back({"4000200/1", "08:05", "Scuola infanzia Lagobello", "", "", "#6", "6", "", "SCENDE"});
    p.file("f.xlsx", f);
    const Risultato prima = p.leggi("f.xlsx", {"--validita", "attivazione"});
    EXPECT_NE(prima.uscita.find("Avviso K-CTR-169"), std::string::npos) << prima.uscita;
    EXPECT_NE(prima.uscita.find("Avviso K-CTR-171"), std::string::npos) << prima.uscita;
    const Risultato uguale = p.leggi("f.xlsx", {"--validita", "attivazione"}, false);
    EXPECT_NE(uguale.uscita.find("3 uguali"), std::string::npos) << uguale.uscita;
    for (const char* x : {"K-CTR-169", "K-CTR-171"}) EXPECT_EQ(uguale.uscita.find(x), std::string::npos) << x << "\n" << uguale.uscita;
    // La 4000123/4 cambiata (un orario): versione nuova, e la sovrapposizione con la 4000123/3 (uguale) si vede ancora.
    Fogli g = f;
    for (std::vector<std::string>& r : g.fermate)
        if (r[0] == "4000123 / 4" && r[1] == "07:20") r[1] = "07:21";
    p.file("g.xlsx", g);
    const Risultato cambiata = p.leggi("g.xlsx", {"--validita-scheda", "4000123/4=2026-11-23"}, false);
    EXPECT_NE(cambiata.uscita.find("versione nuova 2 della scheda SCH-4000123-4"), std::string::npos) << cambiata.uscita;
    EXPECT_NE(cambiata.uscita.find("Avviso K-CTR-169"), std::string::npos) << cambiata.uscita;
    EXPECT_EQ(cambiata.uscita.find("K-CTR-171"), std::string::npos) << cambiata.uscita;
    p.amb.password = {kPw};
    const Risultato c = comando({"controlla", p.chiuso.string(), "--utente", "A", "--includi-dati-particolari"});
    for (const char* x : {"K-CTR-169", "K-CTR-171"}) EXPECT_NE(c.uscita.find(x), std::string::npos) << x;
}
