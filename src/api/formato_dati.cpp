// K-Zero API - elenco dei file di un pacchetto dati (vedi formato_dati.hpp e docs/formato-dati.md).
#include "formato_dati.hpp"

namespace kzero::api {

const std::vector<FileDati>& file_dati() {
    static const std::vector<FileDati> elenco = {
        {"pacchetto.json", "pacchetto", {1, 2, 3}, true,
         "Manifesto: aziende, periodo, origine, livello dei dati, fuso orario, valuta, impostazioni, cassaforte; dalla versione 3 i "
         "falsi allarmi sui testi comuni."},
        {"aziende.json", "aziende", {1, 2, 3, 4}, false,
         "Elenchi e impostazioni di ogni azienda: attivita', assenze, fermi, competenze, norme, ricovero, abbinamento; dalla "
         "versione 2 le categorie di tempo, le soste non scritte, le scelte sugli orari e la chiave del rientro; dalla versione 3 il "
         "tipo di servizio ncc nel modo delle corse; dalla versione 4 le parole sulla salute dell'azienda."},
        {"norme.json", "norme", {1, 2, 3, 4}, false,
         "Norme di un paese: abilitazioni, usi dei mezzi, requisiti per guidare, usi per tipo di servizio, con la fonte; dalla "
         "versione 2 le leggi e i contratti collettivi sugli orari di lavoro (regimi) e i cambi dell'ora; dalla versione 3 il tipo "
         "di servizio ncc e i servizi occasionali tipo per tipo; dalla versione 4 le parole sulla salute del paese."},
        {"luoghi.json", "luoghi", {1, 2}, false, "Luoghi pubblici (fermate, scuole, centri, rimesse...) e luoghi privati."},
        {"tratti.json", "tratti", {1}, false, "Tratti di strada tra due luoghi con i loro limiti."},
        {"regolamenti.json", "regolamenti", {1, 2, 3}, false, "Regolamenti di un territorio o di un committente."},
        {"festivita.json", "festivita", {1}, false, "Calendari delle festivita' di un paese o di un territorio, con la fonte."},
        {"calendari.json", "calendari", {1, 2}, false,
         "Calendari: giorni in cui valgono servizi, corse e turni; possono aggiungere o togliere altri calendari e le festivita'."},
        {"scuole.json", "scuole", {1}, false, "Scuole e centri: grado o tipo, orari, attese concordate."},
        {"committenti.json", "committenti", {1, 2}, false,
         "Committenti: enti e, dalla versione 2, committenti privati (persone: nome e nota sono dati personali)."},
        {"contratti.json", "contratti", {1}, false, "Contratti, senza importi."},
        {"gruppi_servizi.json", "gruppi_servizi", {1}, false, "Gruppi di servizi (es. lotti)."},
        {"servizi.json", "servizi", {1, 2, 3}, false,
         "Servizi con la loro domanda e le competenze richieste; dalla versione 3 il tipo ncc (noleggio con conducente)."},
        {"corse.json", "corse", {1}, false, "Corse con i passaggi, i trasbordi e le coincidenze."},
        {"mezzi.json", "mezzi", {1, 2, 3}, false,
         "Mezzi delle aziende: posti, attrezzature, costi, uso della carta di circolazione, distrazioni, prestiti."},
        {"rimesse.json", "rimesse", {1}, false, "Rimesse e ricoveri."},
        {"tempi_sosta.json", "tempi_sosta", {1}, false, "Tempi di sosta alle fermate."},
        {"persone.json", "persone", {1, 2}, false,
         "Persone (autisti e assistenti): ruoli, rapporto, ore, disponibilita', abilitazioni, competenze, preferenze; dalla "
         "versione 2 le scelte sugli orari, la chiave del rientro e le giornate prima del pacchetto."},
        {"assenze.json", "assenze", {1}, false, "Assenze programmate delle persone: periodo e tipo, mai la causa."},
        {"fermi.json", "fermi", {1}, false, "Fermi programmati dei mezzi (es. revisione)."},
        {"turni.json", "turni", {1}, false,
         "Turni del giorno: mezzo, corse intere o a pezzi, attivita' senza passeggeri, calendario, competenze."},
        {"rotazioni.json", "rotazioni", {1}, false, "Rotazioni: cicli di passi (giorni o mesi) con i turni di ogni persona."},
        {"servizi_fissi.json", "servizi_fissi", {1}, false, "Servizi fissi: un turno sempre alla stessa persona, con i suoi giorni."},
        {"assegnazioni.json", "assegnazioni", {1}, false, "Assegnazioni del singolo giorno: persona, mezzo o tutti e due."},
        {"blocchi.json", "blocchi", {1}, false, "Blocchi: corse legate a un mezzo (mezzo fisso o tutto fisso), per il calcolo."},
        {"profili_elenchi.json", "profili_elenchi", {1}, false,
         "Profili di lettura dell'elenco utenti di un committente (K-019): titoli, parole, sigle e separatori del suo file."},
        {"elenco_utenti.json", "elenco_utenti", {1}, false,
         "Elenco utenti dei committenti come lo mandano (K-019): casa, scuola, stato, fermata indicata, ditta e orari di ogni "
         "tratta, sblocchi. Solo nel livello completo."},
    };
    return elenco;
}

const FileDati* file_dati_per_nome(const std::string& file) {
    for (const auto& f : file_dati())
        if (f.file == file) return &f;
    return nullptr;
}

}  // namespace kzero::api
