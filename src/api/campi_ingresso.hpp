#pragma once
// K-Zero API - i campi di una modifica a mano dell'elenco (K-020), letti dall'ingresso standard come la password: una riga per
// campo, "campo: valore", fino alla fine dell'ingresso o a una riga vuota. I dati personali (nomi, case) non si scrivono mai come
// argomenti della riga di comando: resterebbero nella storia dei comandi del terminale e si vedrebbero nell'elenco dei processi del
// computer (DECISIONI.md, docs/sicurezza.md).
//
// Limiti di buon senso, come per le altre letture, fissi nel programma: una riga al massimo kRigaByte byte (una cella del file
// del committente ha al massimo 2.000 caratteri, fino a 4 byte l'uno in UTF-8, piu' il nome del campo), al massimo kRighe righe
// (i campi sono meno di 20, piu' le colonne degli orari), solo UTF-8 valido, nessun carattere di controllo (la tabulazione vale
// come uno spazio). Una riga fuori dai limiti ferma tutto: niente cambia.

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace kzero::api::campi {

inline constexpr std::size_t kRigaByte = 8192;
inline constexpr std::size_t kRighe = 64;

enum class Motivo { Nessuno, RigaTroppoLunga, TroppeRighe, NonUtf8, Controllo };

struct Righe {
    std::vector<std::string> righe;   // senza l'a capo (anche "\r\n")
    Motivo motivo = Motivo::Nessuno;
    int riga = 0;                     // la riga fuori dai limiti (da 1)
};

// Legge le righe un byte alla volta da "byte" (-1 alla fine): cosi' non legge mai oltre la riga vuota che chiude i campi.
Righe leggi(const std::function<int()>& byte);
// Le stesse righe da un testo (prove e fuzzing).
Righe leggi_testo(const std::string& testo);
// L'ingresso standard, un byte alla volta.
std::function<int()> ingresso_standard();
// Spiegazione del motivo, per i messaggi (mai il contenuto della riga).
std::string spiegazione(Motivo m);

}  // namespace kzero::api::campi
