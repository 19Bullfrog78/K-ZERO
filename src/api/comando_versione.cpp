// K-Zero API - comando "versione".
#include "esecutori.hpp"
#include "versione.hpp"   // generato da CMake

namespace kzero::api {

std::string esegui_versione(const DefinizioneComando& comando, const Parametri& parametri) {
    if (parametri.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["programma"] = kNomeProgramma;
        j["versione"] = kVersione;
        return testo_json(j);
    }
    return std::string(kNomeProgramma) + " " + kVersione + "\n";
}

}  // namespace kzero::api
