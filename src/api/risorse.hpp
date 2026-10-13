#pragma once
// K-Zero API - file incorporati nel programma al momento della compilazione (cmake/incorpora.cmake):
// gli schemi di docs/schema e il pacchetto dimostrativo esempi/demo. Cosi' il programma funziona
// da qualunque cartella venga lanciato e usa sempre gli schemi con cui e' stato compilato.

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace kzero::api::risorse {

struct Risorsa {
    const char* nome;            // percorso dalla cartella principale del repository, es. "esempi/demo/corse.json"
    const unsigned char* dati;
    std::size_t dimensione;      // byte
};

// Tutti i file incorporati, in ordine di nome (definito nel file generato da CMake).
const std::vector<Risorsa>& elenco();

// Contenuto di un file incorporato, se c'e'.
inline std::optional<std::string_view> trova(std::string_view nome) {
    for (const Risorsa& r : elenco())
        if (nome == r.nome) return std::string_view(reinterpret_cast<const char*>(r.dati), r.dimensione);
    return std::nullopt;
}

}  // namespace kzero::api::risorse
