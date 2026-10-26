// K-Zero API - cifratura con libsodium (vedi cripto.hpp).
#include "cripto.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>

#include <sodium.h>

#if defined(__linux__)
#include <sys/prctl.h>
#include <sys/resource.h>
#endif

// Con AddressSanitizer il controllo delle perdite di memoria (LeakSanitizer) ispeziona il programma come farebbe un
// debugger: un processo non "dumpable" glielo impedisce. Nelle prove con i sanitizer resta solo il limite dei core dump.
#if defined(__SANITIZE_ADDRESS__)
#define KZERO_CON_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define KZERO_CON_ASAN 1
#endif
#endif

namespace kzero::api::cripto {

static_assert(kChiave == crypto_aead_xchacha20poly1305_ietf_KEYBYTES, "chiave");
static_assert(kNonce == crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, "nonce");
static_assert(kEtichetta == crypto_aead_xchacha20poly1305_ietf_ABYTES, "etichetta");
static_assert(kSale == crypto_pwhash_SALTBYTES, "sale");
static_assert(kPubblicaBox == crypto_box_PUBLICKEYBYTES && kSegretaBox == crypto_box_SECRETKEYBYTES, "box");
static_assert(kSovraccaricoBusta == crypto_box_SEALBYTES, "busta");
static_assert(kChiave == crypto_auth_KEYBYTES && kAutenticazione == crypto_auth_BYTES, "autenticazione");
static_assert(kChiave == crypto_kdf_KEYBYTES && crypto_kdf_CONTEXTBYTES == 8, "kdf");

void avvia() {
    static const bool pronta = sodium_init() >= 0;
    if (!pronta) throw std::runtime_error("la libreria di cifratura non si avvia");
}

// --- Segreto ---------------------------------------------------------------------------------------------------------

Segreto::Segreto(std::size_t capienza) {
    avvia();
    p_ = static_cast<unsigned char*>(sodium_malloc(capienza == 0 ? 1 : capienza));
    if (!p_) throw std::bad_alloc();
    capienza_ = capienza;
    n_ = capienza;
    if (capienza) sodium_memzero(p_, capienza);
}

Segreto::Segreto(const unsigned char* dati, std::size_t n) : Segreto(n) {
    if (n) std::memcpy(p_, dati, n);
}

Segreto::~Segreto() {
    if (p_) sodium_free(p_);   // sodium_free azzera la memoria prima di liberarla
}

Segreto::Segreto(Segreto&& altro) noexcept
    : p_(std::exchange(altro.p_, nullptr)), n_(std::exchange(altro.n_, 0)), capienza_(std::exchange(altro.capienza_, 0)) {}

Segreto& Segreto::operator=(Segreto&& altro) noexcept {
    if (this != &altro) {
        if (p_) sodium_free(p_);
        p_ = std::exchange(altro.p_, nullptr);
        n_ = std::exchange(altro.n_, 0);
        capienza_ = std::exchange(altro.capienza_, 0);
    }
    return *this;
}

void Segreto::usa(std::size_t n) {
    if (n > capienza_) throw std::logic_error("segreto: lunghezza oltre la capienza");
    if (n < n_) sodium_memzero(p_ + n, n_ - n);
    n_ = n;
}

void casuali(unsigned char* dove, std::size_t n) {
    avvia();
    randombytes_buf(dove, n);
}

Bytes casuali(std::size_t n) {
    Bytes b(n);
    casuali(b.data(), n);
    return b;
}

// --- Argon2id --------------------------------------------------------------------------------------------------------

const PoliticaKdf& politica_di_rilascio() {
    // DECISIONI.md (K-010): 256 MiB e 3 passaggi (libsodium MODERATE), sopra i minimi OWASP (46 MiB con 1 passaggio) e
    // quattro volte la memoria della seconda raccomandazione di RFC 9106 (64 MiB, 3 passaggi); oltre 2 GiB (la prima
    // raccomandazione di RFC 9106) o 10 passaggi un parametro e' assurdo per questo programma.
    static const PoliticaKdf p{{3, 256u * 1024u}, {10, 2u * 1024u * 1024u}, {3, 256u * 1024u}};
    return p;
}

EsitoParametri controlla_parametri(const ParametriKdf& p, const PoliticaKdf& politica) {
    if (p.operazioni > politica.massimi.operazioni || p.memoria_kib > politica.massimi.memoria_kib) return EsitoParametri::Assurdi;
    if (p.operazioni < politica.minimi.operazioni || p.memoria_kib < politica.minimi.memoria_kib) return EsitoParametri::TroppoBassi;
    // I limiti della libreria (es. almeno 8 KiB di memoria e un passaggio) valgono comunque.
    const unsigned long long memoria = static_cast<unsigned long long>(p.memoria_kib) * 1024ull;
    if (p.operazioni < crypto_pwhash_argon2id_OPSLIMIT_MIN || memoria < crypto_pwhash_argon2id_MEMLIMIT_MIN)
        return EsitoParametri::TroppoBassi;
    return EsitoParametri::Ammessi;
}

Segreto deriva(const Segreto& password, const unsigned char* sale, const ParametriKdf& p) {
    avvia();
    Segreto chiave(kChiave);
    const unsigned long long memoria = static_cast<unsigned long long>(p.memoria_kib) * 1024ull;
    if (crypto_pwhash(chiave.dati(), kChiave, reinterpret_cast<const char*>(password.dati()), password.dimensione(), sale,
                      p.operazioni, static_cast<std::size_t>(memoria), crypto_pwhash_ALG_ARGON2ID13) != 0)
        throw std::runtime_error("calcolo della chiave dalla password non riuscito (memoria insufficiente?)");
    return chiave;
}

// --- XChaCha20-Poly1305 ----------------------------------------------------------------------------------------------

Bytes cifra(const Segreto& chiave, const unsigned char* nonce, const unsigned char* dati, std::size_t n, const Bytes& ad) {
    if (chiave.dimensione() != kChiave) throw std::logic_error("cifra: chiave di lunghezza sbagliata");
    Bytes out(n + kEtichetta);
    unsigned long long scritti = 0;
    crypto_aead_xchacha20poly1305_ietf_encrypt(out.data(), &scritti, dati, n, ad.data(), ad.size(), nullptr, nonce,
                                               chiave.dati());
    out.resize(static_cast<std::size_t>(scritti));
    return out;
}

bool decifra(const Segreto& chiave, const unsigned char* nonce, const unsigned char* cifrato, std::size_t n, const Bytes& ad,
             Segreto& in_chiaro) {
    if (chiave.dimensione() != kChiave || n < kEtichetta) return false;
    Segreto out(n - kEtichetta);
    unsigned long long letti = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(out.dati(), &letti, nullptr, cifrato, n, ad.data(), ad.size(), nonce,
                                                   chiave.dati()) != 0)
        return false;
    out.usa(static_cast<std::size_t>(letti));
    in_chiaro = std::move(out);
    return true;
}

// --- Buste -----------------------------------------------------------------------------------------------------------

CoppiaBox nuova_coppia_box() {
    avvia();
    CoppiaBox c{Bytes(kPubblicaBox), Segreto(kSegretaBox)};
    crypto_box_keypair(c.pubblica.data(), c.segreta.dati());
    return c;
}

Bytes pubblica_box(const Segreto& segreta) {
    Bytes p(kPubblicaBox);
    if (segreta.dimensione() != kSegretaBox || crypto_scalarmult_base(p.data(), segreta.dati()) != 0) return {};
    return p;
}

Bytes sigilla(const unsigned char* dati, std::size_t n, const Bytes& pubblica) {
    avvia();
    if (pubblica.size() != kPubblicaBox) throw std::logic_error("sigilla: chiave pubblica di lunghezza sbagliata");
    Bytes out(n + kSovraccaricoBusta);
    if (crypto_box_seal(out.data(), dati, n, pubblica.data()) != 0) throw std::runtime_error("busta non riuscita");
    return out;
}

bool apri_busta(const Bytes& busta, const Bytes& pubblica, const Segreto& segreta, Segreto& in_chiaro) {
    if (busta.size() < kSovraccaricoBusta || pubblica.size() != kPubblicaBox || segreta.dimensione() != kSegretaBox) return false;
    Segreto out(busta.size() - kSovraccaricoBusta);
    if (crypto_box_seal_open(out.dati(), busta.data(), busta.size(), pubblica.data(), segreta.dati()) != 0) return false;
    in_chiaro = std::move(out);
    return true;
}

// --- Autenticazione con una chiave segreta -----------------------------------------------------------------------------

Bytes autentica(const Segreto& chiave, const Bytes& messaggio) {
    if (chiave.dimensione() != kChiave) throw std::logic_error("autentica: chiave di lunghezza sbagliata");
    avvia();
    Bytes t(kAutenticazione);
    crypto_auth(t.data(), messaggio.data(), messaggio.size(), chiave.dati());
    return t;
}

bool verifica_autenticazione(const Bytes& etichetta, const Bytes& messaggio, const Segreto& chiave) {
    if (chiave.dimensione() != kChiave || etichetta.size() != kAutenticazione) return false;
    avvia();
    return crypto_auth_verify(etichetta.data(), messaggio.data(), messaggio.size(), chiave.dati()) == 0;
}

// --- Impronte e chiavi derivate ----------------------------------------------------------------------------------------

Bytes impronta(const unsigned char* dati, std::size_t n) {
    avvia();
    Bytes h(kImpronta);
    crypto_generichash(h.data(), h.size(), dati, n, nullptr, 0);
    return h;
}

Segreto derivata(const Segreto& chiave, std::uint64_t numero, const char (&contesto)[9]) {
    if (chiave.dimensione() != kChiave) throw std::logic_error("derivata: chiave di lunghezza sbagliata");
    Segreto out(kChiave);
    crypto_kdf_derive_from_key(out.dati(), kChiave, numero, contesto, chiave.dati());
    return out;
}

Segreto combinata(const Segreto& prima, const Segreto& seconda, const Bytes& sale, std::string_view contesto) {
    if (prima.dimensione() != kChiave || seconda.dimensione() != kChiave)
        throw std::logic_error("combinata: chiave di lunghezza sbagliata");
    Segreto materiale(2 * kChiave);
    std::memcpy(materiale.dati(), prima.dati(), kChiave);
    std::memcpy(materiale.dati() + kChiave, seconda.dati(), kChiave);
    materiale.usa(2 * kChiave);
    Segreto prk(crypto_kdf_hkdf_sha256_KEYBYTES);
    prk.usa(crypto_kdf_hkdf_sha256_KEYBYTES);
    if (crypto_kdf_hkdf_sha256_extract(prk.dati(), sale.data(), sale.size(), materiale.dati(), materiale.dimensione()) != 0)
        throw std::runtime_error("combinata: HKDF non riuscito");
    Segreto out(kChiave);
    out.usa(kChiave);
    if (crypto_kdf_hkdf_sha256_expand(out.dati(), kChiave, contesto.data(), contesto.size(), prk.dati()) != 0)
        throw std::runtime_error("combinata: HKDF non riuscito");
    return out;
}

bool uguali(const unsigned char* a, const unsigned char* b, std::size_t n) { return sodium_memcmp(a, b, n) == 0; }

void azzera(void* p, std::size_t n) { sodium_memzero(p, n); }

std::string esadecimale(const unsigned char* p, std::size_t n) {
    static const char cifre[] = "0123456789abcdef";
    std::string s;
    s.reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        s += cifre[p[i] >> 4];
        s += cifre[p[i] & 0x0f];
    }
    return s;
}

bool da_esadecimale(std::string_view testo, Bytes& out) {
    if (testo.size() % 2) return false;
    const auto valore = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    Bytes b(testo.size() / 2);
    for (std::size_t i = 0; i < b.size(); ++i) {
        const int a = valore(testo[2 * i]), z = valore(testo[2 * i + 1]);
        if (a < 0 || z < 0) return false;
        b[i] = static_cast<unsigned char>(a * 16 + z);
    }
    out = std::move(b);
    return true;
}

// --- Core dump ---------------------------------------------------------------------------------------------------------

bool niente_core_dump() {
#if defined(__linux__)
    const struct rlimit zero {0, 0};
    bool ok = setrlimit(RLIMIT_CORE, &zero) == 0;
#if !defined(KZERO_CON_ASAN)
    ok = prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) == 0 && ok;
#endif
    return ok;
#else
    return false;   // Windows: con il programma per Windows (DIARIO.md, limiti)
#endif
}

bool core_dump_possibile() {
#if defined(__linux__)
    struct rlimit r {};
    if (getrlimit(RLIMIT_CORE, &r) != 0) return true;
#if defined(KZERO_CON_ASAN)
    return r.rlim_cur != 0;
#else
    return r.rlim_cur != 0 && prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) != 0;
#endif
#else
    return true;
#endif
}

}  // namespace kzero::api::cripto
