#pragma once
// K-Zero API - scritture sul disco della cassaforte e del registro (K-010): atomiche e sotto blocco.
// - Scrittura atomica: il contenuto va in un file temporaneo nella stessa cartella (nome che comincia con ".", mai
//   letto come dato), scritto per intero e portato sul disco (fsync), poi messo al posto del file vero con una sola
//   operazione (rename) e portata sul disco anche la cartella. Un'interruzione in qualunque momento lascia il file vecchio
//   o quello nuovo, intero; al piu' resta un file temporaneo (kzero controlla lo segnala come file non riconosciuto).
// - Blocco: un solo comando K-Zero alla volta scrive nella cartella di un pacchetto (flock esclusivo sulla cartella, anche
//   tra programmi diversi e tra persone su una cartella condivisa, dove il sistema lo permette). Chi arriva dopo aspetta.
//   Se il blocco non si puo' prendere (es. un disco di rete che non lo permette) il comando si ferma: mai scritture senza
//   blocco.
// Oggi solo Linux e macOS (POSIX); Windows con il programma per Windows (DIARIO.md).

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace kzero::api::disco {

class ErroreDisco : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Scrive "contenuto" nel file "nome" della cartella, in modo atomico (vedi sopra). Lancia ErroreDisco.
void scrivi_atomico(const std::filesystem::path& cartella, const std::string& nome, std::string_view contenuto);

// Toglie un file della cartella e porta sul disco la cartella. Lancia ErroreDisco (un file che non c'e' va bene).
void togli(const std::filesystem::path& cartella, const std::string& nome);

// Sovrascrive con zeri i primi "byte" byte del file, sul posto, e li porta sul disco (cancellazione: prima le chiavi).
// Su dischi a stato solido e file system che scrivono altrove (copy-on-write) i byte vecchi possono restare sul disco
// (docs/sicurezza.md). Lancia ErroreDisco.
void azzera_inizio(const std::filesystem::path& cartella, const std::string& nome, std::size_t byte);

// Blocco esclusivo della cartella del pacchetto, finche' l'oggetto vive. Lancia ErroreDisco se non si puo' prendere.
class Blocco {
public:
    explicit Blocco(const std::filesystem::path& cartella);
    ~Blocco();
    Blocco(const Blocco&) = delete;
    Blocco& operator=(const Blocco&) = delete;

private:
    int fd_ = -1;
};

// Solo per le prove interne di interruzione: una funzione chiamata a ogni passo della scrittura atomica ("temporaneo
// creato", "temporaneo scritto", "sostituito"). Nel programma vero non c'e' (nessun parametro la imposta).
using Passo = void (*)(const char* passo, const std::string& nome);
void per_le_prove_a_ogni_passo(Passo passo);

}  // namespace kzero::api::disco
