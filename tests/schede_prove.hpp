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
};
inline Fogli fogli_di_prova() { return {righe_di("schede.tsv"), righe_di("fermate.tsv"), righe_di("date.tsv")}; }

inline std::string file_schede(const Fogli& f, bool date1904 = false) {
    std::vector<xlsx::Foglio> v = {{"Schede", f.schede, false}, {"Fermate", f.fermate, false}};
    if (!f.date.empty()) v.push_back({"Date", f.date, false});
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
    const kzero::CardsRead r = kzero::read_cards(kzero::CardTables{tabella(f.schede), tabella(f.fermate), tabella(f.date)}, *pr, idx, false);
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
