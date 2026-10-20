#pragma once
// K-Zero - strumenti delle prove interne sulla cassaforte (K-010): Argon2id leggero, ora fissa e password di prova al posto
// del terminale (accesso::Ambiente: il programma kzero non li ha), una cartella di lavoro che si cancella da sola, i comandi
// lanciati dalla loro tabella.
#include <atomic>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "accesso.hpp"
#include "comandi.hpp"
#include "cripto.hpp"
#include "lettore.hpp"
#include "parametri.hpp"
#include "password.hpp"

namespace kzero::prove {

inline const kzero::api::cripto::PoliticaKdf& politica_leggera() {
    static const kzero::api::cripto::PoliticaKdf p{{1, 8}, {10, 2u * 1024u * 1024u}, {1, 8}};
    return p;
}

inline std::string ora_fissa() { return "2026-10-20T10:00:00Z"; }

// Le password, una dopo l'altra, come se le scrivesse la persona.
class PasswordDiProva : public kzero::api::password::Ingresso {
public:
    explicit PasswordDiProva(std::vector<std::string> righe) : righe_(std::move(righe)) {}
    kzero::api::cripto::Segreto leggi(const std::string&) override {
        if (i_ >= righe_.size()) throw std::runtime_error("ingresso finito");
        const std::string& r = righe_[i_++];
        return kzero::api::cripto::Segreto(reinterpret_cast<const unsigned char*>(r.data()), r.size());
    }
    bool terminale() const override { return false; }

private:
    std::vector<std::string> righe_;
    std::size_t i_ = 0;
};

// Per tutta la vita dell'oggetto i comandi usano Argon2id leggero, l'ora fissa e le password date (una lista per comando).
class AmbienteDiProva {
public:
    AmbienteDiProva() {
        a_.orologio = ora_fissa;
        a_.politica = &politica_leggera();
        a_.ingresso = [this]() -> std::unique_ptr<kzero::api::password::Ingresso> {
            return std::make_unique<PasswordDiProva>(password);
        };
        kzero::api::accesso::per_le_prove_imposta_ambiente(&a_);
    }
    ~AmbienteDiProva() { kzero::api::accesso::per_le_prove_imposta_ambiente(nullptr); }
    AmbienteDiProva(const AmbienteDiProva&) = delete;
    AmbienteDiProva& operator=(const AmbienteDiProva&) = delete;
    const kzero::api::accesso::Ambiente& ambiente() const { return a_; }
    std::vector<std::string> password;   // le password del prossimo comando

private:
    kzero::api::accesso::Ambiente a_;
};

// Una cartella di lavoro nuova, cancellata alla fine.
class CartellaDiLavoro {
public:
    CartellaDiLavoro() {
        static std::atomic<int> n{0};
        percorso_ = std::filesystem::temp_directory_path() /
                    ("kzero-prove-" + kzero::api::cripto::esadecimale(kzero::api::cripto::casuali(6)) + "-" + std::to_string(n++));
        std::filesystem::create_directories(percorso_);
    }
    ~CartellaDiLavoro() {
        std::error_code ec;
        std::filesystem::remove_all(percorso_, ec);
    }
    const std::filesystem::path& percorso() const { return percorso_; }
    std::filesystem::path operator/(const std::string& nome) const { return percorso_ / nome; }

private:
    std::filesystem::path percorso_;
};

// Il pacchetto demo (incorporato nel programma) scritto in una cartella.
inline void scrivi_demo(const std::filesystem::path& cartella) {
    std::filesystem::create_directories(cartella);
    const kzero::api::PacchettoIncorporato demo("esempi/demo");
    for (const auto& v : demo.voci()) {
        const auto f = demo.leggi(v.nome, kzero::api::kMassimoByteFile);
        std::FILE* out = std::fopen((cartella / v.nome).c_str(), "wb");
        if (!out) throw std::runtime_error("non si scrive " + v.nome);
        std::fwrite(f.testo.data(), 1, f.testo.size(), out);
        std::fclose(out);
    }
}

// Lancia un comando dalla sua tabella, come il programma (senza --formato: testo). Lancia ErroreKZero.
inline kzero::api::Risultato comando(const std::vector<std::string>& argomenti) {
    const kzero::api::DefinizioneComando* c = kzero::api::trova_comando(argomenti.at(0));
    if (!c) throw std::logic_error("comando sconosciuto: " + argomenti.at(0));
    const std::vector<std::string> resto(argomenti.begin() + 1, argomenti.end());
    return c->esegui(*c, kzero::api::leggi_parametri(*c, resto));
}

}  // namespace kzero::prove
