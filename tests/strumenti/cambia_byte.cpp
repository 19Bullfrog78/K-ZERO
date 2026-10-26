// K-Zero - strumento delle prove sul programma (K-010): cambia un byte di un file (XOR con 1), lo tronca o lo svuota, per le
// prove di manomissione della cassaforte e del registro (CMake non sa scrivere byte qualsiasi).
// Uso: cambia_byte <file> <posizione>     (posizione negativa: dalla fine, -1 = l'ultimo byte)
//      cambia_byte <file> tronca <lunghezza>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) return 3;
    std::vector<char> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    if (std::string(argv[2]) == "tronca") {
        if (argc < 4) return 2;
        const std::size_t n = static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10));
        if (n < b.size()) b.resize(n);
    } else {
        long long k = std::strtoll(argv[2], nullptr, 10);
        if (k < 0) k += static_cast<long long>(b.size());
        if (k < 0 || static_cast<std::size_t>(k) >= b.size()) return 4;
        b[static_cast<std::size_t>(k)] = static_cast<char>(b[static_cast<std::size_t>(k)] ^ 1);
    }
    std::ofstream out(argv[1], std::ios::binary | std::ios::trunc);
    out.write(b.data(), static_cast<std::streamsize>(b.size()));
    return out ? 0 : 5;
}
