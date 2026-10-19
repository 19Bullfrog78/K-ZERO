#pragma once
// K-Zero API - lettura di un pacchetto dati: dai file JSON al modello del nucleo.
// Per ogni file: JSON valido e senza chiavi ripetute, schema e versione giusti, rispetto dello schema.
// Un file con un problema di forma non viene usato per niente (mai letto a meta'). Se c'e' almeno un errore
// di forma i controlli sul contenuto non si fanno: darebbero risultati falsati.

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
#include "domain_models.hpp"

namespace kzero::api {

// Limite di buon senso per un file di dati: oltre e' certamente un errore (un file sbagliato o di un altro
// programma), e leggerlo potrebbe esaurire la memoria del PC.
inline constexpr std::uintmax_t kMassimoMBFile = 256;
inline constexpr std::uintmax_t kMassimoByteFile = kMassimoMBFile * 1024 * 1024;

// Esito della lettura di un file del pacchetto. Si legge solo un file normale, mai piu' del massimo di byte.
struct FileLetto {
    enum class Esito {
        Letto,
        Illeggibile,         // permessi, disco
        CollegamentoRotto,   // collegamento che non porta a nessun file
        NonNormale,          // dispositivo, "pipe", presa di rete, cartella...: non un file normale
        FuoriCartella,       // collegamento a un file fuori dalla cartella del pacchetto
        TroppoGrande,        // piu' del massimo, anche se la dimensione non si conosceva o il file cresceva
    };
    Esito esito = Esito::Illeggibile;
    std::string testo;   // solo con Letto
};

// Dove stanno i file del pacchetto.
class Sorgente {
public:
    struct Voce {
        std::string nome;
        bool cartella = false;
    };
    virtual ~Sorgente() = default;
    virtual std::vector<Voce> voci() const = 0;   // in ordine di nome
    // Legge un file del pacchetto, al massimo "massimo" byte: oltre, l'esito e' TroppoGrande.
    virtual FileLetto leggi(const std::string& nome, std::uintmax_t massimo) const = 0;
};

// Lettura blindata di un file della cartella del pacchetto (docs/sicurezza.md):
// - si legge solo un file normale: non un dispositivo (es. /dev/zero), una "pipe", una cartella;
// - un collegamento (link) si segue, ma il file a cui porta deve essere un file normale dentro la cartella del
//   pacchetto (la cartella stessa puo' essere un collegamento, es. una cartella sincronizzata);
// - mai piu' di "massimo" byte, anche se il sistema non dice la dimensione o il file cresce mentre lo si legge:
//   si legge fino al massimo piu' uno e ci si ferma.
FileLetto leggi_dal_disco(const std::filesystem::path& cartella, const std::string& nome, std::uintmax_t massimo);

// Una cartella sul disco.
class CartellaSuDisco : public Sorgente {
public:
    explicit CartellaSuDisco(std::filesystem::path cartella) : cartella_(std::move(cartella)) {}
    std::vector<Voce> voci() const override;
    FileLetto leggi(const std::string& nome, std::uintmax_t massimo) const override;

private:
    std::filesystem::path cartella_;
};

// Un pacchetto incorporato nel programma, es. "esempi/demo" (vedi risorse.hpp).
class PacchettoIncorporato : public Sorgente {
public:
    explicit PacchettoIncorporato(std::string cartella) : cartella_(std::move(cartella)) {}
    std::vector<Voce> voci() const override;
    FileLetto leggi(const std::string& nome, std::uintmax_t massimo) const override;

private:
    std::string cartella_;
};

struct Lettura {
    kzero::Package pacchetto;
    std::vector<kzero::Problem> problemi;   // problemi di forma dei file
    std::map<std::string, int> versioni;    // file usati -> versione del loro schema
    std::set<std::string> presenti;         // file del formato presenti nella cartella
    // Per il filtro delle posizioni (riservatezza.hpp), ricavati con gli schemi da ogni file letto:
    // gli elementi degli elenchi principali per codice: (file, elenco) -> codice -> indice nell'elenco;
    std::map<std::pair<std::string, std::string>, std::map<std::string, std::size_t>> codici;
    // gli elementi che appartengono a un altro ("x-kzero-titolare", es. le assenze): file -> puntatore -> codice.
    std::map<std::string, std::map<std::string, std::string>> titolari;

    bool usato(const std::string& file) const { return versioni.count(file) > 0; }
    // Nessun errore di forma (i file non riconosciuti sono solo avvisi): si possono controllare i contenuti.
    bool forma_valida() const;
};

Lettura leggi_pacchetto(const Sorgente& sorgente);

}  // namespace kzero::api
