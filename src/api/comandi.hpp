#pragma once
// K-Zero API - definizione unica dei comandi e dei loro parametri.
// Da questa tabella nascono la lettura della riga di comando, la descrizione del programma
// (kzero descrivi) e l'esecuzione: nulla va riscritto a mano una seconda volta.

#include <optional>
#include <string>
#include <vector>

#include "uscita.hpp"

namespace kzero::api {

inline constexpr const char* kNomeProgramma = "kzero";
inline constexpr const char* kUso = "kzero <comando> [argomenti] [parametri]";
// Frase finale dei messaggi di errore sui parametri: indica dove trovare l'elenco completo.
inline constexpr const char* kVediDescrivi = " Elenco completo di comandi e parametri: kzero descrivi.";

// Parametri letti e controllati, pronti per il comando.
struct Parametri {
    Formato formato = Formato::Testo;
    bool includi_dati_personali = false;
    bool includi_dati_particolari = false;
    std::optional<kzero::Day> data;       // --data (kzero giornata, kzero ore)
    std::optional<kzero::Day> dal, al;    // --dal e --al (kzero ore, K-011)
    std::string persona;                  // --persona: vuoto = non scritto
    std::string mezzo;                    // --mezzo: vuoto = non scritto
    // Cassaforte (K-010)
    std::string utente;                   // --utente: chi apre la cassaforte; vuoto = non scritto
    bool password_da_ingresso = false;    // --password-da-ingresso: le password dall'ingresso standard, una per riga
    std::string scrivi;                   // --scrivi: la cartella che il comando scrive o cambia; vuoto = non scritto
    std::optional<kzero::Day> conservazione;   // --conservazione: fino a quando si tengono i dati
    std::string nuovo_utente;             // --nuovo-utente
    std::string permessi;                 // --permessi: personali, particolari, tutti
    std::string utente_da_togliere;       // --utente-da-togliere
    std::string per_utente;               // --per-utente: password nuova per un altro utente
    // Elenco utenti del committente (K-019)
    std::string profilo;                  // --profilo: profilo di lettura (profili_elenchi.json)
    int anno_scolastico = 0;              // --anno-scolastico AAAA-AAAA: il primo anno (0 = non scritto)
    std::string elenco;                   // --elenco: completo o parziale
    std::string utente_elenco;            // --utente-elenco: codice dell'utente dell'elenco (kzero sblocco)
    std::string tratta;                   // --tratta: sigla della tratta del profilo (kzero sblocco)
    std::string riferimento;              // --riferimento: riferimento dello sblocco (es. la mail del committente)
    std::optional<kzero::Day> data_sblocco;   // --data dello sblocco, scritta dall'utente
    bool togli = false;                   // --togli: toglie lo sblocco
    std::vector<std::string> argomenti;   // argomenti del comando (es. la cartella), nell'ordine della definizione
};

// Argomento obbligatorio scritto senza nome, dopo il comando (es. kzero controlla <cartella>).
struct DefinizioneArgomento {
    std::string nome;          // "cartella"
    std::string descrizione;
};

enum class TipoParametro {
    Interruttore,   // senza valore: presente o assente (es. --includi-dati-personali)
    Scelta,         // un valore fra quelli ammessi (es. --formato json)
    Valore,         // un valore libero di una certa forma (es. --data 2026-10-15, --persona P-A)
};

// Forma di un valore libero; la controlla la lettura dei parametri, prima del comando.
enum class FormaValore {
    Data,      // AAAA-MM-GG, un giorno che esiste nel calendario
    Codice,    // codice K-Zero: lettere senza accenti, cifre, '-', '_' e '.'; al massimo 64 caratteri
    Percorso,  // una cartella: testo non vuoto, al massimo 4096 byte, senza a capo (K-010, --scrivi)
    Testo,     // K-019: una riga di testo (UTF-8, da 1 a 300 byte, senza caratteri di controllo), es. --riferimento
    AnnoScolastico,   // K-019: AAAA-AAAA, due anni di fila (es. 2026-2027)
};

struct DefinizioneParametro {
    std::string nome;                   // con i due trattini: "--formato"
    TipoParametro tipo = TipoParametro::Interruttore;
    std::vector<std::string> valori;    // Scelta: valori ammessi, in ordine alfabetico
    std::string predefinito;            // Scelta: valore usato se il parametro manca
    FormaValore forma = FormaValore::Codice;   // Valore: forma del valore
    bool obbligatorio = false;                 // Valore e Scelta: va sempre scritto
    std::vector<std::string> incompatibili;    // parametri che non si possono scrivere insieme a questo
    std::string descrizione;
    // Mette il valore nei Parametri ("" per un interruttore presente).
    void (*applica)(Parametri&, const std::string& valore) = nullptr;
};

// Nome e valore del parametro --formato, comune a tutti i comandi.
inline constexpr const char* kParametroFormato = "--formato";
inline constexpr const char* kFormatoJson = "json";

// Risultato di un comando: l'uscita completa (testo o JSON) e il codice di uscita
// (0 riuscito; 1 se il comando ha trovato errori nei dati, es. kzero controlla).
struct Risultato {
    std::string uscita;
    int codice = 0;
};

struct DefinizioneComando;
// Esegue il comando e restituisce il risultato.
using Esecutore = Risultato (*)(const DefinizioneComando&, const Parametri&);

struct DefinizioneComando {
    std::string nome;
    std::string descrizione;
    std::vector<DefinizioneArgomento> argomenti;   // obbligatori, nell'ordine in cui si scrivono
    std::vector<DefinizioneParametro> parametri;   // in ordine alfabetico, --formato compreso
    Schema schema;                                 // schema della sua uscita JSON
    Esecutore esegui = nullptr;
};

// Tutti i comandi, in ordine alfabetico.
const std::vector<DefinizioneComando>& comandi();

// Comando con quel nome, oppure nullptr.
const DefinizioneComando* trova_comando(const std::string& nome);

// Nomi dei comandi separati da virgola: "controlla, descrivi, dimostrazione, versione".
std::string nomi_comandi();

// Come si scrive il comando: "kzero controlla <cartella> [parametri]".
std::string forma_comando(const DefinizioneComando& c);

// Tutti gli schemi delle uscite JSON prodotte oggi (comandi ed errore), in ordine alfabetico.
std::vector<Schema> schemi();

// Schemi di uscite non piu' prodotte, tenuti per controllare i documenti salvati prima.
const std::vector<Schema>& schemi_ritirati();

}  // namespace kzero::api
