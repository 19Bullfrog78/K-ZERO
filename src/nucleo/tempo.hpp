#pragma once
// K-Zero - date e orari.
// Orari: minuti dalla mezzanotte del giorno di servizio. Come in GTFS un orario puo' superare le 24:00:
// una corsa che parte alle 23:40 e arriva dopo mezzanotte si scrive "23:40" ... "24:35" (= 1475 minuti).
// Date: giorni contati dal 1970-01-01 (calendario gregoriano), scritte "AAAA-MM-GG".

#include <optional>
#include <string>
#include <string_view>

namespace kzero {

using Minutes = int;   // minuti dalla mezzanotte del giorno di servizio (07:45 = 465; 25:30 = 1530)
using Day = int;       // giorni dal 1970-01-01

enum class Weekday { Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday };

// Giorni della settimana in cui vale qualcosa (orario di una scuola, calendario...).
class Weekdays {
public:
    void add(Weekday d) { bits_ = static_cast<unsigned char>(bits_ | bit(d)); }
    bool has(Weekday d) const { return (bits_ & bit(d)) != 0; }
    bool empty() const { return bits_ == 0; }

private:
    static unsigned char bit(Weekday d) { return static_cast<unsigned char>(1u << static_cast<unsigned>(d)); }
    unsigned char bits_ = 0;
};

// Data dai suoi tre numeri (la data deve esistere).
Day day_from_ymd(int year, int month, int day);

// Anno, mese e giorno di una data.
void ymd_from_day(Day d, int& year, int& month, int& day);

// Giorno della settimana di una data.
Weekday weekday_of(Day d);

// "AAAA-MM-GG" -> data; nessun valore se la forma e' sbagliata o la data non esiste (es. 2027-02-29).
std::optional<Day> day_from_text(std::string_view text);

// Data -> "AAAA-MM-GG".
std::string text_from_day(Day d);

// "HH:MM" (ore anche oltre 24, es. "25:30") -> minuti; nessun valore se la forma e' sbagliata.
std::optional<Minutes> minutes_from_text(std::string_view text);

// Minuti -> "HH:MM", senza riportare alla mezzanotte: 1530 -> "25:30", 465 -> "07:45".
std::string text_from_minutes(Minutes m);

}  // namespace kzero
