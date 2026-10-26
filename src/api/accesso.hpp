#pragma once
// K-Zero API - aprire e cambiare la cassaforte di una cartella (K-010), per i comandi. Ogni apertura:
// - si fa con la cartella bloccata (disco.hpp): un comando alla volta;
// - controlla la cassaforte (cassaforte.hpp) e il registro (registro.hpp: catena e ancora);
// - scrive una voce nel registro (utente, ora, comando, classi aperte e mostrate) e solo dopo restituisce i dati: se la
//   voce non si scrive, errore K-ERR-030 e nessun dato.
// Ogni cambiamento della cassaforte (utenti, password, conservazione, dati) segue lo stesso ordine sotto il blocco:
// apertura e controlli, cambiamento in memoria, voce del registro (cifrata con la chiave base di prima, che la cassaforte
// nuova conserva nella sua storia), cassaforte nuova con l'ancora dell'ultima voce, e infine, se la chiave base e' cambiata,
// il registro ricifrato con quella nuova. Un'interruzione in qualunque punto lascia file interi che si aprono (al piu' una
// voce che descrive un cambiamento non arrivato in fondo).
// Gli errori della cassaforte e del registro diventano errori K-ERR (docs/errori.md).

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "cassaforte.hpp"
#include "comandi.hpp"
#include "lettore.hpp"
#include "password.hpp"
#include "registro.hpp"

namespace kzero::api::accesso {

// L'orologio e la politica di Argon2id del programma: le prove interne ne usano di propri (DECISIONI.md, K-010).
struct Ambiente {
    registro::Orologio orologio = registro::ora_di_sistema;
    const cripto::PoliticaKdf* politica = &cripto::politica_di_rilascio();
    // Solo prove interne: da dove leggere le password al posto del terminale o dell'ingresso standard.
    std::function<std::unique_ptr<password::Ingresso>()> ingresso;
};

// L'ambiente dei comandi: quello del programma (orologio del sistema, politica di rilascio). Solo le prove interne ne
// impostano un altro (ora fissa, Argon2id leggero); nessun parametro del programma lo cambia.
const Ambiente& ambiente();
void per_le_prove_imposta_ambiente(const Ambiente* a);   // nullptr = quello del programma

// Da dove si leggono le password: dall'ingresso standard con --password-da-ingresso, altrimenti dal terminale. Errore
// K-ERR-019 se non c'e' un terminale.
std::unique_ptr<password::Ingresso> ingresso(const Parametri& parametri);

// La password di un utente (per aprire): normalizzata. Errori K-ERR-019 e K-ERR-022 (forma non valida).
cripto::Segreto chiedi_password(password::Ingresso& in, const std::string& utente);

// Una password nuova per un utente: controllata con le regole delle password nuove (password.hpp), due volte al terminale.
// contesto = parole da non usare (codici dell'utente e del pacchetto). Errori K-ERR-019 e K-ERR-020.
cripto::Segreto chiedi_password_nuova(password::Ingresso& in, const std::string& utente, const std::vector<std::string>& contesto);

// Il manifesto di una cartella che dichiara una cassaforte presente; altrimenti errore K-ERR-031 (nessuna cassaforte),
// K-ERR-038 (dichiarata ma manca) o K-ERR-012 (cartella che non c'e').
ManifestoLetto manifesto_con_cassaforte(const std::filesystem::path& cartella);

// Classi come testi per il registro e le uscite: "personali", "particolari".
std::vector<std::string> classi(bool personali, bool particolari);

struct Apertura {
    cassaforte::Cassaforte cassaforte;
    registro::Registro registro;
};

// Apre la cassaforte della cartella e scrive la voce del registro. mostrate = le classi che il comando mostrera' (entro i
// permessi: va controllato prima); nota = senza dati personali.
Apertura apri(const std::filesystem::path& cartella, const ManifestoLetto& manifesto, const std::string& utente,
              const cripto::Segreto& password, const std::string& comando, const Visibilita& mostrate, const std::string& nota,
              const Ambiente& ambiente);

// Cambia la cassaforte della cartella: "cambia" la modifica in memoria e restituisce la nota del registro. Restituisce la
// cassaforte come e' stata scritta.
cassaforte::Riepilogo cambia(const std::filesystem::path& cartella, const ManifestoLetto& manifesto, const std::string& utente,
                             const cripto::Segreto& password, const std::string& comando,
                             const std::function<std::string(cassaforte::Cassaforte&)>& cambia, const Ambiente& ambiente);

// La data di conservazione e' passata (orologio dell'ambiente)?
bool conservazione_passata(const cassaforte::Riepilogo& r);

// Gli avvisi di ogni apertura, dal catalogo dei controlli (testi solo li'): data di conservazione passata (K-CTR-097) e
// chiavi nuove ricevute in una busta, password da cambiare (K-CTR-098). r = la cassaforte come la vede chi l'ha aperta (dopo
// un cambiamento: com'e' dopo).
std::vector<kzero::Problem> avvisi_dell_apertura(const cassaforte::Riepilogo& r, const std::string& utente);
// Il problema di un avviso, dal catalogo dei controlli (es. K-CTR-099, K-CTR-100): un controllo senza posizione, con i
// parametri dati (codici di utenti e date, mai dati personali).
kzero::Problem avviso(kzero::Check controllo, int variante, std::vector<std::pair<std::string, std::string>> parametri);
// Un avviso come lo mostrano le uscite che non elencano problemi (comandi della cassaforte, giornata): codice K-CTR e
// messaggio del catalogo, uguali in testo e in JSON.
struct AvvisoMostrato {
    std::string codice;   // vuoto = un'indicazione del comando, fuori dal catalogo (es. la cartella in chiaro da cancellare)
    std::string messaggio;
};
AvvisoMostrato mostra(const kzero::Problem& p);

// La lettura di un pacchetto per un comando che mostra dati (controlla, giornata): senza cassaforte come sempre; con la
// cassaforte chiede utente (--utente) e password, controlla che --includi-dati-* stiano nei permessi (K-ERR-023), apre
// (voce nel registro) e unisce i valori che l'utente puo' leggere. Gli avvisi dell'apertura (K-CTR-097, K-CTR-098) stanno
// tra i problemi della lettura e, a parte, in avvisi.
struct LetturaPerComando {
    Lettura lettura;
    Visibilita visibilita;                          // cosa mostrare: i parametri, entro i permessi
    std::optional<cassaforte::Riepilogo> cassaforte;   // solo con la cassaforte
    std::string utente;
    cassaforte::Permessi permessi;
    bool scaduta = false;
    std::vector<kzero::Problem> avvisi;
};
LetturaPerComando leggi_per_comando(const std::string& cartella, const Parametri& parametri, const std::string& comando,
                                    const std::string& nota);

// Errori della cassaforte, del registro e del disco come errori K-ERR.
[[noreturn]] void rilancia(const cassaforte::ErroreCassaforte& e);

}  // namespace kzero::api::accesso
