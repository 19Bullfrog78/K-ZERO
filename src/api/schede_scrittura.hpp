#pragma once
// K-Zero API - schede dei servizi (K-021): come una versione di una scheda diventa servizio, gruppo, corsa e calendario nei file del
// pacchetto (docs/formato-dati.md, "schede_servizi.json"), il registro delle schede come documento, e il confronto degli elementi del
// pacchetto con quelli che la scheda da' (K-CTR-170, in kzero controlla). Una regola sola, usata per scrivere e per controllare.

#include <string>
#include <vector>

#include "lettore.hpp"
#include "schede.hpp"

namespace kzero::api::schede {

inline constexpr const char* kFile = "schede_servizi.json";
// K-023: il motivo delle sospensioni del calendario fatte dalle interruzioni di una scheda.
inline constexpr const char* kMotivoInterruzione = "Interruzione della scheda";

struct Elementi {
    JsonDati servizio;     // con il gruppo dentro (domanda.gruppi)
    JsonDati corsa;
    JsonDati calendario;
};
// Gli elementi di una versione (i codici sono quelli della versione).
Elementi elementi(const kzero::ServiceCard& s, const kzero::CardVersion& v, const kzero::CardProfile& pr, const kzero::Index& idx);
// Una casa porta a porta senza dati personali (solo il codice e il tipo), per luoghi.json.
JsonDati casa(const kzero::Code& codice);

// Il documento schede_servizi.json.
JsonDati registro(const std::vector<kzero::ServiceCard>& schede, const std::vector<kzero::RemovedCardVersions>& tolte);

// Il pacchetto in chiaro, in memoria, con alcuni file cambiati. Un documento preparato dal programma che non rispetta lo schema e'
// un errore del programma (uscita 3), mai un pacchetto scritto a meta'.
Lettura con_documenti(const Lettura& vecchia, const std::map<std::string, JsonDati>& documenti);

// I documenti del pacchetto dopo una lettura delle schede (merge) o dopo averne tolta una (removal): elementi nuovi aggiunti, quelli
// delle versioni tolte tolti, i calendari accorciati (o ripresi) con la loro fine, le case nuove, il registro.
std::map<std::string, JsonDati> documenti_dopo(const Lettura& vecchia, const std::vector<kzero::ServiceCard>& schede,
                                               const std::vector<kzero::RemovedCardVersions>& tolte,
                                               const std::vector<std::pair<kzero::Code, int>>& aggiunte,
                                               const std::vector<kzero::CardVersion>& via,
                                               const std::vector<kzero::CardsMerge::Cut>& fini, const std::vector<kzero::Code>& case_nuove);

// K-CTR-170: gli elementi di ogni versione del registro confrontati con quelli che la scheda da' (i dati comuni che vengono dalla
// scheda: niente tipo, pedana, nome, liberta' oraria, codici esterni, che dipendono dal profilo o sono riservati).
std::vector<kzero::Problem> controlla_elementi(const Lettura& lettura);

}  // namespace kzero::api::schede
