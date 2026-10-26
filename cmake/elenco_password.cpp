// K-Zero - prepara l'elenco delle password comuni da incorporare nel programma (K-010, NIST SP 800-63B-4).
// Legge l'elenco di SecLists (una password per riga), tiene quelle che una persona potrebbe scegliere come password di
// K-Zero (almeno 15 caratteri, al massimo 256, UTF-8 valido, senza caratteri di controllo o non assegnati), le normalizza
// come il programma normalizza le password per confrontarle (NFC e minuscole, src/api/unicode.cpp), toglie i doppioni,
// le mette in ordine e scrive un file C++ con un elenco di byte: password separate da '\n'.
// Uso: kzero_elenco_password <elenco.txt> <uscita.cpp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "unicode.hpp"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Uso: kzero_elenco_password <elenco.txt> <uscita.cpp>\n");
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "Elenco delle password non leggibile: %s\n", argv[1]);
        return 1;
    }
    std::vector<std::string> tenute;
    std::string riga;
    std::vector<std::int32_t> lavoro;
    while (std::getline(in, riga)) {
        if (!riga.empty() && riga.back() == '\r') riga.pop_back();
        if (riga.size() < 15 || riga.size() > 1024) continue;   // meno di 15 byte = meno di 15 caratteri
        lavoro.assign(kzero::api::unicode::capienza_per(riga.size()), 0);
        const auto r = kzero::api::unicode::normalizza(riga, true, lavoro.data(), lavoro.size());
        if (r.esito != kzero::api::unicode::Esito::Normalizzato || r.caratteri < 15 || r.caratteri > 256) continue;
        tenute.emplace_back(reinterpret_cast<const char*>(lavoro.data()), r.byte);
    }
    std::sort(tenute.begin(), tenute.end());
    tenute.erase(std::unique(tenute.begin(), tenute.end()), tenute.end());
    std::string byte;
    for (const std::string& t : tenute) byte += t + "\n";

    std::string out =
        "// Generato da cmake/elenco_password.cpp dall'elenco di SecLists: non modificare a mano.\n"
        "#include <cstddef>\n\nnamespace kzero::api::password {\n\n"
        "extern const unsigned char kPasswordComuni[];\nextern const std::size_t kPasswordComuniByte;\n"
        "extern const std::size_t kPasswordComuniQuante;\n\n"
        "const unsigned char kPasswordComuni[] = {\n";
    char numero[8];
    for (std::size_t i = 0; i < byte.size(); ++i) {
        std::snprintf(numero, sizeof numero, "0x%02x,", static_cast<unsigned char>(byte[i]));
        out += numero;
        if (i % 16 == 15) out += "\n";
    }
    out += "0x00};\nconst std::size_t kPasswordComuniByte = " + std::to_string(byte.size()) + ";\n";
    out += "const std::size_t kPasswordComuniQuante = " + std::to_string(tenute.size()) + ";\n\n}  // namespace kzero::api::password\n";
    std::ofstream f(argv[2], std::ios::binary | std::ios::trunc);
    f << out;
    if (!f) {
        std::fprintf(stderr, "Impossibile scrivere %s\n", argv[2]);
        return 1;
    }
    return 0;
}
