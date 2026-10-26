#pragma once
// K-Zero API - chiudere un pacchetto nella cassaforte e riaprirlo (K-010): dai documenti letti ai file normali e al
// contenuto della cassaforte, e ritorno. Lo usano i comandi chiudi, richiudi ed esporta (comando_cassaforte.cpp) e le prove.

#include <map>
#include <string>

#include "cassaforte.hpp"
#include "lettore.hpp"

namespace kzero::api::chiusura {

struct Separazione {
    std::map<std::string, JsonDati> normali;   // file -> documento senza i valori personali e particolari
    cassaforte::Contenuto contenuto;
};

// Separa un pacchetto letto senza errori di forma (lettura.forma_valida()): ogni valore personale o particolare va nella
// cassaforte, legato al suo elemento per codice (classi.hpp); i dati personali dei passeggeri e delle case legati a un
// luogo sensibile (riservatezza.hpp) vanno nella sezione dei legati, che si decifra solo con tutti e due i permessi. Il
// manifesto resta com'e' (si cambia con manifesto_chiuso).
Separazione separa(const Lettura& lettura);

// Il manifesto con la cassaforte dichiarata (codice e numero della chiusura), o senza ("cassaforte": false).
JsonDati manifesto_chiuso(const JsonDati& manifesto, const std::string& id, std::uint32_t generazione);
JsonDati manifesto_in_chiaro(const JsonDati& manifesto);

// Il testo di un file di dati come lo scrive K-Zero: JSON con rientro di 2 spazi, a capo finale.
std::string testo_file(const JsonDati& documento);

// I dati di due letture sono gli stessi? (stessi file, stessi documenti uniti; nel manifesto non conta la cassaforte).
// Se no, in "differenza" il primo file diverso (mai i valori).
bool stessi_dati(const Lettura& a, const Lettura& b, std::string& differenza);

// Le voci della cassaforte aperta che l'utente puo' leggere, pronte per il lettore.
ValoriCassaforte valori_di(const cassaforte::Cassaforte& c);

}  // namespace kzero::api::chiusura
