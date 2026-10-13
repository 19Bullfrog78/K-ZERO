#pragma once
// K-Zero - ricerca dei luoghi per nome ufficiale, nome alternativo o codice esterno.
// I testi si confrontano dopo una normalizzazione fissa (spiegata in docs/formato-dati.md):
//   1. lettere latine con segni (accenti, dieresi, cediglie, barre...) -> lettera semplice maiuscola,
//      secondo la tabella fold_table() (es. "ò" -> "O", "ß" -> "SS", "ł" -> "L", "æ" -> "AE");
//   2. lettere a-z -> A-Z; segni diacritici staccati (U+0300-U+036F) tolti;
//   3. punteggiatura, simboli e spazi di ogni tipo -> uno spazio; spazi doppi ridotti, spazi ai bordi tolti;
//   4. tutto il resto (cifre, lettere di altri alfabeti, byte non UTF-8) resta com'e'.
// Nessuna somiglianza "a occhio": il nome coincide, oppure e' sconosciuto, oppure e' ambiguo.

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "domain_models.hpp"

namespace kzero {

// Una riga della tabella: il carattere (codice Unicode) e cosa diventa.
struct FoldEntry {
    char32_t letter;
    const char* result;
};

// Tabella completa delle lettere trasformate al passo 1, in ordine di codice Unicode.
const std::vector<FoldEntry>& fold_table();

// Forma normalizzata di un testo UTF-8.
std::string normalize_name(std::string_view text);

// Indice dei luoghi pubblici per la ricerca. I luoghi privati (case) non si cercano per nome.
class PlaceSearch {
public:
    enum class Outcome { Found, Unknown, Ambiguous };
    struct Result {
        Outcome outcome = Outcome::Unknown;
        std::vector<Code> codes;   // il luogo trovato, oppure tutti i luoghi possibili se ambiguo
    };

    // types vuoto = tutti i tipi di luogo pubblico.
    explicit PlaceSearch(const std::vector<Place>& places, std::vector<PlaceType> types = {});

    // Per nome ufficiale o alternativo.
    Result by_name(std::string_view name) const;
    // Per codice esterno: sistema, tipo e codice (tutti e tre normalizzati).
    Result by_external_code(std::string_view system, std::string_view type, std::string_view code) const;

    // Chiavi normalizzate che appartengono a piu' luoghi (per i controlli), in ordine.
    std::vector<std::pair<std::string, std::vector<Code>>> ambiguous_names() const;
    std::vector<std::pair<std::string, std::vector<Code>>> ambiguous_external_codes() const;

    // Chiave normalizzata di un codice esterno.
    static std::string external_key(std::string_view system, std::string_view type, std::string_view code);

private:
    static Result result(const std::map<std::string, std::vector<Code>>& index, const std::string& key);
    std::map<std::string, std::vector<Code>> names_;
    std::map<std::string, std::vector<Code>> external_;
};

}  // namespace kzero
