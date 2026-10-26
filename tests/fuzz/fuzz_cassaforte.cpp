// K-Zero - fuzzing (libFuzzer) della cassaforte (K-010): il bersaglio si sceglie con la variabile d'ambiente KZERO_BERSAGLIO
// (cassaforte, cassaforte-voci, registro, registro-voci, password). Argon2id con la politica leggera delle prove
// (tests/fuzz/bersagli.hpp): il programma kzero non ce l'ha.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "bersagli.hpp"

namespace {
std::string bersaglio;
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    const char* b = std::getenv("KZERO_BERSAGLIO");
    bersaglio = b ? b : "";
    if (bersaglio != "cassaforte" && bersaglio != "cassaforte-voci" && bersaglio != "registro" && bersaglio != "registro-voci" &&
        bersaglio != "password") {
        std::fprintf(stderr, "Indicare il bersaglio: KZERO_BERSAGLIO=cassaforte|cassaforte-voci|registro|registro-voci|password\n");
        std::exit(2);
    }
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* dati, std::size_t n) {
    kzero::fuzz::prova(bersaglio, dati, n);
    return 0;
}
