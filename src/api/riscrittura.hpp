#pragma once
// K-Zero API - riscrivere i dati di un pacchetto chiuso (K-010 richiudi; K-019 leggi-elenco e sblocco): una regola sola.
// Il pacchetto nuovo si prepara in memoria, si riapre con la password come un'apertura vera e deve dare gli stessi dati
// (altrimenti K-ERR-035 e niente si scrive); poi la cassaforte (accesso::cambia) e infine i file normali, il manifesto per
// ultimo con il numero della chiusura: un'interruzione a meta' si vede (K-CTR-093) e si ripara rifacendo il comando.

#include <cstdint>
#include <filesystem>
#include <string>

#include "chiusura.hpp"
#include "cripto.hpp"
#include "lettore.hpp"

namespace kzero::api::riscrittura {

// Scrive un file della cartella con una scrittura atomica; errore K-ERR-030 se non riesce.
void scrivi_o_errore(const std::filesystem::path& cartella, const std::string& nome, const std::string& contenuto);

// I file di un pacchetto chiuso, in memoria: file normali, manifesto con la cassaforte, cassaforte e registro.
PacchettoInMemoria chiuso_in_memoria(const chiusura::Separazione& s, const std::string& id, std::uint32_t generazione,
                                     const std::string& cassaforte_file, const std::string& registro_file);

// Riapre il pacchetto chiuso (con la password, come un'apertura vera) e controlla che i dati uniti siano quelli di
// "originale". Errore K-ERR-035 se no. con_registro: controlla anche il registro.
void controlla_riapertura(const Sorgente& chiuso, const Lettura& originale, const std::string& utente,
                          const cripto::Segreto& password, bool con_registro);

// I file normali della separazione nella cartella chiusa: prima tutti tranne il manifesto, poi tolti quelli del formato che
// non ci sono piu', per ultimo il manifesto con il numero della chiusura.
void scrivi_normali(const std::filesystem::path& chiusa, const chiusura::Separazione& s, const std::string& id,
                    std::uint32_t generazione);

}  // namespace kzero::api::riscrittura
