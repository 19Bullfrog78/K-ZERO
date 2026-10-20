// K-Zero - prove interne del registro degli accessi, delle scritture atomiche e del blocco (K-010): catena e ancora, voce
// non scritta = nessun dato, interruzioni a meta' (processo fermato di colpo), comandi insieme sulla stessa cartella.
// Ora fissa e Argon2id leggero solo qui (cassaforte_prove.hpp): il programma kzero usa l'orologio e la politica di rilascio.
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "accesso.hpp"
#include "cassaforte.hpp"
#include "cassaforte_formato.hpp"
#include "cassaforte_prove.hpp"
#include "chiusura.hpp"
#include "disco.hpp"
#include "errori.hpp"
#include "registro.hpp"

using namespace kzero::api;
using namespace kzero::prove;
using cripto::Bytes;
using cripto::Segreto;
namespace fs = std::filesystem;

namespace {

const std::string kId = "00112233445566778899aabbccddeeff";
const std::string kPw = "cavallo pesca lampione verde";

Segreto pw(const std::string& t) {
    password::Problema p = password::Problema::Nessuno;
    return password::normalizza(Segreto(reinterpret_cast<const unsigned char*>(t.data()), t.size()), p);
}

cassaforte::Cassaforte nuova() {
    return cassaforte::Cassaforte::crea(kId, "2099-12-31", "A", pw(kPw), politica_leggera(), cassaforte::Contenuto{});
}

registro::Voce voce(const std::string& comando) {
    registro::Voce v;
    v.ora = ora_fissa();
    v.utente = "A";
    v.comando = comando;
    v.aperte = {"personali", "particolari"};
    return v;
}

// Un registro riscritto da chi ha le chiavi (come un altro programma): voci date cosi' come sono, cifrate per formato.
std::string riscritto(const JsonDati& voci, const cassaforte::Cassaforte& c) {
    Bytes id;
    cripto::da_esadecimale(kId, id);
    cassaforte::formato::Scrittore w;
    const unsigned char magia[8] = {'K', 'Z', '-', 'R', 'E', 'G', 'I', 'S'};
    w.byte(magia, 8);
    w.u16(1);
    w.byte(id);
    w.u32(c.versione_base());
    const Bytes ad = w.dati();
    const Bytes nonce = cripto::casuali(cripto::kNonce);
    w.byte(nonce);
    const std::string testo = voci.dump();
    w.byte(cripto::cifra(c.chiave_registro(c.versione_base()), nonce.data(), reinterpret_cast<const unsigned char*>(testo.data()),
                         testo.size(), ad));
    return std::string(w.dati().begin(), w.dati().end());
}

JsonDati voci_json(const registro::Registro& r) {
    JsonDati j = JsonDati::array();
    for (const registro::Voce& v : r.voci) j.push_back(v.in_json());
    return j;
}

bool rifiutato(const std::string& file, const cassaforte::Cassaforte& c) {
    try {
        registro::leggi(file, c);
        return false;
    } catch (const registro::ErroreRegistro&) {
        return true;
    }
}

std::string leggi_testo(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

// Un pacchetto demo chiuso nella cartella "chiuso" (utente A, tutti i permessi), con il comando chiudi.
fs::path demo_chiuso(const CartellaDiLavoro& lavoro, AmbienteDiProva& amb) {
    scrivi_demo(lavoro / "chiaro");
    amb.password = {kPw};
    comando({"chiudi", (lavoro / "chiaro").string(), "--scrivi", (lavoro / "chiuso").string(), "--utente", "A", "--conservazione",
             "2099-12-31"});
    return lavoro / "chiuso";
}

registro::Registro registro_di(const fs::path& cartella) {
    const cassaforte::Cassaforte c = cassaforte::Cassaforte::apri(leggi_testo(cartella / cassaforte::kFile),
                                                                  leggi_manifesto(CartellaSuDisco(cartella))->id, "A", pw(kPw),
                                                                  politica_leggera());
    return registro::leggi(leggi_testo(cartella / registro::kFile), c);
}

}  // namespace

TEST(Registro, CatenaEAncora) {
    cassaforte::Cassaforte c = nuova();
    registro::Registro r = registro::nuovo(kId);
    for (const char* x : {"chiudi", "controlla", "giornata"}) registro::aggiungi(r, voce(x));
    c.lega_registro(3, registro::impronta(r.voci.back()));
    const std::string file = registro::scrivi(r, c);
    const registro::Registro letto = registro::leggi(file, c);
    ASSERT_EQ(letto.voci.size(), 3u);
    EXPECT_EQ(letto.voci[2].comando, "giornata");
    EXPECT_EQ(letto.voci[0].ora, ora_fissa());
    // In chiaro nel file nessuna voce (e' cifrato).
    EXPECT_EQ(file.find("giornata"), std::string::npos);
    // Un byte cambiato in qualunque punto: rifiutato.
    for (std::size_t k = 0; k < file.size(); ++k) {
        std::string x = file;
        x[k] = static_cast<char>(x[k] ^ 0x20);
        EXPECT_TRUE(rifiutato(x, c)) << k;
    }
    for (std::size_t n = 0; n < file.size(); n += 7) EXPECT_TRUE(rifiutato(file.substr(0, n), c)) << n;
    // Voci cambiate, tolte o spostate da chi ha le chiavi senza rifare la catena: rifiutate.
    JsonDati j = voci_json(r);
    JsonDati cambiata = j;
    cambiata[1]["utente"] = "Z";
    EXPECT_TRUE(rifiutato(riscritto(cambiata, c), c));
    JsonDati tolta = j;
    tolta.erase(1);
    EXPECT_TRUE(rifiutato(riscritto(tolta, c), c));
    JsonDati spostata = j;
    std::swap(spostata[0], spostata[1]);
    EXPECT_TRUE(rifiutato(riscritto(spostata, c), c));
    // Anche rifacendo numeri e catena: la cassaforte ricorda l'ultima voce (ancora), che non torna.
    registro::Registro accorciato = registro::nuovo(kId);
    registro::aggiungi(accorciato, voce("chiudi"));
    registro::aggiungi(accorciato, voce("giornata"));
    EXPECT_TRUE(rifiutato(registro::scrivi(accorciato, c), c));
    // Un registro di un'altra cassaforte.
    const cassaforte::Cassaforte altra = cassaforte::Cassaforte::crea("ffeeddccbbaa99887766554433221100", "2099-12-31", "A",
                                                                     pw(kPw), politica_leggera(), cassaforte::Contenuto{});
    EXPECT_TRUE(rifiutato(file, altra));
    EXPECT_TRUE(rifiutato("", c));
}

// Dopo un cambio delle chiavi il registro cifrato con la chiave base di prima si legge (storia delle chiavi) e si riscrive
// con quella nuova, che l'utente tolto non ha.
TEST(Registro, ChiaveBaseCambiata) {
    cassaforte::Cassaforte c = nuova();
    c.aggiungi_utente("B", cassaforte::Permessi{true, false}, pw("barca gialla sul lago verde"), politica_leggera());
    registro::Registro r = registro::nuovo(kId);
    registro::aggiungi(r, voce("chiudi"));
    const std::string vecchio = registro::scrivi(r, c);
    c.togli_utente("B");
    EXPECT_EQ(c.versione_base(), 2u);
    const registro::Registro letto = registro::leggi(vecchio, c);
    EXPECT_EQ(letto.versione_base, 1u);
    const std::string nuovo = registro::scrivi(letto, c);
    EXPECT_EQ(registro::leggi(nuovo, c).versione_base, 2u);
}

// Il comando apre, scrive la voce (con l'ora fissa delle prove) e solo dopo da' i dati.
TEST(Accesso, OgniAperturaScriveUnaVoce) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    amb.password = {kPw};
    comando({"controlla", chiuso.string(), "--utente", "A", "--includi-dati-personali"});
    amb.password = {kPw};
    comando({"giornata", chiuso.string(), "--utente", "A", "--data", "2026-10-22", "--mezzo", "ALFA-03"});
    const registro::Registro r = registro_di(chiuso);
    ASSERT_EQ(r.voci.size(), 3u);
    EXPECT_EQ(r.voci[0].comando, "chiudi");
    EXPECT_EQ(r.voci[1].comando, "controlla");
    EXPECT_EQ(r.voci[1].mostrate, (std::vector<std::string>{"personali"}));
    EXPECT_EQ(r.voci[2].nota, "giornata del 2026-10-22, mezzo ALFA-03");
    for (const registro::Voce& v : r.voci) EXPECT_EQ(v.ora, ora_fissa());
    // Nel registro mai i dati: nessun nome del demo.
    for (const registro::Voce& v : r.voci)
        for (const char* nome : {"Bianchi", "Rossi", "Verdi", "Neri"}) EXPECT_EQ(v.in_json().dump().find(nome), std::string::npos);
}

// Se la voce non si scrive, i dati non si aprono e non si mostrano (errore K-ERR-030).
TEST(Accesso, VoceNonScrittaNessunDato) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    disco::per_le_prove_a_ogni_passo([](const char* passo, const std::string& nome) {
        if (nome == registro::kFile && std::string(passo) == "temporaneo creato") throw disco::ErroreDisco("disco pieno (prova)");
    });
    amb.password = {kPw};
    try {
        comando({"giornata", chiuso.string(), "--utente", "A", "--data", "2026-10-22", "--includi-dati-personali"});
        ADD_FAILURE() << "giornata mostrata senza voce nel registro";
    } catch (const ErroreKZero& e) {
        EXPECT_EQ(e.definizione().codice, "K-ERR-030");
    }
    disco::per_le_prove_a_ogni_passo(nullptr);
    EXPECT_EQ(registro_di(chiuso).voci.size(), 1u);   // il registro di prima, intero
}

namespace {

// Lancia "fai" in un processo figlio che si ferma di colpo (come un PC spento) al passo dato della scrittura del file dato.
void interrotto_a(const std::string& file, const std::string& al_passo, const std::function<void()>& fai) {
    static std::string f, p;
    f = file;
    p = al_passo;
    const pid_t figlio = ::fork();
    ASSERT_GE(figlio, 0);
    if (figlio == 0) {
        disco::per_le_prove_a_ogni_passo([](const char* passo, const std::string& nome) {
            if (nome == f && passo == p) ::_exit(0);
        });
        try {
            fai();
        } catch (...) {
            ::_exit(2);
        }
        ::_exit(1);   // il passo non e' mai arrivato
    }
    int stato = 0;
    ::waitpid(figlio, &stato, 0);
    ASSERT_TRUE(WIFEXITED(stato));
    ASSERT_EQ(WEXITSTATUS(stato), 0) << "il processo non si e' fermato al passo " << al_passo << " di " << file;
}

std::vector<std::string> temporanei(const fs::path& cartella) {
    std::vector<std::string> t;
    for (const auto& e : fs::directory_iterator(cartella))
        if (e.path().filename().string().rfind(".", 0) == 0) t.push_back(e.path().filename().string());
    return t;
}

}  // namespace

// Interruzione a meta' della scrittura del registro: resta il registro di prima o quello nuovo, sempre intero e leggibile.
TEST(Accesso, InterruzioneDelRegistro) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    const auto apri = [&] {
        amb.password = {kPw};
        comando({"controlla", chiuso.string(), "--utente", "A"});
    };
    interrotto_a(registro::kFile, "temporaneo creato", apri);
    EXPECT_EQ(registro_di(chiuso).voci.size(), 1u);
    interrotto_a(registro::kFile, "temporaneo scritto", apri);
    EXPECT_EQ(registro_di(chiuso).voci.size(), 1u);
    EXPECT_FALSE(temporanei(chiuso).empty());   // il file temporaneo rimasto (kzero controlla lo segnala)
    interrotto_a(registro::kFile, "sostituito", apri);
    EXPECT_EQ(registro_di(chiuso).voci.size(), 2u);
    apri();   // e si continua
    EXPECT_EQ(registro_di(chiuso).voci.size(), 3u);
}

// Interruzione a meta' della chiusura: la cartella nuova non c'e' (al piu' una cartella temporanea nascosta), l'originale e'
// intatto, e la chiusura si rifa'.
TEST(Accesso, InterruzioneDellaChiusura) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    scrivi_demo(lavoro / "chiaro");
    const auto chiudi = [&] {
        amb.password = {kPw};
        comando({"chiudi", (lavoro / "chiaro").string(), "--scrivi", (lavoro / "chiuso").string(), "--utente", "A",
                 "--conservazione", "2099-12-31"});
    };
    for (const std::string& file : {std::string("persone.json"), std::string(cassaforte::kFile), std::string(registro::kFile)}) {
        interrotto_a(file, "temporaneo scritto", chiudi);
        EXPECT_FALSE(fs::exists(lavoro / "chiuso")) << file;
    }
    const Lettura originale = leggi_pacchetto(CartellaSuDisco(lavoro / "chiaro"));
    EXPECT_TRUE(originale.forma_valida());
    EXPECT_EQ(originale.riservati.at("persone.json").personali, 161u);
    chiudi();
    EXPECT_EQ(registro_di(lavoro / "chiuso").voci.size(), 1u);
}

// Interruzione a meta' della richiusura, dopo la cassaforte e prima del manifesto: si vede (K-CTR-093), nessun dato si
// mette nel posto sbagliato, e rifare la richiusura ripara.
TEST(Accesso, InterruzioneDellaRichiusura) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    amb.password = {kPw};
    comando({"esporta", chiuso.string(), "--scrivi", (lavoro / "uscita").string(), "--utente", "A"});
    const auto richiudi = [&] {
        amb.password = {kPw};
        comando({"richiudi", (lavoro / "uscita").string(), "--scrivi", chiuso.string(), "--utente", "A"});
    };
    interrotto_a(cassaforte::kFile, "sostituito", richiudi);
    amb.password = {kPw};
    const Risultato r = comando({"controlla", chiuso.string(), "--utente", "A"});
    EXPECT_EQ(r.codice, 1);
    EXPECT_NE(r.uscita.find("K-CTR-093"), std::string::npos) << r.uscita;
    richiudi();
    amb.password = {kPw};
    const Risultato dopo = comando({"controlla", chiuso.string(), "--utente", "A"});
    EXPECT_EQ(dopo.codice, 0) << dopo.uscita;
}

// Due (anzi otto) comandi insieme sulla stessa cartella, piu' uno che cambia la cassaforte: nessuna voce persa, nessun file
// rovinato.
TEST(Accesso, ComandiInsieme) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    constexpr int kFigli = 8, kVolte = 3;
    std::vector<pid_t> figli;
    for (int i = 0; i <= kFigli; ++i) {
        const pid_t p = ::fork();
        ASSERT_GE(p, 0);
        if (p == 0) {
            try {
                if (i == kFigli) {
                    amb.password = {kPw, "barca gialla sul lago verde"};
                    comando({"aggiungi-utente", "--scrivi", chiuso.string(), "--utente", "A", "--nuovo-utente", "B", "--permessi",
                             "personali"});
                } else {
                    for (int k = 0; k < kVolte; ++k) {
                        amb.password = {kPw};
                        comando({"controlla", chiuso.string(), "--utente", "A"});
                    }
                }
            } catch (...) {
                ::_exit(1);
            }
            ::_exit(0);
        }
        figli.push_back(p);
    }
    for (pid_t p : figli) {
        int stato = 0;
        ::waitpid(p, &stato, 0);
        EXPECT_TRUE(WIFEXITED(stato) && WEXITSTATUS(stato) == 0);
    }
    const registro::Registro r = registro_di(chiuso);
    EXPECT_EQ(r.voci.size(), 1u + kFigli * kVolte + 1u);
    for (std::size_t i = 0; i < r.voci.size(); ++i) EXPECT_EQ(r.voci[i].numero, i + 1);
    EXPECT_TRUE(temporanei(chiuso).empty());
}
