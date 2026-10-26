// K-Zero - prove interne del registro degli accessi, delle scritture atomiche e del blocco (K-010): catena e ancora, voce
// non scritta = nessun dato, interruzioni a meta' (processo fermato di colpo), comandi insieme sulla stessa cartella.
// Ora fissa e Argon2id leggero solo qui (cassaforte_prove.hpp): il programma kzero usa l'orologio e la politica di rilascio.
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
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
const std::string kPwB = "barca gialla sul lago verde";
const std::string kPwC = "il treno delle sette e mezza";

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

// Un registro scritto come farebbe un altro programma (docs/cassaforte.md): voci date cosi' come sono, cifrate con la
// chiave del registro data, per la versione della chiave base data.
std::string registro_cifrato(const JsonDati& voci, const Bytes& id, std::uint32_t versione_base, const Segreto& chiave) {
    cassaforte::formato::Scrittore w;
    const unsigned char magia[8] = {'K', 'Z', '-', 'R', 'E', 'G', 'I', 'S'};
    w.byte(magia, 8);
    w.u16(1);
    w.byte(id);
    w.u32(versione_base);
    const Bytes ad = w.dati();
    const Bytes nonce = cripto::casuali(cripto::kNonce);
    w.byte(nonce);
    const std::string testo = voci.dump();
    w.byte(cripto::cifra(chiave, nonce.data(), reinterpret_cast<const unsigned char*>(testo.data()), testo.size(), ad));
    return std::string(w.dati().begin(), w.dati().end());
}

// Un registro riscritto da chi ha le chiavi (come un altro programma).
std::string riscritto(const JsonDati& voci, const cassaforte::Cassaforte& c) {
    Bytes id;
    cripto::da_esadecimale(kId, id);
    return registro_cifrato(voci, id, c.versione_base(), c.chiave_registro(c.versione_base()));
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

namespace {

// Per tutta la vita dell'oggetto i file temporanei vanno in una cartella data (TMPDIR), poi si torna a prima.
class TemporaneiIn {
public:
    explicit TemporaneiIn(const fs::path& cartella) {
        const char* prima = std::getenv("TMPDIR");
        if (prima) prima_ = prima;
        c_era_ = prima != nullptr;
        fs::create_directories(cartella);
        ::setenv("TMPDIR", cartella.c_str(), 1);
    }
    ~TemporaneiIn() {
        if (c_era_) ::setenv("TMPDIR", prima_.c_str(), 1);
        else ::unsetenv("TMPDIR");
    }
    TemporaneiIn(const TemporaneiIn&) = delete;
    TemporaneiIn& operator=(const TemporaneiIn&) = delete;

private:
    std::string prima_;
    bool c_era_ = false;
};

std::set<std::string> nomi_in(const fs::path& cartella) {
    std::set<std::string> n;
    for (const auto& e : fs::recursive_directory_iterator(cartella)) n.insert(fs::relative(e.path(), cartella).string());
    return n;
}

}  // namespace

// I dati decifrati restano in memoria (punto 7): dopo ogni comando che apre la cassaforte nessun file nuovo, ne' nella
// cartella del pacchetto ne' tra i temporanei, e nessun valore personale o particolare in nessun file, fuori dal pacchetto in
// chiaro di partenza e dall'uscita in chiaro chiesta apposta (kzero esporta --scrivi).
TEST(Accesso, DatiDecifratiMaiSuDisco) {
    CartellaDiLavoro lavoro;
    const TemporaneiIn temporanei_qui(lavoro / "tmp");
    ASSERT_EQ(fs::temp_directory_path(), lavoro / "tmp");
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    const std::set<std::string> file_del_pacchetto = nomi_in(chiuso);

    // I valori in testo del demo che vanno nella cassaforte, presi con la regola delle classi (nomi, case, indirizzi...),
    // tranne quelli che dopo la chiusura si leggono comunque nei file del pacchetto perche' sono anche un dato comune (es. il
    // comune di una casa e' anche il comune di una fermata; "disabili" e' anche una parola dei regolamenti).
    const chiusura::Separazione s = chiusura::separa(leggi_pacchetto(CartellaSuDisco(lavoro / "chiaro")));
    std::string pacchetto_chiuso;
    for (const std::string& nome : file_del_pacchetto) pacchetto_chiuso += leggi_testo(chiuso / nome);
    std::set<std::string> valori;
    for (const auto* elenco : {&s.contenuto.personali, &s.contenuto.particolari, &s.contenuto.legati})
        for (const Voce& v : *elenco)
            if (v.valore.is_string() && v.valore.get<std::string>().size() >= 6 &&
                pacchetto_chiuso.find(v.valore.get<std::string>()) == std::string::npos)
                valori.insert(v.valore.get<std::string>());
    ASSERT_GT(valori.size(), 30u);
    ASSERT_EQ(valori.count("Rossi Mario"), 1u);
    ASSERT_EQ(valori.count("Casa Bianchi"), 1u);

    const std::string c = chiuso.string();
    const std::vector<std::vector<std::string>> comandi_che_aprono = {
        {"controlla", c, "--utente", "A", "--includi-dati-personali", "--includi-dati-particolari"},
        {"giornata", c, "--utente", "A", "--data", "2026-10-22", "--includi-dati-personali", "--includi-dati-particolari"},
        {"giornata", c, "--utente", "A", "--data", "2026-10-22", "--mezzo", "ALFA-03", "--includi-dati-personali"},
        {"giornata", c, "--utente", "A", "--data", "2026-10-22", "--persona", "P-C", "--includi-dati-personali"},
        {"cassaforte", c, "--utente", "A"},
        {"registro", c, "--utente", "A"},
        {"esporta", c, "--scrivi", (lavoro / "uscita").string(), "--utente", "A"},
        {"richiudi", (lavoro / "uscita").string(), "--scrivi", c, "--utente", "A"},
        {"conservazione", "--scrivi", c, "--utente", "A", "--conservazione", "2098-12-31"},
    };
    for (const auto& argomenti : comandi_che_aprono) {
        amb.password = {kPw};
        EXPECT_EQ(comando(argomenti).codice, 0) << argomenti[0];
    }
    amb.password = {kPw, "barca gialla sul lago verde"};
    EXPECT_EQ(comando({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente", "B", "--permessi", "personali"}).codice, 0);
    amb.password = {"barca gialla sul lago verde", "una password tutta nuova per B"};
    EXPECT_EQ(comando({"password", "--scrivi", c, "--utente", "B"}).codice, 0);
    amb.password = {kPw};
    EXPECT_EQ(comando({"togli-utente", "--scrivi", c, "--utente", "A", "--utente-da-togliere", "B"}).codice, 0);

    EXPECT_TRUE(fs::is_empty(lavoro / "tmp")) << "file temporanei rimasti";
    EXPECT_EQ(nomi_in(chiuso), file_del_pacchetto);   // nessun file nuovo, nemmeno nascosto
    std::size_t letti = 0;
    for (const auto& e : fs::recursive_directory_iterator(lavoro.percorso())) {
        if (!e.is_regular_file()) continue;
        const std::string primo = fs::relative(e.path(), lavoro.percorso()).begin()->string();
        if (primo == "chiaro" || primo == "uscita") continue;
        const std::string testo = leggi_testo(e.path());
        for (const std::string& v : valori) EXPECT_EQ(testo.find(v), std::string::npos) << e.path() << ": " << v;
        ++letti;
    }
    EXPECT_EQ(letti, file_del_pacchetto.size());
}

namespace {

// Una cassaforte finta tutta coerente, preparata da chi ha solo i file (nessuna password, nessuna chiave), seguendo il
// formato di docs/cassaforte.md: chiavi nuove per tutti i livelli con le versioni alzate, storie con chiavi vecchie
// inventate (quelle vere non le conosce), una busta per ogni utente e livello (le chiavi pubbliche sono in chiaro), sezioni
// cifrate con le chiavi nuove (dati inventati), autenticazioni rifatte, e il registro rifatto con la chiave base finta, con
// l'ancora giusta nella cassaforte (cosi' nessun altro controllo la ferma prima di quello delle storie).
struct Finta {
    std::string cassaforte;
    std::string registro;
};

Finta cassaforte_finta(const std::string& vera, const std::vector<Voce>& personali) {
    using namespace cassaforte::formato;
    Analizzato a = analizza(vera);
    Intestazione& i = a.intestazione;
    std::array<Segreto, kLivelli> k;
    for (int l = 0; l < kLivelli; ++l) {
        const std::size_t ls = static_cast<std::size_t>(l);
        i.versioni[ls] += 1;
        const Bytes b = cripto::casuali(cripto::kChiave);
        k[ls] = Segreto(b.data(), b.size());
        Scrittore storia;
        for (std::uint32_t v = 1; v < i.versioni[ls]; ++v) {
            storia.u32(v);
            storia.byte(cripto::casuali(cripto::kChiave));
        }
        const Bytes nonce = cripto::casuali(cripto::kNonce);
        Bytes s = nonce;
        const Bytes c = cripto::cifra(derivata(k[ls], kPerStoria), nonce.data(), storia.dati().data(), storia.dati().size(),
                                      ad_livello(i.id, l, i.versioni[ls]));
        s.insert(s.end(), c.begin(), c.end());
        i.storie[ls] = std::move(s);
    }
    i.buste.clear();
    for (const Utente& u : i.utenti)
        for (int l = 0; l < kLivelli; ++l) {
            if (!ha(u.permessi, l)) continue;
            const std::size_t ls = static_cast<std::size_t>(l);
            const Bytes dentro = contenuto_busta(i.id, l, i.versioni[ls], k[ls]);
            Busta b;
            b.codice = u.codice;
            b.livello = l;
            b.versione = i.versioni[ls];
            b.busta = cripto::sigilla(dentro.data(), dentro.size(), u.pubblica);
            i.buste.push_back(std::move(b));
        }
    std::string sezioni;
    for (std::size_t sezione = 0; sezione < kSezioni; ++sezione) {
        const std::string testo = testo_sezione(sezione == kSezionePersonali ? personali : std::vector<Voce>{});
        const auto* dati = reinterpret_cast<const unsigned char*>(testo.data());
        Sezione& s = i.sezioni[sezione];
        s.nonce = cripto::casuali(cripto::kNonce);
        Bytes c;
        if (sezione == kSezioneLegati) {
            c = cripto::cifra(chiave_legati(i.id, k[kPersonali], k[kParticolari]), s.nonce.data(), dati, testo.size(),
                              ad_legati(i.id, i.versioni[kPersonali], i.versioni[kParticolari], i.generazione));
        } else {
            const int livello = sezione == kSezionePersonali ? kPersonali : kParticolari;
            const std::size_t ls = static_cast<std::size_t>(livello);
            c = cripto::cifra(derivata(k[ls], kPerSezione), s.nonce.data(), dati, testo.size(),
                              ad_sezione(i.id, livello, i.versioni[ls], i.generazione));
        }
        s.lunghezza = c.size();
        s.impronta = cripto::impronta(c);
        sezioni.append(c.begin(), c.end());
    }
    registro::Registro r = registro::nuovo(cripto::esadecimale(i.id));
    registro::Voce v = voce("chiudi");
    v.utente = i.utenti.front().codice;
    registro::aggiungi(r, v);
    i.registro_voci = 1;
    i.registro_impronta = registro::impronta(r.voci.back());
    for (int l : {kPersonali, kParticolari}) {
        const std::size_t ls = static_cast<std::size_t>(l);
        i.autenticazione[ls] = cripto::autentica(derivata(k[ls], kPerAutenticazione), lista(i, l));
    }
    Bytes h = scrivi_intestazione(i);
    const Bytes prima(h.begin(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
    const Bytes t = cripto::autentica(derivata(k[kBase], kPerAutenticazione), prima);
    std::copy(t.begin(), t.end(), h.end() - static_cast<std::ptrdiff_t>(cripto::kAutenticazione));
    Finta f;
    f.cassaforte = std::string(h.begin(), h.end()) + sezioni;
    f.registro = registro_cifrato(voci_json(r), i.id, i.versioni[kBase], derivata(k[kBase], kPerRegistro));
    return f;
}

}  // namespace

// Aggiunta 1 del K-010, punto 2 (sabotaggio dell'arbitro non scoperto): una cassaforte finta tutta coerente, preparata da
// chi ha solo i file, con il suo registro. Ogni utente la rifiuta (le chiavi nuove non vengono da chi aveva quelle
// vecchie: la storia non contiene la chiave della sua fessura) e il programma non mostra nessun dato (K-ERR-028). Senza il
// controllo delle storie la cassaforte finta si aprirebbe davvero, con i dati inventati.
TEST(Accesso, CassaforteFintaFattaSenzaPassword) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);
    const std::string c = chiuso.string();
    amb.password = {kPw, kPwB};
    comando({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente", "B", "--permessi", "personali"});
    amb.password = {kPw, kPwC};
    comando({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente", "C", "--permessi", "particolari"});
    Voce inventata;
    inventata.file = "persone.json";
    inventata.percorso = JsonDati::array({"persone", JsonDati{{"codice", "P-A"}}});
    inventata.campo = "nome";
    inventata.valore = "Nome Inventato";
    const Finta f = cassaforte_finta(leggi_testo(chiuso / cassaforte::kFile), {inventata});
    const std::string id = leggi_manifesto(CartellaSuDisco(chiuso))->id;
    const std::vector<std::pair<std::string, std::string>> utenti = {{"A", kPw}, {"B", kPwB}, {"C", kPwC}};
    for (const auto& [utente, password] : utenti) {
        try {
            const cassaforte::Cassaforte aperta = cassaforte::Cassaforte::apri(f.cassaforte, id, utente, pw(password), politica_leggera());
            ADD_FAILURE() << "la cassaforte finta si e' aperta per " << utente << " ("
                          << aperta.voci(Classe::Personale).size() << " valori personali inventati)";
        } catch (const cassaforte::ErroreCassaforte& e) {
            EXPECT_EQ(e.tipo, cassaforte::ErroreCassaforte::Tipo::Manomessa) << utente << ": " << e.what();
        }
    }
    // Sul programma, con la cassaforte e il registro finti al loro posto.
    disco::scrivi_atomico(chiuso, cassaforte::kFile, f.cassaforte);
    disco::scrivi_atomico(chiuso, registro::kFile, f.registro);
    for (const auto& [utente, password] : utenti) {
        amb.password = {password};
        try {
            const Risultato r = comando({"giornata", c, "--utente", utente, "--data", "2026-10-22"});
            ADD_FAILURE() << "giornata mostrata a " << utente << " con la cassaforte finta (uscita " << r.codice << ")";
        } catch (const ErroreKZero& e) {
            EXPECT_EQ(e.definizione().codice, "K-ERR-028") << utente;
        }
    }
}

// Aggiunta 1 del K-010, punto 3 (sabotaggio dell'arbitro non scoperto): un utente con le chiavi riscrive il registro con lo
// stesso numero di voci e la catena ricalcolata, ma una voce prima dell'ancora diversa (l'uscita in chiaro tolta e
// sostituita). Il registro da solo e' coerente: lo scopre l'impronta dell'ultima voce che la cassaforte ricorda. K-ERR-029,
// nessun dato.
TEST(Accesso, RegistroRiscrittoPrimaDellAncora) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);   // voce 1: chiudi
    const std::string c = chiuso.string();
    amb.password = {kPw};
    comando({"esporta", c, "--scrivi", (lavoro / "uscita").string(), "--utente", "A"});   // voce 2
    amb.password = {kPw, kPwB};
    comando({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente", "B", "--permessi", "personali"});   // voce 3
    const registro::Registro prima = registro_di(chiuso);
    ASSERT_EQ(prima.voci.size(), 3u);
    ASSERT_EQ(prima.voci[1].comando, "esporta");
    const std::string id = leggi_manifesto(CartellaSuDisco(chiuso))->id;
    const cassaforte::Cassaforte cf =
        cassaforte::Cassaforte::apri(leggi_testo(chiuso / cassaforte::kFile), id, "A", pw(kPw), politica_leggera());
    ASSERT_EQ(cf.riepilogo().registro_voci, 3u);   // l'ancora e' l'ultima voce
    registro::Registro dopo = registro::nuovo(id);
    for (registro::Voce v : prima.voci) {
        if (v.comando == "esporta") {
            v.comando = "controlla";
            v.mostrate.clear();
            v.nota.clear();
        }
        registro::aggiungi(dopo, std::move(v));   // numeri e catena ricalcolati
    }
    ASSERT_EQ(dopo.voci.size(), prima.voci.size());
    // Da solo il registro riscritto e' coerente: stesse voci, catena giusta.
    EXPECT_NO_THROW(registro::voci_dal_testo(voci_json(dopo).dump(), dopo.id, 0, Bytes()));
    const std::string file = registro::scrivi(dopo, cf);
    EXPECT_THROW(registro::leggi(file, cf), registro::ErroreRegistro);
    disco::scrivi_atomico(chiuso, registro::kFile, file);
    amb.password = {kPw};
    try {
        const Risultato r = comando({"giornata", c, "--utente", "A", "--data", "2026-10-22", "--includi-dati-personali"});
        ADD_FAILURE() << "giornata mostrata con il registro riscritto (uscita " << r.codice << ")";
    } catch (const ErroreKZero& e) {
        EXPECT_EQ(e.definizione().codice, "K-ERR-029");
    }
}

namespace {

// Gli avvisi della cassaforte (K-CTR-097 ... K-CTR-100) di un'uscita in testo: le righe "Avviso K-CTR-nnn: messaggio", come
// coppie (codice, messaggio).
bool della_cassaforte(const std::string& codice) { return codice >= "K-CTR-097" && codice <= "K-CTR-100"; }

std::vector<std::pair<std::string, std::string>> avvisi_del_testo(const std::string& testo) {
    std::vector<std::pair<std::string, std::string>> out;
    std::istringstream in(testo);
    for (std::string riga; std::getline(in, riga);) {
        if (riga.rfind("Avviso K-CTR-", 0) != 0 || riga.compare(16, 2, ": ") != 0) continue;
        const std::string codice = riga.substr(7, 9);
        if (della_cassaforte(codice)) out.emplace_back(codice, riga.substr(18));
    }
    return out;
}

// Gli avvisi del catalogo di un'uscita JSON: i problemi di kzero controlla, gli avvisi della cassaforte in kzero giornata,
// gli avvisi degli altri comandi (senza le indicazioni del comando, che non hanno codice).
std::vector<std::pair<std::string, std::string>> avvisi_del_json(const std::string& uscita) {
    const JsonDati j = JsonDati::parse(uscita);
    const JsonDati& elenco = j.contains("problemi") ? j.at("problemi")
                             : j.at("schema") == "giornata" ? j.at("cassaforte").at("avvisi")
                                                             : j.at("avvisi");
    std::vector<std::pair<std::string, std::string>> out;
    for (const JsonDati& a : elenco)
        if (a.at("codice").is_string() && della_cassaforte(a.at("codice").get<std::string>()))
            out.emplace_back(a.at("codice").get<std::string>(), a.at("messaggio").get<std::string>());
    return out;
}

bool ha_avviso(const std::vector<std::pair<std::string, std::string>>& avvisi, const std::string& codice) {
    return std::any_of(avvisi.begin(), avvisi.end(), [&](const auto& a) { return a.first == codice; });
}

}  // namespace

// Aggiunta 1 del K-010, punto 4: gli avvisi sulle password. Dopo aggiungi-utente e password --per-utente la password l'ha
// scritta un altro (K-CTR-100); dopo togli-utente le password che l'utente tolto poteva conoscere vanno cambiate
// (K-CTR-099); chi ha ricevuto chiavi nuove in una busta lo vede a ogni apertura, in ogni comando, finche' non cambia la
// password (K-CTR-098). Testi del catalogo, uguali in testo e in JSON. Cambiare la password da' anche una coppia X25519
// nuova: chi conosceva la password vecchia non apre le buste dei prossimi cambi delle chiavi.
TEST(Accesso, AvvisiSullePassword) {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    const fs::path chiuso = demo_chiuso(lavoro, amb);   // A: tutti i permessi
    const std::string c = chiuso.string();
    const auto esegui = [&](const std::vector<std::string>& argomenti, const std::vector<std::string>& password) {
        amb.password = password;
        const Risultato r = comando(argomenti);
        EXPECT_EQ(r.codice, 0) << argomenti[0];
        return r.uscita;
    };
    const auto con_json = [](std::vector<std::string> argomenti) {
        argomenti.insert(argomenti.end(), {"--formato", "json"});
        return argomenti;
    };
    const auto pubblica = [&](const std::string& utente) {
        const auto a = cassaforte::formato::analizza(leggi_testo(chiuso / cassaforte::kFile));
        return a.intestazione.utente(utente)->pubblica;
    };
    const auto messaggio = [](kzero::Check controllo, int variante, std::vector<std::pair<std::string, std::string>> p) {
        return accesso::mostra(accesso::avviso(controllo, variante, std::move(p))).messaggio;
    };

    // Utenti aggiunti: K-CTR-100, in testo (B) e in JSON (C, D).
    const std::string b = esegui({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente", "B", "--permessi",
                                  "personali"}, {kPw, kPwB});
    EXPECT_NE(b.find("Avviso K-CTR-100: L'utente B e' stato aggiunto con una password scritta dall'utente A: B la cambi al "
                     "primo uso (kzero password), cosi' la conosce solo chi la usa."), std::string::npos) << b;
    const auto c_json = avvisi_del_json(esegui(con_json({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente",
                                                         "C", "--permessi", "particolari"}), {kPw, kPwC}));
    ASSERT_EQ(c_json.size(), 1u);
    EXPECT_EQ(c_json[0].first, "K-CTR-100");
    EXPECT_EQ(c_json[0].second, messaggio(kzero::Check::PasswordWrittenByOther, 0, {{"nuovo", "C"}, {"chi", "A"}}));
    esegui({"aggiungi-utente", "--scrivi", c, "--utente", "A", "--nuovo-utente", "D", "--permessi", "personali"},
           {kPw, "nuvole basse sopra il monte"});
    const Bytes pubblica_b = pubblica("B");

    // A toglie D: K-CTR-099 per A, che ha gia' le chiavi nuove (niente K-CTR-098 per lui).
    const auto tolto = avvisi_del_json(esegui(con_json({"togli-utente", "--scrivi", c, "--utente", "A", "--utente-da-togliere",
                                                        "D"}), {kPw}));
    ASSERT_EQ(tolto.size(), 1u);
    EXPECT_EQ(tolto[0].first, "K-CTR-099");
    EXPECT_EQ(tolto[0].second, messaggio(kzero::Check::UserRemovedPasswords, 0, {{"tolto", "D"}, {"utente", "A"}}));

    // Chi ha le chiavi in una busta (B e C: la chiave base e' cambiata per tutti) lo vede in ogni comando, testo e JSON
    // uguali; A no.
    const auto comandi_di = [&](const std::string& u) {
        return std::vector<std::vector<std::string>>{{"controlla", c, "--utente", u},
                                                     {"giornata", c, "--utente", u, "--data", "2026-10-22"},
                                                     {"cassaforte", c, "--utente", u},
                                                     {"registro", c, "--utente", u}};
    };
    for (const auto& [u, password] : {std::pair{std::string("B"), kPwB}, std::pair{std::string("C"), kPwC}}) {
        const std::string atteso = messaggio(kzero::Check::KeysFromEnvelope, 0, {{"utente", u}});
        for (const auto& argomenti : comandi_di(u)) {
            const auto testo = avvisi_del_testo(esegui(argomenti, {password}));
            const auto json = avvisi_del_json(esegui(con_json(argomenti), {password}));
            EXPECT_EQ(testo, json) << u << " " << argomenti[0];
            EXPECT_EQ(testo, (std::vector<std::pair<std::string, std::string>>{{"K-CTR-098", atteso}})) << u << " " << argomenti[0];
        }
    }
    for (const auto& argomenti : comandi_di("A")) EXPECT_TRUE(avvisi_del_testo(esegui(argomenti, {kPw})).empty()) << argomenti[0];

    // B cambia la sua password: nessun avviso, e da li' in poi nessun K-CTR-098; coppia X25519 nuova.
    const std::string kPwB2 = "una frase nuova tutta per B";
    EXPECT_TRUE(avvisi_del_testo(esegui({"password", "--scrivi", c, "--utente", "B"}, {kPwB, kPwB2})).empty());
    for (const auto& argomenti : comandi_di("B"))
        EXPECT_FALSE(ha_avviso(avvisi_del_testo(esegui(argomenti, {kPwB2})), "K-CTR-098")) << argomenti[0];
    EXPECT_NE(pubblica("B"), pubblica_b);

    // A da' una password nuova a C (persa): K-CTR-100 nella forma della password nuova; C con quella non ha piu' buste.
    const std::string kPwC2 = "la password che A ha scritto per C";
    const auto per_c = avvisi_del_testo(esegui({"password", "--scrivi", c, "--utente", "A", "--per-utente", "C"}, {kPw, kPwC2}));
    ASSERT_EQ(per_c.size(), 1u);
    EXPECT_EQ(per_c[0].first, "K-CTR-100");
    EXPECT_EQ(per_c[0].second, messaggio(kzero::Check::PasswordWrittenByOther, 1, {{"nuovo", "C"}, {"chi", "A"}}));
    for (const auto& argomenti : comandi_di("C"))
        EXPECT_FALSE(ha_avviso(avvisi_del_testo(esegui(argomenti, {kPwC2})), "K-CTR-098")) << argomenti[0];
}
