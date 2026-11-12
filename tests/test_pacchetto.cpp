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

// Lunedi', mercoledi' e giovedi', con una data aggiunta, una tolta e la chiusura di fine anno.
TEST(Calendari, giorni_eccezioni_e_sospensioni) {
    const Package& p = demo();
    EXPECT_TRUE(vale(p, "CAL-LMG", "2026-09-02"));    // mercoledi'
    EXPECT_TRUE(vale(p, "CAL-LMG", "2026-09-03"));    // giovedi'
    EXPECT_FALSE(vale(p, "CAL-LMG", "2026-09-01"));   // martedi'
    EXPECT_FALSE(vale(p, "CAL-LMG", "2026-09-05"));   // sabato
    EXPECT_TRUE(vale(p, "CAL-LMG", "2026-12-22"));    // martedi', data aggiunta
    EXPECT_FALSE(vale(p, "CAL-LMG", "2026-12-28"));   // lunedi' nella chiusura del centro
    EXPECT_FALSE(vale(p, "CAL-LMG", "2026-12-24"));   // primo giorno della chiusura
    EXPECT_TRUE(vale(p, "CAL-LMG", "2027-01-04"));    // lunedi' dopo la chiusura
    EXPECT_FALSE(vale(p, "CAL-LMG", "2027-06-02"));   // mercoledi', data tolta
    EXPECT_FALSE(vale(p, "CAL-LMG", "2027-08-02"));   // lunedi' fuori dal periodo
}

// Le date tolte e le sospensioni prevalgono sulle date aggiunte.
TEST(Calendari, tolte_e_sospensioni_prevalgono) {
    Package p = demo();
    Calendar& c = per_codice(p.calendars, "CAL-LMG");
    c.added.push_back({giorno("2027-06-02"), {}});   // aggiunta e tolta
    c.added.push_back({giorno("2026-12-29"), {}});   // aggiunta nella chiusura
    EXPECT_FALSE(vale(p, "CAL-LMG", "2027-06-02"));
    EXPECT_FALSE(vale(p, "CAL-LMG", "2026-12-29"));
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
    EXPECT_EQ(first_active_day(per_codice(p.calendars, "CAL-LMG"), p.manifest.period, idx), giorno("2026-09-02"));
}

// Una catena circolare di basi (errore segnalato dai controlli) non blocca il programma: nessun giorno.
TEST(Calendari, base_circolare) {
    Package p = demo();
    per_codice(p.calendars, "CAL-SCUOLA").base = Ref{"CAL-INFANZIA", {}};
    EXPECT_FALSE(vale(p, "CAL-INFANZIA", "2026-09-21"));
    EXPECT_FALSE(first_active_day(per_codice(p.calendars, "CAL-SCUOLA"), p.manifest.period, Index(p)).has_value());
}

// --- Festivita' e calendari che si combinano (K-008) -------------------------------------------------------

namespace {

// Calendario "festivi" scritto una volta sola: domeniche piu' le festivita' nazionali e il patrono.
Calendar festivi(const Package& p) {
    Calendar c;
    c.code = "CAL-FESTIVI-PROVA";
    c.range = p.manifest.period;
    c.days.add(Weekday::Sunday);
    c.adds = {{CalendarLinkKind::Holidays, Ref{"FEST-IT", {}}}, {CalendarLinkKind::Holidays, Ref{"FEST-VALFONDA", {}}}};
    return c;
}

}  // namespace

// Feriali: da lunedi' a sabato tolte le festivita' (nessuna festa scritta a mano nel calendario).
TEST(Calendari, feriali_tolte_le_festivita) {
    const Package& p = demo();
    EXPECT_TRUE(vale(p, "CAL-FERIALI", "2026-12-07"));    // lunedi'
    EXPECT_FALSE(vale(p, "CAL-FERIALI", "2026-12-08"));   // martedi', Immacolata
    EXPECT_FALSE(vale(p, "CAL-FERIALI", "2027-03-29"));   // lunedi' dell'Angelo: festa mobile, scritta come data
    EXPECT_FALSE(vale(p, "CAL-FERIALI", "2027-05-01"));   // sabato, festa del lavoro
    EXPECT_FALSE(vale(p, "CAL-FERIALI", "2026-12-13"));   // domenica: non e' un giorno del calendario
    EXPECT_TRUE(vale(p, "CAL-FERIALI", "2026-11-11"));    // il patrono di Valfonda non e' tolto da questo calendario
    // La scuola toglie anche il patrono di Valfonda; il calendario senza sabato lo eredita dalla base.
    EXPECT_FALSE(vale(p, "CAL-SCUOLA", "2026-11-11"));
    EXPECT_FALSE(vale(p, "CAL-SCUOLA-LV", "2026-11-11"));
    EXPECT_TRUE(vale(p, "CAL-SCUOLA-LV", "2026-11-12"));
}

// Festivi: domeniche piu' festivita', anche infrasettimanali; le date tolte prevalgono, le aggiunte valgono.
TEST(Calendari, festivi_domeniche_piu_festivita) {
    Package p = demo();
    p.calendars.push_back(festivi(p));
    EXPECT_TRUE(vale(p, "CAL-FESTIVI-PROVA", "2026-12-13"));    // domenica
    EXPECT_TRUE(vale(p, "CAL-FESTIVI-PROVA", "2026-12-08"));    // martedi', Immacolata
    EXPECT_TRUE(vale(p, "CAL-FESTIVI-PROVA", "2026-11-11"));    // mercoledi', patrono
    EXPECT_FALSE(vale(p, "CAL-FESTIVI-PROVA", "2026-12-07"));   // lunedi' feriale
    // Un giorno di festa fuori dal periodo del calendario non vale.
    per_codice(p.calendars, "CAL-FESTIVI-PROVA").range.to = giorno("2026-12-31");
    EXPECT_FALSE(vale(p, "CAL-FESTIVI-PROVA", "2027-01-06"));
    // Data tolta: prevale sulla festa aggiunta. Data aggiunta: prevale sulla festa tolta (feriali).
    per_codice(p.calendars, "CAL-FESTIVI-PROVA").removed.push_back({giorno("2026-12-08"), {}});
    EXPECT_FALSE(vale(p, "CAL-FESTIVI-PROVA", "2026-12-08"));
    per_codice(p.calendars, "CAL-FERIALI").added.push_back({giorno("2026-12-26"), {}});
    EXPECT_TRUE(vale(p, "CAL-FERIALI", "2026-12-26"));
}

// Un calendario puo' aggiungere o togliere i giorni di un altro calendario (non solo delle festivita').
TEST(Calendari, aggiunge_e_toglie_altri_calendari) {
    Package p = demo();
    Calendar c;
    c.code = "CAL-PROVA";
    c.range = p.manifest.period;
    c.days.add(Weekday::Monday);
    c.adds = {{CalendarLinkKind::Calendar, Ref{"CAL-GIO", {}}}};   // piu' i giovedi' del centro
    c.removes = {{CalendarLinkKind::Calendar, Ref{"CAL-SCUOLA", {}}}};    // meno i giorni di scuola
    p.calendars.push_back(c);
    EXPECT_TRUE(vale(p, "CAL-PROVA", "2026-09-07"));    // lunedi' prima della scuola
    EXPECT_FALSE(vale(p, "CAL-PROVA", "2026-09-14"));   // lunedi' di scuola: tolto
    EXPECT_FALSE(vale(p, "CAL-PROVA", "2026-09-17"));   // giovedi' del centro, ma anche di scuola: tolto prevale
    EXPECT_FALSE(vale(p, "CAL-PROVA", "2026-12-31"));   // giovedi' nella chiusura del centro
    EXPECT_TRUE(vale(p, "CAL-PROVA", "2027-06-17"));    // giovedi' del centro dopo la scuola
    EXPECT_FALSE(vale(p, "CAL-PROVA", "2027-06-16"));   // mercoledi'
    // Un calendario delle festivita' inesistente non aggiunge niente (errore segnalato dai controlli).
    c.code = "CAL-PROVA-2";
    c.adds = {{CalendarLinkKind::Holidays, Ref{"FEST-NON-ESISTE", {}}}};
    c.removes.clear();
    p.calendars.push_back(c);
    EXPECT_FALSE(vale(p, "CAL-PROVA-2", "2026-12-08"));
}

// Un calendario che si combina con se stesso (errore K-CTR-049) non blocca il programma.
TEST(Calendari, combinato_con_se_stesso) {
    Package p = demo();
    per_codice(p.calendars, "CAL-FERIALI").adds = {{CalendarLinkKind::Calendar, Ref{"CAL-GIO", {}}}};
    per_codice(p.calendars, "CAL-LMG").removes = {{CalendarLinkKind::Calendar, Ref{"CAL-FERIALI", {}}}};
    EXPECT_NO_THROW(vale(p, "CAL-FERIALI", "2026-12-08"));
    EXPECT_NO_THROW(first_active_day(per_codice(p.calendars, "CAL-LMG"), p.manifest.period, Index(p)));
}

// I giorni calcolati una volta sola coincidono con il calendario, giorno per giorno.
TEST(Calendari, giorni_calcolati_una_volta) {
    Package p = demo();
    p.calendars.push_back(festivi(p));
    const Index idx(p);
    const CalendarDays giorni(idx, p.manifest.period);
    for (const Calendar& c : p.calendars) {
        int n = 0;
        for (Day d = p.manifest.period.from; d <= p.manifest.period.to; ++d) {
            EXPECT_EQ(giorni.active(c.code, d), calendar_active(c, d, idx)) << c.code << " " << text_from_day(d);
            n += calendar_active(c, d, idx) ? 1 : 0;
        }
        EXPECT_EQ(giorni.count(c.code), n) << c.code;
    }
    EXPECT_FALSE(giorni.active("CAL-FERIALI", p.manifest.period.from - 1));   // fuori dal periodo
    EXPECT_FALSE(giorni.active("NON-ESISTE", giorno("2026-10-12")));
    // Domeniche e festivita' del periodo: 52 domeniche, piu' 8 feste nazionali in altri giorni e il patrono.
    EXPECT_EQ(giorni.count("CAL-FESTIVI-PROVA"), 52 + 8 + 1);
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

    // Corsa promiscua: superiori e medie insieme.
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
    const auto consegna = on_board(per_codice(demo().trips, "C-VAL-3R"), idx);
    ASSERT_EQ(consegna.size(), 4u);
    EXPECT_EQ(consegna[1].total, 1);
    EXPECT_EQ(consegna[2].total, 0);
    const auto riceve = on_board(per_codice(demo().trips, "C-VAL-3"), idx);
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
