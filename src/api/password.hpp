#pragma once
// K-Zero API - password della cassaforte (K-010). Regole di NIST SP 800-63B-4 (agosto 2025, sezione 3.1.1.2) per una
// password usata da sola (DECISIONI.md):
// - almeno 15 caratteri; se ne ammettono fino a 256 (NIST: almeno 64); ogni carattere Unicode (code point) conta uno;
// - qualunque carattere stampabile, spazi compresi; nessun obbligo di maiuscole, numeri o simboli;
// - normalizzazione NFC prima di calcolare la chiave (cosi' una lettera accentata vale uguale su Linux e su Windows);
// - rifiutate le password comuni (elenco di SecLists, NIST: "passwords obtained from previous breach corpuses"), quelle
//   fatte di parole del contesto (il programma, la cassaforte, il codice dell'utente o del pacchetto) e le piu' prevedibili
//   (un carattere o un gruppo di al massimo 4 caratteri ripetuto, una sequenza come "abcdefghijklmnop"), sempre sulla
//   password intera, senza badare a maiuscole e minuscole.
// La password non si scrive mai come parametro ne' in una variabile d'ambiente: dal terminale senza eco, oppure
// dall'ingresso standard con un parametro esplicito (--password-da-ingresso).

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "cripto.hpp"

namespace kzero::api::password {

inline constexpr std::size_t kMinimoCaratteri = 15;
inline constexpr std::size_t kMassimoCaratteri = 256;
inline constexpr std::size_t kMassimoByte = 1024;   // una riga piu' lunga non e' una password

enum class Problema {
    Nessuno,
    Vuota,
    TroppoLunga,          // oltre kMassimoCaratteri caratteri o kMassimoByte byte
    NonUtf8,
    CarattereNonAmmesso,  // carattere di controllo o non ancora assegnato da Unicode
    TroppoCorta,          // meno di kMinimoCaratteri caratteri
    Comune,               // nell'elenco delle password comuni
    Prevedibile,          // parole del contesto, ripetizioni, sequenze
};

// Spiegazione in italiano, per i messaggi.
std::string spiegazione(Problema p, std::size_t caratteri);

// La password scritta (byte, in memoria protetta), normalizzata in NFC per calcolare la chiave. Per aprire una cassaforte
// bastano i controlli di forma (UTF-8 e caratteri ammessi): le regole delle password nuove non si applicano.
cripto::Segreto normalizza(const cripto::Segreto& scritta, Problema& problema);

// Una password nuova: normalizzata e controllata con tutte le regole. "contesto" = parole da non usare come password
// (es. il codice dell'utente e del pacchetto). In "caratteri" quanti caratteri ha.
cripto::Segreto nuova(const cripto::Segreto& scritta, const std::vector<std::string>& contesto, Problema& problema,
                      std::size_t& caratteri);

// Quante password ha l'elenco delle password comuni incorporato (almeno 15 caratteri).
std::size_t password_comuni();

// --- Da dove si legge la password -----------------------------------------------------------------------------------
class Ingresso {
public:
    virtual ~Ingresso() = default;
    // Una password: la domanda si mostra solo sul terminale. Restituisce i byte scritti (senza a capo); una riga oltre
    // kMassimoByte ha la dimensione kMassimoByte + 1 (troppo lunga). Lancia std::runtime_error se l'ingresso finisce.
    virtual cripto::Segreto leggi(const std::string& domanda) = 0;
    // true per il terminale: una password nuova si chiede due volte.
    virtual bool terminale() const = 0;
};

// Il terminale (/dev/tty), senza eco; nullptr se il programma non ha un terminale.
std::unique_ptr<Ingresso> dal_terminale();
// L'ingresso standard: una password per riga (a capo "\n" o "\r\n").
std::unique_ptr<Ingresso> dall_ingresso_standard();

}  // namespace kzero::api::password
