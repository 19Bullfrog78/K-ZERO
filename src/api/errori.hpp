#pragma once
// K-Zero API - errori: codice fisso K-ERR-nnn, messaggio in italiano, posizione quando esiste.
// Elenco completo e spiegato in docs/errori.md: una prova controlla che coincida con questo catalogo.

#include <exception>
#include <optional>
#include <string>
#include <vector>

#include "uscita.hpp"

namespace kzero::api {

// Codici di uscita del programma.
enum class CodiceUscita : int {
    Riuscito = 0,
    ErroreDati = 1,
    ErroreParametri = 2,
    ErroreInterno = 3,
};

struct SignificatoUscita {
    CodiceUscita codice;
    std::string significato;
};

// Significato di ogni codice di uscita, in ordine di codice.
const std::vector<SignificatoUscita>& codici_uscita();

// Errori che il programma sa segnalare. Il codice K-ERR di ognuno sta nel catalogo.
enum class Errore {
    MancaComando,
    ComandoSconosciuto,
    ParametroSconosciuto,
    ArgomentoNonPrevisto,
    ValoreMancante,
    ValoreNonAmmesso,
    ParametroRipetuto,
    ParametroSenzaValore,
    ErroreInterno,
    UscitaNonScrivibile,
    MancaArgomento,
    CartellaNonTrovata,
    ParametroObbligatorio,
    ValoreScrittoMale,
    ParametriIncompatibili,
    DataFuoriPeriodo,
    ElementoNonTrovato,
    PacchettoConErrori,
};

struct DefinizioneErrore {
    Errore errore;
    std::string codice;          // "K-ERR-001": fisso, mai riusato per altro
    CodiceUscita uscita;
    std::string descrizione;     // breve; identica in docs/errori.md
};

// Catalogo completo, in ordine di codice.
const std::vector<DefinizioneErrore>& catalogo_errori();
const DefinizioneErrore& definizione_errore(Errore e);

// Schema JSON dei documenti di errore.
const Schema& schema_errore();

// Dove sta il problema, quando lo si sa (es. una cella di un file di dati).
struct Posizione {
    std::string file;
    int riga = 0;      // 0 = non nota
    int colonna = 0;   // 0 = non nota
};

// Errore da mostrare all'utente: codice dal catalogo, messaggio del caso preciso, posizione.
class ErroreKZero : public std::exception {
public:
    ErroreKZero(Errore errore, std::string messaggio, std::optional<Posizione> posizione = std::nullopt);

    const DefinizioneErrore& definizione() const { return *definizione_; }
    const std::string& messaggio() const { return messaggio_; }
    const std::optional<Posizione>& posizione() const { return posizione_; }
    const char* what() const noexcept override { return messaggio_.c_str(); }

private:
    const DefinizioneErrore* definizione_;
    std::string messaggio_;
    std::optional<Posizione> posizione_;
};

// Codice di uscita per un elenco di errori: quello del piu' grave (3 interno, 2 parametri, 1 dati).
int codice_uscita(const std::vector<ErroreKZero>& errori);

// Testo per l'uscita di errore, nel formato richiesto (una riga per errore, oppure un documento JSON).
std::string testo_errori(const std::vector<ErroreKZero>& errori, Formato formato);

}  // namespace kzero::api
