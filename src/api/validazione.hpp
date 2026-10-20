#pragma once
// K-Zero API - controllo di un documento con il suo schema (JSON Schema draft-07), con la libreria valijson.
// I messaggi della libreria sono in inglese: qui diventano italiani.

#include <string>
#include <vector>

#include "schemi_dati.hpp"

namespace kzero::api {

struct ErroreSchema {
    std::string puntatore;               // percorso JSON del dato sbagliato
    std::string messaggio;               // in italiano, con il valore trovato quando serve a capire
    std::string messaggio_senza_valore;  // lo stesso senza il valore: per i dati personali e particolari
    std::string campo_mancante;          // un campo obbligatorio che manca (nell'oggetto al puntatore), se e' questo l'errore
};

// Tutti i punti in cui il documento non rispetta lo schema (nome, versione), in ordine di percorso.
std::vector<ErroreSchema> valida(const JsonDati& documento, const std::string& schema, int versione);

// Messaggio italiano per un messaggio della libreria. sottoschema e istanza (se noti) servono a dire
// cosa era atteso (es. i valori ammessi).
std::string traduci_messaggio(const std::string& originale, const JsonDati* sottoschema, const JsonDati* istanza);

// Ordine dei percorsi JSON come nel documento: "/corse/2" viene prima di "/corse/10".
bool prima_nel_documento(const std::string& a, const std::string& b);

}  // namespace kzero::api
