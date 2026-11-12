#pragma once
// K-Zero API - strumenti comuni ai comandi che mostrano i dati di un pacchetto (dimostrazione, giornata).
// Il documento JSON si costruisce gia' filtrato secondo le classi dei dati (marcatura degli schemi); il testo si
// scrive da quel documento: cosi' il filtro e' lo stesso per le due uscite.
// Di base: dati personali sostituiti dal codice dell'elemento; dati particolari "riservato".

#include <string>

#include "lettore.hpp"
#include "orari.hpp"
#include "pacchetto.hpp"
#include "riservatezza.hpp"
#include "schemi_dati.hpp"
#include "uscita.hpp"

namespace kzero::api {

// Filtro delle classi dei dati per i campi degli elementi di un pacchetto letto.
class Vista {
public:
    // Il pacchetto deve essere stato letto e controllato nel contenuto (codici e riferimenti a posto).
    Vista(const Lettura& lettura, Visibilita visibilita);

    const Visibilita& visibilita() const { return visibilita_; }
    // Passeggeri e case legati a un luogo sensibile (riservatezza.hpp).
    const LegameSensibile& legame() const { return legame_; }

    // Classe del campo di un elemento, dalla marcatura dello schema del suo file (versione letta). Un file non letto
    // conta come particolare: nel dubbio non si mostra.
    Classe classe(const kzero::Source& elemento, const std::string& campo) const;

    // Valore di un campo dell'elemento: visibile, oppure sostituito dal codice (personale) o da "riservato". Un dato
    // personale di un passeggero o di una casa legati a un luogo sensibile si vede solo insieme ai dati particolari.
    Json campo(const kzero::Source& elemento, const std::string& campo, Json valore, const std::string& codice) const;

    // Valore di una classe nota: visibile, oppure sostituito dal codice (personale) o da "riservato".
    Json filtra(Classe c, Json valore, const std::string& codice) const;

    // Il tipo di un servizio e quello di un gruppo (dati particolari), filtrati come campo(). K-017: non noto solo a chi non puo'
    // leggerlo (cassaforte): "riservato", come ogni dato particolare che non vede; mai un tipo inventato.
    Json tipo_servizio(const kzero::Service& s) const;
    Json tipo_gruppo(const kzero::PassengerGroup& g) const;

    // Il nome del committente (K-012): di un ente si vede sempre; di un privato e' un dato personale (di base il suo codice) e,
    // se e' legato a un luogo sensibile, si vede solo insieme ai dati particolari. null se non c'e' il committente, oppure se
    // si potrebbe vedere ma il nome non e' scritto (es. un pacchetto "solo numeri").
    Json nome_committente(const kzero::Client* c) const;

    // Il modo di una corsa per l'azienda del turno (K-011 aggiunta 1): out o tachigrafo, da dove viene la scelta, la
    // forzatura o la dichiarazione; null senza scelta. Una scelta per tipo di servizio rivela il tipo (dato particolare); il
    // motivo e chi l'ha scritta sono dati personali.
    Json modo_corsa(const kzero::TripMode& m) const;

    // Una posizione nei file del pacchetto, filtrata come quelle dei problemi (riservatezza.hpp).
    PosizioneMostrata posizione(const kzero::Source& s) const { return mostra_posizione(s, lettura_, visibilita_); }

private:
    const Lettura& lettura_;
    Visibilita visibilita_;
    kzero::Index indice_;
    LegameSensibile legame_;
};

// --- Etichette dei valori del modello, come nei file dei dati ----------------------------------------------------
// Il modo di una corsa (Vista::modo_corsa) in italiano: "OUT, scelta per il servizio (regime R-561)", con la forzatura o la
// dichiarazione (chi, quando, perche', gia' filtrati).
std::string testo_modo_corsa(const Json& m);
// Il committente di un servizio delle uscite (campi "committente" e "nome_committente", gia' filtrati) in italiano: il codice e,
// se si vede, il nome tra virgolette (K-012).
std::string testo_committente(const Json& servizio);
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
