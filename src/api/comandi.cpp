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
    p.descrizione = "Nel formato json aggiunge nomi e indirizzi, che di base sono esclusi. "
                    "Il formato testo, pensato per le persone, li mostra sempre.";
    p.applica = [](Parametri& par, const std::string&) { par.includi_dati_personali = true; };
    return p;
}

std::vector<DefinizioneComando> crea_comandi() {
    std::vector<DefinizioneComando> c = {
        {"descrivi",
         "Descrive il programma: comandi, parametri, schemi JSON, codici di uscita e di errore.",
         {parametro_formato()},
         {"descrivi", 1},
         esegui_descrivi},
        {"dimostrazione",
         "Stampa di prova con dati inventati: destinazioni, passeggeri, mezzi, autisti, un turno "
         "e alcune regole del mestiere.",
         {parametro_formato(), parametro_dati_personali()},
         {"dimostrazione", 1},
         esegui_dimostrazione},
        {"versione",
         "Mostra la versione del programma.",
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
