// K-Zero - strumento delle prove (K-019): costruisce un file Excel (.xlsx) da un file di testo con le colonne separate da
// tabulazioni (una riga del testo = una riga del foglio), cosi' nel deposito ci sono solo testi leggibili. Una cella che
// comincia con "#" e' un numero; "!" e' una cella con un errore di Excel (tests/xlsx_prove.hpp).
// Uso: crea_xlsx <testo.tsv> <file.xlsx> [nome del foglio]
//      crea_xlsx --fogli <file.xlsx> <nome del foglio>=<testo.tsv> ... (K-021: piu' fogli, es. le schede dei servizi)
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../xlsx_prove.hpp"

namespace {
std::vector<std::vector<std::string>> righe_di(std::istream& in) {
    std::vector<std::vector<std::string>> out;
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
        out.push_back(std::move(celle));
    }
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc >= 4 && std::string(argv[1]) == "--fogli") {
        std::vector<kzero::prove::xlsx::Foglio> fogli;
        for (int i = 3; i < argc; ++i) {
            const std::string a = argv[i];
            const std::size_t u = a.find('=');
            if (u == std::string::npos) {
                std::cerr << "uso: crea_xlsx --fogli <file.xlsx> <nome del foglio>=<testo.tsv> ...\n";
                return 2;
            }
            std::ifstream in(a.substr(u + 1), std::ios::binary);
            if (!in) {
                std::cerr << "non si legge " << a.substr(u + 1) << "\n";
                return 1;
            }
            fogli.push_back({a.substr(0, u), righe_di(in), false});
        }
        std::ofstream o(argv[2], std::ios::binary);
        o << kzero::prove::xlsx::file_xlsx(fogli);
        return o ? 0 : 1;
    }
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
