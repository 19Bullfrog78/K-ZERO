// K-Zero API - tabella dei comandi: l'unico posto dove si dichiarano comandi e parametri.
#include "comandi.hpp"

#include <algorithm>
#include <tuple>

#include "errori.hpp"
#include "esecutori.hpp"

namespace kzero::api {

namespace {

DefinizioneParametro parametro_formato() {
    DefinizioneParametro p;
    p.nome = kParametroFormato;
    p.tipo = TipoParametro::Scelta;
    p.valori = {kFormatoJson, "testo"};
    p.predefinito = "testo";
    p.descrizione = "Formato dell'uscita: testo per le persone, json per i programmi e gli assistenti AI.";
    p.applica = [](Parametri& par, const std::string& valore) {
        par.formato = valore == kFormatoJson ? Formato::Json : Formato::Testo;
    };
    return p;
}

DefinizioneParametro parametro_dati_personali() {
    DefinizioneParametro p;
    p.nome = "--includi-dati-personali";
    p.tipo = TipoParametro::Interruttore;
    p.descrizione = "Mostra i dati personali (nomi, indirizzi, case, punti porta a porta, motivi scritti a mano), "
                    "che di base sono sostituiti dal codice dell'elemento. Vale per il testo e per il JSON.";
    p.applica = [](Parametri& par, const std::string&) { par.includi_dati_personali = true; };
    return p;
}

DefinizioneParametro parametro_dati_particolari() {
    DefinizioneParametro p;
    p.nome = "--includi-dati-particolari";
    p.tipo = TipoParametro::Interruttore;
    p.descrizione = "Mostra i dati particolari sulla salute (carrozzine, assistente necessario, servizio per disabili), "
                    "che di base sono nascosti (\"riservato\"). Vale per il testo e per il JSON.";
    p.applica = [](Parametri& par, const std::string&) { par.includi_dati_particolari = true; };
    return p;
}

std::vector<DefinizioneComando> crea_comandi() {
    std::vector<DefinizioneComando> c = {
        {"controlla",
         "Controlla un pacchetto dati: forma dei file (schemi), codici e riferimenti, calendari, orari delle corse, "
         "persone a bordo, trasbordi, coincidenze, attese e regolamenti. Elenca errori e avvisi con codice K-CTR e "
         "posizione, poi un riepilogo; esce con 1 se trova errori (gli avvisi non bloccano).",
         {{"cartella", "Cartella del pacchetto dati, con il manifesto pacchetto.json."}},
         {parametro_formato(), parametro_dati_personali(), parametro_dati_particolari()},
         {"controlla", 1},
         esegui_controlla},
        {"descrivi",
         "Descrive il programma: comandi, argomenti, parametri, schemi JSON, file dei pacchetti dati, codici di uscita, "
         "di errore e dei controlli.",
         {},
         {parametro_formato()},
         {"descrivi", 2},
         esegui_descrivi},
        {"dimostrazione",
         "Stampa di prova con il pacchetto dimostrativo esempi/demo (dati inventati, incorporato nel programma): "
         "pacchetto, mezzi, luoghi, servizi, corse e alcune regole del mestiere applicate ai dati.",
         {},
         {parametro_formato(), parametro_dati_personali(), parametro_dati_particolari()},
         {"dimostrazione", 2},
         esegui_dimostrazione},
        {"versione",
         "Mostra la versione del programma.",
         {},
         {parametro_formato()},
         {"versione", 1},
         esegui_versione},
    };
    for (auto& comando : c)
        std::sort(comando.parametri.begin(), comando.parametri.end(),
                  [](const DefinizioneParametro& a, const DefinizioneParametro& b) { return a.nome < b.nome; });
    std::sort(c.begin(), c.end(),
              [](const DefinizioneComando& a, const DefinizioneComando& b) { return a.nome < b.nome; });
    return c;
}

}  // namespace

const std::vector<DefinizioneComando>& comandi() {
    static const std::vector<DefinizioneComando> tabella = crea_comandi();
    return tabella;
}

const DefinizioneComando* trova_comando(const std::string& nome) {
    for (const auto& c : comandi())
        if (c.nome == nome) return &c;
    return nullptr;
}

std::string nomi_comandi() {
    std::string s;
    for (const auto& c : comandi()) s += (s.empty() ? "" : ", ") + c.nome;
    return s;
}

std::string forma_comando(const DefinizioneComando& c) {
    std::string s = std::string(kNomeProgramma) + " " + c.nome;
    for (const auto& a : c.argomenti) s += " <" + a.nome + ">";
    return s + " [parametri]";
}

const std::vector<Schema>& schemi_ritirati() {
    // Versioni non piu' prodotte: i file restano in docs/schema per controllare le uscite salvate prima.
    static const std::vector<Schema> s = {{"descrivi", 1}, {"dimostrazione", 1}};
    return s;
}

std::vector<Schema> schemi() {
    std::vector<Schema> s;
    for (const auto& c : comandi()) s.push_back(c.schema);
    s.push_back(schema_errore());
    const auto chiave = [](const Schema& x) { return std::tie(x.nome, x.versione); };
    std::sort(s.begin(), s.end(), [&](const Schema& a, const Schema& b) { return chiave(a) < chiave(b); });
    s.erase(std::unique(s.begin(), s.end(), [&](const Schema& a, const Schema& b) { return chiave(a) == chiave(b); }),
            s.end());
    return s;
}

}  // namespace kzero::api
