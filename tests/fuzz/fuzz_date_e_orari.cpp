// K-Zero - fuzzing (libFuzzer): date e orari scritti come testo.
#include <cstddef>

#include "bersagli.hpp"

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* dati, std::size_t n) {
    kzero::fuzz::prova_date_e_orari(dati, n);
    return 0;
}
