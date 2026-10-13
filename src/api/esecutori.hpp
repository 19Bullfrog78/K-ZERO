#pragma once
// K-Zero API - funzioni che eseguono i comandi, una per comando (vedi la tabella in comandi.cpp).

#include <string>

#include "comandi.hpp"

namespace kzero::api {

Risultato esegui_controlla(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_descrivi(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_dimostrazione(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_versione(const DefinizioneComando& comando, const Parametri& parametri);

}  // namespace kzero::api
