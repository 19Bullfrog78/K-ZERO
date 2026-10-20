#pragma once
// K-Zero API - il registro degli accessi alla cassaforte (K-010): il file registro.kzr accanto a cassaforte.kzc.
// - Una voce per ogni apertura e per ogni azione: utente, data e ora (UTC, dall'orologio del PC), comando, classi aperte
//   (decifrate) e classi mostrate, una nota senza dati personali (es. "aggiunto l'utente B con i dati personali"). Mai i
//   dati.
// - Voci concatenate: ogni voce contiene l'impronta (BLAKE2b-256) di quella prima; la prima parte dall'impronta del codice
//   della cassaforte. Una voce tolta, cambiata o spostata rompe la catena. In piu' la cassaforte, ogni volta che si
//   riscrive, tiene il numero e l'impronta dell'ultima voce (ancora): un registro accorciato prima di quella voce si scopre.
// - Cifrato e autenticato (XChaCha20-Poly1305) con una chiave ricavata dalla chiave base della cassaforte, che hanno tutti
//   gli utenti: ogni utente lo scrive (anche con un solo permesso) e lo legge (kzero registro).
// - In un file accanto alla cassaforte, non dentro: un comando che legge i dati non riscrive mai la cassaforte, solo il
//   registro (DECISIONI.md, K-010). Scrittura atomica sotto il blocco della cartella (disco.hpp).
// Cosa non scopre (docs/sicurezza.md): chi ha le chiavi e sa programmare puo' riscriverlo tutto; una copia vecchia rimessa
// al suo posto insieme alla sua cassaforte; l'ora e' quella dell'orologio del PC.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cassaforte.hpp"
#include "schemi_dati.hpp"

namespace kzero::api::registro {

inline constexpr const char* kFile = "registro.kzr";
inline constexpr std::uint16_t kVersione = 1;

struct Voce {
    std::uint32_t numero = 0;          // 1, 2, 3...
    std::string ora;                   // "2026-10-20T10:34:05Z" (UTC)
    std::string utente;
    std::string comando;
    std::vector<std::string> aperte;   // "personali", "particolari": le classi decifrate
    std::vector<std::string> mostrate; // le classi mostrate nell'uscita
    std::string nota;                  // senza dati personali
    std::string precedente;            // impronta della voce prima (64 cifre esadecimali)

    JsonDati in_json() const;
    static bool da_json(const JsonDati& j, Voce& out);
};

// Il registro letto e verificato.
struct Registro {
    cripto::Bytes id;                  // codice della cassaforte
    std::vector<Voce> voci;
    std::uint32_t versione_base = 0;   // versione della chiave base con cui era cifrato (0 = nuovo)
};

class ErroreRegistro : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Impronta di una voce (per la catena e per l'ancora nella cassaforte).
cripto::Bytes impronta(const Voce& v);

// Legge e verifica il registro di una cassaforte aperta: forma, cifratura, catena e ancora della cassaforte. Lancia
// ErroreRegistro (anche se il file manca: una cassaforte ha sempre il suo registro).
Registro leggi(std::string_view file, const cassaforte::Cassaforte& c);

// Aggiunge una voce in fondo (numero e impronta della precedente li mette questa funzione) e la restituisce: la sua
// impronta e il suo numero vanno nell'ancora della cassaforte quando la si riscrive.
const Voce& aggiungi(Registro& r, Voce v);

// Il file del registro, cifrato con la chiave base di adesso della cassaforte.
std::string scrivi(const Registro& r, const cassaforte::Cassaforte& c);

// Il registro nuovo di una cassaforte appena chiusa (id = 32 cifre esadecimali).
Registro nuovo(const std::string& id);

// Data e ora di adesso, UTC, "AAAA-MM-GGThh:mm:ssZ": l'unico uso dell'orologio del programma insieme alla data di
// conservazione (docs/sicurezza.md). Le prove interne usano un'ora fissa al suo posto.
std::string ora_di_sistema();
// La data di oggi (UTC), "AAAA-MM-GG".
std::string oggi_di_sistema();

using Orologio = std::string (*)();

}  // namespace kzero::api::registro
