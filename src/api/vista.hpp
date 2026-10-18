#pragma once
// K-Zero API - strumenti comuni ai comandi che mostrano i dati di un pacchetto (dimostrazione, giornata).
// Il documento JSON si costruisce gia' filtrato secondo le classi dei dati (marcatura degli schemi); il testo si
// scrive da quel documento: cosi' il filtro e' lo stesso per le due uscite.
// Di base: dati personali sostituiti dal codice dell'elemento; dati particolari "riservato".

#include <string>

#include "lettore.hpp"
#include "schemi_dati.hpp"
#include "uscita.hpp"

namespace kzero::api {

// La classe piu' riservata tra due.
Classe piu_riservata(Classe a, Classe b);

// Filtro delle classi dei dati per i campi degli elementi di un pacchetto letto.
class Vista {
public:
    Vista(const Lettura& lettura, Visibilita visibilita) : lettura_(lettura), visibilita_(visibilita) {}

    const Visibilita& visibilita() const { return visibilita_; }

    // Classe del campo di un elemento, dalla marcatura dello schema del suo file (versione letta). Un file non letto
    // conta come particolare: nel dubbio non si mostra.
    Classe classe(const kzero::Source& elemento, const std::string& campo) const;

    // Valore di un campo dell'elemento: visibile, oppure sostituito dal codice (personale) o da "riservato".
    Json campo(const kzero::Source& elemento, const std::string& campo, Json valore, const std::string& codice) const;

    // Valore di una classe nota: visibile, oppure sostituito dal codice (personale) o da "riservato".
    Json filtra(Classe c, Json valore, const std::string& codice) const;

private:
    const Lettura& lettura_;
    Visibilita visibilita_;
};

// --- Etichette dei valori del modello, come nei file dei dati ----------------------------------------------------
const char* nome_tipo_luogo(kzero::PlaceType t);
const char* nome_categoria(kzero::ServiceType t);
const char* nome_tipo_servizio(kzero::ServiceKind k);
const char* nome_tipo_gruppo(kzero::GroupType t);
const char* nome_categoria_mezzo(kzero::VehicleCategory c);
const char* nome_grado(kzero::Grade g);
const char* nome_stato(kzero::PassengerStatus s);
const char* nome_giorno(kzero::Weekday g);   // "lunedi'" ... "domenica"

// --- Testo per le persone -------------------------------------------------------------------------------------
// "riservato" resta tale, gli altri codici con lo spazio al posto di "_".
std::string leggibile(const std::string& s);
// 12.5 -> "12,5"; con decimali fissi se richiesti (euro: 2), altrimenti al massimo 3 senza zeri in fondo.
std::string numero(double x, int decimali_fissi = -1);
// Testo di un valore del documento: numeri con la virgola, testi come sono, null "-".
std::string testo(const Json& j);
// Importo in euro, oppure "non indicato".
std::string euro(const Json& j);
// "nessun errore", "1 errore", "3 errori".
std::string quanti(long long n, const char* uno, const char* molti);
// Durata in ore e minuti: "45 minuti", "1 ora e 5 minuti", "2 ore".
std::string durata(long long minuti);

}  // namespace kzero::api
