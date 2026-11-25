#pragma once
// K-Zero - strumenti delle prove dei punti degli utenti dell'elenco (K-027): il pacchetto di prova costruito in memoria con le letture del
// programma (il pacchetto delle schede del K-026 con le quattro schede, gli otto alunni e lo stradario di tests/esempi/k027-punti: dati
// inventati), e i punti scritti con le regole del nucleo e la scrittura dell'elenco del programma (la versione 3 del file).
//
// Le schede: 4000901/1 andata dell'infanzia a due giri (Lagobello, Lago due volte e la scuola due volte; l'accompagnatore sale alla scuola e
// scende alla fine), 4000902/1 ritorno del pomeriggio solo il mercoledi', 4000903/1 andata della primaria, 4000904/1 ritorno di mezzogiorno
// della primaria. Gli alunni (committente CVA, ditta ALFA): ROSSI ANNA, BIANCHI SOFIA e MORI LIA all'infanzia (BIANCHI con la fermata
// indicata PRADELLO PIAZZA), VERDI LUCA sospeso (AM e RP), NERI MARCO, GIALLI PAOLO, BRUNI ELSA, VIOLA DINO alla primaria.
#include <functional>
#include <string>
#include <vector>

#include "elenco.hpp"
#include "elenco_scrittura.hpp"
#include "punti.hpp"
#include "schede_prove.hpp"

namespace kzero::prove::punti {

using kzero::api::JsonDati;
using kzero::api::Lettura;
using kzero::api::PacchettoInMemoria;

inline const std::string kCartella27 = KZERO_CARTELLA_PROVE "/esempi/k027-punti";

inline schede::Righe righe_k027(const std::string& file) {
    return schede::righe_tsv(elenco_a_mano::leggi_testo(kCartella27 + "/" + file));
}
inline schede::Fogli fogli_k027() { return {righe_k027("schede.tsv"), righe_k027("fermate.tsv"), righe_k027("date.tsv"), {}}; }

// Il pacchetto di prova: le schede lette con la scelta "attivazione", l'elenco letto dal testo degli otto alunni (lettura completa) e, se
// chiesto, lo stradario.
inline PacchettoInMemoria pacchetto_k027(bool stradario = true) {
    PacchettoInMemoria m = schede::con_schede(schede::pacchetto_k026(), fogli_k027(), "2026-09-10");
    if (stradario) m.file["stradari.json"] = elenco_a_mano::leggi_testo(kCartella27 + "/stradari.json");
    const Lettura l = kzero::api::leggi_pacchetto(m);
    const Index idx(l.pacchetto);
    const ReadingProfile* profilo = idx.reading_profile("PR-CVA");
    const ListRead r = read_list(schede::tabella(righe_k027("elenco.tsv")), *profilo, idx, 2026, nullptr);
    const ListMerge x = merge_list(l.pacchetto, *profilo, 2026, r, true, std::string(64, 'b'));
    m.file["elenco_utenti.json"] = kzero::api::chiusura::testo_file(kzero::api::elenco::documento_con(l, x.list, x.last_number));
    return m;
}

// Una lettura del pacchetto che deve essere valida.
inline Lettura lettura(const PacchettoInMemoria& m) {
    Lettura l = kzero::api::leggi_pacchetto(m);
    if (!l.forma_valida()) throw std::logic_error("pacchetto di prova non valido");
    return l;
}

// L'utente dell'elenco con quel nome (dati inventati).
inline ListUser& utente(ClientList& l, const std::string& nome) {
    for (ListUser& u : l.users)
        if (u.name == nome) return u;
    throw std::logic_error("utente che non c'e': " + nome);
}
inline const ListUser& utente(const Package& p, const std::string& nome) {
    for (const ClientList& l : p.client_lists)
        for (const ListUser& u : l.users)
            if (u.name == nome) return u;
    throw std::logic_error("utente che non c'e': " + nome);
}
inline std::string codice_di(const PacchettoInMemoria& m, const std::string& nome) { return utente(lettura(m).pacchetto, nome).code; }

// Il pacchetto con l'elenco cambiato sul modello del nucleo e riscritto con la scrittura del programma (elenco_utenti.json versione 3).
inline PacchettoInMemoria con_elenco(const PacchettoInMemoria& m, const std::function<void(ClientList&, const Package&)>& cambia) {
    const Lettura l = lettura(m);
    ClientList lista = l.pacchetto.client_lists.at(0);
    cambia(lista, l.pacchetto);
    PacchettoInMemoria out = m;
    out.file["elenco_utenti.json"] =
        kzero::api::chiusura::testo_file(kzero::api::elenco::documento_con(l, lista, l.pacchetto.list_last_number));
    return out;
}

// Una regola: la scheda (dal codice del registro, es. "SCH-4000901-1"), il luogo e la volta, i giorni (vuoti = sempre), la versione.
inline PointRule regola(const Package& p, const std::string& scheda, const std::string& luogo, int volta = 1,
                        const std::vector<Weekday>& giorni = {}, int versione = 1) {
    PointRule r;
    for (const ServiceCard& s : p.service_cards)
        if (s.code == scheda) r.card_id = s.id;
    if (r.card_id.empty()) throw std::logic_error("scheda che non c'e': " + scheda);
    r.place = Ref{luogo, {}};
    r.occurrence = volta;
    r.version = versione;
    r.all_days = giorni.empty();
    for (Weekday w : giorni) r.days.add(w);
    return r;
}
// Una voce nuova della storia dei punti di una tratta (con le regole del nucleo: chiude la voce di prima, sostituisce quelle dopo).
inline void punto(ListUser& u, const std::string& tratta, const std::string& dal, std::vector<PointRule> regole, PointOrigin origine = PointOrigin::Manual,
                  const std::string& consenso = std::string()) {
    UserPoints* up = nullptr;
    for (UserPoints& x : u.points)
        if (x.leg == tratta) up = &x;
    if (!up) {
        u.points.push_back(UserPoints{tratta, {}, {}});
        up = &u.points.back();
    }
    PointEntry e;
    e.from = *day_from_text(dal);
    e.mark = ManualMark{"mail del committente (prova)", *day_from_text(dal)};
    e.origin = origine;
    e.consent = consenso;
    e.rules = std::move(regole);
    add_point_entry(*up, e);
}

// I problemi del confronto di un controllo, raggruppati per giorni come in kzero controlla, in forma "codice vN".
inline std::vector<Problem> problemi_dei_punti(const Lettura& l) {
    std::vector<Problem> v;
    check_points(l.pacchetto, Index(l.pacchetto), v);
    return group_by_days(std::move(v));
}

}  // namespace kzero::prove::punti
