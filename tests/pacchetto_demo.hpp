#pragma once
// K-Zero - prove: il pacchetto dimostrativo esempi/demo (dati inventati), letto come lo legge il programma.
// Le prove non scrivono dati nel codice: partono dal pacchetto demo e, quando serve, cambiano un solo valore.

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "lettore.hpp"

namespace kzero::prove {

// Il pacchetto demo incorporato nel programma (lo stesso che usa "kzero dimostrazione").
inline const kzero::Package& demo() {
    static const kzero::Package pacchetto = [] {
        kzero::api::Lettura lettura = kzero::api::leggi_pacchetto(kzero::api::PacchettoIncorporato("esempi/demo"));
        if (!lettura.problemi.empty()) throw std::logic_error("il pacchetto demo ha problemi di forma");
        return lettura.pacchetto;
    }();
    return pacchetto;
}

// Elemento con quel codice in un elenco del pacchetto (il codice deve esistere).
template <class T>
T& per_codice(std::vector<T>& elenco, const std::string& codice) {
    const auto it = std::find_if(elenco.begin(), elenco.end(), [&](const T& x) { return x.code == codice; });
    if (it == elenco.end()) throw std::logic_error("codice non trovato nel pacchetto demo: " + codice);
    return *it;
}

template <class T>
const T& per_codice(const std::vector<T>& elenco, const std::string& codice) {
    const auto it = std::find_if(elenco.begin(), elenco.end(), [&](const T& x) { return x.code == codice; });
    if (it == elenco.end()) throw std::logic_error("codice non trovato nel pacchetto demo: " + codice);
    return *it;
}

// Gruppo di passeggeri con quel codice, in tutti i servizi.
inline kzero::PassengerGroup& gruppo(kzero::Package& p, const std::string& codice) {
    for (kzero::Service& s : p.services)
        for (kzero::PassengerGroup& g : s.groups)
            if (g.code == codice) return g;
    throw std::logic_error("gruppo non trovato nel pacchetto demo: " + codice);
}

inline const kzero::PassengerGroup& gruppo(const kzero::Package& p, const std::string& codice) {
    for (const kzero::Service& s : p.services)
        for (const kzero::PassengerGroup& g : s.groups)
            if (g.code == codice) return g;
    throw std::logic_error("gruppo non trovato nel pacchetto demo: " + codice);
}

inline kzero::Day giorno(const std::string& testo) {
    const auto d = kzero::day_from_text(testo);
    if (!d) throw std::logic_error("data non valida: " + testo);
    return *d;
}

inline kzero::Minutes ora(const std::string& testo) {
    const auto m = kzero::minutes_from_text(testo);
    if (!m) throw std::logic_error("orario non valido: " + testo);
    return *m;
}

}  // namespace kzero::prove
