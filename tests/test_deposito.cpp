// K-Zero - prova del controllo del deposito pubblico (K-015, tests/strumenti/deposito.hpp) con una lista finta: nomi
// inventati, scritti qui in chiaro, al posto dei quattro nomi veri (di cui il controllo conosce solo le impronte).
// Un caso per ogni regola: parola intera, maiuscole e minuscole, nome di due parole anche a capo e nei commenti, percorsi,
// file binari, formati dei dati e manifesti fuori da tests/ ed esempi/, cartelle escluse, testo dei messaggi.
#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sodium.h>

#include "strumenti/deposito.hpp"

namespace fs = std::filesystem;
using namespace kzero::deposito;

namespace {

Nome finto(char etichetta, const std::string& in_chiaro, int parole) {
    return {etichetta, "nome inventato", "altro", impronta(in_chiaro), parole};
}

// La lista finta: una parola lunga, una sigla di tre lettere, un nome di due parole.
const std::vector<Nome>& finti() {
    static const std::vector<Nome> n = {finto('x', "zorbalino", 1), finto('y', "vrk", 1),
                                        finto('z', "vallerossa trasportini", 2)};
    return n;
}

std::vector<std::size_t> righe(const std::vector<Problema>& p) {
    std::vector<std::size_t> r;
    for (const Problema& q : p) r.push_back(q.riga);
    return r;
}

std::string minuscolo(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Nessun messaggio dice il nome: solo l'etichetta.
void senza_nomi(const std::vector<Problema>& p) {
    for (const Problema& q : p) {
        const std::string m = minuscolo(q.dove + " " + q.cosa);
        EXPECT_EQ(m.find("zorbalino"), std::string::npos) << q.cosa;
        EXPECT_EQ(m.find("vallerossa"), std::string::npos) << q.cosa;
        EXPECT_EQ(m.find("trasportini"), std::string::npos) << q.cosa;
        EXPECT_EQ(q.cosa.find("vrk"), std::string::npos) << q.cosa;
    }
}

class Cartella {
public:
    Cartella() {
        unsigned char c[8];
        randombytes_buf(c, sizeof c);
        char esa[17];
        sodium_bin2hex(esa, sizeof esa, c, sizeof c);
        radice_ = fs::temp_directory_path() / (std::string("kzero-deposito-") + esa);
        fs::create_directories(radice_);
    }
    ~Cartella() {
        std::error_code e;
        fs::remove_all(radice_, e);
    }
    void scrivi(const std::string& relativo, const std::string& testo) const {
        const fs::path p = radice_ / relativo;
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary);
        out << testo;
    }
    const fs::path& radice() const { return radice_; }

private:
    fs::path radice_;
};

std::vector<std::string> dove_e_riga(const Resoconto& r) {
    std::vector<std::string> v;
    for (const Problema& q : r.problemi) v.push_back(q.dove + ":" + std::to_string(q.riga));
    return v;
}

}  // namespace

TEST(Deposito, ImprontaSha256) {
    // Vettore di prova di FIPS 180-2 per "abc".
    const Impronta i = impronta("abc");
    char esa[65];
    sodium_bin2hex(esa, sizeof esa, i.data(), i.size());
    EXPECT_STREQ(esa, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Deposito, NomiDelDepositoSoloImpronte) {
    const std::vector<Nome>& n = nomi_del_deposito();
    ASSERT_EQ(n.size(), 5u);
    std::string etichette;
    for (const Nome& x : n) etichette += x.etichetta;
    EXPECT_EQ(etichette, "abcdd");
    // I sostituti e le parole che restano non sono nomi vietati; "Trentino" da solo resta, come i luoghi.
    const std::string permesso =
        "ALP, il supervisore, il consorzio, sistema \"Consorzio\", l'azienda del trasporto pubblico provinciale, sistema "
        "\"Trasporti Provinciali\", Trentino, Provincia, K-CTR-001, GSH, C:\\Users\\BULLFROG\\Desktop";
    EXPECT_TRUE(cerca_nomi(permesso, "testo", n).empty());
    for (const Nome& f : finti())
        for (const Nome& x : n) EXPECT_NE(f.impronta, x.impronta);
}

TEST(Deposito, ParolaInteraMaiuscoleEMinuscole) {
    const std::string t =
        "zorbalino\n"                         // 1
        "ZORBALINO in un commento\n"          // 2
        "// Zorbalino ha deciso\n"            // 3
        "U-ZORBALINO e (zorbalino)\n"         // 4: due volte
        "prova_zorbalino_2\n"                 // 5
        "\xe2\x80\x9czorbalino\xe2\x80\x9d\n"  // 6: tra virgolette tipografiche
        "zorbalinos azorbalino zorbalino2\n"  // 7: parole diverse
        "zorbalin\xc3\xb2 \xc3\xa8zorbalino\n"  // 8: con lettere accentate attaccate, parole diverse
        "il file del VRK, vrk-001\n"          // 9: due volte
        "vrkx avrk K-CTR-001\n";              // 10: niente
    const std::vector<Problema> p = cerca_nomi(t, "prova.md", finti());
    EXPECT_EQ(righe(p), (std::vector<std::size_t>{1, 2, 3, 4, 4, 5, 6, 9, 9}));
    for (const Problema& q : p) EXPECT_EQ(q.dove, "prova.md");
    EXPECT_NE(p.at(0).cosa.find("nome x"), std::string::npos);
    EXPECT_NE(p.at(7).cosa.find("nome y"), std::string::npos);
    senza_nomi(p);
}

TEST(Deposito, NomeDiDueParoleAncheACapo) {
    const std::string t =
        "vallerossa trasportini\n"                    // 1
        "VALLEROSSA   TRASPORTINI\n"                  // 2
        "le corse di Vallerossa\n"                    // 3 (a capo)
        "Trasportini, linea 3\n"                      // 4
        "// codice di Vallerossa\n"                   // 5 (a capo in un commento C++)
        "// Trasportini nella prova\n"                // 6
        "# Vallerossa\r\n#   Trasportini\n"           // 7 (CMake, Python; a capo Windows)
        "vallerossa-trasportini \"Vallerossa\" \"Trasportini\"\n"  // 9: due volte
        "Vallerossa\xc2\xa0Trasportini\n"             // 10: spazio unificatore
        "vallerossa. trasportini\n"                   // 11: niente (frasi diverse)
        "vallerossa, trasportini; vallerossa e trasportini\n"  // 12: niente
        "solo vallerossa, poi solo trasportini\n";    // 13: niente
    const std::vector<Problema> p = cerca_nomi(t, "prova.cpp", finti());
    EXPECT_EQ(righe(p), (std::vector<std::size_t>{1, 2, 3, 5, 7, 9, 9, 10}));
    for (const Problema& q : p) EXPECT_NE(q.cosa.find("nome z"), std::string::npos);
    senza_nomi(p);
}

TEST(Deposito, CartellaFileFormatiManifestiEsclusioni) {
    Cartella c;
    c.scrivi("README.md", "Nessun nome qui.\n");
    c.scrivi("docs/note.md", "prima riga\nseconda\nscelta di Zorbalino\n");
    c.scrivi("src/codice.cpp", "int x;  // VALLEROSSA\n// TRASPORTINI\n");
    c.scrivi("docs/zorbalino/vuoto.txt", "");                       // nel percorso
    c.scrivi("docs/dati.xlsx", "");                                 // formato dei dati
    c.scrivi("docs/ELENCO.PDF", "");                                // maiuscole
    c.scrivi("docs/pacchetto.json", "{}");                          // manifesto
    c.scrivi("Pacchetto.json", "{}");
    c.scrivi("cassaforte.kzc", std::string("\0\xff\xfe", 3));       // binario e formato dei dati
    c.scrivi("tests/esempi/p/pacchetto.json", "{}");                // nelle prove: ammessi
    c.scrivi("tests/esempi/p/cassaforte.kzc", std::string("\0\xff", 2));
    c.scrivi("tests/dati.csv", "a;b\n");
    c.scrivi("esempi/demo/pacchetto.json", "{}");
    c.scrivi("esempi/foglio.xlsx", "");
    c.scrivi("docs/binario.dat", std::string("\0\xff\xfe zorbalino", 14));       // binario: non letto
    c.scrivi("docs/argomenti", std::string("controlla\0zorbalino\0", 20));       // byte zero ma UTF-8: letto
    c.scrivi("docs/latino.txt", "caff\xe8 zorbalino\n");                         // non UTF-8, senza byte zero: letto
    c.scrivi("build/note.md", "zorbalino\n");                        // escluse: build*/ in cima, .git, --escludi
    c.scrivi("build-san/dati.xlsx", "");
    c.scrivi(".git/config", "zorbalino\n");
    c.scrivi("costruzione/x.md", "zorbalino\n");
    c.scrivi("docs/build/note.md", "zorbalino\n");                  // non in cima: controllata
    c.scrivi("docs/elenco.csv.md", "va bene\n");
    std::error_code e;
    fs::create_symlink(c.radice() / "docs" / "note.md", c.radice() / "docs" / "collegamento.pdf", e);
    const bool collegamento = !e;

    const Resoconto r = controlla_cartella(c.radice(), {c.radice() / "costruzione"}, finti());
    std::vector<std::string> attesi = {"Pacchetto.json:0", "cassaforte.kzc:0", "docs/ELENCO.PDF:0",
                                       "docs/argomenti:1",  "docs/build/note.md:1",
                                       "docs/dati.xlsx:0",  "docs/latino.txt:1", "docs/note.md:3",
                                       "docs/pacchetto.json:0", "docs/zorbalino/vuoto.txt:0", "src/codice.cpp:1"};
    if (collegamento) attesi.insert(attesi.begin() + 5, "docs/collegamento.pdf:0");
    EXPECT_EQ(dove_e_riga(r), attesi);
    EXPECT_EQ(r.binari, (std::vector<std::string>{"cassaforte.kzc", "docs/binario.dat", "tests/esempi/p/cassaforte.kzc"}));
    if (collegamento) { EXPECT_EQ(r.collegamenti, (std::vector<std::string>{"docs/collegamento.pdf"})); }
    for (const Problema& q : r.problemi) {
        std::string atteso;
        if (q.dove == "docs/dati.xlsx") atteso = "formato di dati (.xlsx)";
        if (q.dove == "docs/ELENCO.PDF") atteso = "(.pdf)";
        if (q.dove == "docs/pacchetto.json") atteso = "manifesto di un pacchetto";
        if (q.dove == "docs/zorbalino/vuoto.txt") atteso = "nel percorso c'e' il nome x";
        if (q.dove == "src/codice.cpp") atteso = "c'e' il nome z";
        EXPECT_NE(q.cosa.find(atteso), std::string::npos) << q.dove << ": " << q.cosa;
    }
    // Il messaggio non ripete il nome, tranne nel percorso del file, che e' il dato da correggere.
    std::vector<Problema> senza_percorso;
    for (const Problema& q : r.problemi)
        if (q.dove != "docs/zorbalino/vuoto.txt") senza_percorso.push_back(q);
    senza_nomi(senza_percorso);
}

TEST(Deposito, ProgrammaCodiciDiUscita) {
    std::istringstream vuoto;
    std::ostringstream out;
    EXPECT_EQ(esegui({}, vuoto, finti(), out), 2);
    EXPECT_EQ(esegui({"--cartella"}, vuoto, finti(), out), 2);
    EXPECT_EQ(esegui({"--altro", "x"}, vuoto, finti(), out), 2);
    EXPECT_EQ(esegui({"--cartella", "x", "--testo", "y"}, vuoto, finti(), out), 2);
    EXPECT_EQ(esegui({"--cartella", "/questa/cartella/non/esiste"}, vuoto, finti(), out), 3);

    Cartella c;
    c.scrivi("docs/note.md", "niente\n");
    std::ostringstream pulita;
    EXPECT_EQ(esegui({"--cartella", c.radice().string()}, vuoto, finti(), pulita), 0);
    EXPECT_NE(pulita.str().find("Nessun nome e nessun file di dati."), std::string::npos);
    c.scrivi("docs/altro.md", "riga\nZORBALINO\n");
    std::ostringstream sporca;
    EXPECT_EQ(esegui({"--cartella", c.radice().string()}, vuoto, finti(), sporca), 1);
    EXPECT_NE(sporca.str().find("docs/altro.md:2: c'e' il nome x (nome inventato)"), std::string::npos);
    EXPECT_EQ(minuscolo(sporca.str()).find("zorbalino"), std::string::npos);
    std::ostringstream esclusa;
    EXPECT_EQ(esegui({"--cartella", c.radice().string(), "--escludi", (c.radice() / "docs").string()}, vuoto, finti(),
                     esclusa),
              0);
}

TEST(Deposito, TestoDeiMessaggiEDeiTitoli) {
    std::istringstream titolo("K-999: proposta per Vallerossa\nTrasportini\n");
    std::ostringstream out;
    EXPECT_EQ(esegui({"--testo", "titolo della proposta"}, titolo, finti(), out), 1);
    EXPECT_NE(out.str().find("titolo della proposta:1: c'e' il nome z"), std::string::npos);
    EXPECT_EQ(minuscolo(out.str()).find("vallerossa"), std::string::npos);
    std::istringstream messaggio("K-999: testi\n\nAl posto del nome si scrive il sostituto.\n");
    std::ostringstream pulito;
    EXPECT_EQ(esegui({"--testo", "commit 1234567"}, messaggio, finti(), pulito), 0);
    EXPECT_NE(pulito.str().find("commit 1234567: nessun nome"), std::string::npos);
}
