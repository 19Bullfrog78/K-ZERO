// K-Zero - prove del nucleo sulla normalizzazione dei nomi e sulla ricerca dei luoghi (src/nucleo/ricerca.hpp),
// con i luoghi del pacchetto demo. La tabella delle lettere deve coincidere con quella di docs/formato-dati.md.
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "pacchetto_demo.hpp"
#include "ricerca.hpp"

using namespace kzero;
using namespace kzero::prove;

using Esito = PlaceSearch::Outcome;

// Maiuscole, accenti, punteggiatura e spazi doppi non contano.
TEST(Ricerca, normalizzazione) {
    EXPECT_EQ(normalize_name("Pradello, Piazza"), "PRADELLO PIAZZA");
    EXPECT_EQ(normalize_name("  pradello   PIAZZA  "), "PRADELLO PIAZZA");
    EXPECT_EQ(normalize_name("Pradello - piazza."), "PRADELLO PIAZZA");
    EXPECT_EQ(normalize_name("Pradello P.zza"), "PRADELLO P ZZA");   // un'abbreviazione resta un nome diverso
    EXPECT_EQ(normalize_name("Cagnò"), "CAGNO");
    EXPECT_EQ(normalize_name("Cagno'"), "CAGNO");
    EXPECT_EQ(normalize_name("Sant’Antonio – Chiesa"), "SANT ANTONIO CHIESA");   // apostrofo e trattino tipografici
    EXPECT_EQ(normalize_name("Via 4 Novembre, 2° piano"), "VIA 4 NOVEMBRE 2 PIANO");
    EXPECT_EQ(normalize_name("Café"), "CAFE");   // e + accento staccato
    EXPECT_EQ(normalize_name("﻿Valfonda"), "VALFONDA");   // segno invisibile all'inizio dei file di Windows
    EXPECT_EQ(normalize_name(""), "");
    EXPECT_EQ(normalize_name(" ,.;- "), "");
}

// Lettere di altre lingue: diventano lettere semplici secondo la tabella documentata; le lettere di altri alfabeti
// e i byte che non sono UTF-8 restano come sono, quindi non rendono mai uguali due nomi diversi.
TEST(Ricerca, lettere_di_altre_lingue) {
    EXPECT_EQ(normalize_name("Mühlbach"), "MUHLBACH");          // u con dieresi
    EXPECT_EQ(normalize_name("Ärztehaus"), "ARZTEHAUS");        // A con dieresi
    EXPECT_EQ(normalize_name("Köln"), "KOLN");                  // o con dieresi
    EXPECT_EQ(normalize_name("Straße"), "STRASSE");             // s tedesca
    EXPECT_EQ(normalize_name("STRAẞE"), "STRASSE");             // s tedesca maiuscola
    EXPECT_EQ(normalize_name("España"), "ESPANA");              // n con tilde
    EXPECT_EQ(normalize_name("Façade"), "FACADE");              // c con cediglia
    EXPECT_EQ(normalize_name("Łódź"), "LODZ");        // l con barra, o acuta, z con accento
    EXPECT_EQ(normalize_name("Ærø"), "AERO");              // legatura ae, o barrata
    EXPECT_EQ(normalize_name("Timișoara"), "TIMISOARA");        // s con virgola (rumeno)
    // Come scritto nella documentazione: Straße e Strasse diventano uguali, Köln e Koln anche.
    EXPECT_EQ(normalize_name("Straße"), normalize_name("Strasse"));
    EXPECT_EQ(normalize_name("Köln"), normalize_name("Koln"));
    // Altri alfabeti: restano come sono (maiuscole e minuscole comprese).
    EXPECT_EQ(normalize_name("Αθήνα"), "Αθήνα");   // Atene in greco
    EXPECT_NE(normalize_name("Αθήνα"), normalize_name("Athina"));
    EXPECT_EQ(normalize_name("Москва 1"), "Москва 1");
    // Byte non UTF-8: non si perdono.
    EXPECT_EQ(normalize_name(std::string("a\xFF" "b")), std::string("A\xFF" "B"));
    EXPECT_NE(normalize_name(std::string("a\xFF" "b")), normalize_name("ab"));
}

// Una fermata si trova per nome ufficiale o alternativo, in forma normalizzata; nessuna somiglianza "a occhio".
TEST(Ricerca, per_nome) {
    const PlaceSearch tutti(demo().places);
    const auto trovato = [&](const char* nome, const char* codice) {
        const auto r = tutti.by_name(nome);
        EXPECT_EQ(r.outcome, Esito::Found) << nome;
        EXPECT_EQ(r.codes, std::vector<Code>{codice}) << nome;
    };
    trovato("Pradello, Piazza", "F-PRADELLO");
    trovato("PRADELLO  piazza", "F-PRADELLO");
    trovato("Pradello P.zza", "F-PRADELLO");       // nome alternativo
    trovato("pradello chiesa", "F-PRADELLO");      // nome alternativo
    trovato("Valfonda, Scuola media", "L-MEDIE");
    for (const char* nome : {"Pradelo, Piazza", "Pradello", "Piazza Pradello", "", "Casa Bianchi"}) {
        const auto r = tutti.by_name(nome);
        EXPECT_EQ(r.outcome, Esito::Unknown) << nome;   // "Casa Bianchi" e' un luogo privato: non si cerca per nome
        EXPECT_TRUE(r.codes.empty()) << nome;
    }
    // Solo le fermate: una scuola non e' una fermata.
    const PlaceSearch fermate(demo().places, {PlaceType::Stop});
    EXPECT_EQ(fermate.by_name("Valfonda, Scuola media").outcome, Esito::Unknown);
    EXPECT_EQ(fermate.by_name("Valfonda, Stazione FVV").outcome, Esito::Found);
}

// Due fermate con lo stesso nome dopo la normalizzazione: ambiguo, con tutti i luoghi possibili.
TEST(Ricerca, nome_ambiguo) {
    EXPECT_TRUE(PlaceSearch(demo().places).ambiguous_names().empty());
    Package p = demo();
    per_codice(p.places, "F-CASTELROVO").alt_names.push_back("Pradello - piazza");
    const PlaceSearch s(p.places);
    const auto r = s.by_name("pradello piazza");
    EXPECT_EQ(r.outcome, Esito::Ambiguous);
    EXPECT_EQ(r.codes, (std::vector<Code>{"F-PRADELLO", "F-CASTELROVO"}));
    const auto ambigui = s.ambiguous_names();
    ASSERT_EQ(ambigui.size(), 1u);
    EXPECT_EQ(ambigui[0].first, "PRADELLO PIAZZA");
    // Lo stesso nome due volte nello stesso luogo non e' un'ambiguita'.
    Package q = demo();
    per_codice(q.places, "F-PRADELLO").alt_names.push_back("PRADELLO, PIAZZA");
    EXPECT_EQ(PlaceSearch(q.places).by_name("Pradello Piazza").outcome, Esito::Found);
}

// Codice esterno: sistema, tipo e codice, tutti e tre normalizzati.
TEST(Ricerca, per_codice_esterno) {
    const PlaceSearch s(demo().places);
    auto r = s.by_external_code("Trasporti Provinciali Valfonda", "fermata", "5101");
    EXPECT_EQ(r.outcome, Esito::Found);
    EXPECT_EQ(r.codes, std::vector<Code>{"F-PRADELLO"});
    EXPECT_EQ(s.by_external_code("trasporti provinciali valfonda", "FERMATA", " 5101 ").outcome, Esito::Found);
    EXPECT_EQ(s.by_external_code("Consorzio Valle Alta", "fermata", "5101").outcome, Esito::Unknown);   // altro sistema
    EXPECT_EQ(s.by_external_code("Trasporti Provinciali Valfonda", "linea", "5101").outcome, Esito::Unknown);
    EXPECT_EQ(s.by_external_code("Trasporti Provinciali Valfonda", "fermata", "51").outcome, Esito::Unknown);

    Package p = demo();
    per_codice(p.places, "F-STAZIONE").external[0].code = "5101";
    const PlaceSearch q(p.places);
    r = q.by_external_code("Trasporti Provinciali Valfonda", "fermata", "5101");
    EXPECT_EQ(r.outcome, Esito::Ambiguous);
    EXPECT_EQ(r.codes, (std::vector<Code>{"F-PRADELLO", "F-STAZIONE"}));
    ASSERT_EQ(q.ambiguous_external_codes().size(), 1u);
}

// --- La tabella delle lettere di docs/formato-dati.md coincide con quella del programma ---------------------

namespace {

std::string utf8(char32_t c) {
    std::string s;
    if (c < 0x80) {
        s += static_cast<char>(c);
    } else if (c < 0x800) {
        s += static_cast<char>(0xC0 | (c >> 6));
        s += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        s += static_cast<char>(0xE0 | (c >> 12));
        s += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        s += static_cast<char>(0x80 | (c & 0x3F));
    }
    return s;
}

}  // namespace

TEST(Ricerca, tabella_lettere_documentata) {
    std::ifstream file(KZERO_SORGENTI "/docs/formato-dati.md", std::ios::binary);
    ASSERT_TRUE(file.good()) << "manca docs/formato-dati.md";
    // Righe della tabella: | lettera | U+00E4 | A |
    const std::regex riga(R"(^\| (\S+) \| U\+([0-9A-F]{4}) \| ([A-Z0-9]+) \|$)");
    std::vector<FoldEntry> documentate;
    std::vector<std::string> lettere;
    std::string testo;
    while (std::getline(file, testo)) {
        if (!testo.empty() && testo.back() == '\r') testo.pop_back();
        std::smatch m;
        if (!std::regex_match(testo, m, riga)) continue;
        documentate.push_back({static_cast<char32_t>(std::stoul(m[2].str(), nullptr, 16)), nullptr});
        lettere.push_back(m[1].str());
        lettere.push_back(m[3].str());
    }
    const auto& tabella = fold_table();
    ASSERT_EQ(documentate.size(), tabella.size()) << "docs/formato-dati.md e il programma hanno tabelle diverse";
    for (std::size_t i = 0; i < tabella.size(); ++i) {
        EXPECT_EQ(documentate[i].letter, tabella[i].letter) << "riga " << i + 1;
        EXPECT_EQ(lettere[2 * i], utf8(tabella[i].letter)) << "riga " << i + 1;
        EXPECT_EQ(lettere[2 * i + 1], tabella[i].result) << "riga " << i + 1;
        EXPECT_EQ(normalize_name(utf8(tabella[i].letter)), tabella[i].result) << "riga " << i + 1;
        if (i > 0) {
            EXPECT_LT(tabella[i - 1].letter, tabella[i].letter) << "tabella non in ordine alla riga " << i + 1;
        }
    }
}
