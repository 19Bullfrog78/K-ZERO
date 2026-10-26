#pragma once
// K-Zero API - catalogo dei controlli sui pacchetti dati: codice fisso K-CTR-nnn, livello (errore o avviso),
// descrizione breve e testo dei messaggi. E' l'unico posto dove stanno i testi dei problemi: per tradurli
// in un'altra lingua basta tradurre questo catalogo, senza toccare i controlli.
// Elenco spiegato in docs/controlli.md: una prova controlla che coincida con questo catalogo.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "controlli.hpp"
#include "schemi_dati.hpp"

namespace kzero::api {

enum class Livello { Errore, Avviso };

struct DefinizioneControllo {
    DefinizioneControllo(kzero::Check c, std::string cod, Livello l, Classe riv, std::string descr, std::vector<std::string> mess,
                         std::map<int, Classe> riv_varianti = {}, std::map<int, std::string> pers = {})
        : controllo(c), codice(std::move(cod)), livello(l), rivela(riv), rivela_varianti(std::move(riv_varianti)),
          descrizione(std::move(descr)), messaggi(std::move(mess)), personali(std::move(pers)) {}

    kzero::Check controllo;
    std::string codice;                  // "K-CTR-001": fisso, mai riusato per altro
    Livello livello = Livello::Errore;
    // Cosa rivela il problema in se' (che ci sia, il suo codice, il messaggio di base): Comune = nessun dato personale
    // ne' particolare; Personale (es. un'abilitazione o un'assenza di una persona); Particolare (salute). Dichiarato per
    // ogni controllo; le varianti del messaggio che rivelano altro stanno in rivela_varianti. Nelle uscite conta il piu'
    // riservato tra questo e la classe del dato in cui sta la posizione (riservatezza.hpp).
    Classe rivela = Classe::Particolare;
    std::map<int, Classe> rivela_varianti;
    std::string descrizione;             // breve; identica in docs/controlli.md
    std::vector<std::string> messaggi;   // un modello per variante; {nome} = valore del problema
    // Varianti che parlano di dati personali (es. un'abilitazione o un'assenza di una persona): il modello da usare
    // senza --includi-dati-personali.
    std::map<int, std::string> personali;
    // Cassaforte (K-010): il controllo usa dati personali o particolari per giudicare altro (es. il tipo di servizio,
    // dato particolare, decide quali usi del mezzo servono; le assenze, dato personale, chi puo' fare un turno). Per un
    // utente che non puo' aprire quella classe il controllo e' "non verificato": i suoi risultati sarebbero falsati.
    // Dichiarato nel catalogo; una prova (test_unione) confronta i risultati di utenti con permessi diversi.
    bool richiede_personali = false;
    bool richiede_particolari = false;

    Classe rivela_variante(int variante) const {
        const auto it = rivela_varianti.find(variante);
        return it == rivela_varianti.end() ? rivela : it->second;
    }
};

// Catalogo completo, in ordine di codice.
const std::vector<DefinizioneControllo>& catalogo_controlli();

// Il problema si puo' calcolare con le classi che l'utente puo' leggere (K-010)? No se il suo controllo richiede una classe
// che non legge, oppure se il problema rivela, o sta in, un dato di quella classe (i dati non ci sono). classe_posizione =
// la classe del dato in cui sta la posizione (riservatezza.hpp); in_trasportato = la posizione sta in un passeggero o in
// una casa, i cui dati personali, se sono legati a un luogo sensibile, stanno nella sezione dei legati e si decifrano
// solo con tutti e due i permessi. Con tutte le classi
// leggibili: sempre si'.
bool verificabile(const DefinizioneControllo& d, int variante, Classe classe_posizione, bool in_trasportato,
                  const Visibilita& leggibili);
const DefinizioneControllo& definizione_controllo(kzero::Check c);

const char* nome_livello(Livello l);   // "errore", "avviso"

// Giorni di un problema raggruppati in periodi di giorni consecutivi: (primo, ultimo).
std::vector<std::pair<kzero::Day, kzero::Day>> periodi_di_giorni(const std::vector<kzero::Day>& giorni);
// Giorni di un problema in italiano: "il 2026-10-20", "dal 2026-10-20 al 2026-10-24 (5 giorni)", ...
std::string testo_giorni(const std::vector<kzero::Day>& giorni);

// Scrive i messaggi dei problemi. I riferimenti agli elementi mostrano il codice e, se le classi dei dati
// lo permettono, il nome: il nome di un punto porta a porta (dato personale) compare solo con
// --includi-dati-personali. pacchetto e versioni possono mancare (pacchetto non letto).
class Presentatore {
public:
    Presentatore(const kzero::Package* pacchetto, const std::map<std::string, int>* versioni, Visibilita visibilita);
    ~Presentatore();
    Presentatore(const Presentatore&) = delete;
    Presentatore& operator=(const Presentatore&) = delete;

    std::string messaggio(const kzero::Problem& p) const;

    // Codice dell'elemento con il nome, se visibile: "F-CLES (Cles, Autostazione)".
    std::string riferimento(kzero::Element e, const std::string& codice) const;

private:
    struct Dati;
    std::unique_ptr<Dati> dati_;
};

}  // namespace kzero::api
