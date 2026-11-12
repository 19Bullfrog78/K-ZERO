// K-Zero - fuzzing (libFuzzer): l'elenco utenti del committente (K-019). Il bersaglio si sceglie con la variabile KZERO_BERSAGLIO:
// "elenco-archivio" (un file .xlsx intero), "elenco-foglio" (la parte XML di un foglio), "elenco-righe" (le righe del file).
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "bersagli.hpp"

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* dati, std::size_t n) {
    static const std::string bersaglio = [] {
        const char* b = std::getenv("KZERO_BERSAGLIO");
        return std::string(b ? b : "elenco-archivio");
    }();
    if (!kzero::fuzz::prova(bersaglio, dati, n)) {
        std::fprintf(stderr, "bersaglio sconosciuto: %s\n", bersaglio.c_str());
        std::abort();
    }
    return 0;
}
