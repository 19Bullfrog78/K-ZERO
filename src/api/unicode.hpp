#pragma once
// K-Zero API - normalizzazione Unicode dei testi con la libreria utf8proc (K-010): serve alle password, che devono aprire
// la cassaforte allo stesso modo su Linux e su Windows anche con le lettere accentate. Una lettera accentata si puo'
// scrivere in due modi (es. "e" accentata come un carattere solo, U+00E8, oppure "e" seguita dall'accento U+0300): la
// forma NFC (Unicode, UAX #15), raccomandata da NIST SP 800-63B-4 per le password, li rende uguali.
// Il lavoro si fa dentro una memoria data da chi chiama: per le password e' memoria protetta (cripto.hpp).

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace kzero::api::unicode {

enum class Esito {
    Normalizzato,
    NonUtf8,           // byte che non sono UTF-8
    NonAssegnato,      // carattere che lo standard Unicode non ha ancora assegnato: la sua forma potrebbe cambiare
    Controllo,         // carattere di controllo (es. tabulazione, ritorno a capo): non si scrive in modo affidabile
    TroppoLungo,       // la memoria data non basta
};

struct Risultato {
    Esito esito = Esito::NonUtf8;
    std::size_t byte = 0;         // lunghezza in byte del testo normalizzato (UTF-8), scritto all'inizio della memoria
    std::size_t caratteri = 0;    // caratteri (code point) del testo normalizzato: NIST li conta uno per uno
};

// Normalizza in NFC il testo UTF-8; con "minuscole" anche il "case folding" Unicode (per confrontare senza badare a
// maiuscole e minuscole). Usa la memoria "lavoro" (capienza in numeri a 32 bit) e vi scrive il risultato in UTF-8.
// Basta una capienza di 4 volte i byte del testo piu' 32 (una lettera si scompone al massimo in pochi caratteri).
Risultato normalizza(std::string_view testo, bool minuscole, std::int32_t* lavoro, std::size_t capienza);

// Capienza di lavoro sufficiente per un testo di n byte.
inline constexpr std::size_t capienza_per(std::size_t byte) { return 4 * byte + 32; }

// Versione dello standard Unicode di utf8proc (es. "17.0.0").
const char* versione_unicode();

}  // namespace kzero::api::unicode
