// K-Zero API - comando "versione".
#include "esecutori.hpp"
#include "versione.hpp"   // generato da CMake

namespace kzero::api {

Risultato esegui_versione(const DefinizioneComando& comando, const Parametri& parametri) {
    if (parametri.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["programma"] = kNomeProgramma;
        j["versione"] = kVersione;
        return {testo_json(j), 0};
    }
    return {std::string(kNomeProgramma) + " " + kVersione + "\n", 0};
}

}  // namespace kzero::api
