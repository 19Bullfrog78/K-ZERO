// K-Zero - prove dell'API in memoria: tabella dei comandi, catalogo degli errori e dei controlli,
// formato degli errori (anche con posizione), argomenti vuoti, guasto in scrittura, lettura dei pacchetti
// (controlli sulla forma dei file), messaggi in italiano, classi dei dati e loro filtro.
// Le prove che lanciano il programma vero stanno in tests/programma.
#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "api.hpp"
#include "comandi.hpp"
#include "controlli_catalogo.hpp"
#include "controllo.hpp"
#include "errori.hpp"
#include "formato_dati.hpp"
#include "lettore.hpp"
#include "parametri.hpp"
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
            EXPECT_TRUE(std::regex_match(m, std::regex(R"(([^{}]|\{[a-z_]+\})*)"))) << c.codice << ": " << m;
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
        for (const auto& v : demo.voci()) file[v.nome] = demo.leggi(v.nome);
    }
    std::vector<Voce> voci() const override {
        std::vector<Voce> v;
        for (const auto& [nome, testo] : file) v.push_back({nome, false});
        for (const auto& c : cartelle) v.push_back({c, true});
        std::sort(v.begin(), v.end(), [](const Voce& a, const Voce& b) { return a.nome < b.nome; });
        return v;
    }
    std::optional<std::string> leggi(const std::string& nome) const override {
        const auto it = file.find(nome);
        return it == file.end() ? std::nullopt : it->second;
    }
    // Sostituisce un testo che compare una volta sola nel file.
    void sostituisci(const std::string& nome, const std::string& vecchio, const std::string& nuovo) {
        std::string& t = *file.at(nome);
        const std::size_t i = t.find(vecchio);
        if (i == std::string::npos || t.find(vecchio, i + 1) != std::string::npos)
            throw std::logic_error("testo assente o ripetuto in " + nome + ": " + vecchio);
        t.replace(i, vecchio.size(), nuovo);
    }

    std::optional<std::uintmax_t> dimensione(const std::string& nome) const override {
        const auto it = grandi.find(nome);
        if (it != grandi.end()) return it->second;
        return std::nullopt;
    }

    std::map<std::string, std::optional<std::string>> file;   // nessun valore = illeggibile
    std::vector<std::string> cartelle;
    std::map<std::string, std::uintmax_t> grandi;   // dimensione finta (byte) di alcuni file
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
    EXPECT_EQ(l.versioni.size(), 19u);   // tutti i file del formato tranne tempi_sosta.json
    for (const char* f : {"calendari.json", "regolamenti.json", "servizi.json", "mezzi.json"}) EXPECT_EQ(l.versioni.at(f), 2) << f;
    EXPECT_EQ(l.pacchetto.trips.size(), 10u);
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
    EXPECT_EQ(l.problemi[0].where.line, 79);     // la riga dopo la virgola mancante: "alimentazione"...
    EXPECT_EQ(l.problemi[0].where.column, 21);   // ...letto fino in fondo prima di accorgersi dell'errore
    EXPECT_FALSE(l.usato("mezzi.json"));   // mai letto a meta'
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
    const std::string vecchio = "\"nome\": \"Centro diurno, giovedi'\"";
    Memoria ansi;   // la "i" accentata in Latin-1 (un byte, 0xEC)
    ansi.sostituisci("corse.json", vecchio, "\"nome\": \"Centro diurno, gioved\xEC\"");
    EXPECT_TRUE(problemi(ansi, {{Check::NotUtf8, 0, "corse.json", ""}}));
    const Lettura l = leggi_pacchetto(ansi);
    EXPECT_EQ(l.problemi[0].where.line, 282);
    EXPECT_EQ(l.problemi[0].where.column, 37);
    EXPECT_FALSE(l.usato("corse.json"));
    EXPECT_EQ(messaggio(ansi), "Il file non e' salvato in UTF-8: alla riga 282, colonna 37 c'e' un carattere scritto in "
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
    buono.sostituisci("corse.json", vecchio, "\"nome\": \"Centro diurno, gioved\xC3\xAC\"");
    buono.file["mezzi.json"] = "\xEF\xBB\xBF" + *buono.file.at("mezzi.json");
    EXPECT_TRUE(problemi(buono, {}));
    // Sequenze UTF-8 non valide: troncata, codifica troppo lunga, meta' di una coppia UTF-16.
    for (const std::string& sbagliato : {std::string("\xC3"), std::string("\xC0\xAF"), std::string("\xED\xA0\x80")}) {
        Memoria m;
        m.sostituisci("corse.json", vecchio, "\"nome\": \"Centro diurno, gioved" + sbagliato + "\"");
        EXPECT_TRUE(problemi(m, {{Check::NotUtf8, 0, "corse.json", ""}}));
    }
}

// Un testo vuoto viola due regole dello schema (troppo corto e senza un carattere visibile): un solo errore.
TEST(Api, testo_vuoto_un_solo_errore) {
    Memoria m;
    m.sostituisci("corse.json", "\"nome\": \"Centro diurno, giovedi'\"", "\"nome\": \"\"");
    EXPECT_TRUE(problemi(m, {{Check::SchemaViolation, 0, "corse.json", "/corse/9/nome"}}));
    EXPECT_EQ(messaggio(m), "Non rispetta lo schema: testo vuoto. Il file non e' stato usato.");
    Memoria spazi;
    spazi.sostituisci("corse.json", "\"nome\": \"Centro diurno, giovedi'\"", "\"nome\": \"   \"");
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
                for (const auto& [variante, modello] : d.riservati) modelli.push_back(modello);
                for (std::size_t variante = 0; variante < d.messaggi.size(); ++variante) {
                    kzero::Problem p{d.controllo, static_cast<int>(variante), kzero::Source{}, {}};
                    for (const std::string& modello : modelli)
                        for (std::size_t i = modello.find('{'); i != std::string::npos; i = modello.find('{', i + 1)) {
                            kzero::Param x = kzero::text_param(modello.substr(i + 1, modello.find('}', i) - i - 1), "valore.");
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
    // ipotetica e prestiti), assenze (tranne la persona) e le note dei file che parlano di persone.
    const std::set<std::string> comuni_delle_persone = {"persone/persone/*/codice", "persone/persone/*/azienda",
                                                        "persone/persone/*/presa_servizio", "persone/persone/*/mezzo_abituale",
                                                        "persone/persone/*/ipotetica", "assenze/assenze/*/persona"};
    for (const std::string& p : personali)
        EXPECT_TRUE(p.rfind("luoghi/luoghi_privati/", 0) == 0 || p.find("/passeggeri/*/") != std::string::npos ||
                    p.rfind("persone/persone/", 0) == 0 || p.rfind("assenze/assenze/", 0) == 0)
            << p;
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
    };
    EXPECT_EQ(particolari, attesi);
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
    EXPECT_EQ(personali.riferimento(kzero::Element::Place, "PP-01"), "PP-01 (Casa Bianchi)");
    EXPECT_EQ(base.riferimento(kzero::Element::Passenger, "P-D1"), "P-D1");
    EXPECT_EQ(personali.riferimento(kzero::Element::Passenger, "P-D1"), "P-D1 (Bianchi Luca)");
    EXPECT_EQ(base.riferimento(kzero::Element::Vehicle, "ALFA-01"), "ALFA-01 (AA 101 AA)");
    EXPECT_EQ(base.riferimento(kzero::Element::Place, "NON-ESISTE"), "NON-ESISTE");

    // Un problema su un passeggero, con i due filtri.
    Memoria rotto;
    rotto.sostituisci("servizi.json", "\"salita\": { \"luogo\": \"PP-03\", \"assegnazione\": \"proposta\" },",
                      "\"salita\": { \"luogo\": \"PP-02\", \"assegnazione\": \"proposta\" },");
    const Resoconto r0 = controlla_pacchetto(rotto, Visibilita{});
    const Resoconto r1 = controlla_pacchetto(rotto, Visibilita{true, true});
    ASSERT_GE(r0.errori, 1);
    EXPECT_EQ(r0.problemi[0].messaggio, "Gruppo G-DIS (Centro diurno, andata), fermata PP-02: le persone che salgono (1) "
                                        "non corrispondono ai passeggeri che salgono qui (2).");
    EXPECT_EQ(r1.problemi[0].messaggio, "Gruppo G-DIS (Centro diurno, andata), fermata PP-02 (Casa Verdi): le persone che "
                                        "salgono (1) non corrispondono ai passeggeri che salgono qui (2).");
    for (const auto& p : r0.problemi) {
        EXPECT_EQ(p.messaggio.find("Casa"), std::string::npos) << p.messaggio;
        EXPECT_EQ(p.messaggio.find("Neri"), std::string::npos) << p.messaggio;
    }
}
