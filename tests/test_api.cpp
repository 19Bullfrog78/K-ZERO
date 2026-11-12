// K-Zero - prove dell'API in memoria: tabella dei comandi, catalogo degli errori e dei controlli,
// formato degli errori (anche con posizione), argomenti vuoti, guasto in scrittura, lettura dei pacchetti
// (controlli sulla forma dei file), messaggi in italiano, classi dei dati e loro filtro.
// Le prove che lanciano il programma vero stanno in tests/programma.
#include <algorithm>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#if defined(__unix__)
#include <sys/stat.h>
#endif

#include "api.hpp"
#include "comandi.hpp"
#include "controlli_catalogo.hpp"
#include "controllo.hpp"
#include "errori.hpp"
#include "formato_dati.hpp"
#include "lettore.hpp"
#include "pacchetto_demo.hpp"
#include "parametri.hpp"
#include "riservatezza.hpp"
#include "schemi_dati.hpp"
#include "uscita.hpp"
#include "validazione.hpp"

using namespace kzero::api;
using kzero::Check;

// Ogni errore ha un codice K-ERR-nnn unico, un codice di uscita da 1 a 3 e una descrizione;
// il catalogo e' in ordine di codice e copre tutti gli errori che il programma conosce.
TEST(Api, catalogo_errori) {
    const auto& catalogo = catalogo_errori();
    const std::regex forma("K-ERR-[0-9]{3}");
    std::set<std::string> codici;
    for (const auto& e : catalogo) {
        EXPECT_TRUE(std::regex_match(e.codice, forma)) << e.codice;
        EXPECT_TRUE(codici.insert(e.codice).second) << "codice ripetuto: " << e.codice;
        const int uscita = static_cast<int>(e.uscita);
        EXPECT_TRUE(uscita >= 1 && uscita <= 3) << e.codice;
        EXPECT_FALSE(e.descrizione.empty()) << e.codice;
    }
    EXPECT_TRUE(std::is_sorted(catalogo.begin(), catalogo.end(),
                               [](const DefinizioneErrore& a, const DefinizioneErrore& b) { return a.codice < b.codice; }));
    for (Errore e : {Errore::MancaComando, Errore::ComandoSconosciuto, Errore::ParametroSconosciuto,
                     Errore::ArgomentoNonPrevisto, Errore::ValoreMancante, Errore::ValoreNonAmmesso,
                     Errore::ParametroRipetuto, Errore::ParametroSenzaValore, Errore::ErroreInterno,
                     Errore::UscitaNonScrivibile, Errore::MancaArgomento, Errore::CartellaNonTrovata})
        EXPECT_NO_THROW(definizione_errore(e));
}

// La tabella dei comandi e' coerente: nomi unici e in ordine, ogni parametro con "--",
// le scelte con valori in ordine e predefinito ammesso, --formato in ogni comando,
// argomenti con nome e descrizione, lo schema di ogni comando nell'elenco degli schemi.
TEST(Api, tabella_comandi) {
    const auto& tabella = comandi();
    ASSERT_FALSE(tabella.empty());
    EXPECT_TRUE(std::is_sorted(tabella.begin(), tabella.end(),
                               [](const DefinizioneComando& a, const DefinizioneComando& b) { return a.nome < b.nome; }));
    const std::vector<Schema> tutti = schemi();
    std::set<std::string> nomi;
    for (const auto& c : tabella) {
        EXPECT_TRUE(nomi.insert(c.nome).second) << "comando ripetuto: " << c.nome;
        EXPECT_FALSE(c.descrizione.empty()) << c.nome;
        EXPECT_NE(c.esegui, nullptr) << c.nome;
        for (const auto& a : c.argomenti) {
            EXPECT_TRUE(std::regex_match(a.nome, std::regex("[a-z][a-z-]*"))) << c.nome << ": " << a.nome;
            EXPECT_FALSE(a.descrizione.empty()) << c.nome << ": " << a.nome;
        }
        EXPECT_TRUE(std::is_sorted(c.parametri.begin(), c.parametri.end(),
                                   [](const DefinizioneParametro& a, const DefinizioneParametro& b) {
                                       return a.nome < b.nome;
                                   }))
            << c.nome;
        std::set<std::string> nomi_parametri;
        bool ha_formato = false;
        for (const auto& p : c.parametri) {
            EXPECT_EQ(p.nome.rfind("--", 0), 0u) << p.nome;
            EXPECT_TRUE(nomi_parametri.insert(p.nome).second) << c.nome << ": " << p.nome << " ripetuto";
            EXPECT_NE(p.applica, nullptr) << p.nome;
            EXPECT_FALSE(p.descrizione.empty()) << p.nome;
            if (p.tipo == TipoParametro::Scelta) {
                EXPECT_FALSE(p.valori.empty()) << p.nome;
                EXPECT_TRUE(std::is_sorted(p.valori.begin(), p.valori.end())) << p.nome;
                // Una scelta obbligatoria (es. --permessi, K-010) non ha un valore predefinito: si scrive sempre.
                if (p.obbligatorio)
                    EXPECT_TRUE(p.predefinito.empty()) << p.nome;
                else
                    EXPECT_NE(std::find(p.valori.begin(), p.valori.end(), p.predefinito), p.valori.end()) << p.nome;
            } else {
                EXPECT_TRUE(p.valori.empty()) << p.nome;
                EXPECT_TRUE(p.predefinito.empty()) << p.nome;
            }
            ha_formato = ha_formato || p.nome == kParametroFormato;
        }
        EXPECT_TRUE(ha_formato) << c.nome << ": ogni comando deve accettare --formato";
        EXPECT_TRUE(std::any_of(tutti.begin(), tutti.end(), [&](const Schema& s) {
            return s.nome == c.schema.nome && s.versione == c.schema.versione;
        })) << c.nome;
    }
    EXPECT_EQ(forma_comando(*trova_comando("controlla")), "kzero controlla <cartella> [parametri]");
}

// Il formato dell'errore segue --formato json anche quando la riga di comando e' sbagliata.
TEST(Api, formato_degli_errori) {
    EXPECT_EQ(formato_richiesto({}), Formato::Testo);
    EXPECT_EQ(formato_richiesto({"pianifica", "--formato", "json"}), Formato::Json);
    EXPECT_EQ(formato_richiesto({"--formato=json"}), Formato::Json);
    EXPECT_EQ(formato_richiesto({"versione", "--formato"}), Formato::Testo);
    EXPECT_EQ(formato_richiesto({"versione", "--formato", "xml"}), Formato::Testo);
    EXPECT_EQ(formato_richiesto({"versione", "json"}), Formato::Testo);
}

// Argomenti vuoti (impossibili da passare dalle prove sul programma): errore con codice, uscita 2.
TEST(Api, argomenti_vuoti) {
    const Esito a = esegui({""});
    EXPECT_EQ(a.codice, 2);
    EXPECT_TRUE(a.uscita.empty());
    EXPECT_EQ(a.errori.rfind("Errore K-ERR-002: ", 0), 0u) << a.errori;
    const Esito b = esegui({"versione", ""});
    EXPECT_EQ(b.codice, 2);
    EXPECT_TRUE(b.uscita.empty());
    EXPECT_EQ(b.errori.rfind("Errore K-ERR-004: ", 0), 0u) << b.errori;
    const Esito c = esegui({"controlla", ""});   // cartella vuota: non esiste
    EXPECT_EQ(c.codice, 1);
    EXPECT_TRUE(c.uscita.empty());
    EXPECT_EQ(c.errori.rfind("Errore K-ERR-012: ", 0), 0u) << c.errori;
}

// Errore con posizione: in testo tra parentesi, in JSON nel campo "posizione", solo le parti note.
// Il JSON deve essere identico a tests/esempi/errore_con_posizione.json, che la prova
// schema.esempio.errore_con_posizione confronta con lo schema degli errori.
// (Gli errori del programma oggi non hanno posizione: i problemi nei file di dati sono controlli K-CTR.)
TEST(Api, errore_con_posizione) {
    const ErroreKZero e(Errore::ValoreNonAmmesso, "Valore non ammesso: 'pulmino'.", Posizione{"mezzi.csv", 3, 5});
    EXPECT_EQ(testo_errori({e}, Formato::Testo),
              "Errore K-ERR-006 (mezzi.csv, riga 3, colonna 5): Valore non ammesso: 'pulmino'.\n");

    std::ifstream file(KZERO_CARTELLA_PROVE "/esempi/errore_con_posizione.json", std::ios::binary);
    ASSERT_TRUE(file.good()) << "manca tests/esempi/errore_con_posizione.json";
    std::stringstream atteso;
    atteso << file.rdbuf();
    EXPECT_EQ(testo_errori({e}, Formato::Json), atteso.str());

    const ErroreKZero solo_riga(Errore::ValoreNonAmmesso, "x", Posizione{"", 7, 0});
    EXPECT_EQ(testo_errori({solo_riga}, Formato::Testo), "Errore K-ERR-006 (riga 7): x\n");
    const ErroreKZero senza(Errore::ValoreNonAmmesso, "x", Posizione{});
    EXPECT_EQ(testo_errori({senza}, Formato::Testo), "Errore K-ERR-006: x\n");
    EXPECT_EQ(testo_errori({senza}, Formato::Json).find("posizione"), std::string::npos);
}

// Se il risultato non si puo' scrivere: errore K-ERR-010 e uscita 3, su ogni sistema.
TEST(Api, uscita_non_scrivibile) {
    Esito esito;
    esito.uscita = "risultato\n";
    std::ostringstream uscita, errori;
    uscita.setstate(std::ios::badbit);
    EXPECT_EQ(consegna(esito, uscita, errori), 3);
    EXPECT_EQ(errori.str().rfind("Errore K-ERR-010: ", 0), 0u) << errori.str();
}

// Nelle uscite un orario oltre le 24:00 resta tale: 25:30 non diventa 01:30.
TEST(Api, orario_oltre_mezzanotte) {
    EXPECT_EQ(orario(465), "07:45");
    EXPECT_EQ(orario(1452), "24:12");
    EXPECT_EQ(orario(1530), "25:30");
}

// --- Catalogo dei controlli -------------------------------------------------------------------------------

// Ogni controllo ha un codice K-CTR-nnn unico e consecutivo, un livello, una descrizione e almeno un messaggio;
// i messaggi usano solo segnaposto {nome} chiusi.
TEST(Api, catalogo_controlli) {
    const auto& catalogo = catalogo_controlli();
    std::set<int> visti;
    for (std::size_t i = 0; i < catalogo.size(); ++i) {
        const auto& c = catalogo[i];
        std::string numero = std::to_string(i + 1);
        while (numero.size() < 3) numero = "0" + numero;
        EXPECT_EQ(c.codice, "K-CTR-" + numero);
        EXPECT_TRUE(visti.insert(static_cast<int>(c.controllo)).second) << c.codice << ": controllo ripetuto";
        EXPECT_FALSE(c.descrizione.empty()) << c.codice;
        EXPECT_FALSE(c.messaggi.empty()) << c.codice;
        for (const auto& m : c.messaggi) {
            EXPECT_FALSE(m.empty()) << c.codice;
            // {nome} oppure {nome:forma} (K-011: i giorni come inizi di settimane, mesi o periodi).
            EXPECT_TRUE(std::regex_match(m, std::regex(R"(([^{}]|\{[a-z_]+(:[a-z_]+)?\})*)"))) << c.codice << ": " << m;
            EXPECT_EQ(m.back(), '.') << c.codice << ": " << m;
        }
        EXPECT_NO_THROW(definizione_controllo(c.controllo));
    }
    EXPECT_EQ(definizione_controllo(Check::SchemaViolation).livello, Livello::Errore);
    EXPECT_EQ(definizione_controllo(Check::UnknownFile).livello, Livello::Avviso);
    EXPECT_EQ(definizione_controllo(Check::PlaceWithoutCoordinates).livello, Livello::Avviso);
}

// --- Lettura dei pacchetti: controlli sulla forma dei file ----------------------------------------------

namespace {

// Pacchetto in memoria: una copia del demo in cui si cambia il testo di un file.
class Memoria : public Sorgente {
public:
    Memoria() {
        const PacchettoIncorporato demo("esempi/demo");
        for (const auto& v : demo.voci()) file[v.nome] = demo.leggi(v.nome, kMassimoByteFile).testo;
    }
    std::vector<Voce> voci() const override {
        std::vector<Voce> v;
        for (const auto& [nome, testo] : file) v.push_back({nome, false});
        for (const auto& c : cartelle) v.push_back({c, true});
        std::sort(v.begin(), v.end(), [](const Voce& a, const Voce& b) { return a.nome < b.nome; });
        return v;
    }
    FileLetto leggi(const std::string& nome, std::uintmax_t massimo) const override {
        FileLetto out;
        const auto finto = esiti.find(nome);
        if (finto != esiti.end()) {
            out.esito = finto->second;
            return out;
        }
        const auto it = file.find(nome);
        if (it == file.end() || !it->second) return out;   // illeggibile
        const auto grande = grandi.find(nome);
        if ((grande != grandi.end() && grande->second > massimo) || it->second->size() > massimo) {
            out.esito = FileLetto::Esito::TroppoGrande;
            return out;
        }
        out.esito = FileLetto::Esito::Letto;
        out.testo = *it->second;
        return out;
    }
    // Sostituisce un testo che compare una volta sola nel file.
    void sostituisci(const std::string& nome, const std::string& vecchio, const std::string& nuovo) {
        std::string& t = *file.at(nome);
        const std::size_t i = t.find(vecchio);
        if (i == std::string::npos || t.find(vecchio, i + 1) != std::string::npos)
            throw std::logic_error("testo assente o ripetuto in " + nome + ": " + vecchio);
        t.replace(i, vecchio.size(), nuovo);
    }

    std::map<std::string, std::optional<std::string>> file;   // nessun valore = illeggibile
    std::vector<std::string> cartelle;
    std::map<std::string, std::uintmax_t> grandi;   // dimensione finta (byte) di alcuni file
    std::map<std::string, FileLetto::Esito> esiti;  // esito finto della lettura di alcuni file (es. non normale)
};

struct Atteso {
    Check controllo;
    int variante;
    std::string file;
    std::string puntatore;
};

::testing::AssertionResult problemi(const Memoria& m, const std::vector<Atteso>& attesi) {
    const Lettura l = leggi_pacchetto(m);
    std::string trovati;
    for (const auto& p : l.problemi)
        trovati += "\n  " + definizione_controllo(p.check).codice + "/" + std::to_string(p.variant) + " " + p.where.file +
                   " " + p.where.pointer;
    if (l.problemi.size() != attesi.size()) return ::testing::AssertionFailure() << "problemi trovati:" << trovati;
    for (std::size_t i = 0; i < attesi.size(); ++i) {
        const auto& p = l.problemi[i];
        if (p.check != attesi[i].controllo || p.variant != attesi[i].variante || p.where.file != attesi[i].file ||
            p.where.pointer != attesi[i].puntatore)
            return ::testing::AssertionFailure() << "problema " << i << " diverso; trovati:" << trovati;
    }
    return ::testing::AssertionSuccess();
}

std::string messaggio(const Memoria& m) {
    const Resoconto r = controlla_pacchetto(m, Visibilita{});
    return r.problemi.empty() ? "" : r.problemi.front().messaggio;
}

}  // namespace

TEST(Api, pacchetto_demo_letto) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    EXPECT_TRUE(l.problemi.empty());
    EXPECT_TRUE(l.forma_valida());
    EXPECT_EQ(l.versioni.size(), 24u);   // tutti i file del formato tranne tempi_sosta.json
    for (const char* f : {"calendari.json", "luoghi.json", "persone.json", "committenti.json"}) EXPECT_EQ(l.versioni.at(f), 2) << f;
    // K-012: il tipo di servizio ncc e i servizi occasionali tipo per tipo.
    for (const char* f : {"regolamenti.json", "servizi.json", "mezzi.json", "aziende.json"}) EXPECT_EQ(l.versioni.at(f), 3) << f;
    // K-018: le parole sulla salute del paese (norme 4) e i falsi allarmi (manifesto 3).
    EXPECT_EQ(l.versioni.at("norme.json"), 4);
    EXPECT_EQ(l.versioni.at("pacchetto.json"), 3);
    EXPECT_EQ(l.pacchetto.trips.size(), 29u);   // K-012: le corse del matrimonio e del noleggio privato
}

TEST(Api, K_CTR_001_manca_il_manifesto) {
    Memoria m;
    m.file.erase("pacchetto.json");
    EXPECT_TRUE(problemi(m, {{Check::MissingManifest, 0, "pacchetto.json", ""}}));
    const Resoconto r = controlla_pacchetto(m, Visibilita{});
    EXPECT_FALSE(r.contenuto_controllato);
    EXPECT_EQ(r.errori, 1);
}

TEST(Api, K_CTR_002_file_illeggibile) {
    Memoria m;
    m.file["mezzi.json"] = std::nullopt;
    EXPECT_TRUE(problemi(m, {{Check::UnreadableFile, 0, "mezzi.json", ""}}));
}

TEST(Api, K_CTR_003_json_non_valido) {
    Memoria m;
    m.sostituisci("mezzi.json", "\"posti\": 19,", "\"posti\": 19");   // manca una virgola
    const Lettura l = leggi_pacchetto(m);
    ASSERT_EQ(l.problemi.size(), 1u);
    EXPECT_EQ(l.problemi[0].check, Check::InvalidJson);
    EXPECT_EQ(l.problemi[0].where.file, "mezzi.json");
    EXPECT_EQ(l.problemi[0].where.line, 84);     // la riga dopo la virgola mancante: "alimentazione"...
    EXPECT_EQ(l.problemi[0].where.column, 21);   // ...letto fino in fondo prima di accorgersi dell'errore
    EXPECT_FALSE(l.usato("mezzi.json"));   // mai letto a meta'

    // Un byte zero: la libreria JSON lo prenderebbe per la fine del file e leggerebbe solo la parte prima (K-009).
    Memoria zero;
    zero.file["assenze.json"] = *zero.file.at("assenze.json") + std::string(1, '\0') + "{ \"altro\": 1 }";
    EXPECT_TRUE(problemi(zero, {{Check::InvalidJson, 1, "assenze.json", ""}}));
    const Lettura lz = leggi_pacchetto(zero);
    EXPECT_FALSE(lz.usato("assenze.json"));
    EXPECT_EQ(lz.problemi[0].where.line, 10);   // subito dopo le 9 righe del file
    EXPECT_EQ(lz.problemi[0].where.column, 1);

    // Un numero troppo grande per essere letto: errore nei dati con riga e colonna, mai un errore interno, e il messaggio
    // non ricopia il numero (trovato dal fuzzing, K-009).
    Memoria enorme;
    enorme.sostituisci("mezzi.json", "\"posti\": 19,", "\"posti\": 1e400,");
    EXPECT_TRUE(problemi(enorme, {{Check::InvalidJson, 2, "mezzi.json", ""}}));
    const Lettura le = leggi_pacchetto(enorme);
    EXPECT_EQ(le.problemi[0].where.line, 83);
    EXPECT_EQ(le.problemi[0].where.column, 16);
    EXPECT_EQ(messaggio(enorme), "Il file non e' un JSON valido: alla riga 83, colonna 16 c'e' un numero troppo grande per essere "
                                 "letto (es. 1e400). Il file non e' stato letto.");
    Memoria nel_testo;   // dentro un testo tra virgolette non e' un numero
    nel_testo.sostituisci("mezzi.json", "\"posti\": 19,", "\"nota\": \"9e999\", \"posti\": 1e999,");
    EXPECT_EQ(leggi_pacchetto(nel_testo).problemi[0].where.column, 33);
}

TEST(Api, K_CTR_004_campo_ripetuto) {
    Memoria m;
    m.sostituisci("mezzi.json", "\"targa\": \"AA 102 AA\",", "\"targa\": \"AA 102 AA\", \"targa\": \"AA 999 ZZ\",");
    EXPECT_TRUE(problemi(m, {{Check::DuplicateKey, 0, "mezzi.json", "/mezzi/1/targa"}}));
}

TEST(Api, K_CTR_005_schema_sbagliato) {
    Memoria m;
    m.sostituisci("luoghi.json", "\"schema\": \"luoghi\",", "\"schema\": \"fermate\",");
    EXPECT_TRUE(problemi(m, {{Check::WrongSchema, 1, "luoghi.json", "/schema"}}));
    Memoria n;
    n.sostituisci("luoghi.json", "\"schema\": \"luoghi\",", "");
    EXPECT_TRUE(problemi(n, {{Check::WrongSchema, 0, "luoghi.json", "/schema"}}));
}

// Versione non supportata: file rifiutato con un codice chiaro, mai letto a meta'.
TEST(Api, K_CTR_006_versione_non_supportata) {
    Memoria m;
    m.sostituisci("corse.json", "\"schema_versione\": 1,", "\"schema_versione\": 2,");
    EXPECT_TRUE(problemi(m, {{Check::UnsupportedVersion, 0, "corse.json", "/schema_versione"}}));
    EXPECT_FALSE(leggi_pacchetto(m).usato("corse.json"));
    EXPECT_TRUE(leggi_pacchetto(m).pacchetto.trips.empty());
    EXPECT_EQ(messaggio(m), "Versione dello schema 2 non supportata (versioni lette da questo programma: 1). "
                            "Il file non e' stato letto.");
    Memoria n;
    n.sostituisci("corse.json", "\"schema_versione\": 1,", "\"schema_versione\": \"1\",");
    EXPECT_TRUE(problemi(n, {{Check::UnsupportedVersion, 1, "corse.json", "/schema_versione"}}));
}

// Un file troppo grande non si legge per niente: leggerlo potrebbe esaurire la memoria.
TEST(Api, K_CTR_053_file_troppo_grande) {
    Memoria m;
    m.grandi["corse.json"] = kMassimoMBFile * 1024 * 1024 + 1;
    EXPECT_TRUE(problemi(m, {{Check::FileTooBig, 0, "corse.json", ""}}));
    EXPECT_FALSE(leggi_pacchetto(m).usato("corse.json"));
    EXPECT_EQ(messaggio(m), "Il file e' piu' grande di 256 MB: e' certamente un errore (un file sbagliato o di un altro "
                            "programma) e non e' stato letto.");
    Memoria n;
    n.grandi["corse.json"] = kMassimoMBFile * 1024 * 1024;   // al limite: si legge
    EXPECT_TRUE(problemi(n, {}));
}

namespace {

// Cartella temporanea per le prove sul disco: <temp>/kzero-lettura-<numero>/{pacchetto,fuori}, tolta alla fine.
class CartellaProva {
public:
    CartellaProva() {
        namespace fs = std::filesystem;
        for (int i = 0;; ++i) {
            radice = fs::temp_directory_path() / ("kzero-lettura-" + std::to_string(i));
            std::error_code ec;
            if (fs::create_directory(radice, ec)) break;
        }
        fs::create_directories(pacchetto());
        fs::create_directories(fuori());
        fs::create_directories(pacchetto() / "archivio");
    }
    ~CartellaProva() {
        std::error_code ec;
        std::filesystem::remove_all(radice, ec);
    }
    std::filesystem::path pacchetto() const { return radice / "pacchetto"; }
    std::filesystem::path fuori() const { return radice / "fuori"; }
    static void scrivi(const std::filesystem::path& f, const std::string& testo) {
        std::ofstream o(f, std::ios::binary);
        o << testo;
    }
    std::filesystem::path radice;
};

}  // namespace

// Lettura blindata (K-009): si legge solo un file normale e mai piu' del massimo di byte; un collegamento si segue
// solo se porta a un file normale dentro la cartella del pacchetto.
TEST(Api, lettura_blindata_dal_disco) {
    namespace fs = std::filesystem;
    using E = FileLetto::Esito;
    const CartellaProva c;
    const fs::path p = c.pacchetto();
    CartellaProva::scrivi(p / "mezzi.json", "12345");
    EXPECT_EQ(leggi_dal_disco(p, "mezzi.json", 5).esito, E::Letto);
    EXPECT_EQ(leggi_dal_disco(p, "mezzi.json", 5).testo, "12345");
    EXPECT_EQ(leggi_dal_disco(p, "mezzi.json", 4).esito, E::TroppoGrande);   // un byte oltre il massimo
    CartellaProva::scrivi(p / "vuoto.json", "");
    EXPECT_EQ(leggi_dal_disco(p, "vuoto.json", 5).esito, E::Letto);
    EXPECT_EQ(leggi_dal_disco(p, "non-esiste.json", 5).esito, E::Illeggibile);
    fs::create_directory(p / "persone.json");
    EXPECT_EQ(leggi_dal_disco(p, "persone.json", 5).esito, E::NonNormale);   // una cartella
    // Un file grande piu' del pezzo letto in una volta (64 KB): letto tutto, o fermato al massimo.
    CartellaProva::scrivi(p / "corse.json", std::string(200000, 'x'));
    EXPECT_EQ(leggi_dal_disco(p, "corse.json", 200000).testo.size(), 200000u);
    EXPECT_EQ(leggi_dal_disco(p, "corse.json", 199999).esito, E::TroppoGrande);
#if defined(__unix__)
    // Una "pipe" al posto del file: rifiutata subito, senza restare in attesa di chi scrive.
    ASSERT_EQ(::mkfifo((p / "turni.json").c_str(), 0600), 0);
    EXPECT_EQ(leggi_dal_disco(p, "turni.json", 5).esito, E::NonNormale);
    // Collegamenti: a un file fuori dalla cartella, a /dev/zero, a un file dentro, a nessun file, a una cartella.
    CartellaProva::scrivi(c.fuori() / "luoghi.json", "{}");
    fs::create_symlink(c.fuori() / "luoghi.json", p / "luoghi.json");
    EXPECT_EQ(leggi_dal_disco(p, "luoghi.json", 5).esito, E::FuoriCartella);
    fs::create_symlink("../fuori/luoghi.json", p / "tratti.json");
    EXPECT_EQ(leggi_dal_disco(p, "tratti.json", 5).esito, E::FuoriCartella);
    fs::create_symlink("/dev/zero", p / "rimesse.json");
    EXPECT_EQ(leggi_dal_disco(p, "rimesse.json", 5).esito, E::FuoriCartella);
    CartellaProva::scrivi(p / "archivio" / "fermi-2026.json", "{}");
    fs::create_symlink("archivio/fermi-2026.json", p / "fermi.json");
    EXPECT_EQ(leggi_dal_disco(p, "fermi.json", 5).esito, E::Letto);
    EXPECT_EQ(leggi_dal_disco(p, "fermi.json", 5).testo, "{}");
    fs::create_symlink("archivio/non-c-e.json", p / "blocchi.json");
    EXPECT_EQ(leggi_dal_disco(p, "blocchi.json", 5).esito, E::CollegamentoRotto);
    fs::create_symlink("archivio", p / "norme.json");
    EXPECT_EQ(leggi_dal_disco(p, "norme.json", 5).esito, E::NonNormale);
    // La cartella del pacchetto stessa puo' essere un collegamento (es. una cartella sincronizzata).
    fs::create_directory_symlink(p, c.radice / "collegata");
    EXPECT_EQ(leggi_dal_disco(c.radice / "collegata", "fermi.json", 5).esito, E::Letto);
    EXPECT_EQ(leggi_dal_disco(c.radice / "collegata", "luoghi.json", 5).esito, E::FuoriCartella);
#endif
#if defined(__linux__)
    // Un file che il sistema dice vuoto ma che ha un contenuto (i file di /proc, come un file che cresce mentre lo si
    // legge): la lettura non si fida della dimensione e si ferma comunque un byte dopo il massimo.
    EXPECT_EQ(leggi_dal_disco("/proc/self", "status", 100).esito, E::TroppoGrande);
    const FileLetto stato = leggi_dal_disco("/proc/self", "status", 1024 * 1024);
    EXPECT_EQ(stato.esito, E::Letto);
    EXPECT_GT(stato.testo.size(), 100u);
#endif
}

// Gli esiti della lettura diventano problemi di forma con il loro codice; una cartella con il nome di un file del
// pacchetto e' un errore proprio, senza errori a catena sui codici che quel file avrebbe dato.
TEST(Api, K_CTR_088_089_090_lettura_blindata) {
    Memoria non_normale;
    non_normale.esiti["persone.json"] = FileLetto::Esito::NonNormale;
    EXPECT_TRUE(problemi(non_normale, {{Check::NotRegularFile, 0, "persone.json", ""}}));
    Memoria fuori;
    fuori.esiti["persone.json"] = FileLetto::Esito::FuoriCartella;
    EXPECT_TRUE(problemi(fuori, {{Check::LinkOutsideFolder, 0, "persone.json", ""}}));
    Memoria rotto;
    rotto.esiti["persone.json"] = FileLetto::Esito::CollegamentoRotto;
    EXPECT_TRUE(problemi(rotto, {{Check::UnreadableFile, 1, "persone.json", ""}}));
    Memoria cartella;
    cartella.file.erase("persone.json");
    cartella.cartelle.push_back("persone.json");
    EXPECT_TRUE(problemi(cartella, {{Check::DirectoryInsteadOfFile, 0, "persone.json", ""}}));
    const Resoconto r = controlla_pacchetto(cartella, Visibilita{});
    EXPECT_FALSE(r.contenuto_controllato);
    EXPECT_EQ(r.errori, 1);   // nessun K-CTR-010 sulle persone che mancherebbero
    EXPECT_EQ(r.problemi.front().messaggio, "C'e' una cartella che si chiama persone.json, come un file del pacchetto: il file "
                                            "non e' stato letto. Togliere o rinominare la cartella e mettere al suo posto il file.");
}

// File costruiti apposta per rompere il programma (K-008, sui file nuovi): annidamenti profondissimi, numeri e testi
// enormi, orari e date impossibili, file vuoti. Ogni caso finisce subito con il codice giusto, il file non si usa e
// il programma non si blocca. Un file con il segno BOM di UTF-8 all'inizio si legge (lo scrive il Blocco note).
TEST(Api, file_costruiti_per_rompere) {
    struct Caso {
        std::string nome, file, testo;
        Check atteso;
        std::string puntatore;
    };
    const std::string profondo(300000, '[');
    const std::string chiuso = std::string(100000, '[') + std::string(100000, ']');
    Memoria demo;
    const auto con = [&demo](const std::string& file, const std::string& vecchio, const std::string& nuovo) {
        std::string t = *demo.file.at(file);
        const std::size_t i = t.find(vecchio);
        if (i == std::string::npos) throw std::logic_error("testo assente: " + vecchio);
        return t.replace(i, vecchio.size(), nuovo);
    };
    const std::vector<Caso> casi = {
        {"annidamento aperto", "turni.json", profondo, Check::InvalidJson, ""},
        {"annidamento chiuso", "turni.json", chiuso, Check::WrongSchema, ""},
        {"file vuoto", "rotazioni.json", "", Check::InvalidJson, ""},
        {"numero enorme", "rotazioni.json", con("rotazioni.json", "\"lunghezza\": 14,", "\"lunghezza\": 1e308,"), Check::SchemaViolation,
         "/rotazioni/0/lunghezza"},
        // Trovato dal fuzzing (K-009): un numero oltre il massimo di un numero con decimali era un errore interno.
        {"numero oltre ogni misura", "rotazioni.json", con("rotazioni.json", "\"lunghezza\": 14,", "\"lunghezza\": 1e400,"),
         Check::InvalidJson, ""},
        {"numero piccolissimo", "rotazioni.json", con("rotazioni.json", "\"lunghezza\": 14,", "\"lunghezza\": 1e-400,"),
         Check::SchemaViolation, "/rotazioni/0/lunghezza"},
        {"numero lunghissimo", "rotazioni.json",
         con("rotazioni.json", "\"lunghezza\": 14,", "\"lunghezza\": 123456789012345678901234567890,"), Check::SchemaViolation,
         "/rotazioni/0/lunghezza"},
        {"ciclo troppo lungo", "rotazioni.json", con("rotazioni.json", "\"lunghezza\": 14,", "\"lunghezza\": 1001,"),
         Check::SchemaViolation, "/rotazioni/0/lunghezza"},
        {"ciclo vuoto", "rotazioni.json", con("rotazioni.json", "\"lunghezza\": 14,", "\"lunghezza\": 0,"), Check::SchemaViolation,
         "/rotazioni/0/lunghezza"},
        {"testo enorme", "turni.json", con("turni.json", "\"nome\": \"Primo\",", "\"nome\": \"" + std::string(1000000, 'x') + "\","),
         Check::SchemaViolation, "/turni/0/nome"},
        {"orario impossibile", "turni.json", con("turni.json", "\"dalle\": \"07:15\"", "\"dalle\": \"48:00\""), Check::SchemaViolation,
         "/turni/0/elementi/0/dalle"},
        {"data inesistente", "assegnazioni.json", con("assegnazioni.json", "\"2026-10-19\"", "\"2026-02-30\""), Check::SchemaViolation,
         "/assegnazioni/0/data"},
        {"km negativi", "turni.json", con("turni.json", "\"km_vuoti\": 1.0", "\"km_vuoti\": -1.0"), Check::SchemaViolation,
         "/turni/0/elementi/1/km_vuoti"},
    };
    for (const Caso& c : casi) {
        Memoria m;
        m.file[c.file] = c.testo;
        const Lettura l = leggi_pacchetto(m);
        ASSERT_FALSE(l.problemi.empty()) << c.nome;
        EXPECT_EQ(l.problemi.front().check, c.atteso) << c.nome;
        EXPECT_EQ(l.problemi.front().where.file, c.file) << c.nome;
        if (!c.puntatore.empty()) {
            EXPECT_EQ(l.problemi.front().where.pointer, c.puntatore) << c.nome;
        }
        EXPECT_FALSE(l.usato(c.file)) << c.nome;
        const Resoconto r = controlla_pacchetto(m, Visibilita{});
        EXPECT_FALSE(r.contenuto_controllato) << c.nome;   // con un errore di forma i controlli sul contenuto non partono
        EXPECT_GE(r.errori, 1) << c.nome;
    }
    Memoria bom;
    bom.file["turni.json"] = "\xEF\xBB\xBF" + *bom.file.at("turni.json");
    EXPECT_TRUE(problemi(bom, {}));
}

// Ogni numero e ogni testo dei file di dati (tutte le versioni) ha un massimo di buon senso nello schema: un valore
// enorme (un errore di battitura, un file prodotto male) si ferma alla lettura. Un testo e' limitato da maxLength,
// da un elenco di valori o da un'espressione ancorata senza ripetizioni libere. Eccezione nota del K-007 (versione 1
// del manifesto, non piu' modificabile): il fuso orario, che pero' deve essere un nome della forma Area/Luogo.
TEST(Api, limiti_di_buon_senso_negli_schemi) {
    const std::set<std::string> eccezioni = {"pacchetto/fuso_orario"};
    std::vector<std::string> senza_limite;
    std::function<void(const JsonDati&, const JsonDati&, const std::string&, int)> visita;
    visita = [&](const JsonDati& radice, const JsonDati& n, const std::string& percorso, int profondita) {
        ASSERT_LT(profondita, 60) << percorso;
        if (!n.is_object()) return;
        if (n.contains("$ref")) {
            const std::string ref = n.at("$ref").get<std::string>();
            visita(radice, radice.at("definitions").at(ref.substr(ref.rfind('/') + 1)), percorso, profondita + 1);
        }
        std::vector<std::string> tipi;
        if (n.contains("type")) {
            if (n.at("type").is_string()) tipi.push_back(n.at("type").get<std::string>());
            else
                for (const auto& t : n.at("type")) tipi.push_back(t.get<std::string>());
        }
        const auto ha = [&tipi](const char* t) { return std::find(tipi.begin(), tipi.end(), t) != tipi.end(); };
        const bool elenco = n.contains("enum") || n.contains("const");
        if ((ha("number") || ha("integer")) && !elenco && !n.contains("maximum") && !n.contains("exclusiveMaximum"))
            senza_limite.push_back(percorso + " (numero)");
        if (ha("string") && !elenco && !n.contains("maxLength")) {
            const std::string pat = n.contains("pattern") ? n.at("pattern").get<std::string>() : "";
            const bool ancorato = pat.size() > 1 && pat.front() == '^' && pat.back() == '$' && pat.find('*') == std::string::npos &&
                                  pat.find('+') == std::string::npos;
            if (!ancorato) senza_limite.push_back(percorso + " (testo)");
        }
        for (const auto& [k, v] : n.items()) {
            if (k == "definitions") continue;
            if (k == "properties")
                for (const auto& [nome, figlio] : v.items()) visita(radice, figlio, percorso + "/" + nome, profondita + 1);
            else if (k == "items" || k == "additionalProperties" || k == "not" || k == "if" || k == "then" || k == "else")
                visita(radice, v, percorso, profondita + 1);
            else if (k == "oneOf" || k == "anyOf" || k == "allOf")
                for (const auto& x : v) visita(radice, x, percorso, profondita + 1);
        }
    };
    for (const FileDati& f : file_dati())
        for (int v : f.versioni) {
            const JsonDati* s = SchemiDati::incorporati().schema(f.schema, v);
            ASSERT_NE(s, nullptr) << f.schema;
            senza_limite.clear();
            visita(*s, *s, f.schema, 0);
            for (const std::string& x : senza_limite) {
                const std::string campo = x.substr(0, x.find(' '));
                EXPECT_TRUE(eccezioni.count(campo)) << f.file << " versione " << v << ": " << x << " senza un massimo";
            }
        }
}

// Il rispetto dello schema lo controlla la libreria (valijson); i messaggi diventano italiani.
TEST(Api, K_CTR_007_file_non_conforme_allo_schema) {
    Memoria m;
    m.sostituisci("mezzi.json", "\"categoria\": \"M1\",", "\"categoria\": \"M4\",");
    EXPECT_TRUE(problemi(m, {{Check::SchemaViolation, 0, "mezzi.json", "/mezzi/3/categoria"}}));
    EXPECT_EQ(messaggio(m), "Non rispetta lo schema: valore non ammesso: \"M4\". Valori ammessi: \"M1\", \"M2\", \"M3\". "
                            "Il file non e' stato usato.");
    EXPECT_FALSE(controlla_pacchetto(m, Visibilita{}).contenuto_controllato);

    Memoria orario;
    orario.sostituisci("corse.json", "\"orario\": \"12:05\",", "\"orario\": \"12.05\",");
    EXPECT_TRUE(problemi(orario, {{Check::SchemaViolation, 0, "corse.json", "/corse/3/passaggi/1/orario"}}));
    Memoria data;
    data.sostituisci("calendari.json", "\"2026-12-08\", \"2027-06-02\"", "\"2026-12-08\", \"2027-02-29\"");
    EXPECT_TRUE(problemi(data, {{Check::SchemaViolation, 0, "calendari.json", "/calendari/0/date_tolte/1"}}));
    Memoria campo;
    campo.sostituisci("mezzi.json", "\"posti\": 19,", "\"posti\": 19, \"codice_fiscale\": \"X\",");   // minimizzazione
    EXPECT_TRUE(problemi(campo, {{Check::SchemaViolation, 0, "mezzi.json", "/mezzi/5/codice_fiscale"}}));
    EXPECT_EQ(messaggio(campo), "Non rispetta lo schema: campo non previsto: 'codice_fiscale'. Il file non e' stato usato.");
    Memoria manca;
    manca.sostituisci("luoghi.json", "\"comune\": \"Lagobello\",\n      \"coordinate\": { \"lat\": 46.452,",
                      "\"coordinate\": { \"lat\": 46.452,");
    EXPECT_TRUE(problemi(manca, {{Check::SchemaViolation, 0, "luoghi.json", "/luoghi/8"}}));
    EXPECT_EQ(messaggio(manca), "Non rispetta lo schema: manca il campo obbligatorio 'comune'. Il file non e' stato usato.");
    // Un numero assurdo (errore di battitura) non entra mai nel programma: ogni numero ha un massimo di buon senso.
    Memoria enorme;
    enorme.sostituisci("servizi.json", "{ \"luogo\": \"F-PRADELLO\", \"salgono\": 9 }",
                       "{ \"luogo\": \"F-PRADELLO\", \"salgono\": 3000000000 }");
    EXPECT_TRUE(problemi(enorme, {{Check::SchemaViolation, 0, "servizi.json", "/servizi/0/domanda/gruppi/0/fermate/0/salgono"}}));
    EXPECT_EQ(messaggio(enorme), "Non rispetta lo schema: valore troppo grande: al massimo 10000. Il file non e' stato usato.");
}

TEST(Api, K_CTR_008_file_non_riconosciuto) {
    Memoria m;
    m.file["fermate.json"] = "{}";
    m.cartelle.push_back("vecchi");
    EXPECT_TRUE(problemi(m, {{Check::UnknownFile, 0, "fermate.json", ""}, {Check::UnknownFile, 1, "vecchi", ""}}));
    const Resoconto r = controlla_pacchetto(m, Visibilita{});
    EXPECT_TRUE(r.contenuto_controllato);   // un avviso non ferma i controlli sul contenuto
    EXPECT_EQ(r.errori, 0);
}

namespace {

// Da UTF-8 a UTF-16 (little endian, con il segno iniziale FF FE): come il "Unicode" del Blocco note di Windows.
std::string in_utf16(const std::string& t, bool con_segno = true, bool big_endian = false) {
    std::string out = con_segno ? (big_endian ? std::string("\xFE\xFF") : std::string("\xFF\xFE")) : std::string();
    for (std::size_t i = 0; i < t.size();) {
        const auto b = [&](std::size_t k) { return static_cast<unsigned char>(t[k]); };
        unsigned cp = b(i);
        std::size_t n = 1;
        if (cp >= 0xE0) { cp = ((cp & 0x0Fu) << 12) | ((b(i + 1) & 0x3Fu) << 6) | (b(i + 2) & 0x3Fu); n = 3; }
        else if (cp >= 0xC0) { cp = ((cp & 0x1Fu) << 6) | (b(i + 1) & 0x3Fu); n = 2; }
        const char basso = static_cast<char>(cp & 0xFFu), alto = static_cast<char>(cp >> 8);
        out += big_endian ? std::string{alto, basso} : std::string{basso, alto};
        i += n;
    }
    return out;
}

}  // namespace

// Il JSON e' sempre UTF-8: un file salvato come "ANSI" o come "Unicode" (UTF-16) da' un errore chiaro, con il
// rimedio, invece di un JSON non valido. Il messaggio non riporta il testo del file (potrebbe contenere dati personali).
TEST(Api, K_CTR_048_file_non_in_utf8) {
    const std::string vecchio = "\"nome\": \"Valfonda 4, giovedi'\"";
    Memoria ansi;   // la "i" accentata in Latin-1 (un byte, 0xEC)
    ansi.sostituisci("corse.json", vecchio, "\"nome\": \"Valfonda 4, gioved\xEC\"");
    EXPECT_TRUE(problemi(ansi, {{Check::NotUtf8, 0, "corse.json", ""}}));
    const Lettura l = leggi_pacchetto(ansi);
    EXPECT_EQ(l.problemi[0].where.line, 282);
    EXPECT_EQ(l.problemi[0].where.column, 34);
    EXPECT_FALSE(l.usato("corse.json"));
    EXPECT_EQ(messaggio(ansi), "Il file non e' salvato in UTF-8: alla riga 282, colonna 34 c'e' un carattere scritto in "
                               "un'altra codifica (succede con i file salvati come \"ANSI\" da programmi vecchi). Salvarlo "
                               "di nuovo scegliendo la codifica UTF-8. Il file non e' stato letto.");
    const Resoconto r = controlla_pacchetto(ansi, Visibilita{true, true});
    EXPECT_FALSE(r.contenuto_controllato);
    EXPECT_EQ(r.problemi[0].messaggio.find("gioved"), std::string::npos);

    // Il pacchetto demo salvato in UTF-16, con il segno iniziale o senza, little o big endian.
    Memoria utf16;
    utf16.file["corse.json"] = in_utf16(*utf16.file.at("corse.json"));
    EXPECT_TRUE(problemi(utf16, {{Check::NotUtf8, 1, "corse.json", ""}}));
    EXPECT_EQ(messaggio(utf16).rfind("Il file e' salvato in UTF-16", 0), 0u) << messaggio(utf16);
    Memoria senza_segno;
    senza_segno.file["mezzi.json"] = in_utf16(*senza_segno.file.at("mezzi.json"), false);
    EXPECT_TRUE(problemi(senza_segno, {{Check::NotUtf8, 1, "mezzi.json", ""}}));
    Memoria big;
    big.file["pacchetto.json"] = in_utf16(*big.file.at("pacchetto.json"), false, true);
    EXPECT_TRUE(problemi(big, {{Check::NotUtf8, 1, "pacchetto.json", ""}}));

    // Le lettere accentate scritte in UTF-8 vanno bene, anche con il segno iniziale (BOM) dei programmi di Windows.
    Memoria buono;
    buono.sostituisci("corse.json", vecchio, "\"nome\": \"Valfonda 4, gioved\xC3\xAC\"");
    buono.file["mezzi.json"] = "\xEF\xBB\xBF" + *buono.file.at("mezzi.json");
    EXPECT_TRUE(problemi(buono, {}));
    // Sequenze UTF-8 non valide: troncata, codifica troppo lunga, meta' di una coppia UTF-16.
    for (const std::string& sbagliato : {std::string("\xC3"), std::string("\xC0\xAF"), std::string("\xED\xA0\x80")}) {
        Memoria m;
        m.sostituisci("corse.json", vecchio, "\"nome\": \"Valfonda 4, gioved" + sbagliato + "\"");
        EXPECT_TRUE(problemi(m, {{Check::NotUtf8, 0, "corse.json", ""}}));
    }
}

// Un testo vuoto viola due regole dello schema (troppo corto e senza un carattere visibile): un solo errore.
TEST(Api, testo_vuoto_un_solo_errore) {
    Memoria m;
    m.sostituisci("corse.json", "\"nome\": \"Valfonda 4, giovedi'\"", "\"nome\": \"\"");
    EXPECT_TRUE(problemi(m, {{Check::SchemaViolation, 0, "corse.json", "/corse/9/nome"}}));
    EXPECT_EQ(messaggio(m), "Non rispetta lo schema: testo vuoto. Il file non e' stato usato.");
    Memoria spazi;
    spazi.sostituisci("corse.json", "\"nome\": \"Valfonda 4, giovedi'\"", "\"nome\": \"   \"");
    EXPECT_TRUE(problemi(spazi, {{Check::SchemaViolation, 0, "corse.json", "/corse/9/nome"}}));
    EXPECT_EQ(messaggio(spazi), "Non rispetta lo schema: forma non valida (\"   \"): Testo di una riga, non vuoto (al massimo "
                                "300 caratteri). Il file non e' stato usato.");
}

// Nessun messaggio del catalogo finisce con due punti di fila, neanche quando un valore finisce con il punto.
TEST(Api, messaggi_senza_due_punti) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    for (const Visibilita v : {Visibilita{}, Visibilita{true, true}})
        for (const auto sensibilita : {kzero::Param::Sensitivity::Common, kzero::Param::Sensitivity::Particular}) {
            const Presentatore presentatore(&l.pacchetto, &l.versioni, v);
            for (const DefinizioneControllo& d : catalogo_controlli()) {
                std::vector<std::string> modelli = d.messaggi;
                for (const auto& [variante, modello] : d.personali) modelli.push_back(modello);
                for (std::size_t variante = 0; variante < d.messaggi.size(); ++variante) {
                    kzero::Problem p{d.controllo, static_cast<int>(variante), kzero::Source{}, {}};
                    for (const std::string& modello : modelli)
                        for (std::size_t i = modello.find('{'); i != std::string::npos; i = modello.find('{', i + 1)) {
                            std::string nome = modello.substr(i + 1, modello.find('}', i) - i - 1);
                            nome = nome.substr(0, nome.find(':'));   // {giorni:settimane} (K-011)
                            kzero::Param x = kzero::text_param(nome, "valore.");
                            x.sensitivity = sensibilita;
                            x.hidden = "nascosto.";
                            p.params.push_back(x);
                        }
                    const std::string testo = presentatore.messaggio(p);
                    EXPECT_EQ(testo.find(".."), std::string::npos) << d.codice << ": " << testo;
                    EXPECT_EQ(testo.back(), '.') << d.codice << ": " << testo;
                }
            }
        }
}

// Traduzione dei messaggi della libreria (valijson) in italiano, con i valori attesi presi dallo schema.
TEST(Api, messaggi_dello_schema_in_italiano) {
    const JsonDati sch = JsonDati::parse(R"({"minimum": 1, "maximum": 54, "minLength": 1, "maxLength": 300,
                                            "type": "integer", "minItems": 2})");
    const JsonDati val = JsonDati::parse("7.5");
    EXPECT_EQ(traduci_messaggio("Value type not permitted by 'type' constraint.", &sch, &val),
              "tipo di valore sbagliato: atteso numero intero, trovato numero con decimali");
    EXPECT_EQ(traduci_messaggio("Value type not permitted by 'type' constraint.", &sch, nullptr),
              "tipo di valore sbagliato: atteso numero intero");
    const JsonDati elenco = JsonDati::parse(R"({"enum": ["a", "b"]})");
    const JsonDati c = JsonDati::parse(R"("c")");
    EXPECT_EQ(traduci_messaggio("Failed to match against any enum values.", &elenco, &c),
              "valore non ammesso: \"c\". Valori ammessi: \"a\", \"b\"");
    EXPECT_EQ(traduci_messaggio("Failed to match against any enum values.", &elenco, nullptr),
              "valore non ammesso. Valori ammessi: \"a\", \"b\"");
    EXPECT_EQ(traduci_messaggio("Expected number greater than or equal to 1", &sch, nullptr), "valore troppo piccolo: almeno 1");
    EXPECT_EQ(traduci_messaggio("Expected number less than or equal to 54", &sch, nullptr), "valore troppo grande: al massimo 54");
    EXPECT_EQ(traduci_messaggio("String should be no fewer than 1 characters in length.", &sch, nullptr), "testo vuoto");
    EXPECT_EQ(traduci_messaggio("Array should contain no fewer than 2 elements.", &sch, nullptr),
              "elenco troppo corto: servono almeno 2 elementi");
    EXPECT_EQ(traduci_messaggio("Missing required property 'luogo'.", nullptr, nullptr), "manca il campo obbligatorio 'luogo'");
    EXPECT_EQ(traduci_messaggio("messaggio nuovo della libreria", nullptr, nullptr),
              "non rispetta lo schema (messaggio nuovo della libreria)");
    EXPECT_TRUE(prima_nel_documento("/corse/2", "/corse/10"));
    EXPECT_FALSE(prima_nel_documento("/corse/10", "/corse/2"));
    EXPECT_TRUE(prima_nel_documento("/corse/2", "/corse/2/passaggi"));
}

// --- Classi dei dati -------------------------------------------------------------------------------------

// La classe di un dato viene dalla marcatura dello schema; senza marcatura vale "particolare" (prudenza).
TEST(Api, classi_dei_dati) {
    const SchemiDati& s = SchemiDati::incorporati();
    EXPECT_EQ(s.classe("luoghi", 1, "/luoghi/3/nome"), Classe::Comune);              // fermata pubblica
    EXPECT_EQ(s.classe("luoghi", 1, "/luoghi/3/coordinate/lat"), Classe::Comune);
    EXPECT_EQ(s.classe("luoghi", 1, "/luoghi_privati/0/nome"), Classe::Personale);   // casa di una persona
    EXPECT_EQ(s.classe("luoghi", 1, "/luoghi_privati/0/coordinate/lat"), Classe::Personale);
    EXPECT_EQ(s.classe("luoghi", 1, "/luoghi_privati/0/codice"), Classe::Comune);    // il codice resta
    EXPECT_EQ(s.classe("servizi", 1, "/servizi/3/domanda/gruppi/0/passeggeri/0/nome"), Classe::Personale);
    EXPECT_EQ(s.classe("servizi", 1, "/servizi/3/domanda/gruppi/0/passeggeri/0/motivo"), Classe::Personale);
    EXPECT_EQ(s.classe("servizi", 1, "/servizi/3/domanda/gruppi/0/passeggeri/0/esigenze/carrozzina"), Classe::Particolare);
    EXPECT_EQ(s.classe("servizi", 1, "/servizi/3/domanda/gruppi/0/fermate/0/carrozzine"), Classe::Particolare);
    EXPECT_EQ(s.classe("servizi", 1, "/servizi/3/domanda/gruppi/0/fermate/0/salgono"), Classe::Comune);
    EXPECT_EQ(s.classe("servizi", 1, "/servizi/3/tipo"), Classe::Particolare);       // "disabili" e' un dato sulla salute
    EXPECT_EQ(s.classe("mezzi", 1, "/mezzi/0/targa"), Classe::Comune);
    EXPECT_EQ(s.classe("mezzi", 1, "/mezzi/0/campo_che_non_esiste"), Classe::Particolare);
    EXPECT_EQ(s.classe("schema_sconosciuto", 1, "/x"), Classe::Particolare);
    EXPECT_TRUE(visibile(Classe::Comune, Visibilita{}));
    EXPECT_FALSE(visibile(Classe::Personale, Visibilita{}));
    EXPECT_FALSE(visibile(Classe::Particolare, Visibilita{true, false}));
    EXPECT_TRUE(visibile(Classe::Particolare, Visibilita{false, true}));
}

namespace {

// Tutti i campi foglia di uno schema con la loro classe (marcatura piu' vicina; vuota se manca).
void foglie(const JsonDati& radice, const JsonDati& nodo, const std::string& percorso, const std::string& classe,
            std::map<std::string, std::string>& out, int profondita = 0) {
    ASSERT_LT(profondita, 40) << percorso;
    const JsonDati& n = SchemiDati::risolvi(radice, nodo);
    std::string c = classe;
    if (n.contains("x-kzero-classe")) c = n.at("x-kzero-classe").get<std::string>();
    if (nodo.contains("x-kzero-classe")) c = nodo.at("x-kzero-classe").get<std::string>();   // vale quella accanto al campo
    bool figli = false;
    if (n.contains("properties"))
        for (const auto& [k, v] : n.at("properties").items()) {
            figli = true;
            foglie(radice, v, percorso + "/" + k, c, out, profondita + 1);
        }
    if (n.contains("items") && n.at("items").is_object()) {
        figli = true;
        foglie(radice, n.at("items"), percorso + "/*", c, out, profondita + 1);
    }
    if (!figli) out[percorso] = c;
}

}  // namespace

// K-011 aggiunta 1: nelle forzature e nelle dichiarazioni dell'azienda (modo delle corse) il motivo scritto a mano e chi l'ha
// scritta sono dati personali.
static bool modo_corse_personale(const std::string& p) {
    for (const char* dove : {"servizi", "corse"})
        for (const char* cosa : {"forzatura", "dichiarazione"})
            for (const char* campo : {"motivo", "chi"})
                if (p == std::string("aziende/aziende/*/modo_corse/") + dove + "/*/" + cosa + "/" + campo) return true;
    return false;
}

// Ogni campo dei file di dati ha la sua classe; i dati personali stanno solo nei luoghi privati e nei passeggeri;
// i dati particolari sono proprio quelli elencati qui (cambiarli vuol dire cambiare anche docs/formato-dati.md).
TEST(Api, classi_marcate_negli_schemi) {
    std::set<std::string> personali, particolari;
    for (const FileDati& f : file_dati())
        for (int v : f.versioni) {
            const JsonDati* s = SchemiDati::incorporati().schema(f.schema, v);
            ASSERT_NE(s, nullptr) << f.schema;
            std::map<std::string, std::string> out;
            foglie(*s, *s, "", "", out);
            for (const auto& [percorso, classe] : out) {
                EXPECT_TRUE(classe == "comune" || classe == "personale" || classe == "particolare")
                    << f.file << " " << percorso << ": campo senza classe";
                if (classe == "personale") personali.insert(f.schema + percorso);
                if (classe == "particolare") particolari.insert(f.schema + percorso);
            }
        }
    // Dati personali: luoghi privati, passeggeri, persone (tranne codice, azienda, presa servizio, mezzo abituale,
    // ipotetica e prestiti), assenze (tranne la persona) e le note scritte a mano dei file che dicono chi fa un turno
    // (rotazioni, servizi fissi, assegnazioni del giorno: possono spiegare il motivo di un cambio).
    const std::set<std::string> comuni_delle_persone = {"persone/persone/*/codice", "persone/persone/*/azienda",
                                                        "persone/persone/*/presa_servizio", "persone/persone/*/mezzo_abituale",
                                                        "persone/persone/*/ipotetica", "assenze/assenze/*/persona"};
    for (const std::string& p : personali)
        EXPECT_TRUE(p.rfind("luoghi/luoghi_privati/", 0) == 0 || p.find("/passeggeri/*/") != std::string::npos ||
                    p.rfind("persone/persone/", 0) == 0 || p.rfind("assenze/assenze/", 0) == 0 ||
                    p == "rotazioni/rotazioni/*/nota" || p == "servizi_fissi/servizi_fissi/*/nota" ||
                    p == "assegnazioni/assegnazioni/*/nota" || modo_corse_personale(p) ||
                    p == "committenti/committenti_privati/*/nome" || p == "committenti/committenti_privati/*/nota" ||
                    p.rfind("elenco_utenti/elenchi/*/utenti/*/", 0) == 0)
            << p;
    // K-019: dell'elenco utenti sono personali nome, casa, classe e sezione (D1), la nota dello stato e il testo di una fermata non
    // riconosciuta; nient'altro (codice, scuola, stato, tratte, orari, sblocchi sono comuni).
    std::set<std::string> elenco;
    for (const std::string& p : personali)
        if (p.rfind("elenco_utenti/", 0) == 0) elenco.insert(p);
    const std::string u = "elenco_utenti/elenchi/*/utenti/*/";
    EXPECT_EQ(elenco, (std::set<std::string>{u + "casa/civico", u + "casa/comune", u + "casa/frazione", u + "casa/via", u + "classe",
                                             u + "fermata/testo", u + "nome", u + "sezione", u + "stato/nota"}));
    // K-012: dei committenti privati sono personali nome e nota; codice e codici esterni restano comuni (li sceglie l'utente).
    for (const char* x : {"committenti/committenti_privati/*/nome", "committenti/committenti_privati/*/nota"})
        EXPECT_TRUE(personali.count(x)) << x;
    for (const char* x : {"committenti/committenti_privati/*/codice", "committenti/committenti_privati/*/codici_esterni/*/codice",
                          "committenti/committenti/*/nome", "committenti/committenti/*/nota"})
        EXPECT_FALSE(personali.count(x) || particolari.count(x)) << x;
    for (const char* nota : {"rotazioni/rotazioni/*/nota", "servizi_fissi/servizi_fissi/*/nota", "assegnazioni/assegnazioni/*/nota"})
        EXPECT_TRUE(personali.count(nota)) << nota;
    EXPECT_TRUE(personali.count("servizi/servizi/*/domanda/gruppi/*/passeggeri/*/nome"));
    EXPECT_TRUE(personali.count("servizi/servizi/*/domanda/gruppi/*/passeggeri/*/motivo"));
    for (const char* campo : {"nome", "casa/indirizzo", "ruoli/*", "rapporto/tipo", "contratto_collettivo/codice", "ore/*/previste/settimana",
                              "disponibilita/*/dalle", "costo_orario/*/valore", "abilitazioni/*/tipo", "autorizzazioni/*/scadenza",
                              "competenze/*", "preferenze/*/tipo", "nota"})
        EXPECT_TRUE(personali.count(std::string("persone/persone/*/") + campo)) << campo;
    for (const char* campo : {"tipo", "dal", "al", "dalle", "alle"})
        EXPECT_TRUE(personali.count(std::string("assenze/assenze/*/") + campo)) << campo;
    for (const std::string& c : comuni_delle_persone) EXPECT_FALSE(personali.count(c)) << c;
    EXPECT_FALSE(personali.count("persone/persone/*/prestiti/*/accordo"));
    for (const char* x : {"aziende/aziende/*/modo_corse/servizi/*/forzatura/motivo", "aziende/aziende/*/modo_corse/corse/*/dichiarazione/chi"})
        EXPECT_TRUE(personali.count(x)) << x;
    EXPECT_FALSE(personali.count("aziende/aziende/*/modo_corse/corse/*/forzatura/quando"));
    const std::set<std::string> attesi = {
        "servizi/servizi/*/competenze_richieste/*",   // versione 2: una competenza puo' far capire chi viaggia
        "servizi/servizi/*/tipo",
        "servizi/servizi/*/tipo_descrizione",
        "servizi/servizi/*/regole/pedana_obbligatoria",
        "servizi/servizi/*/domanda/gruppi/*/tipo",
        "servizi/servizi/*/domanda/gruppi/*/tipo_descrizione",
        "servizi/servizi/*/domanda/gruppi/*/fermate/*/carrozzine",
        "servizi/servizi/*/domanda/gruppi/*/fermate/*/ausili_piegati",
        "servizi/servizi/*/domanda/gruppi/*/assistente_richiesto",
        "servizi/servizi/*/domanda/gruppi/*/passeggeri/*/esigenze/carrozzina",
        "servizi/servizi/*/domanda/gruppi/*/passeggeri/*/esigenze/ausili_piegati",
        "servizi/servizi/*/domanda/gruppi/*/passeggeri/*/esigenze/assistente",
        "servizi/servizi/*/domanda/gruppi/*/passeggeri/*/esigenze/altro",
        "turni/turni/*/competenze_richieste/*",   // come per i servizi: una competenza puo' far capire chi viaggia
        // K-011 aggiunta 2: la dichiarazione del modo delle corse (c'e' solo dove l'esclusione e' da confermare: dice il tipo
        // di servizio); motivo e chi, personali, stanno dentro (legati).
        "aziende/aziende/*/modo_corse/servizi/*/dichiarazione/quando",
        "aziende/aziende/*/modo_corse/servizi/*/dichiarazione/fino_al",
        "aziende/aziende/*/modo_corse/corse/*/dichiarazione/quando",
        "aziende/aziende/*/modo_corse/corse/*/dichiarazione/fino_al",
    };
    EXPECT_EQ(particolari, attesi);
}

// --- Dati personali e particolari nei problemi (K-009) -----------------------------------------------------------

// La classe del dato in cui sta una posizione e' la piu' riservata marcata lungo il percorso; di base la posizione si
// ferma all'elemento comune piu' vicino e un'assenza (file di soli dati personali) diventa la persona assente.
TEST(Api, posizioni_e_classi_dei_dati) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    const auto classe = [&l](const std::string& file, const std::string& puntatore) {
        return classe_della_posizione(kzero::Source{file, puntatore, 0, 0}, l);
    };
    EXPECT_EQ(classe("persone.json", "/persone/2"), Classe::Comune);
    EXPECT_EQ(classe("persone.json", "/persone/2/abilitazioni/2"), Classe::Personale);
    EXPECT_EQ(classe("assenze.json", "/assenze/2/persona"), Classe::Personale);   // dentro un'assenza
    EXPECT_EQ(classe("assenze.json", ""), Classe::Personale);
    EXPECT_EQ(classe("servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0/carrozzine"), Classe::Particolare);
    EXPECT_EQ(classe("servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0/salgono"), Classe::Comune);
    EXPECT_EQ(classe("luoghi.json", "/luoghi_privati/0/nome"), Classe::Personale);
    EXPECT_EQ(classe("luoghi.json", "/luoghi_privati/0"), Classe::Comune);
    EXPECT_EQ(classe("mezzi.json", "/mezzi/5/codice_fiscale"), Classe::Comune);   // un campo non previsto: il suo contenitore
    EXPECT_EQ(classe("fermate.json", "/x"), Classe::Comune);                      // non fa parte del formato

    const auto mostra = [&l](const std::string& file, const std::string& puntatore, Visibilita v) {
        return mostra_posizione(kzero::Source{file, puntatore, 0, 0}, l, v);
    };
    const Visibilita base{}, personali{true, false}, tutti{true, true};
    PosizioneMostrata p = mostra("persone.json", "/persone/2/abilitazioni/2", base);
    EXPECT_EQ(p.posizione.file, "persone.json");
    EXPECT_EQ(p.posizione.pointer, "/persone/2");
    EXPECT_EQ(p.ridotta, Classe::Personale);
    p = mostra("persone.json", "/persone/2/abilitazioni/2", personali);
    EXPECT_EQ(p.posizione.pointer, "/persone/2/abilitazioni/2");
    EXPECT_FALSE(p.ridotta);
    p = mostra("assenze.json", "/assenze/2", base);   // P-B, la seconda persona
    EXPECT_EQ(p.posizione.file, "persone.json");
    EXPECT_EQ(p.posizione.pointer, "/persone/1");
    EXPECT_EQ(p.ridotta, Classe::Personale);
    p = mostra("assenze.json", "/assenze/0/tipo", base);   // P-A
    EXPECT_EQ(p.posizione.pointer, "/persone/0");
    p = mostra("assenze.json", "/assenze/2", personali);
    EXPECT_EQ(p.posizione.file, "assenze.json");
    EXPECT_EQ(p.posizione.pointer, "/assenze/2");
    p = mostra("assenze.json", "", base);   // tutto il file: nessuna posizione
    EXPECT_TRUE(p.posizione.file.empty());
    EXPECT_EQ(p.ridotta, Classe::Personale);
    p = mostra("assenze.json", "/assenze/9", base);   // nessuna persona a cui ricondurla: nessuna posizione
    EXPECT_TRUE(p.posizione.file.empty());
    p = mostra("servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0/carrozzine", personali);
    EXPECT_EQ(p.posizione.pointer, "/servizi/3/domanda/gruppi/0/fermate/0");
    EXPECT_EQ(p.ridotta, Classe::Particolare);
    p = mostra("servizi.json", "/servizi/3/domanda/gruppi/0/fermate/0/carrozzine", tutti);
    EXPECT_EQ(p.posizione.pointer, "/servizi/3/domanda/gruppi/0/fermate/0/carrozzine");
    p = mostra("luoghi.json", "/luoghi_privati/0/nome", base);
    EXPECT_EQ(p.posizione.pointer, "/luoghi_privati/0");
    p = mostra("corse.json", "/corse/3/passaggi/2", base);   // dati comuni: invariata
    EXPECT_EQ(p.posizione.pointer, "/corse/3/passaggi/2");
    EXPECT_FALSE(p.ridotta);
    // Un file di soli dati personali non letto (errore di forma): l'assenza non si riconduce a nessuno.
    Memoria rotto;
    rotto.sostituisci("assenze.json", "\"schema_versione\": 1,", "\"schema_versione\": 7,");
    const Lettura lr = leggi_pacchetto(rotto);
    EXPECT_TRUE(mostra_posizione(kzero::Source{"assenze.json", "/assenze/2", 0, 0}, lr, base).posizione.file.empty());
}

// Di base nessuna posizione mostrata entra in un dato personale o particolare, in nessun problema: la prova guarda il
// demo e alcuni pacchetti rotti, su dati comuni, personali e particolari.
TEST(Api, posizioni_mostrate_solo_su_dati_comuni) {
    std::vector<Memoria> casi(5);
    casi[1].sostituisci("assenze.json", "\"tipo\": \"FORMAZIONE\"", "\"tipo\": \"MALATTIA\"");
    casi[2].sostituisci("servizi.json", "\"assistente_richiesto\": true", "\"assistente_richiesto\": false");
    casi[3].sostituisci("persone.json", "\"tipo\": \"CQC-P\", \"scadenza\": \"2027-06-30\"", "\"tipo\": \"CQC-P\", \"scadenza\": \"2026-12-31\"");
    casi[4].file["assenze.json"] = "{ non e' JSON";
    int problemi_visti = 0;
    for (const Memoria& m : casi)
        for (const Visibilita v : {Visibilita{}, Visibilita{true, false}, Visibilita{false, true}}) {
            const Resoconto r = controlla_pacchetto(m, v);
            for (const ProblemaPresentato& p : r.problemi) {
                ++problemi_visti;
                if (p.posizione.file.empty()) continue;
                const Classe c = classe_della_posizione(p.posizione, r.lettura);
                EXPECT_TRUE(visibile(c, v)) << p.codice << " " << p.posizione.file << " " << p.posizione.pointer;
                if (!v.personali) {
                    EXPECT_NE(p.posizione.file, "assenze.json") << p.codice;
                }
            }
        }
    EXPECT_GT(problemi_visti, 30);
}

// Un problema che rivela un dato particolare (es. l'assistente di cui ha bisogno un passeggero) di base non si
// elenca: si conta soltanto, e conta lo stesso negli errori.
TEST(Api, problemi_sui_dati_particolari_contati_e_non_elencati) {
    Memoria m;
    m.sostituisci("servizi.json", "\"assistente_richiesto\": true", "\"assistente_richiesto\": false");
    const Resoconto base = controlla_pacchetto(m, Visibilita{});
    EXPECT_EQ(base.errori, 1);
    EXPECT_EQ(base.errori_riservati, 1);
    // K-013: gli avvisi degli orari dei regimi che dipendono dal tipo di servizio (nel demo R-L138, R-561, R-234) si contano
    // soltanto, gia' nel demo; l'errore nuovo non ne aggiunge.
    const Resoconto demo = controlla_pacchetto(Memoria{}, Visibilita{});
    EXPECT_EQ(demo.avvisi_riservati, 13);
    EXPECT_EQ(base.avvisi_riservati, demo.avvisi_riservati);
    for (const auto& p : base.problemi) EXPECT_NE(p.codice, "K-CTR-047");
    const Resoconto personali = controlla_pacchetto(m, Visibilita{true, false});
    EXPECT_EQ(personali.errori_riservati, 1);   // i dati personali non bastano
    const Resoconto particolari = controlla_pacchetto(m, Visibilita{false, true});
    EXPECT_EQ(particolari.errori, 1);
    EXPECT_EQ(particolari.errori_riservati, 0);
    ASSERT_FALSE(particolari.problemi.empty());
    EXPECT_EQ(particolari.problemi.front().codice, "K-CTR-047");
    EXPECT_EQ(particolari.problemi.front().messaggio,
              "Il passeggero P-D1 ha bisogno dell'assistente a bordo, ma il gruppo G-VAL-3 (Servizio Valfonda 3, andata) non richiede "
              "l'assistente.");
}

// Ogni controllo dichiara cosa rivela in se'; una variante con un messaggio per i dati personali rivela almeno un dato
// personale; le varianti dichiarate esistono. L'elenco di docs/controlli.md coincide con il catalogo.
TEST(Api, controlli_dichiarano_cosa_rivelano) {
    std::map<std::string, std::string> attese;   // "K-CTR-nnn | classe" -> "tutti i casi" / "alcuni casi"
    for (const DefinizioneControllo& d : catalogo_controlli()) {
        for (const auto& [variante, c] : d.rivela_varianti) {
            EXPECT_GE(variante, 0) << d.codice;
            EXPECT_LT(static_cast<std::size_t>(variante), d.messaggi.size()) << d.codice;
            EXPECT_NE(c, d.rivela) << d.codice << ": variante uguale alla dichiarazione del controllo";
        }
        for (const auto& [variante, modello] : d.personali) {
            EXPECT_NE(d.rivela_variante(variante), Classe::Comune) << d.codice << " variante " << variante;
            EXPECT_LT(static_cast<std::size_t>(variante), d.messaggi.size()) << d.codice;
        }
        for (const Classe c : {Classe::Personale, Classe::Particolare}) {
            std::size_t quante = 0;
            for (std::size_t v = 0; v < d.messaggi.size(); ++v) quante += d.rivela_variante(static_cast<int>(v)) == c;
            // K-013: un controllo che puo' dipendere dal tipo di servizio rivela un dato particolare in alcuni casi.
            if (quante == 0 && !(c == Classe::Particolare && d.dal_tipo)) continue;
            attese[d.codice + " | " + (c == Classe::Personale ? "personale" : "particolare")] =
                quante == d.messaggi.size() ? "tutti i casi" : "alcuni casi";
        }
    }
    EXPECT_EQ(definizione_controllo(Check::AssistantNotRequested).rivela, Classe::Particolare);
    EXPECT_EQ(definizione_controllo(Check::CountsDifferFromPassengers).rivela_variante(2), Classe::Particolare);
    EXPECT_EQ(definizione_controllo(Check::CountsDifferFromPassengers).rivela_variante(0), Classe::Comune);

    std::ifstream f(KZERO_CARTELLA_PROVE "/../docs/controlli.md", std::ios::binary);
    ASSERT_TRUE(f.good());
    std::map<std::string, std::string> trovate;
    const std::regex riga(R"(^\| (K-CTR-[0-9]{3}) \| (personale|particolare) \| (tutti i casi|alcuni casi) \|.*)");
    for (std::string r; std::getline(f, r);) {
        if (!r.empty() && r.back() == '\r') r.pop_back();
        std::smatch m;
        if (std::regex_match(r, m, riga)) trovate[m[1].str() + " | " + m[2].str()] = m[3].str();
    }
    std::string differenze;
    for (const auto& [k, v] : attese)
        if (!trovate.count(k) || trovate.at(k) != v) differenze += "\n  nel catalogo, non nel documento: " + k + " | " + v;
    for (const auto& [k, v] : trovate)
        if (!attese.count(k) || attese.at(k) != v) differenze += "\n  nel documento, non nel catalogo: " + k + " | " + v;
    EXPECT_TRUE(differenze.empty()) << differenze;
}

// Luoghi sensibili (K-010, regola nuova): il nome di un passeggero legato a un luogo sensibile (nel demo chi va al centro
// diurno) vale come dato particolare, in ogni messaggio; il luogo si vede sempre. Un luogo "riservato" diceva comunque
// che la destinazione era sensibile (DECISIONI.md).
TEST(Api, luoghi_sensibili_nei_messaggi) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    ASSERT_EQ(l.versioni.at("luoghi.json"), 2);
    const auto messaggio = [&l](const std::string& passeggero, const std::string& luogo, Visibilita v) {
        const Presentatore p(&l.pacchetto, &l.versioni, v);
        return p.messaggio(kzero::Problem{Check::CountsDifferFromPassengers, 3, kzero::Source{},
                                          {kzero::ref_param("passeggero", kzero::Element::Passenger, passeggero),
                                           kzero::ref_param("luogo", kzero::Element::Place, luogo),
                                           kzero::ref_param("gruppo", kzero::Element::Group, "G-VAL-3")}});
    };
    EXPECT_EQ(messaggio("P-D1", "L-CENTRO", Visibilita{true, false}),
              "Il passeggero P-D1 sale o scende a L-CENTRO (Valfonda, Centro diurno Il Faro), che non e' tra le fermate del "
              "gruppo G-VAL-3 (Servizio Valfonda 3, andata).");
    EXPECT_EQ(messaggio("P-D1", "L-CENTRO", Visibilita{}), messaggio("P-D1", "L-CENTRO", Visibilita{true, false}));
    EXPECT_NE(messaggio("P-D1", "L-CENTRO", Visibilita{true, true}).find("P-D1 (Bianchi Luca) sale o scende a L-CENTRO"),
              std::string::npos);
    // Il nome resta nascosto anche in un messaggio che non nomina il centro: il codice non si lega mai al nome.
    EXPECT_EQ(messaggio("P-D1", "L-MEDIE", Visibilita{true, false}).find("Bianchi"), std::string::npos);
    // Un passeggero che non va in un luogo sensibile si vede con i dati personali.
    EXPECT_NE(messaggio("P-A3", "L-MEDIE", Visibilita{true, false}).find("P-A3 (Esposito Sara) sale o scende a L-MEDIE"),
              std::string::npos);
    // Una scuola o un centro in un luogo sensibile si vede; il nome del passeggero no.
    const Presentatore personali(&l.pacchetto, &l.versioni, Visibilita{true, false});
    const std::string con_scuola = personali.messaggio(kzero::Problem{
        Check::WaitOutsideRegulation, 0, kzero::Source{},
        {kzero::ref_param("passeggero", kzero::Element::Passenger, "P-D1"), kzero::ref_param("scuola", kzero::Element::School, "SC-CENTRO"),
         kzero::ref_param("regolamento", kzero::Element::Regulation, "R-TN"), kzero::ref_param("contratto", kzero::Element::Contract, "CT-SCU"),
         kzero::number_param("minuti", 30), kzero::number_param("massimo", 25)}});
    EXPECT_NE(con_scuola.find("SC-CENTRO"), std::string::npos) << con_scuola;
    EXPECT_EQ(con_scuola.find("riservato"), std::string::npos) << con_scuola;
    // Un pacchetto con i luoghi nella versione 1 (senza "sensibile") si legge ancora.
    Memoria v1;
    v1.sostituisci("luoghi.json", "\"schema_versione\": 2,", "\"schema_versione\": 1,");
    v1.sostituisci("luoghi.json", ",\n      \"sensibile\": true", "");
    EXPECT_TRUE(problemi(v1, {}));
}

// Chi e' legato a un luogo sensibile (K-010): calcolato una volta sul pacchetto, dai dati comuni.
TEST(Api, legame_con_i_luoghi_sensibili) {
    const auto legame_di = [](const Memoria& m, const auto& prova) {
        const Lettura l = leggi_pacchetto(m);
        const kzero::Index idx(l.pacchetto);
        prova(LegameSensibile(&idx));
    };
    // Il demo: chi va al centro diurno (gruppi G-VAL-3 e G-VAL-4) e le loro case; non i bambini dell'asilo, non il
    // garage presso la casa di un autista.
    legame_di(Memoria(), [](const LegameSensibile& x) {
        for (const char* q : {"P-D1", "P-D2", "P-D3", "P-D4"}) EXPECT_TRUE(x.passeggero(q)) << q;
        for (const char* q : {"P-A1", "P-A3", "P-A8"}) EXPECT_FALSE(x.passeggero(q)) << q;
        for (const char* c : {"PP-01", "PP-02", "PP-03", "PP-04"}) EXPECT_TRUE(x.casa(c)) << c;
        EXPECT_FALSE(x.casa("PR-01"));
        EXPECT_FALSE(x.casa("L-CENTRO"));
        EXPECT_TRUE(x.legato("passeggero", "P-D1"));
        EXPECT_TRUE(x.legato("casa", "PP-01"));
        EXPECT_FALSE(x.legato("casa", "P-D1"));
        EXPECT_FALSE(x.legato("persona", "P-A"));
    });
    EXPECT_TRUE(LegameSensibile(nullptr).nessuno());
    // Senza luoghi sensibili e senza porta a porta sensibile nessuno e' legato.
    Memoria senza;
    senza.sostituisci("luoghi.json", ",\n      \"sensibile\": true", "");
    senza.sostituisci("pacchetto.json", "\"porta_a_porta_sensibile\": true", "\"porta_a_porta_sensibile\": false");
    legame_di(senza, [](const LegameSensibile& x) { EXPECT_TRUE(x.nessuno()); });
    // Una casa nuova (PP-09) per un bambino dell'asilo: legata solo se il porta a porta e' sensibile.
    Memoria casa_nuova;
    casa_nuova.sostituisci("luoghi.json", "\"luoghi_privati\": [",
                           "\"luoghi_privati\": [\n    { \"codice\": \"PP-09\", \"tipo\": \"porta_a_porta\", \"nome\": \"Casa Rossi\" },");
    casa_nuova.sostituisci("servizi.json", "\"stato\": { \"tipo\": \"attivo\" },\n                \"salita\": { \"luogo\": \"F-SORGENTA\", \"assegnazione\": \"proposta\" },\n                \"discesa\": { \"luogo\": \"L-INFANZIA\", \"assegnazione\": \"proposta\" }\n              },\n              {\n                \"codice\": \"P-A2\"",
                           "\"stato\": { \"tipo\": \"attivo\" },\n                \"salita\": { \"luogo\": \"PP-09\", \"assegnazione\": \"proposta\" },\n                \"discesa\": { \"luogo\": \"L-INFANZIA\", \"assegnazione\": \"proposta\" }\n              },\n              {\n                \"codice\": \"P-A2\"");
    Memoria non_sensibile = casa_nuova;
    non_sensibile.sostituisci("pacchetto.json", "\"porta_a_porta_sensibile\": true", "\"porta_a_porta_sensibile\": false");
    legame_di(casa_nuova, [](const LegameSensibile& x) {
        EXPECT_TRUE(x.casa("PP-09"));
        EXPECT_TRUE(x.passeggero("P-A1"));
    });
    legame_di(non_sensibile, [](const LegameSensibile& x) {
        EXPECT_FALSE(x.casa("PP-09"));
        EXPECT_FALSE(x.passeggero("P-A1"));
        EXPECT_TRUE(x.passeggero("P-D1"));
    });
    // Una casa toccata da una corsa che va al centro, anche senza un passeggero che la nomini.
    Memoria sulla_corsa = non_sensibile;
    sulla_corsa.sostituisci("corse.json", "\"luogo\": \"PP-02\"", "\"luogo\": \"PP-09\"");
    legame_di(sulla_corsa, [](const LegameSensibile& x) {
        EXPECT_TRUE(x.casa("PP-09"));
        EXPECT_TRUE(x.passeggero("P-A1"));   // sale nella casa legata
    });
}

// K-012: i committenti privati seguono la stessa regola dei passeggeri (una sola, LegameSensibile). Nel demo CP-02 noleggia
// per andare al centro diurno (gruppo verso SC-CENTRO, fermata in L-CENTRO): legato; CP-01 (il matrimonio, luoghi normali):
// no; gli enti mai (il nome di un ente e' un dato comune). Poi i casi del mandato sul matrimonio: una sua corsa che passa
// dal centro; un trasbordo con una corsa che va al centro; una casa porta a porta con il porta a porta sensibile (e senza).
TEST(Api, legame_dei_committenti_privati) {
    const Lettura base = leggi_pacchetto(Memoria());
    ASSERT_TRUE(base.forma_valida());
    const auto legame_di = [](const kzero::Package& p, const auto& prova) {
        const kzero::Index idx(p);
        prova(LegameSensibile(&idx));
    };
    legame_di(base.pacchetto, [](const LegameSensibile& x) {
        EXPECT_TRUE(x.committente("CP-02"));
        EXPECT_TRUE(x.legato("committente", "CP-02"));
        EXPECT_FALSE(x.committente("CP-01"));
        for (const char* ente : {"CVA", "TPV", "PROLOCO"}) EXPECT_FALSE(x.committente(ente)) << ente;
        EXPECT_FALSE(x.legato("passeggero", "CP-02"));
    });
    const auto corsa = [](kzero::Package& p, const char* c) -> kzero::Trip& { return kzero::prove::per_codice(p.trips, c); };
    // Una corsa del matrimonio che passa dal centro diurno (in transito), senza nessun gruppo che ci vada.
    kzero::Package passa = base.pacchetto;
    kzero::Passage centro;
    centro.place = kzero::Ref{"L-CENTRO", {}};
    centro.time = corsa(passa, "C-MATRIMONIO-R").passages[1].time;
    centro.transit = true;
    auto& pr = corsa(passa, "C-MATRIMONIO-R").passages;
    pr.insert(pr.begin() + 1, centro);
    legame_di(passa, [](const LegameSensibile& x) { EXPECT_TRUE(x.committente("CP-01")); });
    // Un trasbordo tra la corsa del matrimonio e quella che va al centro (C-VAL-3).
    kzero::Package trasbordo = base.pacchetto;
    kzero::TransferHalf h;
    h.code = "T-PROVA";
    h.group = kzero::Ref{"G-MATRIMONIO-A", {}};
    h.persons = 1;
    corsa(trasbordo, "C-MATRIMONIO-A").passages[1].transfers.push_back(h);
    h.action = kzero::TransferAction::Receive;
    corsa(trasbordo, "C-VAL-3").passages[1].transfers.push_back(h);
    legame_di(trasbordo, [](const LegameSensibile& x) { EXPECT_TRUE(x.committente("CP-01")); });
    // Una casa porta a porta tra le fermate di un gruppo del matrimonio: con il porta a porta sensibile (il demo) e' legato;
    // senza no (la casa e' legata, ma per chi ci abita, non per chi noleggia).
    kzero::Package casa = base.pacchetto;
    kzero::prove::per_codice(casa.services, "S-MATRIMONIO").groups[0].stops[0].place = kzero::Ref{"PP-03", {}};
    legame_di(casa, [](const LegameSensibile& x) { EXPECT_TRUE(x.committente("CP-01")); });
    casa.manifest.door_to_door_sensitive = false;
    legame_di(casa, [](const LegameSensibile& x) {
        EXPECT_FALSE(x.committente("CP-01"));
        EXPECT_TRUE(x.committente("CP-02"));
    });
    // Senza luoghi sensibili e senza porta a porta sensibile nessuno, nemmeno CP-02.
    kzero::Package nessuno = base.pacchetto;
    for (kzero::Place& l : nessuno.places) l.sensitive = false;
    nessuno.manifest.door_to_door_sensitive = false;
    legame_di(nessuno, [](const LegameSensibile& x) { EXPECT_TRUE(x.nessuno()); });
}

// Nei messaggi di kzero controlla la casa di un passeggero legato a un luogo sensibile si vede solo con il codice, se non
// si mostrano i dati particolari; il luogo sensibile si vede.
// K-012: nei messaggi di kzero controlla il nome di un committente privato segue la stessa regola: personale (CP-01, il
// matrimonio) con il permesso dei dati personali; legato (CP-02, il noleggio verso il centro diurno) solo con tutti e due;
// un ente sempre. Il messaggio: K-CTR-019 sul nome del committente privato in un pacchetto "solo numeri".
TEST(Api, committente_privato_nei_messaggi) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    const auto messaggio = [&l](const std::string& committente, Visibilita v) {
        const Presentatore p(&l.pacchetto, &l.versioni, v);
        return p.messaggio(kzero::Problem{Check::NumbersOnlyPersonalData, 8, kzero::Source{},
                                          {kzero::ref_param("committente", kzero::Element::Client, committente)}});
    };
    EXPECT_NE(messaggio("CP-01", Visibilita{true, false}).find("il committente privato CP-01 (Galli Marta) ha il nome"), std::string::npos);
    EXPECT_NE(messaggio("CP-01", Visibilita{}).find("il committente privato CP-01 ha il nome"), std::string::npos);
    EXPECT_EQ(messaggio("CP-01", Visibilita{false, true}).find("Galli Marta"), std::string::npos);
    EXPECT_NE(messaggio("CP-02", Visibilita{true, false}).find("il committente privato CP-02 ha il nome"), std::string::npos);
    EXPECT_EQ(messaggio("CP-02", Visibilita{true, false}).find("Ferri Bruno"), std::string::npos);
    EXPECT_EQ(messaggio("CP-02", Visibilita{false, true}).find("Ferri Bruno"), std::string::npos);
    EXPECT_NE(messaggio("CP-02", Visibilita{true, true}).find("CP-02 (Ferri Bruno)"), std::string::npos);
    EXPECT_NE(messaggio("CVA", Visibilita{}).find("CVA (Consorzio Valle Alta (inventato))"), std::string::npos);
}

TEST(Api, casa_e_luogo_sensibile_nei_messaggi) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    const auto messaggio = [&l](const std::string& da, const std::string& a, Visibilita v) {
        const Presentatore p(&l.pacchetto, &l.versioni, v);
        return p.messaggio(kzero::Problem{Check::TripPartsGap, 0, kzero::Source{},
                                          {kzero::ref_param("corsa", kzero::Element::Trip, "C-VAL-3"),
                                           kzero::ref_param("da", kzero::Element::Place, da), kzero::ref_param("a", kzero::Element::Place, a),
                                           kzero::days_param("giorni", kzero::day_from_ymd(2026, 10, 22))}});
    };
    EXPECT_NE(messaggio("PP-01", "L-CENTRO", Visibilita{true, false}).find("da PP-01 a L-CENTRO (Valfonda, Centro diurno Il Faro),"),
              std::string::npos);
    EXPECT_EQ(messaggio("PP-01", "L-CENTRO", Visibilita{true, false}).find("Casa Bianchi"), std::string::npos);
    EXPECT_EQ(messaggio("PP-01", "L-CENTRO", Visibilita{}), messaggio("PP-01", "L-CENTRO", Visibilita{true, false}));
    EXPECT_NE(messaggio("PP-01", "L-CENTRO", Visibilita{true, true}).find("da PP-01 (Casa Bianchi) a L-CENTRO"), std::string::npos);
    EXPECT_EQ(messaggio("PP-01", "L-MEDIE", Visibilita{true, false}).find("Casa Bianchi"), std::string::npos);
    // Il garage presso la casa di un autista non e' una persona trasportata: si vede con i dati personali.
    EXPECT_NE(messaggio("PR-01", "L-CENTRO", Visibilita{true, false})
                  .find("da PR-01 (Garage presso l'abitazione di un autista) a L-CENTRO (Valfonda, Centro diurno Il Faro)"),
              std::string::npos);
}

// Quali dati personali e particolari contiene un pacchetto, file per file (quanti valori, dalle classi degli schemi):
// serve a dire cosa e' coinvolto se il pacchetto va perso. I file con soli dati comuni non compaiono.
TEST(Api, valori_riservati_di_un_pacchetto) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    ASSERT_TRUE(l.riservati.count("luoghi.json"));
    EXPECT_EQ(l.riservati.at("luoghi.json").personali, 25u);   // 5 case: nome, indirizzo, comune, coordinate...
    EXPECT_EQ(l.riservati.at("luoghi.json").particolari, 0u);
    EXPECT_EQ(l.riservati.at("turni.json").personali, 0u);
    EXPECT_EQ(l.riservati.at("turni.json").particolari, 3u);   // competenze richieste
    EXPECT_EQ(l.riservati.at("assenze.json").personali, 9u);   // 3 assenze: tipo, dal, al
    EXPECT_FALSE(l.riservati.count("mezzi.json"));
    EXPECT_FALSE(l.riservati.count("corse.json"));
    Memoria senza_case;
    senza_case.sostituisci("luoghi.json", "\"luoghi_privati\": [", "\"luoghi_privati_non_previsti\": [");
    EXPECT_FALSE(leggi_pacchetto(senza_case).riservati.count("luoghi.json"));   // file non usato: non si conta
}

// Gli errori K-ERR non nominano mai un file di soli dati personali, in testo e in JSON.
TEST(Api, errori_senza_file_riservati) {
    const ErroreKZero riservato(Errore::ValoreNonAmmesso, "x", Posizione{"assenze.json", 3, 5});
    EXPECT_EQ(testo_errori({riservato}, Formato::Testo), "Errore K-ERR-006: x\n");
    EXPECT_EQ(testo_errori({riservato}, Formato::Json).find("assenze"), std::string::npos);
    const ErroreKZero comune(Errore::ValoreNonAmmesso, "x", Posizione{"mezzi.json", 3, 5});
    EXPECT_EQ(testo_errori({comune}, Formato::Testo), "Errore K-ERR-006 (mezzi.json, riga 3, colonna 5): x\n");
    EXPECT_TRUE(file_riservato("assenze.json"));
    EXPECT_FALSE(file_riservato("persone.json"));
    EXPECT_FALSE(file_riservato("mezzi.csv"));
}

// Filtro delle tre classi nei messaggi: il nome di una fermata pubblica si vede sempre, quello di una casa
// solo con --includi-dati-personali; il tipo di un servizio (es. "disabili") solo con --includi-dati-particolari.
TEST(Api, filtro_delle_classi_nei_messaggi) {
    const Memoria m;
    const Lettura l = leggi_pacchetto(m);
    const Presentatore base(&l.pacchetto, &l.versioni, Visibilita{});
    const Presentatore personali(&l.pacchetto, &l.versioni, Visibilita{true, false});
    EXPECT_EQ(base.riferimento(kzero::Element::Place, "F-PRADELLO"), "F-PRADELLO (Pradello, Piazza)");
    EXPECT_EQ(base.riferimento(kzero::Element::Place, "PP-01"), "PP-01");
    EXPECT_EQ(personali.riferimento(kzero::Element::Place, "PR-01"), "PR-01 (Garage presso l'abitazione di un autista)");
    EXPECT_EQ(base.riferimento(kzero::Element::Passenger, "P-A3"), "P-A3");
    EXPECT_EQ(personali.riferimento(kzero::Element::Passenger, "P-A3"), "P-A3 (Esposito Sara)");
    // Chi va al centro diurno (luogo sensibile): nome e casa solo con i dati particolari (K-010).
    EXPECT_EQ(personali.riferimento(kzero::Element::Place, "PP-01"), "PP-01");
    EXPECT_EQ(personali.riferimento(kzero::Element::Passenger, "P-D1"), "P-D1");
    const Presentatore tutti(&l.pacchetto, &l.versioni, Visibilita{true, true});
    EXPECT_EQ(tutti.riferimento(kzero::Element::Place, "PP-01"), "PP-01 (Casa Bianchi)");
    EXPECT_EQ(tutti.riferimento(kzero::Element::Passenger, "P-D1"), "P-D1 (Bianchi Luca)");
    EXPECT_EQ(base.riferimento(kzero::Element::Vehicle, "ALFA-01"), "ALFA-01 (AA 101 AA)");
    EXPECT_EQ(base.riferimento(kzero::Element::Place, "NON-ESISTE"), "NON-ESISTE");

    // Un problema su un passeggero, con i due filtri.
    Memoria rotto;
    rotto.sostituisci("servizi.json", "\"salita\": { \"luogo\": \"PP-03\", \"assegnazione\": \"proposta\" },",
                      "\"salita\": { \"luogo\": \"PP-02\", \"assegnazione\": \"proposta\" },");
    const Resoconto r0 = controlla_pacchetto(rotto, Visibilita{});
    const Resoconto r1 = controlla_pacchetto(rotto, Visibilita{true, true});
    ASSERT_GE(r0.errori, 1);
    EXPECT_EQ(r0.problemi[0].messaggio, "Gruppo G-VAL-3 (Servizio Valfonda 3, andata), fermata PP-02: le persone che salgono (1) "
                                        "non corrispondono ai passeggeri che salgono qui (2).");
    EXPECT_EQ(r1.problemi[0].messaggio, "Gruppo G-VAL-3 (Servizio Valfonda 3, andata), fermata PP-02 (Casa Verdi): le persone che "
                                        "salgono (1) non corrispondono ai passeggeri che salgono qui (2).");
    for (const auto& p : r0.problemi) {
        EXPECT_EQ(p.messaggio.find("Casa"), std::string::npos) << p.messaggio;
        EXPECT_EQ(p.messaggio.find("Neri"), std::string::npos) << p.messaggio;
    }
}
