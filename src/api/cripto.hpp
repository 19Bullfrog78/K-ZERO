#pragma once
// K-Zero API - cifratura (K-010): uno strato sottile sopra libsodium, usata come prescrive la sua documentazione. Niente
// cifratura fatta in casa: qui ci sono solo le chiamate alla libreria, con i controlli e la memoria protetta.
// - XChaCha20-Poly1305 (crypto_aead_xchacha20poly1305_ietf): cifratura autenticata, nonce casuali di 24 byte;
// - Argon2id v1.3 (crypto_pwhash): chiave dalla password, parametri controllati prima di calcolare;
// - X25519 "sealed box" (crypto_box_seal): le chiavi dei dati chiuse per un utente, senza conoscere la sua password;
// - Ed25519 (crypto_sign): la firma dell'intestazione della cassaforte;
// - BLAKE2b (crypto_generichash) per le impronte, crypto_kdf per le chiavi derivate.
// Chiavi, password e testo decifrato stanno in memoria protetta (Segreto): pagine di guardia, bloccata in memoria dove il
// sistema lo permette, esclusa dai core dump, azzerata quando si libera.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kzero::api::cripto {

using Bytes = std::vector<unsigned char>;

inline constexpr std::size_t kChiave = 32;           // chiavi simmetriche (XChaCha20-Poly1305, crypto_kdf)
inline constexpr std::size_t kNonce = 24;            // nonce di XChaCha20-Poly1305
inline constexpr std::size_t kEtichetta = 16;        // etichetta di autenticazione di XChaCha20-Poly1305
inline constexpr std::size_t kSale = 16;             // sale di Argon2id
inline constexpr std::size_t kImpronta = 32;         // BLAKE2b-256
inline constexpr std::size_t kPubblicaBox = 32;      // X25519
inline constexpr std::size_t kSegretaBox = 32;
inline constexpr std::size_t kSovraccaricoBusta = 48;   // crypto_box_SEALBYTES
inline constexpr std::size_t kPubblicaFirma = 32;    // Ed25519
inline constexpr std::size_t kSegretaFirma = 64;
inline constexpr std::size_t kFirma = 64;

// Avvia la libreria (sodium_init); si puo' chiamare piu' volte. Lancia std::runtime_error se non si avvia.
void avvia();

// Memoria protetta (sodium_malloc): non copiabile, azzerata e liberata alla fine.
class Segreto {
public:
    Segreto() = default;
    explicit Segreto(std::size_t capienza);
    Segreto(const unsigned char* dati, std::size_t n);
    ~Segreto();
    Segreto(const Segreto&) = delete;
    Segreto& operator=(const Segreto&) = delete;
    Segreto(Segreto&& altro) noexcept;
    Segreto& operator=(Segreto&& altro) noexcept;

    unsigned char* dati() { return p_; }
    const unsigned char* dati() const { return p_; }
    std::size_t dimensione() const { return n_; }
    std::size_t capienza() const { return capienza_; }
    bool vuoto() const { return n_ == 0; }
    // Fissa la lunghezza usata (al massimo la capienza).
    void usa(std::size_t n);
    std::string_view testo() const { return {reinterpret_cast<const char*>(p_), n_}; }

private:
    unsigned char* p_ = nullptr;
    std::size_t n_ = 0;
    std::size_t capienza_ = 0;
};

void casuali(unsigned char* dove, std::size_t n);
Bytes casuali(std::size_t n);

// --- Argon2id -------------------------------------------------------------------------------------------------------
struct ParametriKdf {
    std::uint32_t operazioni = 0;    // passaggi sulla memoria (opslimit)
    std::uint32_t memoria_kib = 0;   // memoria in KiB (memlimit / 1024)
};

// Quali parametri si accettano aprendo una cassaforte e quali si usano per una fessura nuova. Il programma kzero usa
// sempre la politica di rilascio (nessun parametro o variabile la cambia); solo le prove interne e il fuzzing, che sono
// programmi a parte, ne usano una con parametri bassi (DECISIONI.md, K-010).
struct PoliticaKdf {
    ParametriKdf minimi;   // sotto: fessura rifiutata (parametri abbassati)
    ParametriKdf massimi;  // sopra: rifiutata prima di calcolare (parametri assurdi, es. memoria enorme)
    ParametriKdf nuovi;    // per le fessure nuove
};

// Politica del programma: fessure nuove con 256 MiB e 3 passaggi; si accettano da quei valori fino a 2 GiB e 10 passaggi.
const PoliticaKdf& politica_di_rilascio();

enum class EsitoParametri { Ammessi, TroppoBassi, Assurdi };
EsitoParametri controlla_parametri(const ParametriKdf& p, const PoliticaKdf& politica);

// Chiave di 32 byte dalla password (gia' normalizzata) e dal sale. I parametri vanno controllati prima. Lancia
// std::runtime_error se la libreria non riesce (es. memoria insufficiente).
Segreto deriva(const Segreto& password, const unsigned char* sale, const ParametriKdf& p);

// --- Cifratura autenticata (XChaCha20-Poly1305) -------------------------------------------------------------------------
// Testo cifrato = dati cifrati + etichetta di 16 byte; ad = dati associati, autenticati ma non cifrati.
Bytes cifra(const Segreto& chiave, const unsigned char* nonce, const unsigned char* dati, std::size_t n, const Bytes& ad);
// false se la chiave e' sbagliata o se il testo cifrato, il nonce o i dati associati sono stati cambiati.
bool decifra(const Segreto& chiave, const unsigned char* nonce, const unsigned char* cifrato, std::size_t n, const Bytes& ad,
             Segreto& in_chiaro);

// --- Buste (X25519 sealed box) -------------------------------------------------------------------------------------
struct CoppiaBox {
    Bytes pubblica;
    Segreto segreta;
};
CoppiaBox nuova_coppia_box();
Bytes pubblica_box(const Segreto& segreta);   // la chiave pubblica di una chiave segreta
Bytes sigilla(const unsigned char* dati, std::size_t n, const Bytes& pubblica);
bool apri_busta(const Bytes& busta, const Bytes& pubblica, const Segreto& segreta, Segreto& in_chiaro);

// --- Firme (Ed25519) -------------------------------------------------------------------------------------------------
struct CoppiaFirma {
    Bytes pubblica;
    Segreto segreta;
};
CoppiaFirma nuova_coppia_firma();
Bytes pubblica_firma(const Segreto& segreta);
Bytes firma(const Bytes& messaggio, const Segreto& segreta);
bool verifica_firma(const Bytes& firma, const Bytes& messaggio, const Bytes& pubblica);

// --- Impronte e chiavi derivate --------------------------------------------------------------------------------------
// BLAKE2b-256 (senza chiave).
Bytes impronta(const unsigned char* dati, std::size_t n);
inline Bytes impronta(const Bytes& b) { return impronta(b.data(), b.size()); }
inline Bytes impronta(std::string_view s) { return impronta(reinterpret_cast<const unsigned char*>(s.data()), s.size()); }
// Chiave derivata di 32 byte (crypto_kdf): contesto di 8 caratteri e numero.
Segreto derivata(const Segreto& chiave, std::uint64_t numero, const char (&contesto)[9]);

// Confronto a tempo costante.
bool uguali(const unsigned char* a, const unsigned char* b, std::size_t n);
// Azzera una memoria normale (es. una copia temporanea).
void azzera(void* p, std::size_t n);
// Testo esadecimale (minuscolo) e ritorno; nullopt-like: vuoto se il testo non e' esadecimale della lunghezza attesa.
std::string esadecimale(const unsigned char* p, std::size_t n);
inline std::string esadecimale(const Bytes& b) { return esadecimale(b.data(), b.size()); }
bool da_esadecimale(std::string_view testo, Bytes& out);

// Niente core dump mentre la cassaforte e' aperta (Linux: limite a 0 e processo non "dumpable"); vale fino alla fine del
// programma. Restituisce false se il sistema non lo permette.
bool niente_core_dump();
// Il processo puo' ancora scrivere un core dump? (prove)
bool core_dump_possibile();

}  // namespace kzero::api::cripto
