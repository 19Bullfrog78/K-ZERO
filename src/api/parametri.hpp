#pragma once
// K-Zero API - lettura dei parametri della riga di comando (con CLI11) e controlli con messaggi in italiano.

#include <string>
#include <vector>

#include "comandi.hpp"

namespace kzero::api {

// Formato chiesto con --formato, cercato anche in una riga sbagliata: serve a scrivere
// l'eventuale messaggio di errore nel formato che chi ha chiamato sa leggere.
Formato formato_richiesto(const std::vector<std::string>& argomenti);

// Legge i parametri di un comando (gli argomenti scritti dopo il suo nome).
// Se qualcosa non va lancia ErroreKZero con un codice K-ERR e uscita 2.
Parametri leggi_parametri(const DefinizioneComando& comando, const std::vector<std::string>& argomenti);

}  // namespace kzero::api
