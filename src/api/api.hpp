#pragma once
// K-Zero API - punto di ingresso: dagli argomenti della riga di comando all'esito completo.
// Questo e' il contratto ufficiale del programma: comandi e parametri sono descritti da
// "kzero descrivi", le uscite JSON dagli schemi in docs/schema, gli errori da docs/errori.md.

#include <ostream>
#include <string>
#include <vector>

#include "formato.hpp"

namespace kzero::api {

struct Esito {
    int codice = 0;                     // codice di uscita del programma (0, 1, 2, 3)
    std::string uscita;                 // per l'uscita standard; vuota se c'e' un errore
    std::string errori;                 // per l'uscita di errore; vuota se tutto bene
    Formato formato = Formato::Testo;   // formato in cui segnalare un guasto in scrittura
};

// Esegue la riga di comando (argomenti senza il nome del programma). Non scrive nulla:
// prepara tutta l'uscita in memoria, cosi' un errore non lascia mai un risultato a meta'.
Esito esegui(const std::vector<std::string>& argomenti);

// Scrive l'esito sulle due uscite e restituisce il codice di uscita del programma.
// Se il risultato non si riesce a scrivere, segnala l'errore K-ERR-010 (uscita 3).
int consegna(const Esito& esito, std::ostream& uscita, std::ostream& errori);

}  // namespace kzero::api
