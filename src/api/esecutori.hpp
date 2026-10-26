#pragma once
// K-Zero API - funzioni che eseguono i comandi, una per comando (vedi la tabella in comandi.cpp).

#include <string>

#include "comandi.hpp"

namespace kzero::api {

Risultato esegui_controlla(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_descrivi(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_dimostrazione(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_giornata(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_versione(const DefinizioneComando& comando, const Parametri& parametri);
// Cassaforte (K-010): comando_cassaforte.cpp
Risultato esegui_chiudi(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_richiudi(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_esporta(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_cassaforte(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_registro(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_aggiungi_utente(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_togli_utente(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_password(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_conservazione(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_cancella(const DefinizioneComando& comando, const Parametri& parametri);

}  // namespace kzero::api
