#pragma once
// K-Zero API - la regola delle classi dei dati applicata ai documenti (K-010): una regola sola, ricavata dalle marcature
// degli schemi ("x-kzero-classe"), per contare i valori personali e particolari di un file (kzero controlla) e per
// separarli dai file normali e metterli nella cassaforte (kzero chiudi), poi riunirli quando la si apre.
//
// La classe di un valore e' la marcatura piu' vicina (accanto al campo o nella sua definizione), come nel filtro delle
// uscite; un campo non previsto dallo schema prende la classe del suo contenitore. Un "valore" e' un testo, un numero o
// un vero/falso; oggetti ed elenchi si attraversano.
//
// Separazione (docs/cassaforte.md, "Cosa va nella cassaforte"):
// - un campo personale o particolare, con tutto quello che contiene della stessa classe, va nella cassaforte intero
//   (es. il nome di un passeggero, la casa di una persona, l'elenco delle abilitazioni);
// - un elemento di un elenco marcato lui stesso personale o particolare (es. un'assenza) va nella cassaforte intero,
//   anche con i campi comuni che contiene (il codice della persona): che l'elemento ci sia e' gia' un dato di quella
//   classe, e nel file normale non resta traccia di quante sono;
// - un oggetto che resta vuoto perche' tutti i suoi campi sono andati nella cassaforte si toglie anche lui (non dice
//   piu' che c'era qualcosa); quando si apre la cassaforte si ricrea;
// - i dati personali di un passeggero o di una casa legati a un luogo sensibile (riservatezza.hpp) valgono come dati
//   particolari e vanno in una sezione loro, i "legati", che si decifra solo con tutti e due i permessi (aggiunta 1 del
//   K-010): un permesso solo non da' mai identita' e salute insieme;
// - per la stessa ragione un campo personale dentro un dato particolare (es. motivo e chi della dichiarazione del modo
//   delle corse, K-011 aggiunta 2) va tra i legati: regola ricavata dagli schemi, vale per ogni campo. Un elemento marcato
//   intero (sopra) non puo' contenere campi di un'altra classe riservata (una prova lo controlla sugli schemi).
// Ogni valore resta legato al suo elemento con il codice stabile dell'elemento, mai con la posizione in un elenco: un
// elemento con "codice" (obbligatorio nel suo schema) si riconosce dal codice; un elemento senza codice (es. una
// fermata dentro un gruppo) dal suo contenuto comune (l'impronta BLAKE2b dei suoi dati comuni) e, tra elementi uguali,
// dall'ordine. Cosi' riordinare un elenco non cambia nulla; togliere o cambiare un elemento che ha valori nella
// cassaforte e' un errore (K-CTR-094), mai un'unione sbagliata.

#include <string>
#include <vector>

#include "controlli.hpp"
#include "schemi_dati.hpp"

namespace kzero::api {

class LegameSensibile;   // riservatezza.hpp

// Un valore tolto da un file normale e messo nella cassaforte.
struct Voce {
    std::string file;               // es. "persone.json"
    // Passi dal documento al contenitore del valore: un testo (campo di un oggetto) oppure, dentro un elenco,
    // {"codice": "P-A"} o {"contenuto": "<impronta>", "n": 0}.
    JsonDati percorso = JsonDati::array();
    std::string campo;              // il campo tolto; vuoto per un elemento intero
    bool elemento = false;          // true: un elemento intero, da aggiungere in fondo all'elenco del percorso
    JsonDati valore;
    Classe classe = Classe::Personale;   // personale o particolare (un dato legato vale come particolare)
    bool legato = false;                 // dato personale di chi e' legato a un luogo sensibile: sezione dei legati

    JsonDati in_json() const;
    // Da JSON (testo della cassaforte decifrato): false se la forma non e' quella attesa.
    static bool da_json(const JsonDati& j, Classe classe, Voce& out);
};

struct ValoriPerClasse {
    std::size_t personali = 0;
    std::size_t particolari = 0;
};

// Quanti valori personali e particolari ha il documento (gia' conforme allo schema), secondo le classi.
ValoriPerClasse conta_valori(const JsonDati& documento, const std::string& schema, int versione);

// Quanti valori di ogni classe ci sono in una voce (contati come nel documento: un campo comune dentro un elemento
// intero, come il codice della persona di un'assenza, resta comune).
ValoriPerClasse conta_valori(const Voce& voce, const std::string& schema, int versione);

// Toglie dal documento (gia' conforme allo schema) i valori personali e particolari e li restituisce come voci.
// legame (puo' mancare): passeggeri e case legati a un luogo sensibile, i cui dati personali diventano voci "legate".
std::vector<Voce> separa(const std::string& file, JsonDati& documento, const std::string& schema, int versione,
                         const LegameSensibile* legame);

// Rimette nel documento normale le voci della cassaforte di quel file (nell'ordine della cassaforte). Un valore che
// non trova il suo elemento, o che trova piu' elementi, o che c'e' gia' in chiaro nel file, e' un problema (K-CTR-094,
// K-CTR-095): in quel caso il documento non va usato (la funzione restituisce false e lo lascia com'era).
bool unisci(const std::string& file, JsonDati& documento, const std::vector<const Voce*>& voci,
            std::vector<kzero::Problem>& problemi);

// --- Testi comuni (K-018, testi_liberi.hpp) ----------------------------------------------------------------------------
// I testi liberi di classe comune di un documento (gia' conforme allo schema), ricavati dalle marcature degli schemi e non
// da un elenco scritto a mano: un campo che rimanda alla definizione "testo" o "nota" del suo schema (nomi, codici esterni,
// note, motivi...) e il codice di un elemento, dove l'elemento e' definito (il campo "codice" obbligatorio degli elementi di
// un elenco), non nei riferimenti. Esclusi i valori a scelta fissa, le date, gli orari, i numeri, i riferimenti e ogni testo
// personale o particolare. Li legge tutti chiunque: sono il posto dove un nome o una parola sulla salute non devono stare.
struct TestoComune {
    std::string puntatore;   // nel documento, es. "/servizi/3/domanda/gruppi/0/nome"
    std::string campo;       // il campo come nella tabella di docs/controlli.md, es. "servizi[].domanda.gruppi[].nome"
    std::string elemento;    // codice dell'elemento piu' vicino che ha un codice (es. il gruppo); vuoto se nessuno
    std::string campo_elemento;   // l'elenco di quell'elemento, es. "servizi[].domanda.gruppi[]"
    std::string elenco;      // l'elenco principale del file (es. "servizi") e l'indice dell'elemento; vuoto e -1 fuori
    long long indice = -1;
    std::string radice;      // codice dell'elemento dell'elenco principale, se ne ha uno
    std::string testo;
};
std::vector<TestoComune> testi_comuni(const JsonDati& documento, const std::string& schema, int versione);

// I campi dei testi comuni di uno schema (la tabella di docs/controlli.md la confronta).
std::vector<std::string> campi_testi_comuni(const std::string& schema, int versione);

// Il file contiene regole pubbliche di un paese o di un territorio, con la fonte (marcatura "x-kzero-regole-pubbliche" della
// radice dello schema: norme, regolamenti): niente controllo delle parole sulla salute (K-018, scelta del supervisore).
bool regole_pubbliche(const std::string& schema, int versione);

}  // namespace kzero::api
