#pragma once
// K-Zero API - i file di un pacchetto dati: nome del file, schema e versioni che il programma sa leggere.
// E' l'unico elenco: lo usano il lettore dei pacchetti, "kzero descrivi" e le prove.

#include <string>
#include <vector>

namespace kzero::api {

struct FileDati {
    std::string file;            // nome del file nella cartella del pacchetto, es. "corse.json"
    std::string schema;          // nome dello schema dichiarato nel file, es. "corse"
    std::vector<int> versioni;   // versioni dello schema che il programma sa leggere
    bool obbligatorio = false;
    std::string contenuto;       // cosa contiene, in breve
};

// Tutti i file del formato, nell'ordine del formato (manifesto per primo).
const std::vector<FileDati>& file_dati();

// File del formato con quel nome, oppure nullptr.
const FileDati* file_dati_per_nome(const std::string& file);

}  // namespace kzero::api
