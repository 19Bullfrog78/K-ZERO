// K-Zero - normalizzazione dei nomi e ricerca dei luoghi (vedi ricerca.hpp e docs/formato-dati.md).
#include "ricerca.hpp"

#include <algorithm>

namespace kzero {

const std::vector<FoldEntry>& fold_table() {
    // Ogni riga di questa tabella e' elencata anche in docs/formato-dati.md (una prova le confronta).
    static const std::vector<FoldEntry> table = {
        // Latin-1 (U+00B2-U+00FF)
        {0x00B2, "2"}, {0x00B3, "3"}, {0x00B9, "1"},
        {0x00C0, "A"}, {0x00C1, "A"}, {0x00C2, "A"}, {0x00C3, "A"}, {0x00C4, "A"}, {0x00C5, "A"}, {0x00C6, "AE"},
        {0x00C7, "C"}, {0x00C8, "E"}, {0x00C9, "E"}, {0x00CA, "E"}, {0x00CB, "E"}, {0x00CC, "I"}, {0x00CD, "I"},
        {0x00CE, "I"}, {0x00CF, "I"}, {0x00D0, "D"}, {0x00D1, "N"}, {0x00D2, "O"}, {0x00D3, "O"}, {0x00D4, "O"},
        {0x00D5, "O"}, {0x00D6, "O"}, {0x00D8, "O"}, {0x00D9, "U"}, {0x00DA, "U"}, {0x00DB, "U"}, {0x00DC, "U"},
        {0x00DD, "Y"}, {0x00DE, "TH"}, {0x00DF, "SS"},
        {0x00E0, "A"}, {0x00E1, "A"}, {0x00E2, "A"}, {0x00E3, "A"}, {0x00E4, "A"}, {0x00E5, "A"}, {0x00E6, "AE"},
        {0x00E7, "C"}, {0x00E8, "E"}, {0x00E9, "E"}, {0x00EA, "E"}, {0x00EB, "E"}, {0x00EC, "I"}, {0x00ED, "I"},
        {0x00EE, "I"}, {0x00EF, "I"}, {0x00F0, "D"}, {0x00F1, "N"}, {0x00F2, "O"}, {0x00F3, "O"}, {0x00F4, "O"},
        {0x00F5, "O"}, {0x00F6, "O"}, {0x00F8, "O"}, {0x00F9, "U"}, {0x00FA, "U"}, {0x00FB, "U"}, {0x00FC, "U"},
        {0x00FD, "Y"}, {0x00FE, "TH"}, {0x00FF, "Y"},
        // Latino esteso A (U+0100-U+017F)
        {0x0100, "A"}, {0x0101, "A"}, {0x0102, "A"}, {0x0103, "A"}, {0x0104, "A"}, {0x0105, "A"},
        {0x0106, "C"}, {0x0107, "C"}, {0x0108, "C"}, {0x0109, "C"}, {0x010A, "C"}, {0x010B, "C"}, {0x010C, "C"},
        {0x010D, "C"},
        {0x010E, "D"}, {0x010F, "D"}, {0x0110, "D"}, {0x0111, "D"},
        {0x0112, "E"}, {0x0113, "E"}, {0x0114, "E"}, {0x0115, "E"}, {0x0116, "E"}, {0x0117, "E"}, {0x0118, "E"},
        {0x0119, "E"}, {0x011A, "E"}, {0x011B, "E"},
        {0x011C, "G"}, {0x011D, "G"}, {0x011E, "G"}, {0x011F, "G"}, {0x0120, "G"}, {0x0121, "G"}, {0x0122, "G"},
        {0x0123, "G"},
        {0x0124, "H"}, {0x0125, "H"}, {0x0126, "H"}, {0x0127, "H"},
        {0x0128, "I"}, {0x0129, "I"}, {0x012A, "I"}, {0x012B, "I"}, {0x012C, "I"}, {0x012D, "I"}, {0x012E, "I"},
        {0x012F, "I"}, {0x0130, "I"}, {0x0131, "I"},
        {0x0132, "IJ"}, {0x0133, "IJ"},
        {0x0134, "J"}, {0x0135, "J"},
        {0x0136, "K"}, {0x0137, "K"}, {0x0138, "K"},
        {0x0139, "L"}, {0x013A, "L"}, {0x013B, "L"}, {0x013C, "L"}, {0x013D, "L"}, {0x013E, "L"}, {0x013F, "L"},
        {0x0140, "L"}, {0x0141, "L"}, {0x0142, "L"},
        {0x0143, "N"}, {0x0144, "N"}, {0x0145, "N"}, {0x0146, "N"}, {0x0147, "N"}, {0x0148, "N"}, {0x0149, "N"},
        {0x014A, "N"}, {0x014B, "N"},
        {0x014C, "O"}, {0x014D, "O"}, {0x014E, "O"}, {0x014F, "O"}, {0x0150, "O"}, {0x0151, "O"},
        {0x0152, "OE"}, {0x0153, "OE"},
        {0x0154, "R"}, {0x0155, "R"}, {0x0156, "R"}, {0x0157, "R"}, {0x0158, "R"}, {0x0159, "R"},
        {0x015A, "S"}, {0x015B, "S"}, {0x015C, "S"}, {0x015D, "S"}, {0x015E, "S"}, {0x015F, "S"}, {0x0160, "S"},
        {0x0161, "S"},
        {0x0162, "T"}, {0x0163, "T"}, {0x0164, "T"}, {0x0165, "T"}, {0x0166, "T"}, {0x0167, "T"},
        {0x0168, "U"}, {0x0169, "U"}, {0x016A, "U"}, {0x016B, "U"}, {0x016C, "U"}, {0x016D, "U"}, {0x016E, "U"},
        {0x016F, "U"}, {0x0170, "U"}, {0x0171, "U"}, {0x0172, "U"}, {0x0173, "U"},
        {0x0174, "W"}, {0x0175, "W"},
        {0x0176, "Y"}, {0x0177, "Y"}, {0x0178, "Y"},
        {0x0179, "Z"}, {0x017A, "Z"}, {0x017B, "Z"}, {0x017C, "Z"}, {0x017D, "Z"}, {0x017E, "Z"},
        {0x017F, "S"},
        // Rumeno con la virgola (latino esteso B) e "ß" maiuscola
        {0x0218, "S"}, {0x0219, "S"}, {0x021A, "T"}, {0x021B, "T"},
        {0x1E9E, "SS"},
    };
    return table;
}

namespace {

// Punteggiatura, simboli e spazi che diventano uno spazio (oltre ai caratteri ASCII non alfanumerici).
bool is_separator(char32_t cp) {
    if (cp >= 0x80 && cp <= 0xBF) return cp != 0xB2 && cp != 0xB3 && cp != 0xB9;   // Latin-1: spazi e segni
    return cp == 0xD7 || cp == 0xF7 ||                 // per, diviso
           (cp >= 0x2000 && cp <= 0x206F) ||           // punteggiatura generale: trattini, virgolette, spazi
           cp == 0xFEFF;                               // spazio invisibile (inizio dei file di Windows)
}

bool is_combining_mark(char32_t cp) { return cp >= 0x300 && cp <= 0x36F; }

const char* folded(char32_t cp) {
    const auto& t = fold_table();
    const auto it = std::lower_bound(t.begin(), t.end(), cp, [](const FoldEntry& e, char32_t c) { return e.letter < c; });
    return it != t.end() && it->letter == cp ? it->result : nullptr;
}

// Legge un carattere UTF-8 da text[i]; restituisce la sua lunghezza in byte (0 = byte non valido).
std::size_t decode(std::string_view text, std::size_t i, char32_t& cp) {
    const auto b = [&](std::size_t k) { return static_cast<unsigned char>(text[k]); };
    const unsigned char c = b(i);
    std::size_t n = 0;
    char32_t min = 0;
    if (c < 0x80) { cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) { n = 2; cp = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { n = 3; cp = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { n = 4; cp = c & 0x07; min = 0x10000; }
    else return 0;
    if (i + n > text.size()) return 0;
    for (std::size_t k = 1; k < n; ++k) {
        if ((b(i + k) & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (b(i + k) & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    return n;
}

}  // namespace

std::string normalize_name(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool space = false;
    const auto put = [&](std::string_view s) {
        if (space && !out.empty()) out += ' ';
        space = false;
        out += s;
    };
    for (std::size_t i = 0; i < text.size();) {
        char32_t cp = 0;
        const std::size_t n = decode(text, i, cp);
        if (n == 0) {   // byte non UTF-8: resta com'e'
            put(text.substr(i, 1));
            ++i;
            continue;
        }
        if (cp < 0x80) {
            const char c = static_cast<char>(cp);
            if (c >= 'a' && c <= 'z') put(std::string(1, static_cast<char>(c - 'a' + 'A')));
            else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) put(std::string(1, c));
            else space = true;
        } else if (is_combining_mark(cp)) {
            // tolto: "e" + accento staccato vale come "e"
        } else if (const char* f = folded(cp)) {
            put(f);
        } else if (is_separator(cp)) {
            space = true;
        } else {
            put(text.substr(i, n));
        }
        i += n;
    }
    return out;
}

std::string PlaceSearch::external_key(std::string_view system, std::string_view type, std::string_view code) {
    return normalize_name(system) + '\x1F' + normalize_name(type) + '\x1F' + normalize_name(code);
}

PlaceSearch::PlaceSearch(const std::vector<Place>& places, std::vector<PlaceType> types) {
    const auto add = [](std::map<std::string, std::vector<Code>>& index, const std::string& key, const Code& code) {
        if (key.empty()) return;
        auto& codes = index[key];
        if (std::find(codes.begin(), codes.end(), code) == codes.end()) codes.push_back(code);
    };
    for (const Place& p : places) {
        if (p.is_private) continue;
        if (!types.empty() && std::find(types.begin(), types.end(), p.type) == types.end()) continue;
        add(names_, normalize_name(p.name), p.code);
        for (const auto& alt : p.alt_names) add(names_, normalize_name(alt), p.code);
        for (const auto& e : p.external) add(external_, external_key(e.system, e.type, e.code), p.code);
    }
}

PlaceSearch::Result PlaceSearch::result(const std::map<std::string, std::vector<Code>>& index, const std::string& key) {
    Result r;
    const auto it = index.find(key);
    if (key.empty() || it == index.end()) return r;
    r.codes = it->second;
    r.outcome = r.codes.size() == 1 ? Outcome::Found : Outcome::Ambiguous;
    return r;
}

PlaceSearch::Result PlaceSearch::by_name(std::string_view name) const { return result(names_, normalize_name(name)); }

PlaceSearch::Result PlaceSearch::by_external_code(std::string_view system, std::string_view type,
                                                  std::string_view code) const {
    return result(external_, external_key(system, type, code));
}

namespace {

std::vector<std::pair<std::string, std::vector<Code>>> with_more_codes(const std::map<std::string, std::vector<Code>>& m) {
    std::vector<std::pair<std::string, std::vector<Code>>> out;
    for (const auto& [key, codes] : m)
        if (codes.size() > 1) out.emplace_back(key, codes);
    return out;
}

}  // namespace

std::vector<std::pair<std::string, std::vector<Code>>> PlaceSearch::ambiguous_names() const {
    return with_more_codes(names_);
}

std::vector<std::pair<std::string, std::vector<Code>>> PlaceSearch::ambiguous_external_codes() const {
    return with_more_codes(external_);
}

}  // namespace kzero
