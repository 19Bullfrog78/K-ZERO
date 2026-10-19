// K-Zero - fuzzing (libFuzzer): righe di comando casuali (argomenti separati dal byte 0).
#include <cstddef>

#include "bersagli.hpp"

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* dati, std::size_t n) {
    kzero::fuzz::prova_parametri(dati, n);
    return 0;
}
