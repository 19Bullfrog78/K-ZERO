// K-Zero - date e orari (vedi tempo.hpp).
#include "tempo.hpp"

namespace kzero {

namespace {

bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int days_in_month(int y, int m) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && is_leap(y) ? 29 : days[m - 1];
}

// Cifre decimali da text[from] per count caratteri; -1 se non sono tutte cifre.
int digits(std::string_view text, std::size_t from, std::size_t count) {
    int value = 0;
    for (std::size_t i = from; i < from + count; ++i) {
        if (text[i] < '0' || text[i] > '9') return -1;
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

std::string two_digits(int v) { return v < 10 ? "0" + std::to_string(v) : std::to_string(v); }

}  // namespace

// Algoritmo "days from civil" (H. Hinnant): esatto per tutto il calendario gregoriano.
Day day_from_ymd(int year, int month, int day) {
    const int y = year - (month <= 2 ? 1 : 0);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;
    const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void ymd_from_day(Day d, int& year, int& month, int& day) {
    const int z = d + 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const int doe = z - era * 146097;
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp + (mp < 10 ? 3 : -9);
    year = yoe + era * 400 + (month <= 2 ? 1 : 0);
}

// 1970-01-01 era un giovedi'.
Weekday weekday_of(Day d) {
    const int from_monday = ((d % 7) + 7 + 3) % 7;
    return static_cast<Weekday>(from_monday);
}

std::optional<Day> day_from_text(std::string_view text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
    const int y = digits(text, 0, 4), m = digits(text, 5, 2), d = digits(text, 8, 2);
    if (y < 0 || m < 1 || m > 12 || d < 1 || d > days_in_month(y, m)) return std::nullopt;
    return day_from_ymd(y, m, d);
}

std::string text_from_day(Day d) {
    int y = 0, m = 0, dd = 0;
    ymd_from_day(d, y, m, dd);
    std::string year = std::to_string(y);
    while (year.size() < 4) year = "0" + year;
    return year + "-" + two_digits(m) + "-" + two_digits(dd);
}

std::optional<Minutes> minutes_from_text(std::string_view text) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon < 2 || colon > 3 || text.size() != colon + 3) return std::nullopt;
    const int h = digits(text, 0, colon), mm = digits(text, colon + 1, 2);
    if (h < 0 || mm < 0 || mm > 59) return std::nullopt;
    return h * 60 + mm;
}

std::string text_from_minutes(Minutes m) {
    if (m < 0) return "-" + text_from_minutes(-m);
    return two_digits(m / 60) + ":" + two_digits(m % 60);
}

}  // namespace kzero
