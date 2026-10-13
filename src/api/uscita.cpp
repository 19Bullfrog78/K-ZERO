// K-Zero API - strumenti comuni per le uscite in testo e in JSON.
#include "uscita.hpp"

#include <cmath>

namespace kzero::api {

std::string percorso_schema(const Schema& s) {
    return "docs/schema/" + s.nome + ".v" + std::to_string(s.versione) + ".schema.json";
}

Json documento(const Schema& s) {
    Json j = Json::object();
    j["schema"] = s.nome;
    j["schema_versione"] = s.versione;
    return j;
}

std::string testo_json(const Json& j) {
    return j.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

double arrotonda(double valore, int decimali) {
    double scala = 1.0;
    for (int i = 0; i < decimali; ++i) scala *= 10.0;
    const double r = std::round(valore * scala) / scala;
    return r == 0.0 ? 0.0 : r;   // niente "-0.0" nell'uscita
}

std::string orario(Minutes m) { return kzero::text_from_minutes(m); }

}  // namespace kzero::api
