#pragma once
// K-Zero - strumenti delle prove delle modifiche a mano dell'elenco (K-020): il pacchetto di prova (il demo con i profili e i codici
// esterni di tests/esempi/k019-elenco, dati inventati), in comune tra le prove del nucleo e quelle dei comandi.
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "lettore.hpp"

namespace kzero::prove::elenco_a_mano {

using kzero::api::JsonDati;
using kzero::api::PacchettoIncorporato;
using kzero::api::PacchettoInMemoria;

inline const std::string kCartella = KZERO_CARTELLA_PROVE "/esempi/k019-elenco";

inline std::string leggi_testo(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

inline PacchettoInMemoria pacchetto_di_prova() {
    PacchettoInMemoria m;
    const PacchettoIncorporato demo("esempi/demo");
    const JsonDati codici = JsonDati::parse(leggi_testo(kCartella + "/codici_esterni.json"));
    for (const auto& v : demo.voci()) {
        std::string t = demo.leggi(v.nome, kzero::api::kMassimoByteFile).testo;
        if (v.nome == "scuole.json" || v.nome == "luoghi.json") {
            JsonDati d = JsonDati::parse(t);
            const char* elenco = v.nome == "scuole.json" ? "scuole" : "luoghi";
            for (JsonDati& x : d.at(elenco)) {
                const std::string c = x.at("codice").get<std::string>();
                if (!codici.at(elenco).contains(c)) continue;
                if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
                for (const JsonDati& e : codici.at(elenco).at(c)) x["codici_esterni"].push_back(e);
            }
            t = d.dump(2);
        }
        m.file[v.nome] = t;
    }
    m.file["profili_elenchi.json"] = leggi_testo(kCartella + "/profili_elenchi.json");
    return m;
}

inline const std::string kPrim = "Scuola primaria Valfonda";
inline const std::vector<std::string> kCasaNuova = {"comune: Pradello", "frazione: Croce", "via: via dei Larici", "civico: 23"};

}  // namespace kzero::prove::elenco_a_mano
