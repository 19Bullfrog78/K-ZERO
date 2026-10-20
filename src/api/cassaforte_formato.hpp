#pragma once
// K-Zero API - il formato del file della cassaforte (cassaforte.kzc), byte per byte: docs/cassaforte.md. Lo usano la
// cassaforte (cassaforte.cpp), le prove di manomissione e il fuzzing, che costruiscono file cambiati come farebbe un altro
// programma. Nessuna di queste funzioni decide se un file e' autentico: lo fa Cassaforte::apri.
// Tutti i numeri sono little-endian.

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include "cassaforte.hpp"

namespace kzero::api::cassaforte::formato {

using cripto::Bytes;

constexpr unsigned char kMagia[8] = {'K', 'Z', '-', 'C', 'A', 'S', 'S', 'A'};
constexpr unsigned char kMagiaLista[8] = {'K', 'Z', '-', 'L', 'I', 'S', 'T', 'A'};
constexpr char kContesto[9] = "KZ-CASSA";   // contesto delle chiavi derivate (crypto_kdf, 8 caratteri)
// Numeri delle chiavi derivate da una chiave di livello: ogni chiave ha un solo uso.
constexpr std::uint64_t kPerSezione = 1, kPerAutenticazione = 2, kPerStoria = 3, kPerRegistro = 4;

constexpr int kBase = 0, kPersonali = 1, kParticolari = 2, kLivelli = 3;
constexpr std::size_t kVoceFessura = 1 + 4 + cripto::kChiave;                       // livello, versione, chiave
constexpr std::size_t kInBusta = kId + 1 + 4 + cripto::kChiave;                      // codice, livello, versione, chiave
constexpr std::size_t kBusta = cripto::kSovraccaricoBusta + kInBusta;               // 101 byte
constexpr std::size_t kVoceStoria = 4 + cripto::kChiave;                             // versione, chiave
constexpr std::size_t kMassimoBuste = kMassimoUtenti * kLivelli;
constexpr int kProfonditaSezione = 64;

const char* nome_livello(int l);
bool ha(const Permessi& p, int livello);       // il livello base l'hanno tutti
std::uint8_t bit(const Permessi& p);           // 1 personali, 2 particolari
int livelli_di(const Permessi& p);             // quanti livelli ha (base compreso)
bool codice_valido(const std::string& s);      // codice di un utente: come i codici di K-Zero

[[noreturn]] void errore(ErroreCassaforte::Tipo t, const std::string& m);
[[noreturn]] void rovinata(const std::string& dove);
[[noreturn]] void manomessa(const std::string& cosa);

// --- Numeri e byte (little-endian) ----------------------------------------------------------------------------------

class Scrittore {
public:
    void u8(std::uint8_t v) { b_.push_back(v); }
    void u16(std::uint16_t v) { intero(v, 2); }
    void u32(std::uint32_t v) { intero(v, 4); }
    void u64(std::uint64_t v) { intero(v, 8); }
    void byte(const unsigned char* p, std::size_t n) { b_.insert(b_.end(), p, p + n); }
    void byte(const Bytes& v) { byte(v.data(), v.size()); }
    void testo8(const std::string& s) {
        u8(static_cast<std::uint8_t>(s.size()));
        byte(reinterpret_cast<const unsigned char*>(s.data()), s.size());
    }
    Bytes& dati() { return b_; }

private:
    void intero(std::uint64_t v, int n) {
        for (int i = 0; i < n; ++i) b_.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
    }
    Bytes b_;
};

// Legge senza mai uscire dal testo: al primo pezzo che manca ok() diventa false e i valori seguenti sono zero.
class Lettore {
public:
    Lettore(const unsigned char* p, std::size_t n) : p_(p), n_(n) {}
    bool ok() const { return ok_; }
    std::size_t posizione() const { return i_; }
    std::size_t resto() const { return n_ - i_; }
    std::uint8_t u8() { return static_cast<std::uint8_t>(intero(1)); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(intero(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(intero(4)); }
    std::uint64_t u64() { return intero(8); }
    Bytes byte(std::size_t k) {
        if (!ok_ || k > n_ - i_) {
            ok_ = false;
            return Bytes();
        }
        Bytes b(p_ + i_, p_ + i_ + k);
        i_ += k;
        return b;
    }
    std::string testo8() {
        const std::size_t n = u8();
        const Bytes b = byte(n);
        return std::string(b.begin(), b.end());
    }
    void fallisci() { ok_ = false; }

private:
    std::uint64_t intero(int n) {
        if (!ok_ || static_cast<std::size_t>(n) > n_ - i_) {
            ok_ = false;
            return 0;
        }
        std::uint64_t v = 0;
        for (int i = 0; i < n; ++i) v |= static_cast<std::uint64_t>(p_[i_ + static_cast<std::size_t>(i)]) << (8 * i);
        i_ += static_cast<std::size_t>(n);
        return v;
    }
    const unsigned char* p_;
    std::size_t n_;
    std::size_t i_ = 0;
    bool ok_ = true;
};


struct Utente {
    std::string codice;
    Permessi permessi;
    Bytes pubblica;   // X25519
    Bytes sale;
    cripto::ParametriKdf kdf;
    Bytes nonce;
    Bytes fessura;    // cifrata
};

struct Busta {
    std::string codice;
    int livello = kBase;
    std::uint32_t versione = 0;
    Bytes busta;
};

struct Sezione {
    Bytes nonce;
    std::uint64_t lunghezza = 0;
    Bytes impronta;
};

struct Intestazione {
    Bytes id;
    std::uint32_t generazione = 1;
    std::array<std::uint32_t, kLivelli> versioni{{1, 1, 1}};
    std::string conservazione;
    std::uint32_t registro_voci = 0;
    Bytes registro_impronta = Bytes(cripto::kImpronta, 0);
    std::uint64_t valori_personali = 0;
    std::uint64_t valori_particolari = 0;
    std::vector<Utente> utenti;
    std::vector<Busta> buste;
    std::array<Bytes, kLivelli> storie;   // nonce + testo cifrato; vuota con la versione 1
    std::array<Sezione, 2> sezioni;       // dati personali, dati particolari
    std::array<Bytes, kLivelli> autenticazione{{Bytes(cripto::kAutenticazione, 0), Bytes(cripto::kAutenticazione, 0),
                                                 Bytes(cripto::kAutenticazione, 0)}};

    const Utente* utente(const std::string& codice) const {
        for (const Utente& u : utenti)
            if (u.codice == codice) return &u;
        return nullptr;
    }
    Utente* utente(const std::string& codice) {
        for (Utente& u : utenti)
            if (u.codice == codice) return &u;
        return nullptr;
    }
};

std::size_t lunghezza_fessura(const Permessi& p);
std::size_t lunghezza_storia(std::uint32_t versione);

// Tutta l'intestazione fino alle tre autenticazioni comprese (cosi' come sono nella struttura).
Bytes scrivi_intestazione(const Intestazione& i);

struct Analizzato {
    Intestazione intestazione;
    std::size_t fine = 0;   // dove finisce l'intestazione (e cominciano le sezioni)
};

// Legge l'intestazione con tutti i controlli di forma (nessuna chiave serve). Lancia ErroreCassaforte.
Analizzato analizza(std::string_view file);

// Dati associati e messaggi autenticati (docs/cassaforte.md).
Bytes inizio(const Bytes& id);                                    // magia, versione, codice della cassaforte
Bytes ad_fessura(const Bytes& id, const Utente& u);
Bytes ad_livello(const Bytes& id, int livello, std::uint32_t versione);   // buste e storie
Bytes ad_sezione(const Bytes& id, int livello, std::uint32_t versione, std::uint32_t generazione);
Bytes lista(const Intestazione& i, int livello);                 // chi ha un livello: codici e chiavi pubbliche
Bytes contenuto_busta(const Bytes& id, int livello, std::uint32_t versione, const cripto::Segreto& chiave);

// Il testo in chiaro di una sezione e ritorno (false se la forma non e' quella attesa).
std::string testo_sezione(const std::vector<Voce>& voci);
bool voci_di(std::string_view testo, Classe classe, std::vector<Voce>& out);

// La chiave derivata da una chiave di livello per un uso (crypto_kdf, contesto kContesto).
cripto::Segreto derivata(const cripto::Segreto& chiave_livello, std::uint64_t uso);

}  // namespace kzero::api::cassaforte::formato
