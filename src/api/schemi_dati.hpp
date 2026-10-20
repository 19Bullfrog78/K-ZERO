#pragma once
// K-Zero API - schemi dei file di dati (incorporati nel programma) e classi dei dati.
// Lo schema e' l'unica fonte delle regole di forma e della classe di ogni campo:
//   "x-kzero-classe": "comune" | "personale" | "particolare" (vale per il campo e per cio' che contiene).
// Il filtro delle uscite parte da questa marcatura ed e' uguale per testo e JSON.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace kzero::api {

// JSON dei file di dati: gli oggetti in ordine di chiave (l'ordine dei campi nei file non conta).
using JsonDati = nlohmann::json;

// Le tre classi dei dati.
enum class Classe {
    Comune,       // fermate pubbliche, scuole, orari, conteggi senza esigenze, mezzi, contratti...
    Personale,    // nomi, indirizzi, case, punti porta a porta, motivi scritti a mano
    Particolare,  // salute: carrozzine, assistente necessario, servizio per disabili
};

// Cosa mostrare nelle uscite: di base i dati personali diventano il codice dell'elemento,
// quelli particolari diventano "riservato".
struct Visibilita {
    bool personali = false;     // --includi-dati-personali
    bool particolari = false;   // --includi-dati-particolari
};

bool visibile(Classe c, const Visibilita& v);

// Testo che prende il posto di un dato particolare nascosto.
inline constexpr const char* kRiservato = "riservato";

class SchemiDati {
public:
    // Gli schemi incorporati nel programma.
    static const SchemiDati& incorporati();

    // Schema di un file di dati, se il programma lo conosce.
    const JsonDati* schema(const std::string& nome, int versione) const;

    // Parte dello schema che descrive il dato a quel percorso JSON di un documento (es. "/luoghi/3/nome"),
    // seguendo properties, items e $ref; nullptr se lo schema non lo prevede.
    const JsonDati* sottoschema(const std::string& nome, int versione, const std::string& puntatore) const;

    // Classe del dato a quel percorso: la marcatura piu' vicina. Senza marcatura: particolare (per prudenza).
    Classe classe(const std::string& nome, int versione, const std::string& puntatore) const;

    // Segue i $ref interni ("#/definitions/...") fino alla definizione vera.
    static const JsonDati& risolvi(const JsonDati& radice, const JsonDati& nodo);

    // Un livello del percorso di un documento visto con lo schema: la radice (livello 0), poi un livello per ogni
    // pezzo del percorso JSON.
    struct Livello {
        bool previsto = false;          // lo schema descrive questo livello (un campo non previsto: false)
        std::optional<Classe> classe;   // marcatura scritta proprio qui (accanto al campo o nella sua definizione)
        // "x-kzero-titolare" di un elemento i cui dati sono tutti personali (es. un'assenza): il campo con il codice
        // dell'elemento comune a cui appartiene e dove sta, es. {"campo": "persona", "file": "persone.json",
        // "elenco": "persone"}.
        const JsonDati* titolare = nullptr;
        // "x-kzero-trasportato" (K-010): l'elemento e' un passeggero ("passeggero") o la casa di una persona ("casa"),
        // i cui dati personali valgono come particolari se e' legato a un luogo sensibile (riservatezza.hpp).
        const JsonDati* trasportato = nullptr;
    };
    // I livelli del percorso: tanti quanti i pezzi del percorso piu' uno. Dopo un pezzo non previsto, anche i livelli
    // seguenti sono non previsti.
    std::vector<Livello> livelli(const std::string& nome, int versione, const std::string& puntatore) const;

private:
    SchemiDati();
    std::map<std::pair<std::string, int>, JsonDati> schemi_;
};

}  // namespace kzero::api
