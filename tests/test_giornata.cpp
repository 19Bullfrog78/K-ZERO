// K-Zero - prove del comando "kzero giornata" (K-008) attraverso l'API: la scheda scuola del committente si ricava
// dall'uscita JSON, la vista di una persona comprende i turni che non puo' fare, i dati personali si vedono solo con il
// parametro, i parametri incompatibili sono dichiarati in tutti e due i versi.
#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "api.hpp"
#include "comandi.hpp"

using kzero::api::esegui;
using kzero::api::Esito;
using Json = nlohmann::json;

namespace {

const std::string kDemo = KZERO_SORGENTI "/esempi/demo";

Json giornata(std::vector<std::string> parametri) {
    std::vector<std::string> argomenti = {"giornata", kDemo, "--formato", "json"};
    argomenti.insert(argomenti.end(), parametri.begin(), parametri.end());
    const Esito e = esegui(argomenti);
    EXPECT_EQ(e.codice, 0) << e.errori;
    return Json::parse(e.uscita);
}

// La corsa di un turno del documento, oppure nullptr.
const Json* corsa(const Json& doc, const std::string& codice, const Json** turno = nullptr) {
    for (const Json& t : doc.at("turni"))
        for (const Json& e : t.at("elementi"))
            if (e.at("tipo") == "corsa" && e.at("corsa") == codice) {
                if (turno) *turno = &t;
                return &e;
            }
    return nullptr;
}

}  // namespace

// Prova di accettazione del mandato: dall'uscita si ricavano tutte le voci della scheda scuola del CTA per il servizio
// scolastico del demo (andata del mattino, tre giri): servizio, mezzo con targa e posti, autista, giorni, orari, carichi
// e scarichi, km per tratto e totali, giorni di servizio nel periodo.
TEST(Giornata, scheda_scuola_del_committente) {
    const Json doc = giornata({"--data", "2026-10-15"});
    struct Atteso {
        std::string corsa, autista, targa;
        int posti;
        std::vector<std::string> orari;
        int salgono, scendono;
        double km_pieni, km_vuoti;
    };
    const std::vector<Atteso> attesi = {
        {"C-AM-1", "P-E", "AA 101 AA", 16, {"06:55", "07:10", "07:18", "07:35"}, 16, 16, 12.0, 6.4},
        {"C-AM-2", "P-F", "AA 102 AA", 16, {"06:50", "07:12", "07:20", "07:28", "07:38", "07:42"}, 14, 14, 15.5, 8.0},
        {"C-AM-3", "P-G", "AA 106 AA", 14, {"07:20", "07:38", "07:52"}, 8, 8, 7.9, 7.5},
    };
    for (const Atteso& a : attesi) {
        const Json* turno = nullptr;
        const Json* c = corsa(doc, a.corsa, &turno);
        ASSERT_NE(c, nullptr) << a.corsa;
        // Servizio, autista, mezzo con targa e posti.
        ASSERT_EQ(c->at("servizi").size(), 1u);
        EXPECT_EQ(c->at("servizi")[0].at("codice"), "S-SCU-AM");
        EXPECT_EQ(turno->at("persona").at("codice"), a.autista) << a.corsa;
        EXPECT_EQ(turno->at("mezzo").at("targa"), a.targa) << a.corsa;
        EXPECT_EQ(turno->at("mezzo").at("posti"), a.posti) << a.corsa;
        // Giorni (calendario) e giorni di servizio nel periodo.
        EXPECT_EQ(c->at("calendario").at("codice"), "CAL-SCUOLA-LV");
        EXPECT_EQ(c->at("calendario").at("giorni_nel_periodo"), 175);
        // Orari, carichi e scarichi, km per tratto e totali.
        const Json& ps = c->at("passaggi");
        ASSERT_EQ(ps.size(), a.orari.size()) << a.corsa;
        int salgono = 0, scendono = 0;
        double pieni = 0, vuoti = 0;
        for (std::size_t i = 0; i < ps.size(); ++i) {
            EXPECT_EQ(ps[i].at("orario"), a.orari[i]) << a.corsa;
            salgono += ps[i].at("salgono").get<int>();
            scendono += ps[i].at("scendono").get<int>();
            pieni += ps[i].at("km_pieni").get<double>();
            vuoti += ps[i].at("km_vuoti").get<double>();
        }
        EXPECT_EQ(salgono, a.salgono) << a.corsa;
        EXPECT_EQ(scendono, a.scendono) << a.corsa;
        EXPECT_NEAR(pieni, a.km_pieni, 1e-9) << a.corsa;
        EXPECT_NEAR(vuoti, a.km_vuoti, 1e-9) << a.corsa;
        EXPECT_NEAR(c->at("km_pieni").get<double>(), a.km_pieni, 1e-9) << a.corsa;
        EXPECT_NEAR(c->at("km_vuoti").get<double>(), a.km_vuoti, 1e-9) << a.corsa;
    }
    // Il primo giro: carico a Pradello (9) e a Castelrovo (7), scarico alla media (16).
    const Json& ps = corsa(doc, "C-AM-1")->at("passaggi");
    EXPECT_EQ(ps[1].at("luogo"), "F-PRADELLO");
    EXPECT_EQ(ps[1].at("salgono"), 9);
    EXPECT_EQ(ps[1].at("km_vuoti"), 6.4);
    EXPECT_EQ(ps[3].at("luogo"), "L-MEDIE");
    EXPECT_EQ(ps[3].at("scendono"), 16);
}

// Cambio dell'autista a meta' corsa: due turni, ognuno con il suo pezzo; i km del pezzo sono solo i suoi.
TEST(Giornata, pezzi_di_corsa) {
    const Json doc = giornata({"--data", "2026-10-15", "--mezzo", "ALFA-05"});
    int pezzi = 0;
    double pieni = 0;
    for (const Json& t : doc.at("turni"))
        for (const Json& e : t.at("elementi"))
            if (e.at("tipo") == "corsa" && e.at("corsa") == "C-LIN-549") {
                ++pezzi;
                EXPECT_FALSE(e.at("intera").get<bool>());
                pieni += e.at("km_pieni").get<double>();
                EXPECT_EQ(e.at("passaggi")[0].at("km_pieni"), 0.0);   // il tratto prima del cambio e' dell'altro turno
            }
    EXPECT_EQ(pezzi, 2);
    EXPECT_NEAR(pieni, 15.1, 1e-9);   // i due pezzi insieme fanno la corsa intera
}

// La vista di una persona comprende il turno che quel giorno non puo' fare (scoperto), con la persona prevista.
// Il motivo (un'assenza) e' un dato personale: si dice solo con --includi-dati-personali.
TEST(Giornata, persona_assente_e_dati_personali) {
    const Json base = giornata({"--data", "2026-11-20", "--persona", "P-B"});
    ASSERT_EQ(base.at("turni").size(), 1u);
    const Json& t = base.at("turni")[0];
    EXPECT_EQ(t.at("codice"), "T-PRIMO");
    EXPECT_TRUE(t.at("scoperto").get<bool>());
    EXPECT_TRUE(t.at("persona").is_null());
    EXPECT_EQ(t.at("persona_prevista").at("codice"), "P-B");
    EXPECT_EQ(t.at("assegnazione").at("modo"), "rotazione");
    EXPECT_EQ(t.at("assegnazione").at("passo"), 12);
    EXPECT_EQ(base.at("persona").at("assenze")[0].at("tipo"), "riservato");
    const std::string testo_base = base.dump();
    for (const char* x : {"FORMAZIONE", "Bianco Anna"}) EXPECT_EQ(testo_base.find(x), std::string::npos) << x;
    const Json tutti = giornata({"--data", "2026-11-20", "--persona", "P-B", "--includi-dati-personali"});
    EXPECT_EQ(tutti.at("persona").at("nome"), "Bianco Anna");
    EXPECT_EQ(tutti.at("persona").at("assenze")[0].at("tipo"), "FORMAZIONE");
    EXPECT_NE(tutti.at("turni")[0].at("motivo_scoperto").get<std::string>().find("FORMAZIONE"), std::string::npos);
    EXPECT_EQ(base.at("totali").at("scoperti"), 1);
}

// La giornata di un mezzo in revisione: il suo turno va con il mezzo di scorta (assegnazione del singolo giorno).
TEST(Giornata, mezzo_in_revisione) {
    const Json doc = giornata({"--data", "2026-10-19", "--mezzo", "ALFA-01"});
    ASSERT_EQ(doc.at("mezzo").at("fermi").size(), 1u);
    EXPECT_EQ(doc.at("mezzo").at("fermi")[0].at("tipo"), "REVISIONE");
    ASSERT_EQ(doc.at("turni").size(), 1u);
    const Json& t = doc.at("turni")[0];
    EXPECT_EQ(t.at("mezzo").at("codice"), "ALFA-07");
    EXPECT_EQ(t.at("mezzo_da"), "giorno");
    EXPECT_EQ(t.at("mezzo_del_turno"), "ALFA-01");
    EXPECT_EQ(doc.at("totali").at("km_pieni"), 0.0);   // ALFA-01 quel giorno non viaggia
}

// Festivo: l'orario festivo della linea di Beta, la festivita' con il suo calendario.
TEST(Giornata, giorno_festivo) {
    const Json doc = giornata({"--data", "2026-12-08"});
    ASSERT_EQ(doc.at("festivita").size(), 1u);
    EXPECT_EQ(doc.at("festivita")[0].at("calendario"), "FEST-IT");
    ASSERT_EQ(doc.at("turni").size(), 1u);
    EXPECT_EQ(doc.at("turni")[0].at("codice"), "T-B-FESTIVO");
    EXPECT_EQ(doc.at("turni")[0].at("persona").at("codice"), "P-B3");
}

// Nella tabella dei comandi l'incompatibilita' tra due parametri e' scritta in tutti e due i versi (descrivi la mostra
// per ognuno), e solo tra parametri dello stesso comando.
TEST(Giornata, parametri_incompatibili_simmetrici) {
    for (const auto& c : kzero::api::comandi())
        for (const auto& p : c.parametri)
            for (const std::string& altro : p.incompatibili) {
                const kzero::api::DefinizioneParametro* q = nullptr;
                for (const auto& x : c.parametri)
                    if (x.nome == altro) q = &x;
                ASSERT_NE(q, nullptr) << c.nome << " " << p.nome << " " << altro;
                EXPECT_NE(std::find(q->incompatibili.begin(), q->incompatibili.end(), p.nome), q->incompatibili.end())
                    << c.nome << " " << altro << " non dichiara " << p.nome;
            }
}

// K-009, aggiunta 1: con i dati personali e senza quelli particolari, una giornata che mostra la casa di un passeggero
// (punto porta a porta con il nome) non mostra il luogo sensibile, ne' il codice ne' il nome, in nessun punto: corse,
// pezzi di corsa, attivita', inizio e fine dei turni. Nelle tre viste: tutti, il mezzo, la persona. Con i dati
// particolari si vede; di base anche (la casa si vede solo con il codice).
TEST(Giornata, case_e_luoghi_sensibili) {
    const std::vector<std::vector<std::string>> viste = {{}, {"--mezzo", "ALFA-03"}, {"--persona", "P-C"}};
    for (const auto& vista : viste) {
        std::vector<std::string> p = {"--data", "2026-10-22"};
        p.insert(p.end(), vista.begin(), vista.end());
        const std::string quale = vista.empty() ? "tutti" : vista[1];
        std::vector<std::string> personali = p;
        personali.push_back("--includi-dati-personali");
        const Json doc = giornata(personali);
        const std::string testo = doc.dump();
        EXPECT_NE(testo.find("Casa Bianchi"), std::string::npos) << quale;
        EXPECT_EQ(testo.find("\"L-CENTRO\""), std::string::npos) << quale;   // "CAL-CENTRO" e' un calendario
        EXPECT_EQ(testo.find("Valfonda, Centro diurno"), std::string::npos) << quale;
        // La corsa arriva a "riservato" e il trasferimento a vuoto parte da li'.
        const Json* turno = nullptr;
        const Json* c = corsa(doc, "C-DIS-1", &turno);
        ASSERT_NE(c, nullptr) << quale;
        EXPECT_EQ(c->at("passaggi").back().at("luogo"), "riservato") << quale;
        EXPECT_EQ(c->at("passaggi").back().at("nome_luogo"), "riservato") << quale;
        bool trasferimento = false;
        for (const Json& e : turno->at("elementi"))
            if (e.at("tipo") == "attivita" && e.at("attivita") == "VUOTO") {
                trasferimento = true;
                EXPECT_EQ(e.at("luogo").at("codice"), "riservato") << quale;
                EXPECT_EQ(e.at("luogo").at("nome"), "riservato") << quale;
            }
        EXPECT_TRUE(trasferimento) << quale;
        // Con i dati particolari, e di base, il centro si vede.
        std::vector<std::string> tutti_i_dati = personali;
        tutti_i_dati.push_back("--includi-dati-particolari");
        EXPECT_NE(giornata(tutti_i_dati).dump().find("Valfonda, Centro diurno Il Faro"), std::string::npos) << quale;
        const std::string base = giornata(p).dump();
        EXPECT_NE(base.find("\"L-CENTRO\""), std::string::npos) << quale;
        EXPECT_EQ(base.find("Casa Bianchi"), std::string::npos) << quale;
    }
    // Una corsa con una casa e nessun luogo sensibile resta com'e': il ramo di Montebrino dell'auto.
    const Json auto_elettrica = giornata({"--data", "2026-10-22", "--mezzo", "ALFA-04", "--includi-dati-personali"});
    const Json* ramo = corsa(auto_elettrica, "C-DIS-R");
    ASSERT_NE(ramo, nullptr);
    std::vector<std::string> luoghi;
    for (const Json& ps : ramo->at("passaggi")) luoghi.push_back(ps.at("luogo").get<std::string>());
    EXPECT_EQ(luoghi, (std::vector<std::string>{"L-RIMESSA-ALFA", "PP-03", "L-TRASB-PONTE", "L-RIMESSA-ALFA"}));
    EXPECT_EQ(ramo->at("passaggi")[1].at("nome_luogo"), "Casa Neri");
}
