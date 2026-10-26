// K-Zero API - normalizzazione Unicode con utf8proc (vedi unicode.hpp).
#include "unicode.hpp"

#include <utf8proc.h>

namespace kzero::api::unicode {

Risultato normalizza(std::string_view testo, bool minuscole, std::int32_t* lavoro, std::size_t capienza) {
    Risultato r;
    const auto* byte = reinterpret_cast<const utf8proc_uint8_t*>(testo.data());
    const auto n = static_cast<utf8proc_ssize_t>(testo.size());
    // Prima i controlli carattere per carattere: UTF-8 valido, niente caratteri di controllo ne' non assegnati.
    for (utf8proc_ssize_t i = 0; i < n;) {
        utf8proc_int32_t cp = 0;
        const utf8proc_ssize_t l = utf8proc_iterate(byte + i, n - i, &cp);
        if (l <= 0 || cp < 0) {
            r.esito = Esito::NonUtf8;
            return r;
        }
        const utf8proc_category_t c = utf8proc_category(cp);
        if (c == UTF8PROC_CATEGORY_CN) {
            r.esito = Esito::NonAssegnato;
            return r;
        }
        if (c == UTF8PROC_CATEGORY_CC) {
            r.esito = Esito::Controllo;
            return r;
        }
        i += l;
    }
    // NFC come utf8proc_NFC (STABLE e COMPOSE), con il "case folding" se richiesto; i caratteri non assegnati sono gia'
    // esclusi sopra (REJECTNA per sicurezza).
    int opzioni = UTF8PROC_STABLE | UTF8PROC_COMPOSE | UTF8PROC_REJECTNA;
    if (minuscole) opzioni |= UTF8PROC_CASEFOLD;
    const auto o = static_cast<utf8proc_option_t>(opzioni);
    const utf8proc_ssize_t scomposti = utf8proc_decompose(byte, n, lavoro, static_cast<utf8proc_ssize_t>(capienza), o);
    if (scomposti < 0) {
        r.esito = scomposti == UTF8PROC_ERROR_NOTASSIGNED ? Esito::NonAssegnato : Esito::NonUtf8;
        return r;
    }
    // reencode vuole almeno un byte libero dopo i caratteri scomposti.
    if (static_cast<std::size_t>(scomposti) >= capienza) {
        r.esito = Esito::TroppoLungo;
        return r;
    }
    // reencode ricompone (NFC) e scrive l'UTF-8 all'inizio della stessa memoria, come utf8proc_map.
    const utf8proc_ssize_t scritti = utf8proc_reencode(lavoro, scomposti, o);
    if (scritti < 0) {
        r.esito = Esito::NonUtf8;
        return r;
    }
    const auto* uscita = reinterpret_cast<const utf8proc_uint8_t*>(lavoro);
    for (utf8proc_ssize_t i = 0; i < scritti;) {
        utf8proc_int32_t cp = 0;
        const utf8proc_ssize_t l = utf8proc_iterate(uscita + i, scritti - i, &cp);
        if (l <= 0) {
            r.esito = Esito::NonUtf8;
            return r;
        }
        ++r.caratteri;
        i += l;
    }
    r.esito = Esito::Normalizzato;
    r.byte = static_cast<std::size_t>(scritti);
    return r;
}

const char* versione_unicode() { return utf8proc_unicode_version(); }

}  // namespace kzero::api::unicode
