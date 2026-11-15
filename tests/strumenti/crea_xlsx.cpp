// K-Zero - strumento delle prove (K-019): costruisce un file Excel (.xlsx) da un file di testo con le colonne separate da
// tabulazioni (una riga del testo = una riga del foglio), cosi' nel deposito ci sono solo testi leggibili. Una cella che
// comincia con "#" e' un numero; "!" e' una cella con un errore di Excel (tests/xlsx_prove.hpp).
// Uso: crea_xlsx <testo.tsv> <file.xlsx> [nome del foglio]
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../xlsx_prove.hpp"

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::cerr << "uso: crea_xlsx <testo.tsv> <file.xlsx> [nome del foglio]\n";
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::cerr << "non si legge " << argv[1] << "\n";
        return 1;
    }
    kzero::prove::xlsx::Foglio f;
    if (argc == 4) f.nome = argv[3];
    std::string riga;
    while (std::getline(in, riga)) {
        if (!riga.empty() && riga.back() == '\r') riga.pop_back();
        std::vector<std::string> celle;
        std::size_t a = 0;
        for (;;) {
            const std::size_t b = riga.find('\t', a);
            celle.push_back(riga.substr(a, b == std::string::npos ? std::string::npos : b - a));
            if (b == std::string::npos) break;
            a = b + 1;
        }
        f.righe.push_back(std::move(celle));
    }
    const std::string out = kzero::prove::xlsx::file_xlsx(std::vector<kzero::prove::xlsx::Foglio>{f});
    std::ofstream o(argv[2], std::ios::binary);
    o.write(out.data(), static_cast<std::streamsize>(out.size()));
    return o ? 0 : 1;
}
