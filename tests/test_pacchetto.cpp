// K-Zero - prove del nucleo su calendari (eccezioni, sospensioni, calendari di base) e persone a bordo
// (gruppi, trasbordi, conteggi non noti), con i dati del pacchetto demo.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "pacchetto.hpp"
#include "pacchetto_demo.hpp"

using namespace kzero;
using namespace kzero::prove;

namespace {

bool vale(const Package& p, const std::string& calendario, const std::string& data) {
    return calendar_active(per_codice(p.calendars, calendario), giorno(data), Index(p));
}

}  // namespace

// Centro diurno: lunedi', mercoledi' e giovedi', con una data aggiunta, una tolta e la chiusura di fine anno.
TEST(Calendari, giorni_eccezioni_e_sospensioni) {
    const Package& p = demo();
    EXPECT_TRUE(vale(p, "CAL-CENTRO", "2026-09-02"));    // mercoledi'
    EXPECT_TRUE(vale(p, "CAL-CENTRO", "2026-09-03"));    // giovedi'
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2026-09-01"));   // martedi'
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2026-09-05"));   // sabato
    EXPECT_TRUE(vale(p, "CAL-CENTRO", "2026-12-22"));    // martedi', data aggiunta
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2026-12-28"));   // lunedi' nella chiusura del centro
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2026-12-24"));   // primo giorno della chiusura
    EXPECT_TRUE(vale(p, "CAL-CENTRO", "2027-01-04"));    // lunedi' dopo la chiusura
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2027-06-02"));   // mercoledi', data tolta
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2027-08-02"));   // lunedi' fuori dal periodo
}

// Le date tolte e le sospensioni prevalgono sulle date aggiunte.
TEST(Calendari, tolte_e_sospensioni_prevalgono) {
    Package p = demo();
    Calendar& c = per_codice(p.calendars, "CAL-CENTRO");
    c.added.push_back({giorno("2027-06-02"), {}});   // aggiunta e tolta
    c.added.push_back({giorno("2026-12-29"), {}});   // aggiunta nella chiusura
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2027-06-02"));
    EXPECT_FALSE(vale(p, "CAL-CENTRO", "2026-12-29"));
}

// Calendario di base: l'infanzia vale nei giorni di scuola escluso il sabato, dal 21 settembre.
TEST(Calendari, calendario_di_base) {
    const Package& p = demo();
    EXPECT_TRUE(vale(p, "CAL-SCUOLA", "2026-09-19"));        // sabato di scuola
    EXPECT_FALSE(vale(p, "CAL-SCUOLA-LV", "2026-09-19"));    // ...ma escluso dal calendario senza sabato
    EXPECT_TRUE(vale(p, "CAL-SCUOLA-LV", "2026-09-14"));     // primo giorno di scuola
    EXPECT_FALSE(vale(p, "CAL-INFANZIA", "2026-09-14"));     // l'infanzia comincia il 21
    EXPECT_TRUE(vale(p, "CAL-INFANZIA", "2026-09-21"));
    EXPECT_FALSE(vale(p, "CAL-INFANZIA", "2026-12-08"));     // festa tolta nel calendario di base
    EXPECT_FALSE(vale(p, "CAL-INFANZIA", "2026-12-23"));     // vacanze di Natale del calendario di base
    EXPECT_FALSE(vale(p, "CAL-INFANZIA", "2027-02-08"));     // carnevale
    EXPECT_TRUE(vale(p, "CAL-INFANZIA", "2027-02-10"));
    const Index idx(p);
    EXPECT_EQ(first_active_day(per_codice(p.calendars, "CAL-INFANZIA"), p.manifest.period, idx), giorno("2026-09-21"));
    EXPECT_EQ(first_active_day(per_codice(p.calendars, "CAL-CENTRO"), p.manifest.period, idx), giorno("2026-09-02"));
}

// Una catena circolare di basi (errore segnalato dai controlli) non blocca il programma: nessun giorno.
TEST(Calendari, base_circolare) {
    Package p = demo();
    per_codice(p.calendars, "CAL-SCUOLA").base = Ref{"CAL-INFANZIA", {}};
    EXPECT_FALSE(vale(p, "CAL-INFANZIA", "2026-09-21"));
    EXPECT_FALSE(first_active_day(per_codice(p.calendars, "CAL-SCUOLA"), p.manifest.period, Index(p)).has_value());
}

// Persone a bordo dopo ogni passaggio, gruppo per gruppo.
TEST(ABordo, gruppi_sulla_corsa) {
    const Index idx(demo());
    const auto v = on_board(per_codice(demo().trips, "C-AM-2"), idx);
    ASSERT_EQ(v.size(), 6u);
    const std::vector<int> totali = {0, 1, 8, 14, 6, 0};
    for (std::size_t i = 0; i < v.size(); ++i) EXPECT_EQ(v[i].total, totali[i]) << "passaggio " << i;
    EXPECT_EQ(v[3].by_group.at("G-AM-MEDIE"), 8);
    EXPECT_EQ(v[3].by_group.at("G-AM-PRIM"), 6);
    EXPECT_EQ(v[4].by_group.at("G-AM-MEDIE"), 0);

    // Corsa ibrida: superiori e medie insieme.
    const auto r = on_board(per_codice(demo().trips, "C-RM-1"), idx);
    ASSERT_EQ(r.size(), 7u);
    EXPECT_EQ(r[1].total, 16);
    EXPECT_EQ(r[1].by_group.at("G-RM-SUP"), 5);
    EXPECT_EQ(r[2].total, 16);   // transito
    EXPECT_EQ(r[3].total, 10);
    EXPECT_EQ(r[6].total, 0);
}

// Trasbordo T-01: la corsa che consegna scende di una persona, quella che riceve sale di una.
// L'assistente non e' un passeggero.
TEST(ABordo, trasbordi) {
    const Index idx(demo());
    const auto consegna = on_board(per_codice(demo().trips, "C-DIS-R"), idx);
    ASSERT_EQ(consegna.size(), 4u);
    EXPECT_EQ(consegna[1].total, 1);
    EXPECT_EQ(consegna[2].total, 0);
    const auto riceve = on_board(per_codice(demo().trips, "C-DIS-1"), idx);
    ASSERT_EQ(riceve.size(), 5u);
    const std::vector<int> totali = {0, 1, 2, 3, 0};
    for (std::size_t i = 0; i < riceve.size(); ++i) EXPECT_EQ(riceve[i].total, totali[i]) << "passaggio " << i;
}

// Le linee hanno conteggi non noti: non contano a bordo.
TEST(ABordo, conteggi_non_noti) {
    const Index idx(demo());
    for (const auto& a : on_board(per_codice(demo().trips, "C-LIN-533"), idx)) {
        EXPECT_EQ(a.total, 0);
        EXPECT_TRUE(a.by_group.empty());
    }
}
