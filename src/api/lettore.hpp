#pragma once
// K-Zero API - lettura di un pacchetto dati: dai file JSON al modello del nucleo.
// Per ogni file: JSON valido e senza chiavi ripetute, schema e versione giusti, rispetto dello schema.
// Un file con un problema di forma non viene usato per niente (mai letto a meta'). Se c'e' almeno un errore
// di forma i controlli sul contenuto non si fanno: darebbero risultati falsati.

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "controlli.hpp"
#include "domain_models.hpp"

namespace kzero::api {

// Dove stanno i file del pacchetto.
class Sorgente {
public:
    struct Voce {
        std::string nome;
        bool cartella = false;
    };
    virtual ~Sorgente() = default;
    virtual std::vector<Voce> voci() const = 0;   // in ordine di nome
    virtual std::optional<std::string> leggi(const std::string& nome) const = 0;   // nessun valore se illeggibile
};

// Una cartella sul disco.
class CartellaSuDisco : public Sorgente {
public:
    explicit CartellaSuDisco(std::filesystem::path cartella) : cartella_(std::move(cartella)) {}
    std::vector<Voce> voci() const override;
    std::optional<std::string> leggi(const std::string& nome) const override;

private:
    std::filesystem::path cartella_;
};

// Un pacchetto incorporato nel programma, es. "esempi/demo" (vedi risorse.hpp).
class PacchettoIncorporato : public Sorgente {
public:
    explicit PacchettoIncorporato(std::string cartella) : cartella_(std::move(cartella)) {}
    std::vector<Voce> voci() const override;
    std::optional<std::string> leggi(const std::string& nome) const override;

private:
    std::string cartella_;
};

struct Lettura {
    kzero::Package pacchetto;
    std::vector<kzero::Problem> problemi;   // problemi di forma dei file
    std::map<std::string, int> versioni;    // file usati -> versione del loro schema
    std::set<std::string> presenti;         // file del formato presenti nella cartella

    bool usato(const std::string& file) const { return versioni.count(file) > 0; }
    // Nessun errore di forma (i file non riconosciuti sono solo avvisi): si possono controllare i contenuti.
    bool forma_valida() const;
};

Lettura leggi_pacchetto(const Sorgente& sorgente);

}  // namespace kzero::api
