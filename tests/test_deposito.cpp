// K-Zero - prova del controllo del deposito pubblico (K-015, tests/strumenti/deposito.hpp) con una lista finta: nomi
// inventati, scritti qui in chiaro, al posto dei quattro nomi veri (di cui il controllo conosce solo le impronte).
// Un caso per ogni regola: parola intera, maiuscole e minuscole, nome di due parole anche a capo e nei commenti, percorsi,
// file binari, formati dei dati e manifesti fuori da tests/ ed esempi/, cartelle saltate (le stesse di .gitignore, K-016)
// ed escluse, testo dei messaggi.
#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        "zorbalinos azorbalino zorbalinoa\n"  // 7: parole diverse
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

// K-016: lettere e cifre non stanno nella stessa parola, nei due versi; il resto della regola resta.
TEST(Deposito, LettereECifreSeparano) {
    const std::string t =
        "zorbalino2027\n"                     // 1
        "78zorbalino\n"                       // 2
        "// 78ZORBALINO2027 in un commento\n" // 3
        "vrk1 A1vrk\n"                        // 4: due volte
        "zorbalino2x\n"                       // 5
        "2\xcc\x81zorbalino\n"                // 6: un segno dopo una cifra resta con la cifra
        "zorbalino\xd9\xa3\n"                 // 7: cifra araba
        "x2zorbalinos zorbalino\xcc\x81 zorbalino_\n"  // 8: niente (parole diverse, poi "_" separa: una volta)
        "vallerossa trasportini2027\n"        // 9
        "2026vallerossa\n"                    // 10 (a capo)
        "Trasportini\n"                       // 11
        "vallerossa1 trasportini\n"           // 12: niente (tra le due parole c'e' la cifra)
        "K-CTR-001 R-TN 24:12 2026-11-10\n";  // 13: niente
    const std::vector<Problema> p = cerca_nomi(t, "prova.md", finti());
    EXPECT_EQ(righe(p), (std::vector<std::size_t>{1, 2, 3, 4, 4, 5, 6, 7, 8, 9, 10}));
    EXPECT_NE(p.at(3).cosa.find("nome y"), std::string::npos);
    EXPECT_NE(p.at(9).cosa.find("nome z"), std::string::npos);
    EXPECT_NE(p.at(10).cosa.find("nome z"), std::string::npos);
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
    c.scrivi("docs/argomenti", std::string("controlla\0zorbalino\0", 20));   // UTF-8 con byte zero: letto, ma in docs/
                                                                             // e' un problema (K-016 aggiunta 1)
    c.scrivi("docs/latino.txt", "caff\xe8 zorbalino\n");                         // non UTF-8, senza byte zero: letto
    // Saltate: build/ e build-*/ a ogni livello (come .gitignore), .git, --escludi.
    c.scrivi("build/note.md", "zorbalino\n");
    c.scrivi("build-san/dati.xlsx", "");
    c.scrivi("docs/build/note.md", "zorbalino\n");
    c.scrivi("tests/a/build-x/note.md", "zorbalino\n");
    c.scrivi("src/build-/note.md", "zorbalino\n");
    c.scrivi(".git/config", "zorbalino\n");
    c.scrivi("costruzione/x.md", "zorbalino\n");
    // Controllate: nomi che .gitignore non ignora, e un file (non una cartella) che si chiama build.
    c.scrivi("buildx/note.md", "zorbalino\n");
    c.scrivi("Build/note.md", "zorbalino\n");
    c.scrivi("docs/build.vecchio/note.md", "zorbalino\n");
    c.scrivi("docs/altro/build", "zorbalino\n");
    c.scrivi("docs/elenco.csv.md", "va bene\n");
    std::error_code e;
    fs::create_symlink(c.radice() / "docs" / "note.md", c.radice() / "docs" / "collegamento.pdf", e);
    const bool collegamento = !e;

    const Resoconto r = controlla_cartella(c.radice(), {c.radice() / "costruzione"}, finti());
    std::vector<std::string> attesi = {"Build/note.md:1", "Pacchetto.json:0", "buildx/note.md:1", "cassaforte.kzc:0",
                                       "cassaforte.kzc:0", "docs/ELENCO.PDF:0", "docs/altro/build:1",
                                       "docs/argomenti:0", "docs/argomenti:1", "docs/binario.dat:0", "docs/build.vecchio/note.md:1",
                                       "docs/dati.xlsx:0", "docs/latino.txt:0", "docs/note.md:3",
                                       "docs/pacchetto.json:0", "docs/zorbalino/vuoto.txt:0", "src/codice.cpp:1"};
    if (collegamento)
        attesi.insert(std::find(attesi.begin(), attesi.end(), "docs/dati.xlsx:0"), "docs/collegamento.pdf:0");
    EXPECT_EQ(dove_e_riga(r), attesi);
    // Binario ammesso solo la cassaforte dentro tests/; quella in cima e docs/binario.dat non sono testo UTF-8 (K-016).
    EXPECT_EQ(r.binari, (std::vector<std::string>{"tests/esempi/p/cassaforte.kzc"}));
    if (collegamento) { EXPECT_EQ(r.collegamenti, (std::vector<std::string>{"docs/collegamento.pdf"})); }
    for (const Problema& q : r.problemi) {
        std::string atteso;
        if (q.dove == "docs/dati.xlsx") atteso = "formato di dati (.xlsx)";
        if (q.dove == "docs/ELENCO.PDF") atteso = "(.pdf)";
        if (q.dove == "docs/pacchetto.json") atteso = "manifesto di un pacchetto";
        if (q.dove == "docs/zorbalino/vuoto.txt") atteso = "nel percorso c'e' il nome x";
        if (q.dove == "src/codice.cpp") atteso = "c'e' il nome z";
        if (q.dove == "docs/binario.dat" || q.dove == "docs/latino.txt")
            atteso = "non e' testo UTF-8 ne' un binario ammesso";
        if (q.dove == "docs/argomenti") atteso = q.riga == 0 ? "testo con un byte zero" : "c'e' il nome x";
        EXPECT_NE(q.cosa.find(atteso), std::string::npos) << q.dove << ": " << q.cosa;
    }
    // Il messaggio non ripete il nome, tranne nel percorso del file, che e' il dato da correggere.
    std::vector<Problema> senza_percorso;
    for (const Problema& q : r.problemi)
        if (q.dove != "docs/zorbalino/vuoto.txt") senza_percorso.push_back(q);
    senza_nomi(senza_percorso);
}

namespace {

// Un testo in UTF-16 o UTF-32, con il suo BOM; in UTF-16 un carattere oltre U+FFFF diventa una coppia di surrogati.
std::string codificato(const std::u32string& testo, std::size_t passo, bool grande) {
    std::string t;
    auto unita = [&](std::uint32_t u) {
        for (std::size_t j = 0; j < passo; ++j) {
            const std::size_t spostamento = 8 * (grande ? passo - 1 - j : j);
            t.push_back(static_cast<char>((u >> spostamento) & 0xFF));
        }
    };
    unita(0xFEFF);
    for (const char32_t c : testo) {
        const std::uint32_t u = c;
        if (passo == 2 && u > 0xFFFF) {
            unita(0xD800 + ((u - 0x10000) >> 10));
            unita(0xDC00 + ((u - 0x10000) & 0x3FF));
        } else {
            unita(u);
        }
    }
    return t;
}

}  // namespace

// K-016: ogni file e' testo UTF-8, tranne i binari ammessi (.kzc e .kzr in tests/ ed esempi/); un testo UTF-16 o UTF-32
// con il BOM e' un problema e se ne cercano anche i nomi; ogni altro file non UTF-8 e' un problema.
TEST(Deposito, TestiUtf8EBinariAmmessi) {
    Cartella c;
    c.scrivi("docs/utf16le.txt", codificato(U"zorbalino\r\nriga di vrk\r\n", 2, false));
    c.scrivi("docs/utf16be.md", codificato(U"prima\nVallerossa\nTrasportini\n", 2, true));
    c.scrivi("docs/utf32le.txt", codificato(U"nessun nome\n", 4, false));
    c.scrivi("docs/utf32be.txt", codificato(U"ZORBALINO\n", 4, true));
    // Un carattere oltre U+FFFF (U+1D400, una lettera) attaccato al nome: decodificata la coppia di surrogati, e' una
    // parola sola e il nome non c'e'; la cifra U+1D7CE invece separa.
    c.scrivi("docs/surrogati16.txt", codificato(U"zorbalino\U0001D400\n\U0001D7CEvrk\n", 2, false));
    c.scrivi("docs/surrogati32.txt", codificato(U"zorbalino\U0001D400\n\U0001D7CEvrk\n", 4, true));
    // UTF-16 rovinato: un surrogato senza coppia e un byte a meta' in fondo; si decodifica quello che si puo'.
    c.scrivi("docs/rotto16.txt", std::string("\xff\xfe" "z\0\x00\xd8" "v\0r\0k\0" "\x41", 13));
    c.scrivi("docs/immagine.png", std::string("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16));
    c.scrivi("docs/latino.txt", "caff\xe8 zorbalino\n");                    // Latin-1: problema, nomi non cercati
    c.scrivi("docs/finto.kzc", std::string("\0\xff", 2));                  // fuori da tests/ ed esempi/: due problemi
    c.scrivi("tests/esempi/p/cassaforte.kzc", std::string("\0\xff zorbalino", 12));   // ammessi: solo il percorso
    c.scrivi("tests/esempi/p/registro.kzr", std::string("\xff\0", 2));
    c.scrivi("esempi/q/cassaforte.kzc", "\xfe");
    c.scrivi("tests/esempi/p/testo.kzc", "zorbalino\n");          // UTF-8 valido: si legge come testo
    c.scrivi("tests/X.KZC", "\xff");                               // estensione in maiuscolo: non ammessa
    c.scrivi("tests/immagine.png", "\x89PNG\xff");                  // anche in tests/ solo .kzc e .kzr
    c.scrivi("testsx/a.kzr", "\xff");                              // cartella diversa: due problemi
    // UTF-8 con byte zero sotto tests/fuzz/: testo, va bene (fuori di li' e' un problema: prova ByteZero).
    c.scrivi("tests/fuzz/semi/parametri/argomenti", std::string("controlla\0esempi\0", 18));

    const Resoconto r = controlla_cartella(c.radice(), {}, finti());
    EXPECT_EQ(dove_e_riga(r), (std::vector<std::string>{
                                  "docs/finto.kzc:0", "docs/finto.kzc:0", "docs/immagine.png:0", "docs/latino.txt:0",
                                  "docs/rotto16.txt:0", "docs/rotto16.txt:1", "docs/surrogati16.txt:0",
                                  "docs/surrogati16.txt:2", "docs/surrogati32.txt:0", "docs/surrogati32.txt:2",
                                  "docs/utf16be.md:0", "docs/utf16be.md:2",
                                  "docs/utf16le.txt:0", "docs/utf16le.txt:1", "docs/utf16le.txt:2", "docs/utf32be.txt:0",
                                  "docs/utf32be.txt:1", "docs/utf32le.txt:0", "tests/X.KZC:0",
                                  "tests/esempi/p/testo.kzc:1", "tests/immagine.png:0", "testsx/a.kzr:0",
                                  "testsx/a.kzr:0"}));
    EXPECT_EQ(r.binari, (std::vector<std::string>{"esempi/q/cassaforte.kzc", "tests/esempi/p/cassaforte.kzc",
                                                  "tests/esempi/p/registro.kzr"}));
    for (const Problema& q : r.problemi) {
        if (q.riga > 0) {
            EXPECT_EQ(q.cosa.rfind("c'e' il nome ", 0), 0u) << q.dove << ": " << q.cosa;
            continue;
        }
        if (q.cosa.find("file di un formato di dati") != std::string::npos) {   // fuori da tests/ ed esempi/
            EXPECT_TRUE(q.dove == "docs/finto.kzc" || q.dove == "testsx/a.kzr") << q.dove;
            continue;
        }
        EXPECT_NE(q.cosa.find("non e' testo UTF-8 ne' un binario ammesso (binari ammessi: .kzc in tests/ o esempi/, "
                              ".kzr in tests/ o esempi/)"),
                  std::string::npos)
            << q.dove << ": " << q.cosa;
        const bool bom = q.dove.find("utf") != std::string::npos || q.dove.find("16") != std::string::npos ||
                         q.dove.find("32") != std::string::npos;
        EXPECT_EQ(q.cosa.find(" con BOM: ") != std::string::npos, bom) << q.dove << ": " << q.cosa;
    }
    auto cosa = [&r](const std::string& dove, std::size_t riga) {
        for (const Problema& q : r.problemi)
            if (q.dove == dove && q.riga == riga) return q.cosa;
        return std::string();
    };
    EXPECT_NE(cosa("docs/utf16le.txt", 0).find("testo UTF-16 little endian con BOM"), std::string::npos);
    EXPECT_NE(cosa("docs/utf16be.md", 0).find("testo UTF-16 big endian con BOM"), std::string::npos);
    EXPECT_NE(cosa("docs/utf32le.txt", 0).find("testo UTF-32 little endian con BOM"), std::string::npos);
    EXPECT_NE(cosa("docs/utf32be.txt", 0).find("testo UTF-32 big endian con BOM"), std::string::npos);
    EXPECT_NE(cosa("docs/utf16le.txt", 1).find("nome x"), std::string::npos);
    EXPECT_NE(cosa("docs/utf16le.txt", 2).find("nome y"), std::string::npos);
    EXPECT_NE(cosa("docs/utf16be.md", 2).find("nome z"), std::string::npos);
    EXPECT_NE(cosa("docs/utf32be.txt", 1).find("nome x"), std::string::npos);
    EXPECT_NE(cosa("docs/rotto16.txt", 1).find("nome y"), std::string::npos);
    EXPECT_NE(cosa("docs/surrogati16.txt", 2).find("nome y"), std::string::npos);
    EXPECT_NE(cosa("docs/surrogati32.txt", 2).find("nome y"), std::string::npos);
    EXPECT_EQ(r.letti, 9u);   // i 7 testi con BOM, testo.kzc e argomenti di tests/fuzz/
    senza_nomi(r.problemi);
}

// K-016 aggiunta 1: un byte zero solo nei testi sotto tests/fuzz/; in ogni testo con byte zero i nomi si cercano anche
// senza i byte zero (UTF-16 e UTF-32 senza BOM), con le righe del file e senza doppioni.
TEST(Deposito, ByteZero) {
    Cartella c;
    auto senza_bom = [](const std::u32string& t, std::size_t passo, bool grande) {
        return codificato(t, passo, grande).substr(passo);
    };
    c.scrivi("docs/utf16le-senza-bom.txt", senza_bom(U"Scelta di zorbalino\n", 2, false));
    c.scrivi("docs/utf16be-senza-bom.md", senza_bom(U"prima\nVallerossa\nTrasportini\n", 2, true));
    c.scrivi("docs/utf32le-senza-bom.txt", senza_bom(U"riga\nil file del VRK\n", 4, false));
    c.scrivi("docs/zero.md", std::string("testo\0senza nome\n", 17));                  // problema, nessun nome
    c.scrivi("tests/esempi/p/argomenti", std::string("controlla\0demo\n", 15));       // tests/ ma non tests/fuzz/
    c.scrivi("tests/fuzzx/argomenti", std::string("a\0b\n", 4));                      // l'inizio del percorso conta
    c.scrivi("docs/tests/fuzz/argomenti", std::string("a\0b\n", 4));                  // non in cima: non ammessa
    c.scrivi("tests/fuzz/semi/x/utf16le", senza_bom(U"nota\nScelta di zorbalino\n", 2, false));   // solo il nome
    c.scrivi("tests/fuzz/semi/x/utf16be", senza_bom(U"ZORBALINO\n", 2, true));
    c.scrivi("tests/fuzz/casi/a/attaccato", std::string("\0zorbalino\0\n", 12));     // nei due modi: una volta
    // Riga 1: due volte nel testo com'e', nessuna senza zeri (le parole si attaccano): due. Riga 2: solo senza zeri.
    c.scrivi("tests/fuzz/casi/a/due", std::string("zorbalino\0zorbalino\nzor\0bal\0ino\n", 32));
    c.scrivi("tests/fuzz/casi/a/pulito", std::string("controlla\0esempi/demo\0--formato\0json", 36));

    const Resoconto r = controlla_cartella(c.radice(), {}, finti());
    EXPECT_EQ(dove_e_riga(r), (std::vector<std::string>{
                                  "docs/tests/fuzz/argomenti:0", "docs/utf16be-senza-bom.md:0",
                                  "docs/utf16be-senza-bom.md:2", "docs/utf16le-senza-bom.txt:0",
                                  "docs/utf16le-senza-bom.txt:1", "docs/utf32le-senza-bom.txt:0",
                                  "docs/utf32le-senza-bom.txt:2", "docs/zero.md:0", "tests/esempi/p/argomenti:0",
                                  "tests/fuzz/casi/a/attaccato:1", "tests/fuzz/casi/a/due:1", "tests/fuzz/casi/a/due:1",
                                  "tests/fuzz/casi/a/due:2", "tests/fuzz/semi/x/utf16be:1",
                                  "tests/fuzz/semi/x/utf16le:2", "tests/fuzzx/argomenti:0"}));
    EXPECT_TRUE(r.binari.empty());
    EXPECT_EQ(r.letti, 12u);
    for (const Problema& q : r.problemi) {
        if (q.riga == 0) {
            EXPECT_NE(q.cosa.find("testo con un byte zero fuori dalle cartelle ammesse (tests/fuzz/): un testo con "
                                  "byte zero (es. UTF-16 o UTF-32 senza BOM) si salva di nuovo in UTF-8"),
                      std::string::npos)
                << q.dove << ": " << q.cosa;
            continue;
        }
        const char* atteso = q.dove == "docs/utf16be-senza-bom.md" ? "nome z"
                             : q.dove == "docs/utf32le-senza-bom.txt" ? "nome y"
                                                                       : "nome x";
        EXPECT_NE(q.cosa.find(atteso), std::string::npos) << q.dove << ": " << q.cosa;
    }
    senza_nomi(r.problemi);
}

TEST(Deposito, CartelleConByteZero) {
    EXPECT_EQ(cartelle_con_byte_zero(), (std::vector<std::string>{"tests/fuzz/"}));
    for (const char* si : {"tests/fuzz/semi/parametri/controlla", "tests/fuzz/casi/x/y", "tests/fuzz/a"})
        EXPECT_TRUE(byte_zero_ammesso(si)) << si;
    for (const char* no : {"tests/fuzz", "tests/fuzz/", "tests/fuzzx/a", "tests/a", "docs/tests/fuzz/a", "Tests/fuzz/a",
                           "esempi/fuzz/a"})
        EXPECT_FALSE(byte_zero_ammesso(no)) << no;
}

TEST(Deposito, BinariAmmessi) {
    for (const char* si : {"tests/esempi/k010-cassaforte/cassaforte.kzc", "tests/esempi/k010-cassaforte/registro.kzr",
                           "esempi/x/cassaforte.kzc", "tests/a.kzr"})
        EXPECT_TRUE(binario_ammesso(si)) << si;
    for (const char* no : {"cassaforte.kzc", "docs/cassaforte.kzc", "tests/X.KZC", "tests/immagine.png",
                           "testsx/a.kzc", "tests/a.kzc.txt", "tests/.kzc", "src/tests/a.kzc"})
        EXPECT_FALSE(binario_ammesso(no)) << no;
}

TEST(Deposito, CartellaSaltata) {
    for (const char* si : {"build", "build-", "build-san", "build-fuzz", "build-x-y"})
        EXPECT_TRUE(cartella_saltata(si)) << si;
    for (const char* no : {"buildx", "Build", "BUILD", "xbuild", "build.vecchio", "build_san", "costruzione", ""})
        EXPECT_FALSE(cartella_saltata(no)) << no;
}

TEST(Deposito, RigheDelleCartelle) {
    const std::string gitignore =
        "build/\r\n"
        "# commento con build/\n"
        "\n"
        "   \n"
        "*.[cC][sS][vV]\n"
        "!/tests/**/*.[cC][sS][vV]\n"
        "build-*/   \n"
        "node_modules\n"
        "/altro/\n";
    EXPECT_EQ(righe_delle_cartelle(gitignore), (std::vector<std::string>{"build/", "build-*/", "node_modules", "/altro/"}));
    EXPECT_TRUE(righe_delle_cartelle("").empty());
}

// La rete del .gitignore e quella del controllo saltano le stesse cartelle: legge il .gitignore del deposito (basta
// l'albero dei file, anche senza .git) e confronta le sue righe sulle cartelle con le cartelle saltate dal controllo.
TEST(Deposito, CartelleSaltateComeGitignore) {
    std::ifstream in(fs::path(KZERO_SORGENTI) / ".gitignore", std::ios::binary);
    ASSERT_TRUE(in) << ".gitignore non si legge";
    const std::string testo((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::string> attese;
    for (const std::string& c : cartelle_saltate()) attese.push_back(c + "/");
    EXPECT_EQ(righe_delle_cartelle(testo), attese)
        << "le cartelle ignorate da .gitignore e quelle saltate dal controllo (cartelle_saltate in "
           "tests/strumenti/deposito.cpp) devono essere le stesse: chi cambia una delle due cambia anche l'altra";
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
    EXPECT_NE(pulita.str().find("Nessun nome, nessun file di dati e nessun file che non e' testo UTF-8 ne' un binario "
                                "ammesso."),
              std::string::npos);
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
