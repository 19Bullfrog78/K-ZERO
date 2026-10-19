// K-Zero - fuzzing (libFuzzer): un file del pacchetto demo sostituito da dati casuali e deformati.
// Il file da provare si sceglie con la variabile d'ambiente KZERO_FILE (es. KZERO_FILE=corse.json): un file alla
// volta, partendo dal file del demo (CLAUDE.md, sezione Comandi).
#include <cstdio>
#include <cstdlib>
#include <string>

#include "bersagli.hpp"
#include "formato_dati.hpp"

namespace {
std::string file_da_provare;
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
    const char* f = std::getenv("KZERO_FILE");
    if (!f || !kzero::api::file_dati_per_nome(f)) {
        std::fprintf(stderr, "Indicare il file del pacchetto da provare: KZERO_FILE=<file> (es. KZERO_FILE=corse.json)\n");
        std::exit(2);
    }
    file_da_provare = f;
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* dati, std::size_t n) {
    kzero::fuzz::prova_file_del_pacchetto(file_da_provare, dati, n);
    return 0;
}
