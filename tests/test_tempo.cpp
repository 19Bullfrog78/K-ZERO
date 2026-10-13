// K-Zero - prove del nucleo su date e orari (src/nucleo/tempo.hpp), anche oltre le 24:00.
#include <string>

#include <gtest/gtest.h>

#include "tempo.hpp"

using namespace kzero;

// Come in GTFS un orario puo' superare le 24:00 e resta tale anche in uscita: 25:30 non diventa 01:30.
TEST(Tempo, orari_oltre_mezzanotte) {
    EXPECT_EQ(minutes_from_text("00:00"), 0);
    EXPECT_EQ(minutes_from_text("07:45"), 465);
    EXPECT_EQ(minutes_from_text("23:59"), 1439);
    EXPECT_EQ(minutes_from_text("24:00"), 1440);
    EXPECT_EQ(minutes_from_text("24:12"), 1452);
    EXPECT_EQ(minutes_from_text("25:30"), 1530);
    EXPECT_EQ(minutes_from_text("100:05"), 6005);   // tre cifre per le ore: ammesse dal nucleo

    EXPECT_EQ(text_from_minutes(0), "00:00");
    EXPECT_EQ(text_from_minutes(465), "07:45");
    EXPECT_EQ(text_from_minutes(1440), "24:00");
    EXPECT_EQ(text_from_minutes(1452), "24:12");
    EXPECT_EQ(text_from_minutes(1530), "25:30");
    EXPECT_EQ(text_from_minutes(-5), "-00:05");   // differenza negativa (es. in un messaggio)
    for (Minutes m = 0; m < 48 * 60; ++m) EXPECT_EQ(minutes_from_text(text_from_minutes(m)), m);
}

TEST(Tempo, orari_scritti_male) {
    for (const char* t : {"", "7:45", "07:4", "07:450", "07.45", "07:60", "0a:45", "07:4b", ":", "07:", "1000:00", " 07:45"})
        EXPECT_FALSE(minutes_from_text(t).has_value()) << "'" << t << "'";
}

// Date: calendario gregoriano, anni bisestili compresi; giorni della settimana dal 1970-01-01 (giovedi').
TEST(Tempo, date_e_giorni_della_settimana) {
    EXPECT_EQ(day_from_text("1970-01-01"), 0);
    EXPECT_EQ(weekday_of(0), Weekday::Thursday);
    EXPECT_EQ(weekday_of(*day_from_text("2026-09-14")), Weekday::Monday);
    EXPECT_EQ(weekday_of(*day_from_text("2026-12-25")), Weekday::Friday);
    EXPECT_EQ(weekday_of(*day_from_text("1969-12-31")), Weekday::Wednesday);
    EXPECT_EQ(*day_from_text("2027-03-01") - *day_from_text("2027-02-28"), 1);
    EXPECT_EQ(*day_from_text("2028-03-01") - *day_from_text("2028-02-28"), 2);   // 2028 bisestile
    EXPECT_TRUE(day_from_text("2000-02-29").has_value());
    EXPECT_FALSE(day_from_text("2100-02-29").has_value());
    EXPECT_FALSE(day_from_text("2027-02-29").has_value());
    for (const char* t : {"", "2026-9-14", "2026/09/14", "2026-13-01", "2026-00-10", "2026-04-31", "2026-09-1a", "20260914"})
        EXPECT_FALSE(day_from_text(t).has_value()) << "'" << t << "'";
    for (Day d = *day_from_text("1999-12-01"); d < *day_from_text("2031-01-31"); ++d) {
        EXPECT_EQ(day_from_text(text_from_day(d)), d);
        int a = 0, m = 0, g = 0;
        ymd_from_day(d, a, m, g);
        EXPECT_EQ(day_from_ymd(a, m, g), d);
    }
}

TEST(Tempo, giorni_della_settimana_scelti) {
    Weekdays w;
    EXPECT_TRUE(w.empty());
    w.add(Weekday::Monday);
    w.add(Weekday::Wednesday);
    w.add(Weekday::Thursday);
    EXPECT_FALSE(w.empty());
    EXPECT_TRUE(w.has(Weekday::Monday));
    EXPECT_FALSE(w.has(Weekday::Tuesday));
    EXPECT_TRUE(w.has(Weekday::Thursday));
    EXPECT_FALSE(w.has(Weekday::Sunday));
}
