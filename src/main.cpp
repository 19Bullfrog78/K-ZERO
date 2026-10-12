// K-Zero - punto di ingresso del programma kzero.
// Tutto il lavoro sta nell'API (src/api); regole e calcoli nel nucleo (src/nucleo).
#include <iostream>
#include <string>
#include <vector>

#include "api.hpp"

int main(int argc, char** argv) {
    try {
        std::vector<std::string> argomenti;
        for (int i = 1; i < argc; ++i) argomenti.emplace_back(argv[i]);
        return kzero::api::consegna(kzero::api::esegui(argomenti), std::cout, std::cerr);
    } catch (...) {
        // Ultima difesa: l'API gestisce gia' ogni errore; qui si arriva solo se manca la memoria.
        // Codice e uscita come nel catalogo: K-ERR-009, errore interno = 3.
        std::cerr << "Errore K-ERR-009: Errore interno.\n";
        return 3;
    }
}
