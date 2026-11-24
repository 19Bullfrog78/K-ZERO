#pragma once
// K-Zero - strumenti delle prove delle schede dei servizi (K-021): il pacchetto di prova (il demo con i codici esterni del K-019 e
// del K-021 e il profilo delle schede di tests/esempi/k021-schede, dati inventati) e i file Excel costruiti dai testi (nessun .xlsx
// nel deposito).
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "elenco_a_mano_prove.hpp"
#include "chiusura.hpp"
#include "lettore.hpp"
#include "schede.hpp"
#include "schede_scrittura.hpp"
#include "xlsx_prove.hpp"

namespace kzero::prove::schede {

using kzero::api::JsonDati;
using kzero::api::PacchettoInMemoria;

inline const std::string kCartella = KZERO_CARTELLA_PROVE "/esempi/k021-schede";
inline const std::string kCartella23 = KZERO_CARTELLA_PROVE "/esempi/k023-schede";
inline const std::string kCartella24 = KZERO_CARTELLA_PROVE "/esempi/k024-schede";
inline const std::string kCartella25 = KZERO_CARTELLA_PROVE "/esempi/k025-schede";
inline const std::string kCartella26 = KZERO_CARTELLA_PROVE "/esempi/k026-schede";

using Righe = std::vector<std::vector<std::string>>;

// Le righe di un testo con le colonne separate da tabulazioni.
inline Righe righe_tsv(const std::string& testo) {
    Righe out;
    std::istringstream in(testo);
    for (std::string r; std::getline(in, r);) {
        if (!r.empty() && r.back() == '\r') r.pop_back();
        std::vector<std::string> celle;
        std::size_t a = 0;
        for (;;) {
            const std::size_t b = r.find('\t', a);
            celle.push_back(r.substr(a, b == std::string::npos ? std::string::npos : b - a));
            if (b == std::string::npos) break;
            a = b + 1;
        }
        out.push_back(celle);
    }
    return out;
}
inline Righe righe_di(const std::string& file) { return righe_tsv(elenco_a_mano::leggi_testo(kCartella + "/" + file)); }

struct Fogli {
    Righe schede, fermate, date;
    Righe interruzioni;   // K-023: il quarto foglio (vuoto = il file non lo ha)
};
inline Fogli fogli_di_prova() { return {righe_di("schede.tsv"), righe_di("fermate.tsv"), righe_di("date.tsv"), {}}; }
// K-023: le tre schede di prova dei servizi per disabili (profilo PS-CVA-H): una con tre interruzioni, e la coppia in coincidenza al
// punto di trasbordo del Ponte di Sorgenta (4000502/1 finisce li', 4000503/1 prende li' la persona). Dati inventati.
inline Fogli fogli_k023() {
    const auto r = [](const std::string& f) { return righe_tsv(elenco_a_mano::leggi_testo(kCartella23 + "/" + f)); };
    return {r("schede.tsv"), r("fermate.tsv"), r("date.tsv"), r("interruzioni.tsv")};
}

// K-024: le quattro schede di prova del tipo per scheda (profilo PS-CVA alla versione 3 di tests/esempi/k024-schede, con la regola RT-1
// sul calendario SPECIALE-A e la colonna dei disabili, l'ultima del foglio): una scolastica, una per disabili riconosciuta dalla regola,
// una scolastica con 1 disabile (promiscua), una per disabili con 0. Dati inventati.
inline Fogli fogli_k024() {
    const auto r = [](const std::string& f) { return righe_tsv(elenco_a_mano::leggi_testo(kCartella24 + "/" + f)); };
    return {r("schede.tsv"), r("fermate.tsv"), r("date.tsv"), r("interruzioni.tsv")};
}

// K-025: le quattro schede della rilettura del file intero (profilo PS-CVA alla versione 3 del K-024, con la colonna dei disabili; pacchetto
// pacchetto_k024): 4000701/1 finisce il 20/11/2026 (gia' finita alla seconda lettura), 4000702/1 per disabili (regola RT-1) che finisce
// al punto di trasbordo del Ponte di Sorgenta (coincidenza, K-CTR-176), 4000703/1 con Tot Giorni 170 contro i 175 del calendario
// (K-CTR-163), 4000704/1 normale. Dati inventati.
inline Fogli fogli_k025() {
    const auto r = [](const std::string& f) { return righe_tsv(elenco_a_mano::leggi_testo(kCartella25 + "/" + f)); };
    return {r("schede.tsv"), r("fermate.tsv"), r("date.tsv"), r("interruzioni.tsv")};
}

// K-026: le tre schede del mezzo della scheda (profilo PS-CVA alla versione 4 di tests/esempi/k026-schede, con la colonna "Mezzo", l'ultima del
// foglio, 23): 4000801/1 dell'infanzia senza accompagnatore con la targa scritta con gli spazi e il trattino ("AA 101-AA", il mezzo ALFA-01
// del demo, 16 posti), 4000802/1 con una targa che non c'e' ("ZZ 999 ZZ"), 4000803/1 con la cella vuota. Dati inventati.
inline Fogli fogli_k026() {
    const auto r = [](const std::string& f) { return righe_tsv(elenco_a_mano::leggi_testo(kCartella26 + "/" + f)); };
    return {r("schede.tsv"), r("fermate.tsv"), r("date.tsv"), r("interruzioni.tsv")};
}

// K-022: due schede con due giri, che passano due volte dallo stesso luogo con delle persone. La prima dell'infanzia (come le schede
// vere): l'accompagnatore sale per primo alla scuola (St. Car. 1 = solo lui) e scende per ultimo (St. Scar. = i bambini del secondo giro
// + lui), la scuola a meta' con le discese del primo giro, i km solo sull'ultima riga. La seconda un'andata con la stessa scuola due
// volte, senza accompagnatore.
inline Fogli due_giri() {
    const Fogli base = fogli_di_prova();
    Fogli f;
    f.schede = {base.schede[0],
                {"4000300 / 1", "5c1e2f40-0a1b-4c2d-8e3f-000000000300", "Andata Mattino", "Scuola infanzia Lagobello", "14/09/2026",
                 "30/06/2027", "X", "X", "X", "X", "X", "", "", "#23", "", "", "#170", "25", "0", "MATERNA", "Sempre", "Scheda nuova"},
                {"4000301 / 1", "5c1e2f40-0a1b-4c2d-8e3f-000000000301", "Andata Mattino", "Scuola primaria Valfonda", "14/09/2026",
                 "12/06/2027", "X", "X", "X", "X", "X", "", "", "#9", "", "", "#175", "13", "6,0", "SCOLASTICO", "Sempre", "Scheda nuova"}};
    f.fermate = {base.fermate[0],
                 {"4000300 / 1", "07:00", "Scuola infanzia Lagobello", "", "#1", "", "", "", "SALE"},
                 {"4000300 / 1", "07:10", "Lagobello, Lago", "", "#6", "", "", "", ""},
                 {"4000300 / 1", "07:20", "PRADELLO PIAZZA", "", "#5", "", "", "", ""},
                 {"4000300 / 1", "07:30", "Scuola infanzia Lagobello", "", "", "#11", "", "", ""},
                 {"4000300 / 1", "07:40", "Castelrovo, Bivio", "", "#7", "", "", "", ""},
                 {"4000300 / 1", "07:50", "MONTEBRINO CENTRO", "", "#4", "", "", "", ""},
                 {"4000300 / 1", "08:05", "Scuola infanzia Lagobello", "", "", "#12", "25", "", "SCENDE"},
                 {"4000301 / 1", "07:00", "PRADELLO PIAZZA", "", "#5", "", "", "6,0", ""},
                 {"4000301 / 1", "07:15", "Scuola primaria Valfonda", "", "", "#5", "7", "", ""},
                 {"4000301 / 1", "07:25", "Castelrovo, Bivio", "", "#4", "", "3", "", ""},
                 {"4000301 / 1", "07:40", "Scuola primaria Valfonda", "", "", "#4", "3", "", ""}};
    f.date = base.date;
    return f;
}

inline std::string file_schede(const Fogli& f, bool date1904 = false) {
    std::vector<xlsx::Foglio> v = {{"Schede", f.schede, false}, {"Fermate", f.fermate, false}};
    if (!f.date.empty()) v.push_back({"Date", f.date, false});
    if (!f.interruzioni.empty()) v.push_back({"Interruzioni", f.interruzioni, false});
    return xlsx::file_xlsx(v, 8, date1904);
}

// Il pacchetto di prova: il demo (con i codici esterni del K-019) e i codici esterni e il profilo del K-021.
inline PacchettoInMemoria pacchetto_di_prova() {
    PacchettoInMemoria m = elenco_a_mano::pacchetto_di_prova();
    const JsonDati codici = JsonDati::parse(elenco_a_mano::leggi_testo(kCartella + "/codici_esterni.json"));
    for (const auto& [file, elenco] : {std::make_pair(std::string("calendari.json"), std::string("calendari")),
                                       std::make_pair(std::string("luoghi.json"), std::string("luoghi"))}) {
        JsonDati d = JsonDati::parse(m.file.at(file));
        for (JsonDati& x : d.at(elenco)) {
            const std::string c = x.at("codice").get<std::string>();
            if (!codici.at(elenco).contains(c)) continue;
            if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
            for (const JsonDati& e : codici.at(elenco).at(c)) x["codici_esterni"].push_back(e);
        }
        m.file[file] = d.dump(2);
    }
    m.file["profili_schede.json"] = elenco_a_mano::leggi_testo(kCartella + "/profili_schede.json");
    return m;
}

// K-023: il pacchetto di prova del K-021 con i codici esterni di tests/esempi/k023-schede (il calendario dei giorni feriali) e i suoi
// profili alla versione 2: PS-CVA con il foglio delle interruzioni (le schede del K-021 si leggono come prima) e PS-CVA-H per i servizi
// per disabili.
inline PacchettoInMemoria pacchetto_k023() {
    PacchettoInMemoria m = pacchetto_di_prova();
    const JsonDati codici = JsonDati::parse(elenco_a_mano::leggi_testo(kCartella23 + "/codici_esterni.json"));
    JsonDati d = JsonDati::parse(m.file.at("calendari.json"));
    for (JsonDati& x : d.at("calendari")) {
        const std::string c = x.at("codice").get<std::string>();
        if (!codici.at("calendari").contains(c)) continue;
        if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
        for (const JsonDati& e : codici.at("calendari").at(c)) x["codici_esterni"].push_back(e);
    }
    m.file["calendari.json"] = d.dump(2);
    m.file["profili_schede.json"] = elenco_a_mano::leggi_testo(kCartella23 + "/profili_schede.json");
    return m;
}

// K-024: il pacchetto del K-023 con i codici esterni di tests/esempi/k024-schede (il calendario SPECIALE-A) e il suo profilo alla
// versione 3 (un profilo solo per il committente: PS-CVA, con la regola del tipo e la colonna dei disabili).
inline PacchettoInMemoria pacchetto_k024() {
    PacchettoInMemoria m = pacchetto_k023();
    const JsonDati codici = JsonDati::parse(elenco_a_mano::leggi_testo(kCartella24 + "/codici_esterni.json"));
    JsonDati d = JsonDati::parse(m.file.at("calendari.json"));
    for (JsonDati& x : d.at("calendari")) {
        const std::string c = x.at("codice").get<std::string>();
        if (!codici.at("calendari").contains(c)) continue;
        if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
        for (const JsonDati& e : codici.at("calendari").at(c)) x["codici_esterni"].push_back(e);
    }
    m.file["calendari.json"] = d.dump(2);
    m.file["profili_schede.json"] = elenco_a_mano::leggi_testo(kCartella24 + "/profili_schede.json");
    return m;
}

// K-026: il pacchetto del K-024 con i profili alla versione 4 di tests/esempi/k026-schede: PS-CVA (con la colonna del mezzo) e PS-CVA-SM
// (lo stesso senza la colonna, per la colonna aggiunta dopo).
inline PacchettoInMemoria pacchetto_k026() {
    PacchettoInMemoria m = pacchetto_k024();
    m.file["profili_schede.json"] = elenco_a_mano::leggi_testo(kCartella26 + "/profili_schede.json");
    return m;
}

// Le righe come tabella del nucleo (una cella che comincia con "#" e' un numero, "!" una cella con un errore di Excel).
inline kzero::Table tabella(const Righe& righe) {
    kzero::Table t;
    for (std::size_t r = 0; r < righe.size(); ++r) {
        kzero::TableRow x;
        x.number = static_cast<int>(r + 1);
        for (std::size_t c = 0; c < righe[r].size(); ++c) {
            std::string v = righe[r][c];
            if (v.empty()) continue;
            if (v[0] == '#') v = v.substr(1);
            x.cells.push_back({static_cast<int>(c + 1), v, v == "!"});
        }
        if (!x.cells.empty()) t.rows.push_back(x);
    }
    return t;
}

// Il pacchetto dopo una lettura delle schede (nel nucleo, senza la cassaforte), con la scelta "attivazione" per tutto il file.
inline PacchettoInMemoria con_schede(const PacchettoInMemoria& m, const Fogli& f, const std::string& presa = "2026-11-16") {
    const kzero::api::Lettura l = kzero::api::leggi_pacchetto(m);
    const kzero::Index idx(l.pacchetto);
    const kzero::CardProfile* pr = idx.card_profile("PS-CVA");
    const kzero::CardsRead r =
        kzero::read_cards(kzero::CardTables{tabella(f.schede), tabella(f.fermate), tabella(f.date), tabella(f.interruzioni)}, *pr, idx, false);
    kzero::DateChoices scelte;
    scelte.file.kind = kzero::DateChoice::Kind::Activation;
    const kzero::CardsMerge x = kzero::merge_cards(l.pacchetto, *pr, "CT-SCU", *kzero::day_from_text(presa), "mail del committente", scelte, r);
    PacchettoInMemoria out = m;
    for (const auto& [file, doc] : kzero::api::schede::documenti_dopo(l, x.cards, l.pacchetto.removed_cards, x.added, x.removed, x.cuts, x.new_homes))
        out.file[file] = kzero::api::chiusura::testo_file(doc);
    return out;
}

// Il pacchetto di prova con le schede: la prima del file con "Servizio Individualizzato" e "Attrezzato" segnati (i due dati
// particolari nuovi) e la seconda (due scuole).
inline PacchettoInMemoria pacchetto_con_schede() {
    Fogli f = fogli_di_prova();
    f.schede.resize(3);
    f.schede[1][14] = "X";
    f.schede[1][15] = "X";
    return con_schede(pacchetto_di_prova(), f);
}

}  // namespace kzero::prove::schede
