// K-Zero API - elenco dei file di un pacchetto dati (vedi formato_dati.hpp e docs/formato-dati.md).
#include "formato_dati.hpp"

namespace kzero::api {

const std::vector<FileDati>& file_dati() {
    static const std::vector<FileDati> elenco = {
        {"pacchetto.json", "pacchetto", {1}, true,
         "Manifesto: aziende, periodo, origine, livello dei dati, fuso orario, valuta, impostazioni."},
        {"luoghi.json", "luoghi", {1}, false, "Luoghi pubblici (fermate, scuole, centri, rimesse...) e luoghi privati."},
        {"tratti.json", "tratti", {1}, false, "Tratti di strada tra due luoghi con i loro limiti."},
        {"regolamenti.json", "regolamenti", {1}, false, "Regolamenti di un territorio o di un committente."},
        {"festivita.json", "festivita", {1}, false, "Calendari delle festivita' di un paese o di un territorio, con la fonte."},
        {"calendari.json", "calendari", {1, 2}, false,
         "Calendari: giorni in cui valgono servizi, corse e turni; possono aggiungere o togliere altri calendari e le festivita'."},
        {"scuole.json", "scuole", {1}, false, "Scuole e centri: grado o tipo, orari, attese concordate."},
        {"committenti.json", "committenti", {1}, false, "Committenti."},
        {"contratti.json", "contratti", {1}, false, "Contratti, senza importi."},
        {"gruppi_servizi.json", "gruppi_servizi", {1}, false, "Gruppi di servizi (es. lotti)."},
        {"servizi.json", "servizi", {1}, false, "Servizi con la loro domanda."},
        {"corse.json", "corse", {1}, false, "Corse con i passaggi, i trasbordi e le coincidenze."},
        {"mezzi.json", "mezzi", {1}, false, "Mezzi delle aziende."},
        {"rimesse.json", "rimesse", {1}, false, "Rimesse e ricoveri."},
        {"tempi_sosta.json", "tempi_sosta", {1}, false, "Tempi di sosta alle fermate."},
    };
    return elenco;
}

const FileDati* file_dati_per_nome(const std::string& file) {
    for (const auto& f : file_dati())
        if (f.file == file) return &f;
    return nullptr;
}

}  // namespace kzero::api
