// K-Zero API - elenco dei file di un pacchetto dati (vedi formato_dati.hpp e docs/formato-dati.md).
#include "formato_dati.hpp"

namespace kzero::api {

const std::vector<FileDati>& file_dati() {
    static const std::vector<FileDati> elenco = {
        {"pacchetto.json", "pacchetto", {1, 2}, true,
         "Manifesto: aziende, periodo, origine, livello dei dati, fuso orario, valuta, impostazioni, cassaforte."},
        {"aziende.json", "aziende", {1}, false,
         "Elenchi e impostazioni di ogni azienda: attivita', assenze, fermi, competenze, norme, ricovero, abbinamento."},
        {"norme.json", "norme", {1}, false,
         "Norme di un paese: abilitazioni, usi dei mezzi, requisiti per guidare, usi per tipo di servizio, con la fonte."},
        {"luoghi.json", "luoghi", {1, 2}, false, "Luoghi pubblici (fermate, scuole, centri, rimesse...) e luoghi privati."},
        {"tratti.json", "tratti", {1}, false, "Tratti di strada tra due luoghi con i loro limiti."},
        {"regolamenti.json", "regolamenti", {1, 2}, false, "Regolamenti di un territorio o di un committente."},
        {"festivita.json", "festivita", {1}, false, "Calendari delle festivita' di un paese o di un territorio, con la fonte."},
        {"calendari.json", "calendari", {1, 2}, false,
         "Calendari: giorni in cui valgono servizi, corse e turni; possono aggiungere o togliere altri calendari e le festivita'."},
        {"scuole.json", "scuole", {1}, false, "Scuole e centri: grado o tipo, orari, attese concordate."},
        {"committenti.json", "committenti", {1}, false, "Committenti."},
        {"contratti.json", "contratti", {1}, false, "Contratti, senza importi."},
        {"gruppi_servizi.json", "gruppi_servizi", {1}, false, "Gruppi di servizi (es. lotti)."},
        {"servizi.json", "servizi", {1, 2}, false, "Servizi con la loro domanda e le competenze richieste."},
        {"corse.json", "corse", {1}, false, "Corse con i passaggi, i trasbordi e le coincidenze."},
        {"mezzi.json", "mezzi", {1, 2}, false,
         "Mezzi delle aziende: posti, attrezzature, costi, uso della carta di circolazione, distrazioni, prestiti."},
        {"rimesse.json", "rimesse", {1}, false, "Rimesse e ricoveri."},
        {"tempi_sosta.json", "tempi_sosta", {1}, false, "Tempi di sosta alle fermate."},
        {"persone.json", "persone", {1}, false,
         "Persone (autisti e assistenti): ruoli, rapporto, ore, disponibilita', abilitazioni, competenze, preferenze."},
        {"assenze.json", "assenze", {1}, false, "Assenze programmate delle persone: periodo e tipo, mai la causa."},
        {"fermi.json", "fermi", {1}, false, "Fermi programmati dei mezzi (es. revisione)."},
        {"turni.json", "turni", {1}, false,
         "Turni del giorno: mezzo, corse intere o a pezzi, attivita' senza passeggeri, calendario, competenze."},
        {"rotazioni.json", "rotazioni", {1}, false, "Rotazioni: cicli di passi (giorni o mesi) con i turni di ogni persona."},
        {"servizi_fissi.json", "servizi_fissi", {1}, false, "Servizi fissi: un turno sempre alla stessa persona, con i suoi giorni."},
        {"assegnazioni.json", "assegnazioni", {1}, false, "Assegnazioni del singolo giorno: persona, mezzo o tutti e due."},
        {"blocchi.json", "blocchi", {1}, false, "Blocchi: corse legate a un mezzo (mezzo fisso o tutto fisso), per il calcolo."},
    };
    return elenco;
}

const FileDati* file_dati_per_nome(const std::string& file) {
    for (const auto& f : file_dati())
        if (f.file == file) return &f;
    return nullptr;
}

}  // namespace kzero::api
