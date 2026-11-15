#pragma once
// K-Zero API - funzioni che eseguono i comandi, una per comando (vedi la tabella in comandi.cpp).

#include <string>

#include "accesso.hpp"
#include "comandi.hpp"

namespace kzero::api {

Risultato esegui_controlla(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_descrivi(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_dimostrazione(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_giornata(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_ore(const DefinizioneComando& comando, const Parametri& parametri);   // K-011: comando_ore.cpp
Risultato esegui_versione(const DefinizioneComando& comando, const Parametri& parametri);
// I comandi su un pacchetto gia' letto (K-011 aggiunta 2): esegui_* leggono la cartella (con la cassaforte) e poi chiamano
// questi; le prove interne li usano con un pacchetto in memoria, come lo riceve un utente con permessi parziali.
Risultato controlla_su(const DefinizioneComando& comando, const Parametri& parametri, accesso::LetturaPerComando lc);
Risultato giornata_su(const DefinizioneComando& comando, const Parametri& parametri, accesso::LetturaPerComando lc);
Risultato ore_su(const DefinizioneComando& comando, const Parametri& parametri, accesso::LetturaPerComando lc);
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
// Elenco utenti del committente (K-019): comando_elenco.cpp
Risultato esegui_leggi_elenco(const DefinizioneComando& comando, const Parametri& parametri);
Risultato esegui_sblocco(const DefinizioneComando& comando, const Parametri& parametri);

}  // namespace kzero::api
