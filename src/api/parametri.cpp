// K-Zero API - lettura dei parametri con CLI11. CLI11 separa e conta i parametri;
// i controlli e i messaggi (in italiano, con codice K-ERR) sono di K-Zero.
#include "parametri.hpp"

#include <algorithm>

#include <CLI11.hpp>

#include "errori.hpp"

namespace kzero::api {

namespace {

std::string tra_apici(const std::string& s) { return "'" + s + "'"; }

std::string unisci(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

std::string nomi_parametri(const DefinizioneComando& c) {
    std::vector<std::string> nomi;
    for (const auto& p : c.parametri) nomi.push_back(p.nome);
    return unisci(nomi);
}

std::string valori_ammessi(const DefinizioneParametro& p) { return "Valori ammessi: " + unisci(p.valori) + "."; }

const DefinizioneParametro* trova_parametro(const DefinizioneComando& c, const std::string& nome) {
    for (const auto& p : c.parametri)
        if (p.nome == nome) return &p;
    return nullptr;
}

// Controlli sulla forma degli argomenti, prima di CLI11: messaggi precisi per i casi
// che CLI11 segnalerebbe solo in inglese ("--formato" senza valore, "--interruttore=valore")
// o accetterebbe in silenzio ("--").
void controlla_forma(const DefinizioneComando& c, const std::vector<std::string>& argomenti) {
    for (std::size_t i = 0; i < argomenti.size(); ++i) {
        const std::string& a = argomenti[i];
        if (a == "--")
            throw ErroreKZero(Errore::ArgomentoNonPrevisto, "Argomento non previsto: '--'. Il comando '" + c.nome +
                                                                "' accetta solo i parametri: " + nomi_parametri(c) +
                                                                "." + kVediDescrivi);
        if (a.rfind("--", 0) != 0) continue;
        const std::size_t uguale = a.find('=');
        const DefinizioneParametro* p = trova_parametro(c, a.substr(0, uguale));
        if (p == nullptr) continue;   // parametro sconosciuto: lo segnala il controllo dopo CLI11
        if (p->tipo == TipoParametro::Interruttore && uguale != std::string::npos)
            throw ErroreKZero(Errore::ParametroSenzaValore,
                              "Il parametro " + p->nome + " non accetta valori: va scritto da solo, senza '='." +
                                  kVediDescrivi);
        const bool vuoto = uguale != std::string::npos && uguale + 1 == a.size();
        const bool ultimo = uguale == std::string::npos && i + 1 == argomenti.size();
        if (p->tipo == TipoParametro::Scelta && (vuoto || ultimo))
            throw ErroreKZero(Errore::ValoreMancante,
                              "Manca il valore del parametro " + p->nome + ". " + valori_ammessi(*p) + kVediDescrivi);
    }
}

}  // namespace

Formato formato_richiesto(const std::vector<std::string>& argomenti) {
    const std::string nome = kParametroFormato;
    const std::string json = kFormatoJson;
    for (std::size_t i = 0; i < argomenti.size(); ++i) {
        if (argomenti[i] == nome + "=" + json) return Formato::Json;
        if (argomenti[i] == nome && i + 1 < argomenti.size() && argomenti[i + 1] == json) return Formato::Json;
    }
    return Formato::Testo;
}

Parametri leggi_parametri(const DefinizioneComando& c, const std::vector<std::string>& argomenti) {
    controlla_forma(c, argomenti);

    CLI::App app{c.descrizione, std::string(kNomeProgramma) + " " + c.nome};
    app.set_help_flag();      // niente aiuto automatico in inglese: c'e' kzero descrivi
    app.allow_extras(true);   // gli argomenti sconosciuti li segnala K-Zero, in italiano
    // Nessuna variabile collegata: i valori si leggono dopo, con count() e results().
    std::vector<CLI::Option*> opzioni;
    for (const DefinizioneParametro& p : c.parametri) {
        if (p.tipo == TipoParametro::Interruttore)
            opzioni.push_back(app.add_flag(p.nome)->disable_flag_override());
        else
            opzioni.push_back(app.add_option(p.nome)
                                  ->expected(1)
                                  ->allow_extra_args(false)
                                  ->multi_option_policy(CLI::MultiOptionPolicy::TakeAll));
    }

    std::vector<std::string> al_contrario(argomenti.rbegin(), argomenti.rend());   // CLI11 li vuole cosi'
    try {
        app.parse(al_contrario);
    } catch (const CLI::ParseError& e) {
        // Non dovrebbe succedere: i casi noti li ferma controlla_forma.
        throw ErroreKZero(Errore::ErroreInterno, "Errore interno nella lettura dei parametri (" + e.get_name() + ").");
    }

    // Argomenti avanzati: parametri sconosciuti oppure valori scritti senza parametro.
    const std::vector<std::string> avanzati = app.remaining();
    if (!avanzati.empty()) {
        const std::string& a = avanzati.front();
        if (a.size() > 1 && a[0] == '-')
            throw ErroreKZero(Errore::ParametroSconosciuto, "Parametro sconosciuto per il comando '" + c.nome +
                                                                "': " + tra_apici(a) + ". Parametri ammessi: " +
                                                                nomi_parametri(c) + "." + kVediDescrivi);
        const std::string un_solo = trova_comando(a) ? " Si puo' dare un solo comando alla volta." : "";
        throw ErroreKZero(Errore::ArgomentoNonPrevisto, "Argomento non previsto: " + tra_apici(a) + "." + un_solo +
                                                            " Il comando '" + c.nome + "' accetta solo i parametri: " +
                                                            nomi_parametri(c) + "." + kVediDescrivi);
    }

    Parametri risultato;
    for (std::size_t i = 0; i < c.parametri.size(); ++i) {
        const DefinizioneParametro& p = c.parametri[i];
        const CLI::Option& o = *opzioni[i];
        if (o.count() > 1)
            throw ErroreKZero(Errore::ParametroRipetuto,
                              "Il parametro " + p.nome + " e' ripetuto: va scritto una volta sola." + kVediDescrivi);
        if (p.tipo == TipoParametro::Interruttore) {
            if (o.count() == 1) p.applica(risultato, "");
            continue;
        }
        const std::string valore = o.count() == 1 ? o.results().front() : p.predefinito;
        if (std::find(p.valori.begin(), p.valori.end(), valore) == p.valori.end())
            throw ErroreKZero(Errore::ValoreNonAmmesso, "Valore non ammesso per " + p.nome + ": " + tra_apici(valore) +
                                                            ". " + valori_ammessi(p) + kVediDescrivi);
        p.applica(risultato, valore);
    }
    return risultato;
}

}  // namespace kzero::api
