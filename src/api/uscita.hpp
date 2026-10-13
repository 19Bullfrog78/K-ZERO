#pragma once
// K-Zero API - strumenti comuni per preparare le uscite in testo e in JSON.

#include <string>

#include <nlohmann/json.hpp>

#include "domain_models.hpp"
#include "formato.hpp"

namespace kzero::api {

// Schema JSON di un documento in uscita: file docs/schema/<nome>.v<versione>.schema.json
struct Schema {
    std::string nome;
    int versione = 0;
};

// Percorso del file dello schema, relativo alla cartella principale del repository.
std::string percorso_schema(const Schema& s);

// JSON con le chiavi nell'ordine in cui vengono inserite: "schema" e "schema_versione" per prime.
using Json = nlohmann::ordered_json;

// Documento JSON vuoto che dichiara il proprio schema.
Json documento(const Schema& s);

// Testo finale del documento: rientro di 2 spazi, a capo finale. I byte non UTF-8
// (ad esempio in un argomento ricopiato in un messaggio) diventano il carattere di sostituzione.
std::string testo_json(const Json& j);

// Valore calcolato arrotondato a un numero fisso di decimali (km: 3 = metri, euro: 2 = centesimi).
double arrotonda(double valore, int decimali);

// Orario "HH:MM" da minuti dalla mezzanotte (es. 465 -> "07:45"). Dopo mezzanotte l'orario continua
// come nei dati (1530 -> "25:30"): non si riporta mai alla mezzanotte.
std::string orario(Minutes m);

}  // namespace kzero::api
