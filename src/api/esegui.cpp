// K-Zero API - esecuzione di una riga di comando: comando, parametri, risultato oppure errore.
#include <exception>

#include "api.hpp"
#include "comandi.hpp"
#include "errori.hpp"
#include "parametri.hpp"

namespace kzero::api {

namespace {

// Il comando e' sempre il primo argomento.
const DefinizioneComando& comando_richiesto(const std::vector<std::string>& argomenti) {
    const std::string aiuto = " Uso: " + std::string(kUso) + ". Comandi disponibili: " + nomi_comandi() + "." +
                              kVediDescrivi;
    if (argomenti.empty()) throw ErroreKZero(Errore::MancaComando, "Manca il comando." + aiuto);
    const std::string& nome = argomenti.front();
    if (const DefinizioneComando* c = trova_comando(nome)) return *c;
    if (!nome.empty() && nome[0] == '-')
        throw ErroreKZero(Errore::MancaComando,
                          "Manca il comando: il primo argomento deve essere il comando, non '" + nome + "'." + aiuto);
    throw ErroreKZero(Errore::ComandoSconosciuto, "Comando sconosciuto: '" + nome + "'." + aiuto);
}

Esito esito_errore(const ErroreKZero& e, Formato formato) {
    Esito esito;
    esito.codice = codice_uscita({e});
    esito.errori = testo_errori({e}, formato);
    esito.formato = formato;
    return esito;
}

}  // namespace

Esito esegui(const std::vector<std::string>& argomenti) {
    const Formato formato_errori = formato_richiesto(argomenti);
    try {
        const DefinizioneComando& comando = comando_richiesto(argomenti);
        const std::vector<std::string> scritti(argomenti.begin() + 1, argomenti.end());
        const Parametri parametri = leggi_parametri(comando, scritti);
        const Risultato risultato = comando.esegui(comando, parametri);
        Esito esito;
        esito.codice = risultato.codice;
        esito.uscita = risultato.uscita;
        esito.formato = parametri.formato;
        return esito;
    } catch (const ErroreKZero& e) {
        return esito_errore(e, formato_errori);
    } catch (const std::exception& e) {
        return esito_errore(ErroreKZero(Errore::ErroreInterno, std::string("Errore interno: ") + e.what() + "."),
                            formato_errori);
    } catch (...) {
        return esito_errore(ErroreKZero(Errore::ErroreInterno, "Errore interno di tipo sconosciuto."), formato_errori);
    }
}

int consegna(const Esito& esito, std::ostream& uscita, std::ostream& errori) {
    if (!esito.uscita.empty()) {
        uscita << esito.uscita;
        uscita.flush();
        if (!uscita) {
            const Esito guasto = esito_errore(
                ErroreKZero(Errore::UscitaNonScrivibile, "Impossibile scrivere il risultato sull'uscita standard."),
                esito.formato);
            errori << guasto.errori;
            errori.flush();
            return guasto.codice;
        }
    }
    if (!esito.errori.empty()) {
        errori << esito.errori;
        errori.flush();
    }
    return esito.codice;
}

}  // namespace kzero::api
