#pragma once
// K-Zero API - definizione unica dei comandi e dei loro parametri.
// Da questa tabella nascono la lettura della riga di comando, la descrizione del programma
// (kzero descrivi) e l'esecuzione: nulla va riscritto a mano una seconda volta.

#include <string>
#include <vector>

#include "uscita.hpp"

namespace kzero::api {

inline constexpr const char* kNomeProgramma = "kzero";
inline constexpr const char* kUso = "kzero <comando> [parametri]";
// Frase finale dei messaggi di errore sui parametri: indica dove trovare l'elenco completo.
inline constexpr const char* kVediDescrivi = " Elenco completo di comandi e parametri: kzero descrivi.";

// Parametri letti e controllati, pronti per il comando.
struct Parametri {
    Formato formato = Formato::Testo;
    bool includi_dati_personali = false;
};

enum class TipoParametro {
    Interruttore,   // senza valore: presente o assente (es. --includi-dati-personali)
    Scelta,         // un valore fra quelli ammessi (es. --formato json)
};

struct DefinizioneParametro {
    std::string nome;                   // con i due trattini: "--formato"
    TipoParametro tipo = TipoParametro::Interruttore;
    std::vector<std::string> valori;    // Scelta: valori ammessi, in ordine alfabetico
    std::string predefinito;            // Scelta: valore usato se il parametro manca
    std::string descrizione;
    // Mette il valore nei Parametri ("" per un interruttore presente).
    void (*applica)(Parametri&, const std::string& valore) = nullptr;
};

// Nome e valore del parametro --formato, comune a tutti i comandi.
inline constexpr const char* kParametroFormato = "--formato";
inline constexpr const char* kFormatoJson = "json";

struct DefinizioneComando;
// Esegue il comando e restituisce l'uscita completa (testo o JSON).
using Esecutore = std::string (*)(const DefinizioneComando&, const Parametri&);

struct DefinizioneComando {
    std::string nome;
    std::string descrizione;
    std::vector<DefinizioneParametro> parametri;   // in ordine alfabetico, --formato compreso
    Schema schema;                                 // schema della sua uscita JSON
    Esecutore esegui = nullptr;
};

// Tutti i comandi, in ordine alfabetico.
const std::vector<DefinizioneComando>& comandi();

// Comando con quel nome, oppure nullptr.
const DefinizioneComando* trova_comando(const std::string& nome);

// Nomi dei comandi separati da virgola: "descrivi, dimostrazione, versione".
std::string nomi_comandi();

// Tutti gli schemi delle uscite JSON (comandi ed errore), in ordine alfabetico.
std::vector<Schema> schemi();

}  // namespace kzero::api
