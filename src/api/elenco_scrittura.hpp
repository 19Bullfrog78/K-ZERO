#pragma once
// K-Zero API - quello che i comandi dell'elenco utenti hanno in comune (K-019 leggi-elenco e sblocco; K-020 le modifiche a mano):
// permessi, cartella di --scrivi, il documento elenco_utenti.json e la scrittura nella cassaforte (riscrittura.hpp).
//
// Tutti lavorano solo su un pacchetto chiuso (i dati personali stanno solo nella cassaforte) e chiedono, per ora, i permessi dei
// dati personali e particolari (come richiudi): il controllo sta in un punto solo, permessi_per_l_elenco, dove arrivera' la
// scrittura con i soli dati personali (docs/cassaforte.md).

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "accesso.hpp"
#include "cassaforte.hpp"
#include "lettore.hpp"

namespace kzero::api::elenco {

inline constexpr const char* kFile = "elenco_utenti.json";
inline constexpr int kVersione = 2;   // la versione che scrivono i comandi (K-020); la 1 si legge ancora

// Il controllo dei permessi dell'elenco, in un punto solo (oggi: tutti e due).
void permessi(const cassaforte::Permessi& p, const std::string& utente, const std::string& comando);
// Prima della password, dall'intestazione ancora da autenticare: solo per fermarsi subito (si ricontrolla dopo l'apertura).
void permessi_prima_della_password(const std::filesystem::path& cartella, const std::string& utente, const std::string& comando);
// --scrivi deve nominare la cartella del pacchetto (stessa cartella, anche scritta in un altro modo): K-ERR-046.
void stessa_cartella(const std::string& pacchetto, const std::string& scrivi);
// Il pacchetto non ha errori nella forma dei file ed e' di livello completo (K-ERR-031, K-ERR-047).
void pacchetto_completo(const Lettura& l, const std::string& cartella, const std::string& utente);

// L'elenco di un committente nel pacchetto, se c'e'.
const kzero::ClientList* elenco_di(const kzero::Package& p, const std::string& committente);

// Il documento elenco_utenti.json nuovo (versione kVersione): quello di prima con l'elenco di quel committente sostituito o
// aggiunto, in ordine di committente.
JsonDati documento_con(const Lettura& vecchia, const kzero::ClientList& l, int ultimo_numero);
// Il pacchetto in chiaro, in memoria, con un file cambiato. Un documento preparato dal programma che non rispetta lo schema e' un
// errore del programma (uscita 3), mai un pacchetto scritto a meta'.
Lettura con_documento(const Lettura& vecchia, const std::string& file, const JsonDati& doc);

// Scrive un pacchetto nuovo, preparato in memoria da "prepara" con la cassaforte aperta: lo riapre, poi cassaforte e file normali
// (riscrittura.hpp). prepara restituisce la lettura in chiaro nuova e la nota del registro (solo codici e conteggi).
cassaforte::Riepilogo scrivi(const std::filesystem::path& cartella, const ManifestoLetto& manifesto, const std::string& utente,
                             const cripto::Segreto& password, const std::string& comando,
                             const std::function<std::pair<Lettura, std::string>(const Lettura& vecchia)>& prepara);

// Gli avvisi dell'apertura della cassaforte (password da cambiare, conservazione), come si mostrano.
std::vector<accesso::AvvisoMostrato> avvisi_di(const cassaforte::Riepilogo& r, const std::string& utente);

}  // namespace kzero::api::elenco
