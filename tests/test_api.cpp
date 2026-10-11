// K-Zero - prove dell'API in memoria: tabella dei comandi, catalogo degli errori,
// formato degli errori (anche con posizione), argomenti vuoti, guasto in scrittura.
// Le prove che lanciano il programma vero stanno in tests/programma.
#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "api.hpp"
#include "comandi.hpp"
#include "errori.hpp"
#include "parametri.hpp"

using namespace kzero::api;

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
                     Errore::UscitaNonScrivibile})
        EXPECT_NO_THROW(definizione_errore(e));
}

// La tabella dei comandi e' coerente: nomi unici e in ordine, ogni parametro con "--",
// le scelte con valori in ordine e predefinito ammesso, --formato in ogni comando,
// lo schema di ogni comando nell'elenco degli schemi.
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
}

// Errore con posizione: in testo tra parentesi, in JSON nel campo "posizione", solo le parti note.
// Il JSON deve essere identico a tests/esempi/errore_con_posizione.json, che la prova
// schema.esempio.errore_con_posizione confronta con lo schema degli errori.
// (Nessun comando di oggi legge file: le posizioni serviranno con i dati, K-007.)
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
