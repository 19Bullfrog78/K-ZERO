#pragma once
// K-Zero API - la cassaforte dei dati personali (K-010): il file cassaforte.kzc nella cartella del pacchetto, con tutti i
// valori personali e particolari (classi.hpp), cifrati, e gli utenti che la possono aprire. Formato byte per byte in
// docs/cassaforte.md; scelte e motivi in DECISIONI.md (K-010).
//
// Chiavi (tutte di 32 byte, casuali, mai scritte in chiaro):
// - tre chiavi di livello: "base" (tutti gli utenti: autentica l'intestazione e cifra il registro), "personali" (cifra la
//   sezione dei dati personali), "particolari" (cifra la sezione dei dati particolari). Ognuna ha una versione: quando si
//   toglie un utente, le chiavi dei livelli che aveva si rifanno (versione + 1) e i dati si ricifrano;
// - ogni utente ha una coppia X25519 e una "fessura": la sua chiave segreta X25519 e le chiavi dei soli livelli che ha,
//   cifrate (XChaCha20-Poly1305) con la chiave ricavata dalla sua password (Argon2id). I dati associati della fessura sono
//   codice, permessi, chiave pubblica, sale e parametri: cambiarne uno nell'intestazione e la fessura non si apre piu';
// - quando le chiavi si rifanno, chi le rifa' le consegna agli altri utenti in una "busta" (crypto_box_seal per la loro
//   chiave pubblica), senza conoscere le loro password. Una chiave arrivata in una busta vale solo se la "storia" di quel
//   livello (le chiavi vecchie, cifrate con la chiave nuova) contiene la chiave che l'utente ha gia' nella sua fessura:
//   cosi' la busta l'ha preparata qualcuno che aveva la chiave vecchia, non chiunque conosca la chiave pubblica. Una
//   busta per un livello che l'utente non ha nella sua fessura non vale mai (i permessi non si alzano con le buste);
// - l'intestazione e' autenticata (crypto_auth, HMAC-SHA-512-256) con una chiave ricavata dalla chiave base: solo chi apre
//   la cassaforte la puo' rifare, e la chiave base si rifa' quando si toglie un utente. L'elenco di chi ha i dati personali
//   (codici e chiavi pubbliche) e' autenticato anche con la chiave dei personali, quello di chi ha i dati particolari con la
//   chiave dei particolari: un utente con i soli dati personali non puo' dare a se' o ad altri il permesso dei particolari
//   in modo che il prossimo cambio delle chiavi glieli consegni (DECISIONI.md, K-010, risposta 1 dell'arbitro).
// Si usa solo libsodium, come prescrive la sua documentazione (cripto.hpp).

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "classi.hpp"
#include "cripto.hpp"

namespace kzero::api::cassaforte {

inline constexpr const char* kFile = "cassaforte.kzc";
inline constexpr std::uint16_t kVersione = 1;   // versione del formato del file
inline constexpr std::size_t kId = 16;          // byte del codice della cassaforte (32 cifre esadecimali nel manifesto)
inline constexpr std::size_t kMassimoUtenti = 64;
inline constexpr std::size_t kMassimoVersioni = 4096;   // chiavi rifatte per livello: oltre e' certamente un errore

struct Permessi {
    bool personali = false;
    bool particolari = false;
    // Ha almeno i permessi di "altri".
    bool contiene(const Permessi& altri) const {
        return (personali || !altri.personali) && (particolari || !altri.particolari);
    }
    bool tutti() const { return personali && particolari; }
    bool nessuno() const { return !personali && !particolari; }
    bool operator==(const Permessi& o) const { return personali == o.personali && particolari == o.particolari; }
};

// "i dati personali", "i dati particolari", "i dati personali e particolari".
std::string testo_permessi(const Permessi& p);
// Lo stesso senza l'articolo, per gli elenchi: "dati personali", "dati personali e particolari", "nessun dato".
std::string testo_permessi_breve(const Permessi& p);

// Un utente come lo vede l'intestazione (nessun segreto). Sono dati autenticati solo dopo l'apertura.
struct UtenteVisibile {
    std::string codice;
    Permessi permessi;
    cripto::ParametriKdf kdf;
};

// Problemi nell'apertura o nella modifica: il comando li trasforma in errori K-ERR (docs/errori.md).
class ErroreCassaforte : public std::runtime_error {
public:
    enum class Tipo {
        NonCassaforte,         // non e' una cassaforte K-Zero, oppure e' troncata, vuota, rovinata
        VersioneSconosciuta,   // versione del formato che questo programma non conosce
        AltroPacchetto,        // codice della cassaforte diverso da quello del manifesto
        UtenteSconosciuto,
        ParametriNonAmmessi,   // parametri di Argon2id assurdi o sotto il minimo (rifiutati prima di calcolare)
        PasswordSbagliata,     // oppure fessura dell'utente cambiata: non si possono distinguere
        Manomessa,             // intestazione, buste, storia o sezioni non autentiche
        PermessoMancante,      // la modifica chiede permessi che l'utente non ha
        UtentiNonAmmessi,      // es. codice gia' usato, ultimo utente, ultimo che legge una classe con dati
    };
    ErroreCassaforte(Tipo t, const std::string& messaggio) : std::runtime_error(messaggio), tipo(t) {}
    Tipo tipo;
};

// Il contenuto della cassaforte di un pacchetto da chiudere: i valori separati dai file normali (classi.hpp) e quanti
// valori di ogni classe (contati con la regola delle classi, per i resoconti).
struct Contenuto {
    std::vector<Voce> personali;
    std::vector<Voce> particolari;
    ValoriPerClasse valori;
};

// Le parti della cassaforte che si mostrano a chi la apre (nessun dato personale).
struct Riepilogo {
    std::string id;                 // 32 cifre esadecimali
    std::uint32_t generazione = 0;  // quante volte i dati sono stati chiusi o richiusi
    std::string conservazione;      // AAAA-MM-GG
    ValoriPerClasse valori;
    std::vector<UtenteVisibile> utenti;
    std::uint32_t registro_voci = 0;   // l'ultima voce del registro legata alla cassaforte (vedi registro.hpp)
    cripto::Bytes registro_impronta;
};

// Quanti byte occupa l'intestazione (con le fessure degli utenti): la parte da distruggere per prima nella cancellazione.
// Lancia ErroreCassaforte se il file non si legge.
std::size_t lunghezza_intestazione(std::string_view file);

// Prima dell'apertura: cosa dice l'intestazione senza nessuna chiave (per i messaggi e il fuzzing). Lancia ErroreCassaforte.
Riepilogo leggi_intestazione(std::string_view file);

// Una cassaforte aperta da un utente: le chiavi dei suoi livelli in memoria protetta, i valori delle classi che puo' leggere.
class Cassaforte {
public:
    // Apre il file: id_atteso = il codice della cassaforte scritto nel manifesto (32 cifre esadecimali). La password e'
    // gia' normalizzata (password::normalizza). Controlla tutto prima di usare qualunque dato: forma, codice, utente,
    // parametri (prima di calcolare), fessura, buste e storie, autenticazione dell'intestazione, sezioni. Lancia ErroreCassaforte.
    static Cassaforte apri(std::string_view file, const std::string& id_atteso, const std::string& utente,
                           const cripto::Segreto& password, const cripto::PoliticaKdf& politica);

    // Una cassaforte nuova (chiusura di un pacchetto): chi chiude e' il primo utente, con i permessi di tutte e due le classi
    // (DECISIONI.md, K-010). id = 32 cifre esadecimali; conservazione = AAAA-MM-GG.
    static Cassaforte crea(const std::string& id, const std::string& conservazione, const std::string& utente,
                           const cripto::Segreto& password, const cripto::PoliticaKdf& politica, Contenuto contenuto);

    Cassaforte(Cassaforte&&) noexcept;
    Cassaforte& operator=(Cassaforte&&) noexcept;
    ~Cassaforte();

    const std::string& utente() const { return utente_; }
    Permessi permessi() const;
    Riepilogo riepilogo() const;
    // Le voci di una classe che l'utente puo' leggere (vuoto per le altre).
    const std::vector<Voce>& voci(Classe classe) const;

    // Chiave del registro per una versione della chiave base (la versione di adesso o una vecchia, dalla storia): ogni
    // utente la conosce. Lancia ErroreCassaforte se la versione non c'e'.
    cripto::Segreto chiave_registro(std::uint32_t versione_base) const;
    std::uint32_t versione_base() const;

    // --- Modifiche: cambiano la cassaforte in memoria; file() da' i byte da scrivere. Lanciano ErroreCassaforte. ----------------
    // Un utente nuovo con al massimo i permessi di chi lo aggiunge; password gia' normalizzata e controllata.
    void aggiungi_utente(const std::string& codice, const Permessi& permessi, const cripto::Segreto& password,
                         const cripto::PoliticaKdf& politica);
    // Toglie un utente: chi lo toglie deve avere tutti i suoi permessi; le chiavi dei livelli che aveva si rifanno e si
    // consegnano in una busta agli utenti rimasti che hanno quei livelli; le sezioni si ricifrano.
    void togli_utente(const std::string& codice);
    // Password nuova per se' (stessa chiave X25519) o per un altro utente con permessi che chi la da' ha tutti (coppia nuova).
    void cambia_password(const std::string& codice, const cripto::Segreto& password, const cripto::PoliticaKdf& politica);
    void cambia_conservazione(const std::string& data);
    // Dati nuovi (richiusura): servono tutti e due i permessi; la generazione cresce di uno.
    void sostituisci_contenuto(Contenuto contenuto);
    // L'ultima voce del registro scritta insieme a questa cassaforte (registro.hpp).
    void lega_registro(std::uint32_t voci, const cripto::Bytes& impronta);

    // Il file intero, pronto da scrivere.
    std::string file() const;

private:
    Cassaforte();
    struct Stato;
    std::unique_ptr<Stato> s_;
    std::string utente_;
};

// Un codice della cassaforte nuovo e casuale (32 cifre esadecimali), per il manifesto.
std::string nuovo_id();

}  // namespace kzero::api::cassaforte
