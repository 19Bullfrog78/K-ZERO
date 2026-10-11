#pragma once
// K-Zero API - funzioni che eseguono i comandi, una per comando (vedi la tabella in comandi.cpp).

#include <string>

#include "comandi.hpp"

namespace kzero::api {

std::string esegui_descrivi(const DefinizioneComando& comando, const Parametri& parametri);
std::string esegui_dimostrazione(const DefinizioneComando& comando, const Parametri& parametri);
std::string esegui_versione(const DefinizioneComando& comando, const Parametri& parametri);

}  // namespace kzero::api
