// K-Zero - prova sistematica sulle uscite dei comandi dell'elenco utenti (K-021, punto 0): leggi-elenco, sblocco e i cinque comandi a
// mano (aggiungi-in-elenco, cambia-in-elenco, togli-da-elenco, rimetti-in-elenco, cancella-da-elenco), in testo e in JSON.
//
// Ogni valore personale degli utenti di prova (nome, ogni parte della casa, sezione, nota dello stato, testo della fermata, e i loro
// valori di prima dopo una modifica a mano) contiene "zqn" se l'utente non e' legato a un luogo sensibile, "zql" se lo e' (per la
// scuola o la fermata, di oggi o di prima); la classe (un numero) si guarda campo per campo. Regole:
// - senza --includi-dati-personali nessun valore compare ("zq" mai, classe mai);
// - con il solo --includi-dati-personali nessun valore di un utente legato ("zql" mai);
// - con tutti e due i parametri i valori si vedono (prova che la prova li troverebbe).
// Dati inventati (il demo con i profili e i codici esterni di tests/esempi/k019-elenco).
#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "cassaforte_prove.hpp"
#include "elenco_a_mano_prove.hpp"
#include "errori.hpp"
#include "xlsx_prove.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove;
using namespace kzero::prove::elenco_a_mano;
namespace fs = std::filesystem;

namespace {

const std::string kPw = "cavallo pesca lampione verde";

const std::vector<std::string> kTitoli = {"SCUOLA", "FERMATA", "COMUNE", "FRAZIONE", "VIA", "CIVICO", "UTENTE", "CLASSE",
                                          "SEZ.", "STATO", "PERCORSI", "ORARI MATTINA", "ORARI POMERIGGIO"};
const std::string kCentro = "Centro diurno Il Faro";   // la scuola nel centro diurno (luogo sensibile L-CENTRO)
const std::string kFermataCentro = "CENTRO FARO";      // codice esterno (fermata) del centro diurno, aggiunto dalla prova

// Una riga del file: tutti i valori personali con il segno s ("zqn" o "zql") e il numero n.
std::vector<std::string> riga(const std::string& scuola, const std::string& fermata, const std::string& s, const std::string& n,
                              const std::string& classe) {
    return {scuola, fermata, s + "comune" + n, s + "frazione" + n, "via " + s + "via" + n, n + "/" + s, s + " nome " + n, "#" + classe,
            s + "sez" + n, "SOSPESO - " + s + "nota" + n, "AM/ALFA RM/ALFA", "AM: 08:00 LU-MA-ME-GI-VE", ""};
}
// I campi a mano: tutti i valori personali nuovi con il segno s e il numero n.
std::vector<std::string> campi(const std::string& s, const std::string& n, const std::string& classe) {
    return {"nome: " + s + " nome " + n, "comune: " + s + "comune" + n, "frazione: " + s + "frazione" + n, "via: via " + s + "via" + n,
            "civico: " + n + "/" + s, "classe: " + classe, "sezione: " + s + "sez" + n, "stato: SOSPESO - " + s + "nota" + n,
            "fermata: " + s + " fermata " + n};
}

std::string minuscole(std::string t) {
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return t;
}

struct Modo {
    bool personali = false, particolari = false;
    bool json = false;
    std::string nome() const {
        return std::string(json ? "json" : "testo") + (personali ? " +personali" : "") + (particolari ? " +particolari" : "");
    }
};
const std::vector<Modo> kModi = {{false, false, false}, {false, false, true}, {true, false, false},
                                 {true, false, true},   {true, true, false},  {true, true, true}};

struct Prova {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    fs::path chiuso;
    std::string valori;
    std::size_t letti = 0;
    std::map<std::string, std::string> codici;   // " nome 1" ... -> U-000001 ...
    std::size_t uscite = 0, viste_n = 0, viste_l = 0;

    Prova() {
        PacchettoInMemoria m = pacchetto_di_prova();
        JsonDati luoghi = JsonDati::parse(m.file.at("luoghi.json"));
        for (JsonDati& l : luoghi.at("luoghi"))
            if (l.at("codice") == "L-CENTRO")
                l["codici_esterni"] = JsonDati::array({{{"sistema", "Consorzio Valle Alta"}, {"tipo", "fermata"}, {"codice", kFermataCentro}}});
        m.file["luoghi.json"] = luoghi.dump(2);
        fs::create_directories(lavoro / "chiaro");
        for (const auto& [f, t] : m.file) std::ofstream(lavoro / "chiaro" / f, std::ios::binary) << t;
        amb.password = {kPw};
        comando({"chiudi", (lavoro / "chiaro").string(), "--scrivi", (lavoro / "chiuso").string(), "--utente", "A", "--conservazione", "2099-12-31"});
        chiuso = lavoro / "chiuso";
        const_cast<accesso::Ambiente&>(amb.ambiente()).valori = [this]() -> int {
            return letti < valori.size() ? static_cast<unsigned char>(valori[letti++]) : -1;
        };
    }

    Risultato esegui(std::vector<std::string> a, const Modo& m, const std::vector<std::string>& righe = {}) {
        amb.password = {kPw};
        valori.clear();
        for (const std::string& r : righe) valori += r + "\n";
        letti = 0;
        if (m.json) a.insert(a.end(), {"--formato", "json"});
        if (m.personali) a.push_back("--includi-dati-personali");
        if (m.particolari) a.push_back("--includi-dati-particolari");
        return comando(a);
    }

    // Controlla un'uscita. legato: l'utente di un comando a mano e' legato (per la classe, che non ha segno).
    void controlla(const Risultato& r, const Modo& m, const std::string& cosa, bool legato_a_mano = false) {
        ++uscite;
        const std::string t = minuscole(r.uscita);
        const std::string dove = cosa + " (" + m.nome() + ")";
        if (!m.personali) {
            EXPECT_EQ(t.find("zq"), std::string::npos) << dove << "\n" << r.uscita;
        }
        if (!m.particolari) {
            EXPECT_EQ(t.find("zql"), std::string::npos) << dove << "\n" << r.uscita;
        }
        if (m.personali && t.find("zqn") != std::string::npos) ++viste_n;
        if (m.personali && m.particolari && t.find("zql") != std::string::npos) ++viste_l;
        // La classe: un numero, senza segno; si guarda nel campo "classe" delle uscite dei comandi a mano.
        const bool classe_nascosta = !m.personali || (legato_a_mano && !m.particolari);
        if (!classe_nascosta) return;
        if (m.json) {
            const JsonDati j = JsonDati::parse(r.uscita);
            if (j.contains("campi") && j.at("campi").is_array())
                for (const JsonDati& c : j.at("campi"))
                    if (c.at("campo") == "classe") {
                        EXPECT_TRUE(c.at("prima").is_null()) << dove;
                        EXPECT_TRUE(c.at("dopo").is_null()) << dove;
                    }
        } else {
            std::size_t a = 0;
            for (std::size_t b; (b = r.uscita.find('\n', a)) != std::string::npos; a = b + 1) {
                const std::string l = r.uscita.substr(a, b - a);
                if (l.rfind("  classe:", 0) != 0) continue;
                EXPECT_TRUE(std::none_of(l.begin(), l.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) << dove << ": " << l;
            }
        }
    }

    // Un comando che scrive (--scrivi): le sei uscite in anteprima, poi la scrittura (anche la sua uscita si controlla).
    void in_tutti_i_modi(const std::vector<std::string>& a, const std::string& cosa, const std::vector<std::string>& righe = {},
                         bool legato_a_mano = false, bool anteprima = true) {
        if (anteprima)
            for (const Modo& m : kModi) controlla(esegui(a, m, righe), m, cosa, legato_a_mano);
        std::vector<std::string> s = a;
        s.insert(s.end(), {"--scrivi", chiuso.string()});
        controlla(esegui(s, Modo{}, righe), Modo{}, cosa + ", scritto", legato_a_mano);
    }

    std::vector<std::string> leggi(const std::string& file) {
        return {"leggi-elenco", (lavoro / file).string(), chiuso.string(), "--profilo", "PR-CVA", "--anno-scolastico", "2026-2027",
                "--elenco", "completo", "--utente", "A"};
    }
    std::vector<std::string> a_mano(const std::string& cmd, const std::string& utente) {
        std::vector<std::string> a = {cmd, chiuso.string(), "--utente", "A", "--riferimento", "mail del committente", "--data", "2026-11-12"};
        if (!utente.empty()) a.insert(a.end(), {"--utente-elenco", utente});
        return a;
    }
    void scrivi_file(const std::string& nome, const std::vector<std::vector<std::string>>& righe) {
        std::ofstream(lavoro / nome, std::ios::binary) << xlsx::file_righe(righe);
    }
};

}  // namespace

TEST(UsciteElenco, NessunDatoPersonaleSenzaIParametri) {
    Prova p;
    // Gli utenti: 1 non legato; 2 legato per la scuola; 3 legato per la fermata; 4 legato per la scuola di prima (cambiata a mano);
    // 5 legato per la fermata di prima (cambiata a mano).
    const std::vector<std::vector<std::string>> prima = {kTitoli,
                                                          riga(kPrim, "", "zqn", "1", "3"),
                                                          riga(kCentro, "", "zql", "2", "4"),
                                                          riga(kPrim, kFermataCentro, "zql", "3", "4"),
                                                          riga(kCentro, "", "zql", "4", "4"),
                                                          riga(kPrim, kFermataCentro, "zql", "5", "4")};
    p.scrivi_file("prima.xlsx", prima);
    p.in_tutti_i_modi(p.leggi("prima.xlsx"), "prima lettura");
    // I codici dati: dall'uscita JSON con tutti i parametri.
    {
        const Risultato r = p.esegui(p.leggi("prima.xlsx"), Modo{true, true, true});
        const JsonDati j = JsonDati::parse(r.uscita);
        for (const JsonDati& u : j.at("utenti")) {
            const std::string nome = u.at("nome").get<std::string>();
            p.codici[nome.substr(3)] = u.at("codice").get<std::string>();
        }
        ASSERT_EQ(p.codici.size(), 5u);
    }
    const auto u = [&](int n) { return p.codici.at(" nome " + std::to_string(n)); };

    // Sblocco (gli utenti sono sospesi): non ha i parametri dei dati personali, e la sua uscita non ne deve avere; aggiunto e tolto,
    // in testo e in JSON.
    for (const int n : {1, 2})
        for (const Modo& m : {Modo{false, false, false}, Modo{false, false, true}}) {
            std::vector<std::string> a = {"sblocco", p.chiuso.string(), "--utente-elenco", u(n), "--tratta", "AM", "--utente", "A", "--scrivi",
                                          p.chiuso.string()};
            std::vector<std::string> b = a;
            a.insert(a.end(), {"--riferimento", "mail del committente", "--data", "2026-11-10"});
            b.push_back("--togli");
            p.controlla(p.esegui(a, m), m, "sblocco aggiunto");
            p.controlla(p.esegui(b, m), m, "sblocco tolto");
        }

    // Le fermate di prima e le scuole di prima nel centro: 4 cambia scuola, 5 cambia fermata (da qui legati solo per il valore di prima).
    p.in_tutti_i_modi(p.a_mano("cambia-in-elenco", u(4)), "scuola di prima nel centro", {"scuola: " + kPrim}, true);
    p.in_tutti_i_modi(p.a_mano("cambia-in-elenco", u(5)), "fermata di prima nel centro", {"fermata: PRADELLO PIAZZA"}, true);
    // Tutti i campi personali, due volte (la seconda: il valore di prima e' gia' ricordato).
    for (const int n : {1, 2, 3, 4, 5}) {
        const std::string s = n == 1 ? "zqn" : "zql";
        p.in_tutti_i_modi(p.a_mano("cambia-in-elenco", u(n)), "cambio di tutti i campi dell'utente " + std::to_string(n),
                          campi(s, std::to_string(n) + "b", "7"), n != 1);
        p.in_tutti_i_modi(p.a_mano("cambia-in-elenco", u(n)), "secondo cambio dell'utente " + std::to_string(n),
                          campi(s, std::to_string(n) + "c", "8"), n != 1);
    }
    // Aggiunte: un utente non legato e uno legato (scuola nel centro).
    std::vector<std::string> aggiungi = p.a_mano("aggiungi-in-elenco", "");
    aggiungi.insert(aggiungi.end(), {"--profilo", "PR-CVA"});
    for (const auto& [s, scuola] : {std::make_pair(std::string("zqn"), kPrim), std::make_pair(std::string("zql"), kCentro)}) {
        std::vector<std::string> nuovo = campi(s, s == "zqn" ? "6" : "7", "2");
        nuovo.insert(nuovo.end(), {"scuola: " + scuola, "percorsi: AM/ALFA"});
        p.in_tutti_i_modi(aggiungi, "aggiunta di un utente (" + s + ")", nuovo, s == "zql");
    }
    // La seconda lettura: il file ha ancora i valori di prima (K-CTR-156) e gli aggiunti non ci sono.
    p.scrivi_file("seconda.xlsx", prima);
    for (const Modo& m : kModi) p.controlla(p.esegui(p.leggi("seconda.xlsx"), m), m, "seconda lettura");
    // Togliere, rimettere e cancellare un utente non legato e uno legato.
    for (const int n : {1, 2, 4, 5}) {
        const bool legato = n != 1;
        p.in_tutti_i_modi(p.a_mano("togli-da-elenco", u(n)), "togliere l'utente " + std::to_string(n), {}, legato);
        p.in_tutti_i_modi(p.a_mano("rimetti-in-elenco", u(n)), "rimettere l'utente " + std::to_string(n), {}, legato);
        p.in_tutti_i_modi(p.a_mano("cancella-da-elenco", u(n)), "cancellare l'utente " + std::to_string(n), {}, legato);
    }
    // La prova vede i valori quando li deve vedere (altrimenti non proverebbe niente).
    EXPECT_GT(p.viste_n, 20u);
    EXPECT_GT(p.viste_l, 20u);
    std::cout << "Uscite controllate: " << p.uscite << " (con i valori non legati visibili: " << p.viste_n << ", legati: " << p.viste_l << ")\n";
}
