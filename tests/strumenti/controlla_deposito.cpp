// K-Zero - controllo del deposito pubblico (K-015): nessuno dei quattro nomi nei file del deposito (o nel testo
// dell'ingresso standard: messaggi dei commit e titoli delle proposte) e, fuori da tests/ ed esempi/, nessun file di
// dati. Vedi deposito.hpp; lo usano la prova deposito.nomi_e_file e il controllo GitHub delle proposte.
// Uso: controlla_deposito --cartella <cartella> [--escludi <cartella>]...
//      controlla_deposito --testo <dove>
#include <iostream>
#include <string>
#include <vector>

#include "deposito.hpp"

int main(int argc, char** argv) {
    std::vector<std::string> argomenti(argv + 1, argv + argc);
    return kzero::deposito::esegui(argomenti, std::cin, kzero::deposito::nomi_del_deposito(), std::cout);
}
