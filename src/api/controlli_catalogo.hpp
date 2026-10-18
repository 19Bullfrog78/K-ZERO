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
    DefinizioneControllo(kzero::Check c, std::string cod, Livello l, std::string descr, std::vector<std::string> mess,
                         std::map<int, std::string> ris = {}, std::map<int, std::string> pers = {})
        : controllo(c), codice(std::move(cod)), livello(l), descrizione(std::move(descr)), messaggi(std::move(mess)),
          riservati(std::move(ris)), personali(std::move(pers)) {}

    kzero::Check controllo;
    std::string codice;                  // "K-CTR-001": fisso, mai riusato per altro
    Livello livello = Livello::Errore;
    std::string descrizione;             // breve; identica in docs/controlli.md
    std::vector<std::string> messaggi;   // un modello per variante; {nome} = valore del problema
    // Varianti che parlano di dati particolari: il modello da usare senza --includi-dati-particolari.
    std::map<int, std::string> riservati;
    // Varianti che parlano di dati personali (es. un'abilitazione o un'assenza di una persona): il modello da usare
    // senza --includi-dati-personali.
    std::map<int, std::string> personali;
};

// Catalogo completo, in ordine di codice.
const std::vector<DefinizioneControllo>& catalogo_controlli();
const DefinizioneControllo& definizione_controllo(kzero::Check c);

const char* nome_livello(Livello l);   // "errore", "avviso"

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
