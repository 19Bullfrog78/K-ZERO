// K-Zero - strumento delle prove (K-019): scrive in una cartella nuova il pacchetto demo (incorporato nel programma) con i
// profili di lettura di prova e i codici esterni di tests/esempi/k019-elenco, in chiaro. Le prove poi lo chiudono con kzero.
// Uso: prepara_k019 <cartella dei file di prova> <cartella nuova>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "lettore.hpp"
#include "schemi_dati.hpp"

namespace fs = std::filesystem;
using kzero::api::JsonDati;

namespace {

std::string leggi(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

void scrivi(const fs::path& p, const std::string& t) {
    std::ofstream o(p, std::ios::binary);
    o << t;
}

void aggiungi_codici(JsonDati& elenco, const JsonDati& codici) {
    for (JsonDati& x : elenco) {
        const std::string c = x.at("codice").get<std::string>();
        if (!codici.contains(c)) continue;
        if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
        for (const JsonDati& e : codici.at(c)) x["codici_esterni"].push_back(e);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "uso: prepara_k019 <cartella dei file di prova> <cartella nuova>\n";
        return 2;
    }
    const fs::path prove(argv[1]), out(argv[2]);
    fs::create_directories(out);
    const kzero::api::PacchettoIncorporato demo("esempi/demo");
    const JsonDati codici = JsonDati::parse(leggi(prove / "codici_esterni.json"));
    for (const auto& v : demo.voci()) {
        const auto f = demo.leggi(v.nome, kzero::api::kMassimoByteFile);
        if (v.nome == "scuole.json" || v.nome == "luoghi.json") {
            JsonDati d = JsonDati::parse(f.testo);
            if (v.nome == "scuole.json") aggiungi_codici(d.at("scuole"), codici.at("scuole"));
            else aggiungi_codici(d.at("luoghi"), codici.at("luoghi"));
            scrivi(out / v.nome, d.dump(2) + "\n");
        } else {
            scrivi(out / v.nome, f.testo);
        }
    }
    scrivi(out / "profili_elenchi.json", leggi(prove / "profili_elenchi.json"));
    return 0;
}
