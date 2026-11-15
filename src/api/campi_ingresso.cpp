// K-Zero API - i campi di una modifica a mano letti dall'ingresso (vedi campi_ingresso.hpp).
#include "campi_ingresso.hpp"

#include <cerrno>
#include <cstdio>
#include <memory>

#if defined(__unix__) || defined(__APPLE__)
#define KZERO_TERMINALE_POSIX 1
#include <unistd.h>
#endif

namespace kzero::api::campi {

namespace {

// UTF-8 valido (RFC 3629): niente forme troppo lunghe, niente meta' di coppie (U+D800-DFFF), niente oltre U+10FFFF.
// Nessun carattere di controllo C0 (tranne la tabulazione), ne' DEL, ne' i controlli C1 (U+0080-009F).
Motivo controlla(const std::string& s) {
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            if ((c < 0x20 && c != '\t') || c == 0x7F) return Motivo::Controllo;
            ++i;
            continue;
        }
        std::size_t n = 0;
        unsigned long cp = 0;
        if (c >= 0xC2 && c <= 0xDF) {
            n = 1;
            cp = c & 0x1F;
        } else if (c >= 0xE0 && c <= 0xEF) {
            n = 2;
            cp = c & 0x0F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            n = 3;
            cp = c & 0x07;
        } else {
            return Motivo::NonUtf8;
        }
        for (std::size_t k = 1; k <= n; ++k) {
            if (i + k >= s.size()) return Motivo::NonUtf8;
            const unsigned char d = static_cast<unsigned char>(s[i + k]);
            if ((d & 0xC0) != 0x80) return Motivo::NonUtf8;
            cp = (cp << 6) | (d & 0x3F);
        }
        if ((n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return Motivo::NonUtf8;
        if (cp >= 0x80 && cp <= 0x9F) return Motivo::Controllo;
        i += n + 1;
    }
    return Motivo::Nessuno;
}

}  // namespace

Righe leggi(const std::function<int()>& byte) {
    Righe out;
    std::string riga;
    bool troppo_lunga = false;
    for (;;) {
        const int b = byte();
        if (b < 0 || b == '\n') {
            if (b < 0 && riga.empty() && !troppo_lunga) break;   // fine dell'ingresso
            const int n = static_cast<int>(out.righe.size()) + 1;
            if (troppo_lunga) {
                out.motivo = Motivo::RigaTroppoLunga;
                out.riga = n;
                return out;
            }
            if (!riga.empty() && riga.back() == '\r') riga.pop_back();
            if (riga.empty()) break;   // una riga vuota chiude i campi
            if (out.righe.size() >= kRighe) {
                out.motivo = Motivo::TroppeRighe;
                out.riga = n;
                return out;
            }
            const Motivo m = controlla(riga);
            if (m != Motivo::Nessuno) {
                out.motivo = m;
                out.riga = n;
                return out;
            }
            out.righe.push_back(std::move(riga));
            riga.clear();
            if (b < 0) break;
            continue;
        }
        if (riga.size() >= kRigaByte) troppo_lunga = true;   // il resto della riga si scarta, ma la riga e' troppo lunga
        else riga += static_cast<char>(b);
    }
    return out;
}

Righe leggi_testo(const std::string& testo) {
    std::size_t i = 0;
    return leggi([&]() -> int { return i < testo.size() ? static_cast<unsigned char>(testo[i++]) : -1; });
}

std::function<int()> ingresso_standard() {
    return []() -> int {
#if defined(KZERO_TERMINALE_POSIX)
        for (;;) {
            unsigned char c = 0;
            const ssize_t n = ::read(0, &c, 1);
            if (n < 0 && errno == EINTR) continue;
            return n == 1 ? c : -1;
        }
#else
        const int c = std::getchar();
        return c == EOF ? -1 : c;
#endif
    };
}

std::string spiegazione(Motivo m) {
    switch (m) {
        case Motivo::Nessuno: return "";
        case Motivo::RigaTroppoLunga: return "la riga e' piu' lunga del massimo (" + std::to_string(kRigaByte) + " byte)";
        case Motivo::TroppeRighe: return "ci sono piu' righe del massimo (" + std::to_string(kRighe) + ")";
        case Motivo::NonUtf8: return "la riga non e' testo UTF-8 valido";
        case Motivo::Controllo: return "la riga ha un carattere di controllo";
    }
    return "";
}

}  // namespace kzero::api::campi
