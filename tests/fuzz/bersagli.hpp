#pragma once
// K-Zero - bersagli del fuzzing (prove con dati casuali e deformati, docs/sicurezza.md). Ogni bersaglio riceve dei byte
// qualsiasi e li fa passare dal programma come farebbe un file o una riga di comando vera; un guasto (memoria,
// comportamento non definito, eccezione non prevista, risultato incoerente) ferma tutto. Gli stessi bersagli li usano:
// - i programmi di libFuzzer (fuzz_*.cpp, con -DKZERO_FUZZ=ON e clang), per cercare guasti nuovi;
// - il programma rigioca_fuzz, nelle prove normali, per ripetere ogni guasto gia' trovato (tests/fuzz/casi).

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kzero::fuzz {

// Il pacchetto demo con un file (es. "corse.json") sostituito dai byte da provare: lettura (codifica, JSON, schema,
// conversione), controlli sul contenuto, messaggi e posizioni con le classi dei dati.
void prova_file_del_pacchetto(const std::string& file, const std::uint8_t* dati, std::size_t n);

// Una riga di comando: gli argomenti sono separati dal byte 0 (un argomento vero non puo' contenerlo).
void prova_parametri(const std::uint8_t* dati, std::size_t n);

// Una data o un orario scritti come testo.
void prova_date_e_orari(const std::uint8_t* dati, std::size_t n);

// Cassaforte (K-010). Per non calcolare Argon2id a ogni caso questi bersagli usano una politica delle prove (1 passaggio,
// al massimo 64 KiB: tutto il resto e' rifiutato prima di calcolare), con una cassaforte preparata una volta sola dal
// demo (tre utenti, un cambio delle chiavi) e password di prova: il programma kzero non ha questa politica.
// - il file della cassaforte, aperto da ognuno dei tre utenti;
void prova_cassaforte(const std::uint8_t* dati, std::size_t n);
// - il testo decifrato di una sezione (le voci), unito ai file normali del demo;
void prova_cassaforte_voci(const std::uint8_t* dati, std::size_t n);
// - il file del registro degli accessi, con la cassaforte aperta;
void prova_registro(const std::uint8_t* dati, std::size_t n);
// - il testo decifrato del registro (voci e catena);
void prova_registro_voci(const std::uint8_t* dati, std::size_t n);
// - una password scritta (forma e regole delle password nuove).
void prova_password(const std::uint8_t* dati, std::size_t n);
// I semi dei bersagli della cassaforte, come file nella cartella <cartella>/<bersaglio>/ (preparati a ogni giro: la
// cassaforte ha chiavi casuali).
void scrivi_semi_cassaforte(const std::string& cartella);
// Ogni seme della cassaforte nel suo bersaglio (prove normali, rigioca_fuzz --semi).
int prova_semi_cassaforte();

// I file del formato, cioe' i bersagli "pacchetto-<file>".
std::vector<std::string> file_del_formato();

// Esegue il bersaglio con quel nome ("pacchetto-corse.json", "parametri", "date-e-orari", "cassaforte",
// "cassaforte-voci", "registro", "registro-voci", "password"); false se non esiste.
bool prova(const std::string& bersaglio, const std::uint8_t* dati, std::size_t n);

}  // namespace kzero::fuzz
