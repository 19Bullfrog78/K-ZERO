// K-Zero - prove delle schede dei servizi (K-021): lettura dei tre fogli con il profilo, totali, accompagnatore, km scritti solo su
// alcune righe, case porta a porta, cadenze, scelta della data, versioni, togliere una scheda, controlli sul registro. I comandi e le
// uscite: test_schede_comandi.cpp. Dati inventati (tests/esempi/k021-schede).
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "controlli_catalogo.hpp"
#include "excel.hpp"
#include "riservatezza.hpp"
#include "schede.hpp"
#include "schede_prove.hpp"
#include "schede_scrittura.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove::schede;

namespace {

Lettura lettura_di_prova() {
    Lettura l = leggi_pacchetto(pacchetto_di_prova());
    EXPECT_TRUE(l.forma_valida());
    return l;
}
const CardProfile& profilo(const Lettura& l, const std::string& codice = "PS-CVA") {
    for (const CardProfile& p : l.pacchetto.card_profiles)
        if (p.code == codice) return p;
    throw std::logic_error("profilo che non c'e'");
}

const Day kPresa = day_from_ymd(2026, 11, 16);

struct Letto {
    CardsRead r;
    CardsMerge m;
    Lettura dopo;
};
Letto leggi(const Lettura& l, const Fogli& f, DateChoices scelte = {}, Day presa = kPresa, bool date1904 = false,
            const std::string& codice_profilo = "PS-CVA") {
    const Index idx(l.pacchetto);
    Letto x;
    x.r = read_cards(CardTables{tabella(f.schede), tabella(f.fermate), tabella(f.date), tabella(f.interruzioni)}, profilo(l, codice_profilo),
                     idx, date1904);
    EXPECT_EQ(x.r.fault, TableFault::None) << x.r.fault_title;
    x.m = merge_cards(l.pacchetto, profilo(l, codice_profilo), "CT-SCU", presa, "mail del committente", scelte, x.r);
    x.dopo = schede::con_documenti(l, schede::documenti_dopo(l, x.m.cards, l.pacchetto.removed_cards, x.m.added, x.m.removed, x.m.cuts,
                                                             x.m.new_homes));
    return x;
}
DateChoices attivazione() {
    DateChoices s;
    s.file.kind = DateChoice::Kind::Activation;
    return s;
}
std::vector<std::string> messaggi(const Lettura& l, const std::vector<Problem>& v) {
    const Presentatore pr(&l.pacchetto, &l.versioni, Visibilita{true, true});
    std::vector<std::string> out;
    for (const Problem& p : v) out.push_back(definizione_controllo(p.check).codice + " v" + std::to_string(p.variant) + ": " + pr.messaggio(p));
    return out;
}

}  // namespace

TEST(Schede, TreSchedeDelFile) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, fogli_di_prova(), attivazione());
    for (const std::string& m : messaggi(x.dopo, x.m.problems)) std::cout << m << "\n";
    ASSERT_EQ(x.m.outcomes.size(), 3u);
    EXPECT_EQ(x.m.outcomes[0].kind, CardOutcome::Kind::New);
    EXPECT_EQ(x.m.outcomes[1].kind, CardOutcome::Kind::New);
    EXPECT_EQ(x.m.outcomes[2].kind, CardOutcome::Kind::Stopped);
    EXPECT_EQ(x.m.outcomes[2].fault, CardFault::TotalsAlighting);
    EXPECT_TRUE(x.dopo.forma_valida());
}

namespace {

ServiceCard nel_registro(const Lettura& l, const std::string& codice) {
    for (const ServiceCard& s : l.pacchetto.service_cards)
        if (s.code == codice) return s;
    throw std::logic_error("scheda che non c'e': " + codice);
}
JsonDati elemento(const Lettura& l, const std::string& file, const char* elenco, const std::string& codice) {
    for (const JsonDati& x : l.documenti.at(file).at(elenco))
        if (x.at("codice") == codice) return x;
    throw std::logic_error("elemento che non c'e': " + codice);
}
// Una sola scheda: l'intestazione e la riga i del foglio delle schede, con le sue fermate.
Fogli una(std::size_t i) {
    const Fogli f = fogli_di_prova();
    Fogli o;
    o.schede = {f.schede[0], f.schede[i]};
    o.fermate = {f.fermate[0]};
    const std::string nr = card_nr_key(f.schede[i][0], profilo(lettura_di_prova()));
    for (std::size_t k = 1; k < f.fermate.size(); ++k)
        if (card_nr_key(f.fermate[k][0], profilo(lettura_di_prova())) == nr) o.fermate.push_back(f.fermate[k]);
    o.date = f.date;
    return o;
}
// Una scheda dell'infanzia con l'accompagnatore: sale alla scuola dell'infanzia (St. Car. 1 = solo lui), poi i bambini, poi scende
// alla fine con loro (St. Scar. = bambini + accompagnatore).
Fogli infanzia(const std::string& sale = "07:30", bool accompagnatore_prima = true) {
    Fogli f;
    const Fogli base = fogli_di_prova();
    f.schede = {base.schede[0],
                {"4000200 / 1", "5c1e2f40-0a1b-4c2d-8e3f-000000000200", "Andata Mattino", "Scuola infanzia Lagobello", "14/09/2026",
                 "30/06/2027", "X", "X", "X", "X", "X", "", "", "#6", "", "", "#0", "9", "2", "MATERNA", "Sempre", "Scheda nuova"}};
    f.fermate = {base.fermate[0]};
    if (accompagnatore_prima) f.fermate.push_back({"4000200/1", sale, "Scuola infanzia Lagobello", "", "#1", "", "", "2", "SALE"});
    f.fermate.push_back({"4000200/1", "07:40", "Lagobello, Lago", "", "#2", "", "3", accompagnatore_prima ? "" : "2", ""});
    f.fermate.push_back({"4000200/1", "07:50", "PRADELLO PIAZZA", "", accompagnatore_prima ? "#3" : "#4", "", "", "", accompagnatore_prima ? "" : "SALE"});
    f.fermate.push_back({"4000200/1", "08:05", "Scuola infanzia Lagobello", "", "", "#6", "6", "", "SCENDE"});
    f.date = base.date;
    return f;
}

}  // namespace

// I totali della scheda: salgono = scendono = Totali alunni (accompagnatore compreso), km pieni e vuoti = i totali del piede. Se non
// tornano la scheda si ferma, con riga e colonna (mai il valore). Sabotaggio: totali che non tornano accettati.
TEST(Schede, TotaliCheNonTornano) {
    const Lettura l = lettura_di_prova();
    const auto con = [&](std::size_t colonna, const std::string& valore) {
        Fogli f = una(1);
        f.schede[1][colonna] = valore;
        return leggi(l, f, attivazione()).m.outcomes.at(0);
    };
    EXPECT_EQ(con(13, "#10").fault, CardFault::TotalsBoarding);   // Totali alunni 10, salgono 9
    EXPECT_EQ(con(17, "12,5").fault, CardFault::TotalLoadedKm);
    EXPECT_EQ(con(18, "6,1").fault, CardFault::TotalEmptyKm);
    EXPECT_EQ(con(17, "12,4").kind, CardOutcome::Kind::New);       // al centesimo
    Fogli f = una(1);
    f.fermate[3][5] = "#8";   // scendono 8 su 9
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::TotalsAlighting);
    f.fermate[3][5] = "#10";   // scendono piu' di quelli a bordo
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::MoreAlightingThanOnBoard);
    // Il messaggio: riga e colonna, mai il valore di una cella.
    const Letto x = leggi(l, fogli_di_prova(), attivazione());
    const std::vector<std::string> m = messaggi(x.dopo, x.m.problems);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_NE(m[0].find("K-CTR-160 v29: Scheda 4000125/2 (foglio \"Schede\", riga 4, colonna \"Totali alunni\")"), std::string::npos) << m[0];
}

// Due scuole: un gruppo solo, senza scuola, con le salite come sulla scheda (mai divise) e le discese vere a ogni scuola. Sabotaggio:
// St. Car. divisi tra le due scuole.
TEST(Schede, DueScuoleUnGruppoSenzaScuola) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, fogli_di_prova(), attivazione());
    const JsonDati s = elemento(x.dopo, "servizi.json", "servizi", "S-4000124-1");
    ASSERT_EQ(s.at("domanda").at("gruppi").size(), 1u);
    const JsonDati& g = s.at("domanda").at("gruppi").at(0);
    EXPECT_FALSE(g.contains("scuola"));
    EXPECT_EQ(g.at("fermate"), JsonDati::parse(R"([{"luogo":"F-MONTEBRINO","salgono":6},{"luogo":"F-CASTELROVO-ALTO","salgono":5},
                                                  {"luogo":"L-MEDIE","scendono":7},{"luogo":"L-PRIM","scendono":4}])"));
    EXPECT_EQ(nel_registro(x.dopo, "SCH-4000124-1").versions[0].card.schools, (std::vector<Code>{"SC-MEDIE", "SC-PRIM"}));
    // Con una scuola sola il gruppo ha la sua scuola.
    EXPECT_EQ(elemento(x.dopo, "servizi.json", "servizi", "S-4000123-3").at("domanda").at("gruppi").at(0).at("scuola"), "SC-PRIM");
}

// L'accompagnatore: sale e scende dove dice la sua colonna, i numeri della scheda lo comprendono; nel gruppo si toglie (mai un
// passeggero), sulla corsa e' il movimento dell'assistente, e il gruppo chiede l'assistente. Sabotaggio: accompagnatore contato come
// passeggero (non tolto dai numeri del gruppo).
TEST(Schede, AccompagnatoreMaiUnPasseggero) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, infanzia(), attivazione());
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New) << (x.m.outcomes.at(0).fault ? static_cast<int>(*x.m.outcomes.at(0).fault) : -1);
    const JsonDati g = elemento(x.dopo, "servizi.json", "servizi", "S-4000200-1").at("domanda").at("gruppi").at(0);
    EXPECT_EQ(g.at("fermate"), JsonDati::parse(R"([{"luogo":"F-LAGOBELLO","salgono":2},{"luogo":"F-PRADELLO","salgono":3},
                                                  {"luogo":"L-INFANZIA","scendono":5}])"));
    EXPECT_TRUE(g.at("assistente_richiesto").get<bool>());
    const JsonDati c = elemento(x.dopo, "corse.json", "corse", "C-4000200-1");
    EXPECT_EQ(c.at("passaggi").at(0).at("assistente"), "sale");
    EXPECT_FALSE(c.at("passaggi").at(0).contains("persone"));
    EXPECT_EQ(c.at("passaggi").at(3).at("assistente"), "scende");
    EXPECT_EQ(c.at("passaggi").at(3).at("persone"), JsonDati::parse(R"([{"gruppo":"G-4000200-1","scendono":5}])"));
    // La scheda si ferma: sale dove St. Car. e' 0; scende prima di salire; ancora a bordo alla fine; la parola nel punto.
    Fogli f = infanzia();
    f.fermate[1][4] = "";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::EscortZero);
    f = infanzia();
    f.fermate[3][8] = "SALE";   // sale quando e' gia' a bordo
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::EscortOrder);
    f = infanzia();
    f.fermate[1][8] = "";
    f.fermate[2][8] = "SCENDE";   // scende prima di essere salito (la riga ha chi scende)
    f.fermate[2][5] = "#1";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::EscortOrder);
    f = infanzia();
    f.fermate[4][8] = "";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::EscortStillOnBoard);
    f = infanzia();
    f.fermate[4][2] = "SCARICO + ACCOMPAGNATORE, Scuola infanzia Lagobello";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::EscortWordInPoint);
    f = infanzia();
    f.fermate[1][8] = "SU";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::EscortWordUnknown);
}

// K-CTR-171: la regola "assistente a bordo" del regolamento (R-TN: infanzia sui mezzi con piu' di 8 posti per i passeggeri) su una
// corsa di una scheda con qualcuno a bordo prima che salga l'accompagnatore: avviso (dipende dai posti del mezzo: variante 1). Con
// l'accompagnatore che sale per primo, nessun avviso; senza la regola nel regolamento, nessun avviso (niente valori nel programma).
// Sabotaggi: l'avviso che manca; l'avviso dato anche senza la regola.
TEST(Schede, AccompagnatoreCheMancaConLaRegolaDelRegolamento) {
    const Lettura l = lettura_di_prova();
    const auto avvisi = [](const Lettura& d) {
        std::vector<Problem> v;
        const Index idx(d.pacchetto);
        check_cards(d.pacchetto, idx, v);
        std::vector<Problem> out;
        for (const Problem& p : v)
            if (p.check == Check::CardEscortMissing) out.push_back(p);
        return out;
    };
    EXPECT_TRUE(avvisi(leggi(l, infanzia(), attivazione()).dopo).empty());
    const Letto tardi = leggi(l, infanzia("07:30", false), attivazione());
    ASSERT_EQ(tardi.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    const std::vector<Problem> v = avvisi(tardi.dopo);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_EQ(v[0].variant, 1);
    EXPECT_EQ(messaggi(tardi.dopo, v)[0],
              "K-CTR-171 v1: Corsa C-4000200-1 (4000200/1 Andata Mattino) (scheda SCH-4000200-1): dal passaggio F-LAGOBELLO (Lagobello, Lago) "
              "(alle 07:40) a F-PRADELLO (Pradello, Piazza) c'e' qualcuno a bordo senza l'accompagnatore; il regolamento R-TN (Testo unico del "
              "trasporto scolastico (regole pubbliche, esempio)) lo chiede se il mezzo ha piu' di 8 posti per i passeggeri (Delibera della Giunta provinciale n. 2 "
              "del 10/01/2025, 1.C1.9): dipende dal mezzo, che la scheda non dice.");
    // Il regolamento senza "assistente a bordo": nessun avviso.
    PacchettoInMemoria m = pacchetto_di_prova();
    JsonDati reg = JsonDati::parse(m.file.at("regolamenti.json"));
    for (JsonDati& r : reg.at("regolamenti")) r.at("parametri").erase("assistente_a_bordo");
    m.file["regolamenti.json"] = reg.dump(2);
    const Lettura senza = leggi_pacchetto(m);
    ASSERT_TRUE(senza.forma_valida());
    EXPECT_TRUE(avvisi(leggi(senza, infanzia("07:30", false), attivazione()).dopo).empty());
}

// Una cella dei km vuota vuol dire "non scritto", mai 0 (0,0 scritto e' zero); i km di una riga coprono il tratto dalla riga prima
// che ha dei km; i totali della corsa sono quelli del piede. Sabotaggio: la cella vuota letta come 0.
TEST(Schede, KmScrittiSoloSuAlcuneRighe) {
    const Lettura l = lettura_di_prova();
    Fogli f = fogli_di_prova();
    f.fermate[5][6] = "0,0";   // Castelrovo, Alto: zero scritto
    const Letto x = leggi(l, f, attivazione());
    const JsonDati c = elemento(x.dopo, "corse.json", "corse", "C-4000124-1");
    EXPECT_EQ(c.at("passaggi").at(0).at("km"), JsonDati::parse(R"({"vuoti":3.5})"));
    EXPECT_EQ(c.at("passaggi").at(1).at("km"), JsonDati::parse(R"({"pieni":0.0})"));
    const JsonDati c1 = elemento(x.dopo, "corse.json", "corse", "C-4000123-3");
    EXPECT_FALSE(c1.at("passaggi").at(0).contains("km") && c1.at("passaggi").at(0).at("km").contains("pieni"));
    EXPECT_EQ(c.at("km_totali"), JsonDati::parse(R"({"pieni":15.2,"vuoti":3.5})"));
    // Un pezzo della corsa con un capo senza km (Castelrovo Alto, cella vuota): i km del pezzo non si sanno; con i capi che hanno dei
    // km (o i capi della corsa) si'.
    const Letto y = leggi(l, fogli_di_prova(), attivazione());
    const Index idx(y.dopo.pacchetto);
    const Trip* t = idx.trip("C-4000124-1");
    ASSERT_NE(t, nullptr);
    EXPECT_FALSE(part_km(*t, 1, 2, true).known);
    EXPECT_TRUE(part_km(*t, 0, 2, true).known);
    EXPECT_DOUBLE_EQ(part_km(*t, 0, 2, true).loaded, 14.0);
    EXPECT_TRUE(part_km(*t, 2, 3, true).known);
    EXPECT_TRUE(part_km(*t, 1, 2, false).known);   // una corsa che non viene da una scheda: come prima
}

// La scelta della data: attivazione uguale o dopo la presa in carico = dall'attivazione; prima, senza una scelta la scheda si ferma
// (K-CTR-162, con le scelte), mai scelta dal programma; la scelta per una scheda vince su quella del file; una data prima
// dell'attivazione ferma la scheda. Sabotaggio: attivazione prima della presa in carico scelta dal programma.
TEST(Schede, SceltaDellaData) {
    const Lettura l = lettura_di_prova();
    const Letto senza = leggi(l, una(1));
    EXPECT_EQ(senza.m.outcomes.at(0).kind, CardOutcome::Kind::ChoiceMissing);
    EXPECT_TRUE(senza.m.cards.empty());
    ASSERT_EQ(senza.m.problems.size(), 1u);
    EXPECT_EQ(senza.m.problems[0].check, Check::CardChoiceMissing);
    // Presa in carico prima dell'attivazione: nessuna scelta serve.
    const Letto prima = leggi(l, una(1), {}, day_from_ymd(2026, 9, 1));
    EXPECT_EQ(prima.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(prima.m.outcomes.at(0).from, ChoiceFrom::None);
    EXPECT_EQ(prima.m.outcomes.at(0).valid.from, day_from_ymd(2026, 9, 14));
    // Le tre scelte.
    DateChoices s;
    s.file.kind = DateChoice::Kind::TakenInCharge;
    EXPECT_EQ(leggi(l, una(1), s).m.outcomes.at(0).valid.from, kPresa);
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 12, 1)};
    const Letto per_scheda = leggi(l, una(1), s);
    EXPECT_EQ(per_scheda.m.outcomes.at(0).valid.from, day_from_ymd(2026, 12, 1));
    EXPECT_EQ(per_scheda.m.outcomes.at(0).from, ChoiceFrom::Card);
    EXPECT_EQ(nel_registro(per_scheda.dopo, "SCH-4000123-3").versions[0].mode, ChoiceMode::Date);
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 9, 1)};
    EXPECT_EQ(leggi(l, una(1), s).m.outcomes.at(0).fault, CardFault::ChoiceBeforeActivation);
    // Il calendario della scheda vale dalla data scelta: i giorni prima non ci sono.
    const Calendar* c = Index(per_scheda.dopo.pacchetto).calendar("CAL-4000123-3");
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->range.from, day_from_ymd(2026, 12, 1));
}

// Versioni: lo stesso ID = versione nuova, dalla data scelta; quella di prima finisce il giorno prima; i giorni prima della data
// scelta non cambiano (calendari e corse uguali, giorno per giorno). Con una data prima della presa in carico (scelta B dell'arbitro)
// l'uscita dice quante giornate cambiano; le versioni che cominciavano da quella data in poi si tolgono. Contenuto uguale: niente.
// Sabotaggio: versione nuova che cambia i giorni passati.
TEST(Schede, VersioniNuoveIGiorniPrimaNonCambiano) {
    const Lettura l0 = lettura_di_prova();
    const Letto v1 = leggi(l0, una(1), attivazione());
    // Uguale: niente da fare (la parola nuova/modifica non conta).
    Fogli uguale = una(1);
    uguale.schede[1][21] = "Modifica";
    const Letto u = leggi(v1.dopo, uguale, attivazione());
    EXPECT_EQ(u.m.outcomes.at(0).kind, CardOutcome::Kind::Same);
    EXPECT_TRUE(u.m.added.empty());
    // Una modifica dal 2026-12-01.
    Fogli mod = una(1);
    mod.schede[1][21] = "Modifica";
    mod.fermate[1][1] = "07:25";
    DateChoices s;
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 12, 1)};
    const Letto v2 = leggi(v1.dopo, mod, s);
    ASSERT_EQ(v2.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(v2.m.outcomes.at(0).version, 2);
    EXPECT_EQ(v2.m.outcomes.at(0).cut_to, day_from_ymd(2026, 11, 30));
    ASSERT_EQ(v2.m.outcomes.at(0).changes.size(), 1u);
    EXPECT_EQ(v2.m.outcomes.at(0).changes[0].what, "orari");
    const ServiceCard sc = nel_registro(v2.dopo, "SCH-4000123-3");
    ASSERT_EQ(sc.versions.size(), 2u);
    EXPECT_EQ(sc.versions[0].valid.to, day_from_ymd(2026, 11, 30));
    EXPECT_EQ(sc.versions[1].valid.from, day_from_ymd(2026, 12, 1));
    EXPECT_EQ(sc.versions[1].previous_end, day_from_ymd(2027, 6, 12));
    // Giorno per giorno, prima del 2026-12-01: le stesse corse con gli stessi orari.
    const Index a(v1.dopo.pacchetto), b(v2.dopo.pacchetto);
    const auto corse_del_giorno = [](const Index& idx, Day d) {
        std::vector<std::string> v;
        for (const Trip& t : idx.package().trips)
            if (const Calendar* c = idx.calendar(t.calendar.code))
                if (calendar_active(*c, d, idx)) v.push_back(t.code + " " + text_from_minutes(t.start()));
        return v;
    };
    for (Day d = day_from_ymd(2026, 9, 1); d < day_from_ymd(2026, 12, 1); ++d) EXPECT_EQ(corse_del_giorno(a, d), corse_del_giorno(b, d)) << text_from_day(d);
    EXPECT_NE(corse_del_giorno(a, day_from_ymd(2026, 12, 1)), corse_del_giorno(b, day_from_ymd(2026, 12, 1)));
    // Una terza versione da una data prima della presa in carico (e prima della seconda): la seconda si toglie, la prima finisce prima.
    mod.fermate[1][1] = "07:15";
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 11, 2)};
    const Letto v3 = leggi(v2.dopo, mod, s, day_from_ymd(2026, 11, 20));
    ASSERT_EQ(v3.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(v3.m.outcomes.at(0).version, 3);
    EXPECT_EQ(v3.m.outcomes.at(0).removed_versions, std::vector<int>{2});
    EXPECT_GT(v3.m.outcomes.at(0).days_before_taken, 0);
    const ServiceCard sc3 = nel_registro(v3.dopo, "SCH-4000123-3");
    ASSERT_EQ(sc3.versions.size(), 2u);
    EXPECT_EQ(sc3.versions[0].valid.to, day_from_ymd(2026, 11, 1));
    EXPECT_EQ(sc3.versions[1].number, 3);
    EXPECT_EQ(Index(v3.dopo.pacchetto).service("S-4000123-3-V2"), nullptr);   // gli elementi della versione tolta se ne vanno
    bool avviso = false;
    for (const Problem& p : v3.m.problems) avviso = avviso || (p.check == Check::CardChosenBeforeTaken && p.variant == 1);
    EXPECT_TRUE(avviso);
    const Index c(v3.dopo.pacchetto);
    for (Day d = day_from_ymd(2026, 9, 1); d < day_from_ymd(2026, 11, 2); ++d) EXPECT_EQ(corse_del_giorno(a, d), corse_del_giorno(c, d)) << text_from_day(d);
}

// Due schede con ID diversi non si uniscono mai, nemmeno con lo stesso Nr: e' una scheda nuova, con l'avviso dei periodi che si
// sovrappongono (K-CTR-169). Lo stesso ID con un altro contratto ferma la scheda. Sabotaggio: schede con ID diversi unite.
TEST(Schede, IdDiversiMaiUniti) {
    const Lettura l = lettura_di_prova();
    const Letto v1 = leggi(l, una(1), attivazione());
    Fogli altro = una(1);
    altro.schede[1][1] = "5c1e2f40-0a1b-4c2d-8e3f-0000000000ff";
    altro.fermate[1][1] = "07:25";
    const Letto x = leggi(v1.dopo, altro, attivazione());
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(x.m.cards.size(), 2u);
    EXPECT_EQ(nel_registro(x.dopo, "SCH-4000123-3").versions.size(), 1u);
    EXPECT_EQ(x.m.outcomes.at(0).card_code, "SCH-4000123-3-2");   // il codice della scheda e' gia' usato: si salta al numero dopo
    std::vector<Problem> v;
    const Index idx(x.dopo.pacchetto);
    check_cards(x.dopo.pacchetto, idx, v);
    EXPECT_TRUE(std::any_of(v.begin(), v.end(), [](const Problem& p) { return p.check == Check::CardServiceOverlap; }));
    // L'ID si confronta senza maiuscole; lo stesso ID con un altro contratto: ferma.
    Fogli maiuscole = una(1);
    maiuscole.schede[1][1] = "5C1E2F40-0A1B-4C2D-8E3F-000000000001";
    EXPECT_EQ(leggi(v1.dopo, maiuscole, attivazione()).m.outcomes.at(0).kind, CardOutcome::Kind::Same);
    const Index i1(v1.dopo.pacchetto);
    const CardsRead r = read_cards(CardTables{tabella(una(1).schede), tabella(una(1).fermate), {}, {}}, profilo(v1.dopo), i1, false);
    const CardsMerge m = merge_cards(v1.dopo.pacchetto, profilo(v1.dopo), "CT-ASILO", kPresa, "x", attivazione(), r);
    EXPECT_EQ(m.outcomes.at(0).fault, CardFault::OtherContract);
}

// Cadenze: una cadenza che il profilo non conosce ferma la scheda (mai trattata come "Sempre"); a date elencate valgono solo le date
// del foglio delle date (mai un ritmo ricavato), con l'avviso per una data fuori dai giorni segnati. Sabotaggio: cadenza sconosciuta
// trattata come "Sempre".
TEST(Schede, Cadenze) {
    const Lettura l = lettura_di_prova();
    Fogli f = una(1);
    f.schede[1][20] = "Ogni 7 giorni";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::CadenceUnknown);
    f.schede[1][20] = "Ogni 15 giorni";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::NoListedDates);
    f.date = {{"Nr", "Data"}, {"4000123 / 3", "21/09/2026"}, {"4000123 / 3", "#46300"}, {"4000123 / 3", "19/10/2026"}, {"4000123 / 3", "17/10/2026"}};
    f.schede[1][16] = "#4";
    const Letto x = leggi(l, f, attivazione());
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    const Calendar* c = Index(x.dopo.pacchetto).calendar("CAL-4000123-3");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->days.empty());
    ASSERT_EQ(c->added.size(), 4u);
    EXPECT_EQ(text_from_day(c->added[1].day), "2026-10-05");   // 46295: la data di Excel (sistema 1900)
    // Il 17/10/2026 e' un sabato: nessuna X, avviso (non indovino quale sia giusto).
    bool avviso = false;
    for (const Problem& p : x.m.problems) avviso = avviso || p.check == Check::CardListedDateOutside;
    EXPECT_TRUE(avviso);
    f.date.push_back({"4000123 / 3", "01/09/2026"});
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::DateOutsidePeriod);
}

// Tot Giorni diverso dai giorni del calendario: avviso, la scheda va avanti (il calendario del committente o quello del pacchetto e'
// diverso). Sabotaggio: Tot Giorni diverso che ferma la scheda.
TEST(Schede, TotGiorniDiversoAvvisoSoltanto) {
    const Lettura l = lettura_di_prova();
    Fogli f = una(1);
    f.schede[1][16] = "#170";
    const Letto x = leggi(l, f, attivazione());
    EXPECT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    ASSERT_EQ(x.m.problems.size(), 1u);
    EXPECT_EQ(messaggi(x.dopo, x.m.problems)[0].substr(0, 50), "K-CTR-163 v0: Scheda 4000123/3: Tot Giorni e' 170,");
    EXPECT_EQ(card_days(x.m.cards.at(0).versions[0].card, Index(x.dopo.pacchetto)), 175);
}

// La casa di un utente: un codice di luogo privato porta a porta (mai l'indirizzo); se non c'e' si crea senza dati personali, tra i
// luoghi privati, mai tra i luoghi comuni; un codice di un luogo pubblico ferma la scheda. Con il porta a porta sensibile (demo) la
// casa e il suo giro sono legati (C9). Sabotaggio: casa di un utente scritta come luogo comune.
TEST(Schede, CasaPortaAPortaSenzaIndirizzo) {
    const Lettura l = lettura_di_prova();
    Fogli f = una(1);
    f.fermate[1][2] = "";
    f.fermate[1][3] = "PAP-001";
    f.schede[1][14] = "X";   // individualizzato
    const Letto x = leggi(l, f, attivazione());
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(x.m.new_homes, std::vector<Code>{"PAP-001"});
    const Place* p = Index(x.dopo.pacchetto).place("PAP-001");
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(p->is_private);
    EXPECT_EQ(p->type, PlaceType::DoorToDoor);
    for (const JsonDati& y : x.dopo.documenti.at("luoghi.json").at("luoghi")) EXPECT_NE(y.at("codice"), "PAP-001");
    EXPECT_EQ(elemento(x.dopo, "luoghi.json", "luoghi_privati", "PAP-001"), JsonDati::parse(R"({"codice":"PAP-001","tipo":"porta_a_porta"})"));
    EXPECT_TRUE(elemento(x.dopo, "servizi.json", "servizi", "S-4000123-3").at("individualizzato").get<bool>());
    // C9: con il porta a porta sensibile (manifesto del demo) la casa e' legata.
    const Index idx(x.dopo.pacchetto);
    EXPECT_TRUE(LegameSensibile(&idx).casa("PAP-001"));
    // Una seconda scheda con la stessa casa la riusa (andata e ritorno della stessa persona).
    Fogli g = una(3);
    g.fermate[3][2] = "";
    g.fermate[3][3] = "PAP-001";
    const Letto y = leggi(x.dopo, g, attivazione());
    ASSERT_EQ(y.m.outcomes.at(0).kind, CardOutcome::Kind::Stopped);   // la terza scheda ha un totale sbagliato
    f.fermate[1][3] = "F-PRADELLO";   // un luogo pubblico come casa: ferma
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::HomeNotDoorToDoor);
    f.fermate[1][3] = "via Roma 3";   // non e' un codice
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::HomeNotDoorToDoor);
    f.fermate[1][2] = "PRADELLO PIAZZA";   // punto e casa insieme
    f.fermate[1][3] = "PAP-001";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::PointAndHome);
}

// Date e orari come li scrive Excel (numeri: sistema 1900 e 1904) o come testo; un orario oltre 47:59, una data che non esiste, un
// punto di raccolta sconosciuto o ambiguo, una scuola sconosciuta, un calendario sconosciuto: la scheda si ferma.
TEST(Schede, CelleEMotiviDiStop) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, una(2), attivazione());   // la seconda: attivazione e disattivazione e un orario come numeri di Excel
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(x.m.cards.at(0).versions[0].card.from, day_from_ymd(2026, 9, 14));
    EXPECT_EQ(x.m.cards.at(0).versions[0].card.stops[0].time, 7 * 60);
    // Le stesse date nel sistema del 1904 (1462 giorni in meno).
    Fogli f = una(2);
    f.schede[1][4] = "#44817";
    f.schede[1][5] = "#45088";
    const Letto y = leggi(l, f, attivazione(), kPresa, true);
    EXPECT_EQ(y.m.cards.at(0).versions[0].card.from, day_from_ymd(2026, 9, 14));
    const auto ferma = [&](std::size_t foglio, std::size_t riga, std::size_t colonna, const std::string& valore) {
        Fogli g = una(1);
        (foglio == 0 ? g.schede : g.fermate)[riga][colonna] = valore;
        return leggi(l, g, attivazione()).m.outcomes.at(0).fault;
    };
    EXPECT_EQ(ferma(1, 1, 1, "48:00"), CardFault::BadTime);
    EXPECT_EQ(ferma(1, 1, 1, "7.20"), CardFault::BadTime);
    EXPECT_EQ(ferma(0, 1, 4, "31/04/2027"), CardFault::BadDate);
    EXPECT_EQ(ferma(0, 1, 5, "01/09/2026"), CardFault::DatesReversed);
    EXPECT_EQ(ferma(1, 2, 2, "Bivio sconosciuto"), CardFault::PointUnknown);
    EXPECT_EQ(ferma(0, 1, 3, "Scuola che non c'e'"), CardFault::SchoolUnknown);
    EXPECT_EQ(ferma(0, 1, 19, "ESTIVO"), CardFault::CalendarUnknown);
    EXPECT_EQ(ferma(0, 1, 2, "Andata Sera"), CardFault::ServiceText);
    EXPECT_EQ(ferma(0, 1, 6, "si"), CardFault::BadMark);
    EXPECT_EQ(ferma(0, 1, 21, "Nuovissima"), CardFault::NewOrChangeUnknown);
    EXPECT_EQ(ferma(1, 1, 4, "cinque"), CardFault::BadNumber);
    EXPECT_EQ(ferma(1, 1, 4, "!"), CardFault::UnreadableCell);
    EXPECT_EQ(ferma(0, 1, 3, "Scuola media Valfonda"), CardFault::SchoolRowNotInHeader);
    // La riga delle fermate di una scheda che non c'e': K-CTR-161.
    Fogli g = una(1);
    g.fermate.push_back({"9999999 / 1", "08:00", "PRADELLO PIAZZA", "", "#1", "", "", "", ""});
    const Letto z = leggi(l, g, attivazione());
    ASSERT_EQ(z.r.problems.size(), 1u);
    EXPECT_EQ(z.r.problems[0].check, Check::CardRowWithoutCard);
    // Lo stesso Nr due volte: ferme tutte e due.
    Fogli h = una(1);
    h.schede.push_back(h.schede[1]);
    h.schede[2][1] = "5c1e2f40-0a1b-4c2d-8e3f-0000000000aa";
    const Letto w = leggi(l, h, attivazione());
    EXPECT_EQ(w.m.outcomes.at(0).fault, CardFault::DuplicateNr);
    EXPECT_EQ(w.m.outcomes.at(1).fault, CardFault::DuplicateNr);
}

// Togliere una scheda scritta per errore: l'ultima versione, e quella di prima riprende la sua fine; con "tutte" sparisce. Non si toglie
// un'ultima versione che ha tolto altre versioni. Riferimento e data restano nel registro; i numeri delle versioni non si ridanno.
TEST(Schede, TogliereUnaScheda) {
    const Lettura l = lettura_di_prova();
    const Letto v1 = leggi(l, una(1), attivazione());
    Fogli mod = una(1);
    mod.fermate[1][1] = "07:25";
    DateChoices s;
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 12, 1)};
    const Letto v2 = leggi(v1.dopo, mod, s);
    std::vector<ServiceCard> schede = v2.dopo.pacchetto.service_cards;
    std::vector<RemovedCardVersions> tolte;
    const CardRemoval r = remove_card(schede, tolte, "SCH-4000123-3", false, "copiata per errore", day_from_ymd(2026, 11, 17), v2.dopo.pacchetto);
    ASSERT_EQ(r.refusal, CardRemovalRefusal::None);
    ASSERT_TRUE(r.restored);
    EXPECT_EQ(r.restored->to, day_from_ymd(2027, 6, 12));
    ASSERT_EQ(schede.size(), 1u);
    EXPECT_EQ(schede[0].versions.size(), 1u);
    ASSERT_EQ(tolte.size(), 1u);
    EXPECT_EQ(tolte[0].versions, std::vector<int>{2});
    const Lettura dopo = schede::con_documenti(v2.dopo, schede::documenti_dopo(v2.dopo, schede, tolte, {}, r.removed, {*r.restored}, {}));
    EXPECT_EQ(Index(dopo.pacchetto).service("S-4000123-3-V2"), nullptr);
    EXPECT_EQ(Index(dopo.pacchetto).calendar("CAL-4000123-3")->range.to, day_from_ymd(2027, 6, 12));
    // La versione dopo non riusa il numero 2.
    const Letto v3 = leggi(dopo, mod, s);
    EXPECT_EQ(v3.m.outcomes.at(0).version, 3);
    // Tutta la scheda.
    std::vector<ServiceCard> s2 = v2.dopo.pacchetto.service_cards;
    std::vector<RemovedCardVersions> t2;
    const CardRemoval tutto = remove_card(s2, t2, "SCH-4000123-3", true, "copiata per errore", day_from_ymd(2026, 11, 17), v2.dopo.pacchetto);
    EXPECT_TRUE(tutto.card_gone);
    EXPECT_TRUE(s2.empty());
    EXPECT_EQ(tutto.removed.size(), 2u);
}

// K-CTR-170: un elemento della scheda cambiato a mano (qui un orario della corsa) o che non c'e' piu': avviso in kzero controlla.
TEST(Schede, ElementoCambiatoAMano) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, una(1), attivazione());
    EXPECT_TRUE(schede::controlla_elementi(x.dopo).empty());
    std::map<std::string, JsonDati> d = {{"corse.json", x.dopo.documenti.at("corse.json")}};
    for (JsonDati& c : d.at("corse.json").at("corse"))
        if (c.at("codice") == "C-4000123-3") c["passaggi"][0]["orario"] = "07:21";
    const Lettura cambiata = schede::con_documenti(x.dopo, d);
    const std::vector<Problem> v = schede::controlla_elementi(cambiata);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_NE(messaggi(cambiata, v)[0].find("K-CTR-170 v0: Scheda SCH-4000123-3, versione 1: l'elemento C-4000123-3"), std::string::npos)
        << messaggi(cambiata, v)[0];
}

// Fuzzing del K-021: un testo non UTF-8 (o oltre i 300 byte dei testi del formato) in una cella copiata nel registro (Nr, ID) rompeva
// la scrittura dei documenti. Il lettore Excel da' sempre UTF-8 valido, ma la regola deve reggere da sola: la cella non si legge e la
// scheda si ferma; i documenti fatti dal programma restano conformi agli schemi. (Il caso del fuzzing non puo' stare in
// tests/fuzz/casi: il deposito ammette solo testi UTF-8, K-016.)
TEST(Schede, CelleNonUtf8OTroppoLunghe) {
    const Lettura l = lettura_di_prova();
    for (const std::string& id : {std::string("5c1e2f40-\x82-0001"), std::string(301, 'a')}) {
        Fogli f = fogli_di_prova();
        f.schede[1][1] = id;
        const Letto x = leggi(l, f, attivazione());   // con documenti_dopo e con_documenti: nessuna eccezione
        EXPECT_EQ(x.m.outcomes.at(0).fault, CardFault::UnreadableCell) << id.size();
        EXPECT_EQ(x.m.outcomes.at(1).kind, CardOutcome::Kind::New);
    }
    Fogli f = fogli_di_prova();
    f.schede[1][2] = "Andata  Mattino \x82";
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).fault, CardFault::UnreadableCell);
    f = fogli_di_prova();
    f.schede[1][2] = "andata, mattino";   // la parola del profilo si riconosce e nel registro si tiene quella del profilo
    const Letto y = leggi(l, f, attivazione());
    ASSERT_EQ(y.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(y.m.cards.at(0).versions[0].card.service_text, "Andata Mattino");
}

// --- K-022 ------------------------------------------------------------------------------------------------------------------------

namespace {

// I problemi di kzero controlla (nucleo) su uno dei gruppi dati.
std::vector<Problem> sui_gruppi(const std::vector<Problem>& v, const std::set<std::string>& gruppi, Check c) {
    std::vector<Problem> out;
    for (const Problem& p : v)
        if (p.check == c)
            for (const Param& x : p.params)
                if (x.name == "gruppo" && gruppi.count(x.text)) out.push_back(p);
    return out;
}

}  // namespace

// K-022: una scheda con due giri passa due volte dallo stesso luogo con delle persone (l'infanzia: la scuola a meta' con le discese
// del primo giro e alla fine; un'andata con la stessa scuola due volte). Nella domanda del gruppo il luogo e' una volta sola, con le
// persone sommate; la corsa tiene i due passaggi. kzero controlla non da' K-CTR-042 (domanda contro corse) ne' K-CTR-175 su quei
// gruppi, e il confronto con il pacchetto (K-CTR-170) torna. Sabotaggio: una voce per passaggio (K-CTR-042 falso, due volte).
TEST(Schede, DueGiriUnLuogoUnaVoltaSolaNellaDomanda) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, due_giri(), attivazione());
    for (const std::string& m : messaggi(x.dopo, x.m.problems)) std::cout << m << "\n";
    ASSERT_EQ(x.m.outcomes.size(), 2u);
    ASSERT_EQ(x.m.outcomes[0].kind, CardOutcome::Kind::New) << (x.m.outcomes[0].fault ? static_cast<int>(*x.m.outcomes[0].fault) : -1);
    ASSERT_EQ(x.m.outcomes[1].kind, CardOutcome::Kind::New) << (x.m.outcomes[1].fault ? static_cast<int>(*x.m.outcomes[1].fault) : -1);
    const JsonDati g = elemento(x.dopo, "servizi.json", "servizi", "S-4000300-1").at("domanda").at("gruppi").at(0);
    EXPECT_EQ(g.at("fermate"), JsonDati::parse(R"([{"luogo":"F-LAGOBELLO","salgono":6},{"luogo":"F-PRADELLO","salgono":5},
                                                  {"luogo":"L-INFANZIA","scendono":22},{"luogo":"F-CASTELROVO","salgono":7},
                                                  {"luogo":"F-MONTEBRINO","salgono":4}])"));
    EXPECT_TRUE(g.at("assistente_richiesto").get<bool>());
    const JsonDati c = elemento(x.dopo, "corse.json", "corse", "C-4000300-1");
    ASSERT_EQ(c.at("passaggi").size(), 7u);
    EXPECT_EQ(c.at("passaggi").at(3).at("persone"), JsonDati::parse(R"([{"gruppo":"G-4000300-1","scendono":11}])"));
    EXPECT_EQ(c.at("passaggi").at(6).at("persone"), JsonDati::parse(R"([{"gruppo":"G-4000300-1","scendono":11}])"));
    EXPECT_EQ(c.at("passaggi").at(6).at("assistente"), "scende");
    EXPECT_EQ(c.at("passaggi").at(6).at("km"), JsonDati::parse(R"({"pieni":25.0})"));
    EXPECT_EQ(elemento(x.dopo, "servizi.json", "servizi", "S-4000301-1").at("domanda").at("gruppi").at(0).at("fermate"),
              JsonDati::parse(R"([{"luogo":"F-PRADELLO","salgono":5},{"luogo":"L-PRIM","scendono":9},{"luogo":"F-CASTELROVO","salgono":4}])"));
    // kzero controlla: niente K-CTR-042 ne' K-CTR-175 sui due gruppi; gli elementi corrispondono alla scheda.
    const std::vector<Problem> v = check_package(x.dopo.pacchetto);
    const std::set<std::string> gruppi = {"G-4000300-1", "G-4000301-1"};
    EXPECT_TRUE(sui_gruppi(v, gruppi, Check::DemandDiffersFromTrips).empty()) << messaggi(x.dopo, sui_gruppi(v, gruppi, Check::DemandDiffersFromTrips)).front();
    EXPECT_TRUE(sui_gruppi(v, gruppi, Check::DemandSamePlaceTwice).empty());
    EXPECT_TRUE(schede::controlla_elementi(x.dopo).empty());
}

// K-022: un gruppo scritto con lo stesso luogo due volte nella domanda (qui come lo scriveva la lettura del K-021: una voce per
// passaggio). Il confronto con le corse (K-CTR-042) somma le voci dello stesso luogo: nessun avviso falso; l'avviso di pulizia
// K-CTR-175 lo dice, sulla seconda voce, una volta per luogo; il confronto con la scheda (K-CTR-170) vede che il gruppo non e' quello
// che la scheda da'. Sabotaggi: l'avviso che manca; K-CTR-042 sulle voci una per una.
TEST(Schede, StessoLuogoDueVolteAvvisoNuovo) {
    const Lettura l = lettura_di_prova();
    const Letto x = leggi(l, due_giri(), attivazione());
    std::map<std::string, JsonDati> d = {{"servizi.json", x.dopo.documenti.at("servizi.json")}};
    for (JsonDati& s : d.at("servizi.json").at("servizi"))
        if (s.at("codice") == "S-4000300-1")
            s["domanda"]["gruppi"][0]["fermate"] = JsonDati::parse(R"([{"luogo":"F-LAGOBELLO","salgono":6},{"luogo":"F-PRADELLO","salgono":5},
                {"luogo":"L-INFANZIA","scendono":11},{"luogo":"F-CASTELROVO","salgono":7},{"luogo":"F-MONTEBRINO","salgono":4},
                {"luogo":"L-INFANZIA","scendono":11}])");
    const Lettura vecchia = schede::con_documenti(x.dopo, d);
    const std::vector<Problem> v = check_package(vecchia.pacchetto);
    const std::vector<Problem> due = sui_gruppi(v, {"G-4000300-1"}, Check::DemandSamePlaceTwice);
    ASSERT_EQ(due.size(), 1u);
    EXPECT_EQ(due[0].where.file, "servizi.json");
    EXPECT_EQ(due[0].where.pointer.substr(due[0].where.pointer.size() - 10), "/fermate/5") << due[0].where.pointer;
    EXPECT_EQ(messaggi(vecchia, due)[0],
              "K-CTR-175 v0: Gruppo G-4000300-1: il luogo L-INFANZIA (Lagobello, Scuola dell'infanzia) e' due volte nella domanda. Unire "
              "le voci in una sola (salgono e scendono sommati): i controlli contano gia' il luogo una volta sola, con le voci sommate.");
    EXPECT_TRUE(sui_gruppi(v, {"G-4000300-1"}, Check::DemandDiffersFromTrips).empty());   // K-CTR-042 somma le voci: niente di falso
    const std::vector<Problem> e = schede::controlla_elementi(vecchia);
    ASSERT_EQ(e.size(), 1u);
    EXPECT_NE(messaggi(vecchia, e)[0].find("S-4000300-1"), std::string::npos) << messaggi(vecchia, e)[0];
}

// S3: un orario della scheda che torna indietro ferma la scheda (variante 26), con la riga e la colonna. Sabotaggio: accettato.
TEST(Schede, OrarioCheTornaIndietro) {
    const Lettura l = lettura_di_prova();
    Fogli f = una(1);
    f.fermate[2][1] = "07:19";   // Castelrovo prima di Pradello (07:20)
    const Letto x = leggi(l, f, attivazione());
    EXPECT_EQ(x.m.outcomes.at(0).fault, CardFault::TimeBackwards);
    ASSERT_EQ(x.m.problems.size(), 1u);
    EXPECT_EQ(messaggi(x.dopo, x.m.problems)[0],
              "K-CTR-160 v26: Scheda 4000123/3 (foglio \"Fermate\", riga 3, colonna \"Orario\"): l'orario e' prima di quello della riga "
              "prima: la scheda si ferma (le altre del file vanno avanti). Correggere la copia e rileggere il file.");
    f.fermate[2][1] = "07:20";   // alla stessa ora: va bene
    EXPECT_EQ(leggi(l, f, attivazione()).m.outcomes.at(0).kind, CardOutcome::Kind::New);
}

// S5: l'attivazione il giorno prima della presa in carico vuole una scelta (K-CTR-162); lo stesso giorno no: vale dall'attivazione.
// Sabotaggi: la scelta che serve solo da due giorni prima; la scelta chiesta anche lo stesso giorno.
TEST(Schede, AttivazioneIlGiornoPrimaOLoStessoGiorno) {
    const Lettura l = lettura_di_prova();
    Fogli f = una(1);
    f.schede[1][4] = "15/11/2026";
    const Letto prima = leggi(l, f, {}, kPresa);
    EXPECT_EQ(prima.m.outcomes.at(0).kind, CardOutcome::Kind::ChoiceMissing);
    ASSERT_EQ(prima.m.problems.size(), 1u);
    EXPECT_EQ(prima.m.problems[0].check, Check::CardChoiceMissing);
    f.schede[1][4] = "16/11/2026";
    const Letto stesso = leggi(l, f, {}, kPresa);
    ASSERT_EQ(stesso.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(stesso.m.outcomes.at(0).from, ChoiceFrom::None);
    EXPECT_EQ(stesso.m.outcomes.at(0).mode, ChoiceMode::Activation);
    EXPECT_EQ(stesso.m.outcomes.at(0).valid.from, kPresa);
}

// S8: una versione nuova scelta dallo stesso giorno in cui comincia una versione di prima toglie quella versione (non la accorcia a
// un periodo che finisce prima di cominciare); quella prima ancora finisce il giorno prima. Sabotaggio: ">" al posto di ">=".
TEST(Schede, VersioneNuovaDalloStessoGiornoDiUnaDiPrima) {
    const Lettura l0 = lettura_di_prova();
    const Letto v1 = leggi(l0, una(1), attivazione());
    Fogli mod = una(1);
    mod.schede[1][21] = "Modifica";
    mod.fermate[1][1] = "07:25";
    DateChoices s;
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 12, 1)};
    const Letto v2 = leggi(v1.dopo, mod, s);
    ASSERT_EQ(v2.m.outcomes.at(0).version, 2);
    mod.fermate[1][1] = "07:15";
    const Letto v3 = leggi(v2.dopo, mod, s);   // ancora dal 2026-12-01
    ASSERT_EQ(v3.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(v3.m.outcomes.at(0).removed_versions, std::vector<int>{2});
    EXPECT_FALSE(v3.m.outcomes.at(0).cut_version);   // la versione 1 finiva gia' il 2026-11-30
    const ServiceCard sc = nel_registro(v3.dopo, "SCH-4000123-3");
    ASSERT_EQ(sc.versions.size(), 2u);
    EXPECT_EQ(sc.versions[0].number, 1);
    EXPECT_EQ(sc.versions[0].valid.to, day_from_ymd(2026, 11, 30));
    EXPECT_EQ(sc.versions[1].number, 3);
    EXPECT_EQ(sc.versions[1].valid.from, day_from_ymd(2026, 12, 1));
    for (const ServiceCard& x : v3.dopo.pacchetto.service_cards)
        for (const CardVersion& v : x.versions) EXPECT_LE(v.valid.from, v.valid.to) << x.code << " " << v.number;
    EXPECT_EQ(Index(v3.dopo.pacchetto).service("S-4000123-3-V2"), nullptr);
    std::vector<Problem> p;
    check_cards(v3.dopo.pacchetto, Index(v3.dopo.pacchetto), p);
    for (const Problem& x : p) EXPECT_NE(x.check, Check::CardRegisterInvalid) << messaggi(v3.dopo, {x})[0];
}

// S9: un solo bambino a bordo senza l'accompagnatore basta per l'avviso K-CTR-171 (regola R-TN dell'infanzia). Sabotaggio: la soglia
// "piu' di uno a bordo".
TEST(Schede, UnSoloBambinoSenzaAccompagnatore) {
    const Lettura l = lettura_di_prova();
    Fogli f = infanzia("07:30", false);
    f.fermate[1][4] = "#1";   // Lagobello: sale un bambino solo, l'accompagnatore sale dopo a Pradello (con quattro bambini)
    f.fermate[2][4] = "#5";
    const Letto x = leggi(l, f, attivazione());
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New) << (x.m.outcomes.at(0).fault ? static_cast<int>(*x.m.outcomes.at(0).fault) : -1);
    std::vector<Problem> v;
    check_cards(x.dopo.pacchetto, Index(x.dopo.pacchetto), v);
    std::vector<Problem> avvisi;
    for (const Problem& p : v)
        if (p.check == Check::CardEscortMissing) avvisi.push_back(p);
    ASSERT_EQ(avvisi.size(), 1u);
    EXPECT_NE(messaggi(x.dopo, avvisi)[0].find("dal passaggio F-LAGOBELLO (Lagobello, Lago) (alle 07:40) a F-PRADELLO"), std::string::npos)
        << messaggi(x.dopo, avvisi)[0];
}

// S16: un pezzo di una corsa da scheda che finisce in un passaggio senza km (Castelrovo Alto): i km del pezzo non si sanno (anche se
// comincia dal primo passaggio), e il turno con quel pezzo ha l'avviso K-CTR-172; un pezzo che finisce dove ci sono dei km no.
// Sabotaggio: il capo finale non guardato (i km diventano "noti", con una somma sbagliata).
TEST(Schede, PezzoConIlCapoFinaleSenzaKm) {
    PacchettoInMemoria m = pacchetto_con_schede();
    const Lettura l0 = leggi_pacchetto(m);
    const Index i0(l0.pacchetto);
    const Trip* t = i0.trip("C-4000124-1");
    ASSERT_NE(t, nullptr);
    EXPECT_FALSE(t->passages[1].loaded_km || t->passages[1].empty_km);
    EXPECT_FALSE(part_km(*t, 0, 1, true).known);
    EXPECT_EQ(part_km(*t, 0, 1, true).loaded, 0.0);
    EXPECT_TRUE(part_km(*t, 0, 2, true).known);
    const auto avvisi = [&](const std::string& a) {
        PacchettoInMemoria q = m;
        JsonDati turni = JsonDati::parse(q.file.at("turni.json"));
        turni.at("turni").at(0).at("elementi").push_back({{"corsa", "C-4000124-1"}, {"a", {{"luogo", a}}}});
        q.file["turni.json"] = turni.dump(2);
        const Lettura l = leggi_pacchetto(q);
        EXPECT_TRUE(l.forma_valida());
        std::vector<Problem> v, out;
        check_cards(l.pacchetto, Index(l.pacchetto), v);
        for (const Problem& p : v)
            if (p.check == Check::CardTripPartKmUnknown) out.push_back(p);
        return out;
    };
    ASSERT_EQ(avvisi("F-CASTELROVO-ALTO").size(), 1u);
    EXPECT_TRUE(avvisi("L-MEDIE").empty());
}

// S17: le giornate prima della presa in carico che cambiano sono solo quelle in cui una delle versioni o quella nuova ha servizio
// (qui dal lunedi' 2/11 al giovedi' 19/11: 13 giorni di scuola dal lunedi' al venerdi', patrono dell'11/11 escluso, non 18).
// Sabotaggio: contarle tutte.
TEST(Schede, GiornatePrimaDellaPresaInCaricoSoloConServizio) {
    const Lettura l0 = lettura_di_prova();
    const Letto v1 = leggi(l0, una(1), attivazione());
    Fogli mod = una(1);
    mod.schede[1][21] = "Modifica";
    mod.fermate[1][1] = "07:25";
    DateChoices s;
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 11, 2)};
    const Letto v2 = leggi(v1.dopo, mod, s, day_from_ymd(2026, 11, 20));
    ASSERT_EQ(v2.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(v2.m.outcomes.at(0).days_before_taken, 13);
}

// Punto 3 (K-022): i testi con un numero al singolare con 1. K-CTR-168: "cambia 1 giornata" (non "cambiano 1 giornate") e "si toglie la
// versione 2, che cominciava"; con piu' giornate il testo resta quello di prima. Sabotaggio: la forma del numero ignorata.
TEST(Schede, TestiAlSingolareConUno) {
    const Lettura l0 = lettura_di_prova();
    const Letto v1 = leggi(l0, una(1), attivazione());
    Fogli mod = una(1);
    mod.schede[1][21] = "Modifica";
    mod.fermate[1][1] = "07:25";
    DateChoices s;
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 11, 19)};
    const Letto v2 = leggi(v1.dopo, mod, s, day_from_ymd(2026, 11, 20));
    std::vector<std::string> m = messaggi(v2.dopo, v2.m.problems);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0], "K-CTR-168 v0: Scheda 4000123/3: la versione nuova vale dal 2026-11-19, prima della presa in carico (2026-11-20): cambia 1 "
                    "giornata prima della presa in carico (scelta dell'utente, scritta nel registro).");
    mod.fermate[1][1] = "07:15";
    s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 11, 18)};
    const Letto v3 = leggi(v2.dopo, mod, s, day_from_ymd(2026, 11, 20));
    m = messaggi(v3.dopo, v3.m.problems);
    ASSERT_EQ(m.size(), 1u);
    EXPECT_EQ(m[0], "K-CTR-168 v1: Scheda 4000123/3: la versione nuova vale dal 2026-11-18, prima della presa in carico (2026-11-20): cambiano "
                    "2 giornate prima della presa in carico e si toglie la versione 2, che cominciava da quel giorno in poi (scelta "
                    "dell'utente, scritta nel registro).");
    // K-CTR-163 con 1 giorno: una scheda di un giorno solo (lunedi' 21/09/2026) con Tot Giorni 2.
    Fogli f = una(1);
    f.schede[1][4] = "21/09/2026";
    f.schede[1][5] = "21/09/2026";
    f.schede[1][16] = "#2";
    m = messaggi(leggi(l0, f, attivazione()).dopo, leggi(l0, f, attivazione()).m.problems);
    ASSERT_FALSE(m.empty());
    EXPECT_NE(m[0].find("nel periodo della scheda da' 1 giorno: "), std::string::npos) << m[0];
}

// --- K-023 ------------------------------------------------------------------------------------------------------------------------

// I formati alla versione 2 (K-023): il profilo con il foglio delle interruzioni (nome e titoli, insieme) e il registro con le interruzioni
// nella copia della scheda. La versione 1 si legge ancora: un profilo alla 1 non ha il foglio, un registro alla 1 nessuna interruzione.
// Il foglio senza i suoi titoli (o i titoli senza il foglio) e le interruzioni in un file alla versione 1 non rispettano lo schema.
TEST(Schede, ProfiliERegistroAllaVersioneDue) {
    const Lettura v1 = lettura_di_prova();
    EXPECT_TRUE(profilo(v1).sheet_breaks.empty());
    const Lettura v2 = leggi_pacchetto(pacchetto_k023());
    ASSERT_TRUE(v2.forma_valida());
    EXPECT_EQ(v2.versioni.at("profili_schede.json"), 2);
    EXPECT_EQ(profilo(v2).sheet_breaks, "Interruzioni");
    EXPECT_EQ(profilo(v2).break_cols.nr, "Nr");
    EXPECT_EQ(profilo(v2).break_cols.from, "Dal");
    EXPECT_EQ(profilo(v2).break_cols.to, "Al");
    const CardProfile* h = Index(v2.pacchetto).card_profile("PS-CVA-H");
    ASSERT_NE(h, nullptr);
    ASSERT_TRUE(h->kind.has_value());
    EXPECT_EQ(h->kind->kind, ServiceKind::Disability);
    // Il foglio e i suoi titoli: tutti e due o nessuno.
    for (const char* togli : {"fogli", "colonne"}) {
        PacchettoInMemoria m = pacchetto_k023();
        JsonDati d = JsonDati::parse(m.file.at("profili_schede.json"));
        d.at("profili").at(0).at(togli).erase("interruzioni");
        m.file["profili_schede.json"] = d.dump(2);
        EXPECT_FALSE(leggi_pacchetto(m).forma_valida()) << togli;
        d.at("profili").at(0).at(togli == std::string("fogli") ? "colonne" : "fogli").erase("interruzioni");
        m.file["profili_schede.json"] = d.dump(2);
        const Lettura senza = leggi_pacchetto(m);
        EXPECT_TRUE(senza.forma_valida()) << togli;   // alla 2 senza il foglio: come alla 1
        EXPECT_TRUE(profilo(senza).sheet_breaks.empty());
        d["schema_versione"] = 1;
        d.at("profili").at(0)["fogli"]["interruzioni"] = "Interruzioni";
        d.at("profili").at(0)["colonne"]["interruzioni"] = {{"nr", "Nr"}, {"dal", "Dal"}, {"al", "Al"}};
        m.file["profili_schede.json"] = d.dump(2);
        EXPECT_FALSE(leggi_pacchetto(m).forma_valida()) << "il foglio in un profilo alla versione 1";
    }
    // Il registro: le interruzioni della copia della scheda si leggono alla versione 2, non alla 1.
    PacchettoInMemoria m = pacchetto_con_schede();
    JsonDati r = JsonDati::parse(m.file.at("schede_servizi.json"));
    r["schema_versione"] = 2;
    for (JsonDati& s : r.at("schede"))   // K-024: il registro si scrive alla 3; il tipo scelto alla 2 non c'e'
        for (JsonDati& v : s.at("versioni")) v.at("scheda").erase("tipo");
    r.at("schede").at(0).at("versioni").at(0).at("scheda")["interruzioni"] =
        JsonDati::array({{{"dal", "2026-12-23"}, {"al", "2027-01-06"}}, {{"dal", "2027-02-15"}, {"al", "2027-02-15"}}});
    m.file["schede_servizi.json"] = r.dump(2);
    const Lettura l = leggi_pacchetto(m);
    ASSERT_TRUE(l.forma_valida());
    const std::vector<DateRange>& b = l.pacchetto.service_cards.at(0).versions.at(0).card.breaks;
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0].from, day_from_ymd(2026, 12, 23));
    EXPECT_EQ(b[0].to, day_from_ymd(2027, 1, 6));
    EXPECT_EQ(b[1].from, b[1].to);
    EXPECT_TRUE(l.pacchetto.service_cards.at(1).versions.at(0).card.breaks.empty());
    r["schema_versione"] = 1;
    m.file["schede_servizi.json"] = r.dump(2);
    EXPECT_FALSE(leggi_pacchetto(m).forma_valida());
}

namespace {

Lettura lettura_k023() {
    Lettura l = leggi_pacchetto(pacchetto_k023());
    EXPECT_TRUE(l.forma_valida());
    return l;
}
// La scheda 4000123/3 (andata, scuola primaria, da lunedi' a venerdi', calendario SCOLASTICO, dal 14/09/2026 al 12/06/2027: 175 giorni)
// con le interruzioni date (righe Nr, Dal, Al) e il Tot Giorni dato.
Fogli con_interruzioni(const Righe& righe, const std::string& tot_giorni) {
    Fogli f = una(1);
    f.schede[1][16] = tot_giorni;
    f.interruzioni = {{"Nr", "Dal", "Al"}};
    for (const auto& r : righe) f.interruzioni.push_back(r);
    return f;
}
// Tre interruzioni, scritte non in ordine: la prima comprende le vacanze di Natale del calendario SCOLASTICO (23/12 - 06/01) e due giorni
// prima e dopo (21-22/12, 07-08/01: 4 giorni di servizio in meno); la seconda una settimana di febbraio (5 giorni); la terza un lunedi'
// (1 giorno). 175 - 10 = 165.
const Righe kTreInterruzioni = {{"4000123 / 3", "15/02/2027", "19/02/2027"},
                                {"4000123 / 3", "21/12/2026", "08/01/2027"},
                                {"4000123/3", "#46510", "#46510"}};   // 2027-05-03 come numero di data di Excel
bool c_e(const std::vector<Problem>& v, Check c) {
    return std::any_of(v.begin(), v.end(), [c](const Problem& p) { return p.check == c; });
}
int errori_di(const Package& p) {
    const std::vector<Problem> v = check_package(p);
    return static_cast<int>(std::count_if(v.begin(), v.end(), [](const Problem& x) {
        return definizione_controllo(x.check).livello == Livello::Errore;
    }));
}

}  // namespace

// Punto 1 (K-023): le interruzioni diventano sospensioni del calendario della versione e contano nel Tot Giorni come insieme di giorni (le
// vacanze di Natale gia' tolte dal calendario SCOLASTICO non si tolgono due volte): Tot Giorni 165, nessun K-CTR-163; con 175 l'avviso dice
// 165. Nel registro le interruzioni in ordine; il calendario non vale nei giorni interrotti e vale il giorno dopo; kzero controlla senza
// errori e senza K-CTR-170. Un profilo alla versione 1 non legge il foglio. Sabotaggi: interruzioni ignorate; contate due volte.
TEST(Schede, InterruzioniNelCalendarioEInTotGiorni) {
    const Lettura l = lettura_k023();
    const Letto x = leggi(l, con_interruzioni(kTreInterruzioni, "#165"), attivazione());
    ASSERT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New) << (x.m.outcomes.at(0).fault ? static_cast<int>(*x.m.outcomes.at(0).fault) : -1);
    EXPECT_EQ(x.r.break_rows, 3);
    EXPECT_FALSE(c_e(x.m.problems, Check::CardTotalDaysDiffer)) << ::testing::PrintToString(messaggi(x.dopo, x.m.problems));
    const ServiceCard sc = nel_registro(x.dopo, "SCH-4000123-3");
    const Card& k = sc.versions.at(0).card;
    ASSERT_EQ(k.breaks.size(), 3u);
    EXPECT_EQ(text_from_day(k.breaks[0].from) + " " + text_from_day(k.breaks[0].to), "2026-12-21 2027-01-08");
    EXPECT_EQ(text_from_day(k.breaks[1].from) + " " + text_from_day(k.breaks[1].to), "2027-02-15 2027-02-19");
    EXPECT_EQ(text_from_day(k.breaks[2].from) + " " + text_from_day(k.breaks[2].to), "2027-05-03 2027-05-03");
    EXPECT_EQ(card_days(k, Index(x.dopo.pacchetto)), 165);
    const Index idx(x.dopo.pacchetto);
    const Calendar* c = idx.calendar("CAL-4000123-3");
    ASSERT_NE(c, nullptr);
    ASSERT_EQ(c->suspensions.size(), 3u);
    EXPECT_EQ(c->suspensions[0].reason, "Interruzione della scheda");
    for (const auto& [g, vale] : std::vector<std::pair<Day, bool>>{{day_from_ymd(2026, 12, 18), true}, {day_from_ymd(2026, 12, 21), false},
                                                                    {day_from_ymd(2027, 1, 8), false},  {day_from_ymd(2027, 1, 11), true},
                                                                    {day_from_ymd(2027, 2, 17), false}, {day_from_ymd(2027, 2, 22), true},
                                                                    {day_from_ymd(2027, 5, 3), false},  {day_from_ymd(2027, 5, 4), true}})
        EXPECT_EQ(calendar_active(*c, g, idx), vale) << text_from_day(g);
    EXPECT_EQ(errori_di(x.dopo.pacchetto), 0) << ::testing::PrintToString(messaggi(x.dopo, check_package(x.dopo.pacchetto)));
    EXPECT_TRUE(schede::controlla_elementi(x.dopo).empty());
    // Con il Tot Giorni senza interruzioni (175): l'avviso dice 165.
    const Letto y = leggi(l, con_interruzioni(kTreInterruzioni, "#175"), attivazione());
    ASSERT_EQ(y.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    ASSERT_TRUE(c_e(y.m.problems, Check::CardTotalDaysDiffer));
    EXPECT_NE(messaggi(y.dopo, y.m.problems)[0].find("175"), std::string::npos);
    EXPECT_NE(messaggi(y.dopo, y.m.problems)[0].find("165"), std::string::npos) << messaggi(y.dopo, y.m.problems)[0];
    EXPECT_NE(messaggi(y.dopo, y.m.problems)[0].find("le interruzioni"), std::string::npos) << messaggi(y.dopo, y.m.problems)[0];
    EXPECT_EQ(y.m.problems[0].variant, 1);   // la variante delle schede con interruzioni (quella di oggi, 0, non cambia)
    // Un profilo alla versione 1 (k021) non legge il quarto foglio: la scheda come prima, senza interruzioni.
    const Letto z = leggi(lettura_di_prova(), con_interruzioni(kTreInterruzioni, "#175"), attivazione());
    EXPECT_EQ(z.r.break_rows, 0);
    EXPECT_TRUE(nel_registro(z.dopo, "SCH-4000123-3").versions.at(0).card.breaks.empty());
    EXPECT_FALSE(c_e(z.m.problems, Check::CardTotalDaysDiffer));
}

// Punto 1 (K-023): le interruzioni che non tornano fermano la scheda, nel foglio delle interruzioni (le altre schede vanno avanti): data
// scritta male (7) o vuota (1), Dal dopo Al (43), fuori dal periodo della scheda (44, prima dell'attivazione o dopo la disattivazione),
// sovrapposte (45, anche due righe uguali), con dentro una data elencata (46). Una riga con un Nr che non c'e': K-CTR-161. Sabotaggi: Dal
// dopo Al accettato; interruzione fuori dalla validita' senza segnalazione; data elencata dentro un'interruzione accettata.
TEST(Schede, InterruzioniCheFermanoLaScheda) {
    const Lettura l = lettura_k023();
    const auto motivo = [&](const Righe& righe, Fogli f) {
        f.interruzioni = {{"Nr", "Dal", "Al"}};
        for (const auto& r : righe) f.interruzioni.push_back(r);
        f.schede.push_back(fogli_di_prova().schede[2]);   // un'altra scheda del file (due scuole), senza interruzioni: va avanti
        for (std::size_t i = 1; i < fogli_di_prova().fermate.size(); ++i)
            if (fogli_di_prova().fermate[i][0] == "4000124 / 1") f.fermate.push_back(fogli_di_prova().fermate[i]);
        const Letto x = leggi(l, f, attivazione());
        EXPECT_EQ(x.m.outcomes.size(), 2u);
        if (x.m.outcomes.size() == 2) {
            EXPECT_NE(x.m.outcomes[1].kind, CardOutcome::Kind::Stopped) << "l'altra scheda";
        }
        const CardRead& r = x.r.cards.at(0);
        if (r.fault) {
            EXPECT_EQ(r.fault_sheet, "Interruzioni");
        }
        return std::make_pair(r.fault ? static_cast<int>(*r.fault) : -1, r.fault_row);
    };
    const Fogli f = una(1);
    EXPECT_EQ(motivo({{"4000123 / 3", "31/02/2027", "03/03/2027"}}, f), std::make_pair(7, 2));
    EXPECT_EQ(motivo({{"4000123 / 3", "15/02/2027", ""}}, f), std::make_pair(1, 2));
    EXPECT_EQ(motivo({{"4000123 / 3", "19/02/2027", "15/02/2027"}}, f), std::make_pair(43, 2));
    EXPECT_EQ(motivo({{"4000123 / 3", "10/09/2026", "15/09/2026"}}, f), std::make_pair(44, 2));
    EXPECT_EQ(motivo({{"4000123 / 3", "10/06/2027", "14/06/2027"}}, f), std::make_pair(44, 2));
    EXPECT_EQ(motivo({{"4000123 / 3", "15/02/2027", "19/02/2027"}, {"4000123 / 3", "21/12/2026", "08/01/2027"},
                      {"4000123 / 3", "05/01/2027", "10/01/2027"}},
                     f),
              std::make_pair(45, 4));
    EXPECT_EQ(motivo({{"4000123 / 3", "15/02/2027", "19/02/2027"}, {"4000123 / 3", "15/02/2027", "19/02/2027"}}, f), std::make_pair(45, 3));
    EXPECT_EQ(motivo({{"4000123 / 3", "15/02/2027", "19/02/2027"}, {"4000123 / 3", "19/02/2027", "22/02/2027"}}, f), std::make_pair(45, 3));
    EXPECT_EQ(motivo({{"4000123 / 3", "15/02/2027", "19/02/2027"}, {"4000123 / 3", "20/02/2027", "22/02/2027"}}, f), std::make_pair(-1, 0));
    // Una data elencata dentro un'interruzione (cadenza a date elencate).
    Fogli d = f;
    d.schede[1][20] = "Ogni 15 giorni";
    d.schede[1][16] = "#3";
    d.date = {{"Nr", "Data"}, {"4000123 / 3", "21/09/2026"}, {"4000123 / 3", "05/10/2026"}, {"4000123 / 3", "19/10/2026"}};
    EXPECT_EQ(motivo({{"4000123 / 3", "02/11/2026", "06/11/2026"}}, d), std::make_pair(-1, 0));
    EXPECT_EQ(motivo({{"4000123 / 3", "02/11/2026", "06/11/2026"}, {"4000123 / 3", "05/10/2026", "05/10/2026"}}, d), std::make_pair(46, 3));
    // Una riga con un Nr che non c'e': K-CTR-161, nel foglio delle interruzioni.
    Fogli n = con_interruzioni({{"4000999 / 1", "15/02/2027", "19/02/2027"}}, "#175");
    const Letto x = leggi(l, n, attivazione());
    EXPECT_EQ(x.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    ASSERT_TRUE(c_e(x.r.problems, Check::CardRowWithoutCard));
    EXPECT_NE(messaggi(x.dopo, x.r.problems)[0].find("Foglio \"Interruzioni\", riga 2"), std::string::npos) << messaggi(x.dopo, x.r.problems)[0];
}

// Punto 1 (K-023): un'interruzione a cavallo di due versioni (Natale, 21/12 - 08/01; la versione 2 vale dal 2027-01-01) si taglia al
// periodo di ogni versione: la 1, accorciata al 2026-12-31, ha la sospensione 21/12 - 31/12; la 2 dal 01/01 all'08/01. Una versione che
// comincia dopo l'interruzione (dal 2027-01-11) non la ha nel suo calendario (resta nella copia della scheda). kzero controlla senza errori
// (una sospensione fuori dal periodo sarebbe K-CTR-013). Tra due versioni "cosa cambia" dice le interruzioni; due schede uguali tranne
// le interruzioni non sono uguali. Sabotaggio: interruzione a cavallo di due versioni non tagliata.
TEST(Schede, InterruzioneACavalloDiDueVersioni) {
    const Lettura l = lettura_k023();
    const Righe natale = {{"4000123 / 3", "21/12/2026", "08/01/2027"}};
    const Letto v1 = leggi(l, con_interruzioni(natale, "#171"), attivazione());
    ASSERT_EQ(v1.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_FALSE(c_e(v1.m.problems, Check::CardTotalDaysDiffer));
    const auto sospensioni = [](const Lettura& d, const Code& cal) {
        std::string t;
        const Calendar* c = Index(d.pacchetto).calendar(cal);
        if (!c) return std::string("nessun calendario");
        for (const Suspension& s : c->suspensions) t += (t.empty() ? "" : ", ") + text_from_day(s.range.from) + " " + text_from_day(s.range.to);
        return t;
    };
    for (const auto& [dal, prima, dopo] : std::vector<std::tuple<Day, std::string, std::string>>{
             {day_from_ymd(2027, 1, 1), "2026-12-21 2026-12-31", "2027-01-01 2027-01-08"},
             {day_from_ymd(2027, 1, 11), "2026-12-21 2027-01-08", ""}}) {
        Fogli mod = con_interruzioni(natale, "#171");
        mod.schede[1][21] = "Modifica";
        mod.fermate[1][1] = "07:25";
        DateChoices s;
        s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, dal};
        const Letto v2 = leggi(v1.dopo, mod, s);
        ASSERT_EQ(v2.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
        EXPECT_EQ(sospensioni(v2.dopo, "CAL-4000123-3"), prima) << text_from_day(dal);
        EXPECT_EQ(sospensioni(v2.dopo, "CAL-4000123-3-V2"), dopo) << text_from_day(dal);
        EXPECT_EQ(errori_di(v2.dopo.pacchetto), 0) << ::testing::PrintToString(messaggi(v2.dopo, check_package(v2.dopo.pacchetto)));
        EXPECT_TRUE(schede::controlla_elementi(v2.dopo).empty());
        EXPECT_EQ(nel_registro(v2.dopo, "SCH-4000123-3").versions.at(1).card.breaks.size(), 1u);   // la copia resta intera
    }
    // Uguale tranne le interruzioni: versione nuova, e "cosa cambia" lo dice.
    Fogli altre = con_interruzioni({{"4000123 / 3", "21/12/2026", "08/01/2027"}, {"4000123 / 3", "15/02/2027", "19/02/2027"}}, "#166");
    altre.schede[1][21] = "Modifica";
    const Letto v3 = leggi(v1.dopo, altre, attivazione());
    ASSERT_EQ(v3.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
    bool interruzioni = false;
    for (const CardChange& c : v3.m.outcomes.at(0).changes)
        if (c.what == "interruzioni") {
            interruzioni = true;
            EXPECT_EQ(c.before, "2026-12-21 - 2027-01-08");
            EXPECT_EQ(c.after, "2026-12-21 - 2027-01-08, 2027-02-15 - 2027-02-19");
        }
    EXPECT_TRUE(interruzioni);
    Fogli uguale = con_interruzioni(natale, "#171");
    uguale.schede[1][21] = "Modifica";
    EXPECT_EQ(leggi(v1.dopo, uguale, attivazione()).m.outcomes.at(0).kind, CardOutcome::Kind::Same);
    // Le sospensioni del calendario cambiate a mano: K-CTR-170 (campo "sospensioni").
    PacchettoInMemoria m;
    for (const auto& [f, d] : v1.dopo.documenti) m.file[f] = chiusura::testo_file(d);
    JsonDati cal = JsonDati::parse(m.file.at("calendari.json"));
    for (JsonDati& c : cal.at("calendari"))
        if (c.at("codice") == "CAL-4000123-3") c.at("sospensioni").at(0)["al"] = "2027-01-07";
    m.file["calendari.json"] = cal.dump(2);
    const std::vector<Problem> p = schede::controlla_elementi(leggi_pacchetto(m));
    ASSERT_EQ(p.size(), 1u);
    EXPECT_NE(messaggi(v1.dopo, p)[0].find("sospensioni"), std::string::npos) << messaggi(v1.dopo, p)[0];
}

// Punto 1 (K-023, D6): una versione senza giorni non si scrive. La scheda 4000123/3 (da lunedi' a venerdi', fino al 12/06/2027, un sabato):
// scelta dal 2027-06-12 = la versione nuova non ha nessun giorno di servizio; dal 2027-06-07 con l'interruzione 07/06 - 12/06 lo stesso.
// Una versione 1 dal 2026-09-19 (sabato) e una versione 2 dal 2026-09-21 (lunedi'): la 1, accorciata al 20/09, resterebbe senza giorni.
// In ogni caso la scheda si ferma (variante 47), niente e' scritto e restano solo l'errore (gli avvisi della lettura di quella scheda
// no); con un giorno di servizio si scrive. Sabotaggio: versione senza giorni scritta.
TEST(Schede, VersioneSenzaGiorniNonSiScrive) {
    const Lettura l = lettura_k023();
    const Day presa = day_from_ymd(2026, 9, 1);
    const auto scelta = [](Day d) {
        DateChoices s;
        s.by_nr["4000123/3"] = DateChoice{DateChoice::Kind::Date, d};
        return s;
    };
    const Fogli f = con_interruzioni({}, "#175");
    const Letto a = leggi(l, f, scelta(day_from_ymd(2027, 6, 12)), presa);
    EXPECT_EQ(a.m.outcomes.at(0).kind, CardOutcome::Kind::Stopped);
    EXPECT_EQ(a.m.outcomes.at(0).fault, CardFault::VersionWithoutDays);
    EXPECT_TRUE(a.m.added.empty());
    ASSERT_EQ(a.m.problems.size(), 1u) << ::testing::PrintToString(messaggi(a.dopo, a.m.problems));
    EXPECT_EQ(a.m.problems[0].check, Check::CardUnreadable);
    EXPECT_EQ(a.m.problems[0].variant, 47);
    EXPECT_EQ(leggi(l, f, scelta(day_from_ymd(2027, 6, 11)), presa).m.outcomes.at(0).kind, CardOutcome::Kind::New);   // un venerdi'
    const Fogli g = con_interruzioni({{"4000123 / 3", "07/06/2027", "12/06/2027"}}, "#170");
    EXPECT_EQ(leggi(l, g, scelta(day_from_ymd(2027, 6, 7)), presa).m.outcomes.at(0).fault, CardFault::VersionWithoutDays);
    EXPECT_EQ(leggi(l, g, scelta(day_from_ymd(2027, 6, 4)), presa).m.outcomes.at(0).kind, CardOutcome::Kind::New);
    // La versione di prima, accorciata, resterebbe senza giorni.
    const Letto v1 = leggi(l, f, scelta(day_from_ymd(2026, 9, 19)), presa);
    ASSERT_EQ(v1.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    Fogli mod = f;
    mod.schede[1][21] = "Modifica";
    mod.fermate[1][1] = "07:25";
    const Letto v2 = leggi(v1.dopo, mod, scelta(day_from_ymd(2026, 9, 21)), presa);
    EXPECT_EQ(v2.m.outcomes.at(0).fault, CardFault::VersionWithoutDays);
    EXPECT_TRUE(v2.m.added.empty());
    EXPECT_TRUE(v2.m.cuts.empty());
    EXPECT_EQ(nel_registro(v2.dopo, "SCH-4000123-3").versions.size(), 1u);
    ASSERT_EQ(v2.m.problems.size(), 1u) << ::testing::PrintToString(messaggi(v2.dopo, v2.m.problems));
    EXPECT_NE(messaggi(v2.dopo, v2.m.problems)[0].find("resterebbe senza giorni"), std::string::npos);
    // Dal 22/09 la versione 1 tiene il lunedi' 21/09: si scrive, e kzero controlla non ha errori.
    const Letto v3 = leggi(v1.dopo, mod, scelta(day_from_ymd(2026, 9, 22)), presa);
    EXPECT_EQ(v3.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(errori_di(v3.dopo.pacchetto), 0) << ::testing::PrintToString(messaggi(v3.dopo, check_package(v3.dopo.pacchetto)));
}

namespace {
// K-023: le schede dei servizi per disabili (profilo PS-CVA-H) lette con la scelta "attivazione".
Letto leggi_h(const Lettura& l, const Fogli& f) { return leggi(l, f, attivazione(), kPresa, false, "PS-CVA-H"); }
// Una scheda sola per disabili (4000502/1 di tests/esempi/k023-schede: andata, al Centro diurno Il Faro), con il verso e momento, le
// scuole e le righe date (Orario, Punto di raccolta, Casa, St. Car., St. Scar., Km pieno, Km vuoto, Accompagnatore; il Nr lo mette la
// funzione). Totali alunni e km del piede si fanno dalle righe.
Fogli una_h(const std::string& servizio, const std::string& scuole, const Righe& righe) {
    const Fogli k = fogli_k023();
    Fogli f;
    f.schede = {k.schede[0], k.schede[2]};
    f.fermate = {k.fermate[0]};
    f.date = k.date;
    f.schede[1][2] = servizio;
    f.schede[1][3] = scuole;
    int su = 0;
    double pieni = 0, vuoti = 0;
    for (const auto& r : righe) {
        std::vector<std::string> x = {"4000502 / 1"};
        x.insert(x.end(), r.begin(), r.end());
        f.fermate.push_back(x);
        if (!r[3].empty()) su += std::stoi(r[3].substr(1));
        const auto numero = [](std::string t) {
            std::replace(t.begin(), t.end(), ',', '.');
            return std::stod(t);
        };
        if (!r[5].empty()) pieni += numero(r[5]);
        if (!r[6].empty()) vuoti += numero(r[6]);
    }
    const auto km = [](double v) {
        std::string t = std::to_string(std::llround(v * 10));
        if (t.size() < 2) t = "0" + t;
        return t.substr(0, t.size() - 1) + "," + t.substr(t.size() - 1);
    };
    f.schede[1][13] = "#" + std::to_string(su);
    f.schede[1][17] = km(pieni);
    f.schede[1][18] = km(vuoti);
    return f;
}
std::optional<int> motivo_h(const Lettura& l, const Fogli& f) {
    const Letto x = leggi_h(l, f);
    const CardRead& r = x.r.cards.at(0);
    return r.fault ? std::optional<int>(static_cast<int>(*r.fault)) : std::nullopt;
}
}  // namespace

// Punto 2 (K-023): le coincidenze. La scheda 4000502/1 ha nell'intestazione il Centro diurno Il Faro e finisce al punto di trasbordo del
// Ponte di Sorgenta (autorizzato con il documento), dove la persona scende; la 4000503/1 passa dal Ponte (la persona sale) e arriva al
// centro. Tutte e due si leggono: la prima con l'avviso K-CTR-176 (variante 0, scuola e luogo), il suo gruppo tiene la scuola (la meta) e
// la domanda e' quella delle righe; kzero controlla senza errori e senza K-CTR-170. La scuola e' quella che lega la casa al centro sensibile:
// senza il porta a porta tutto sensibile, la casa PAP-202 e' legata (la corsa non passa dal centro). Sabotaggi: avviso della coincidenza che
// manca; la scuola tolta dal gruppo.
TEST(Schede, CoincidenzaAlPuntoDiTrasbordo) {
    PacchettoInMemoria m = pacchetto_k023();
    JsonDati man = JsonDati::parse(m.file.at("pacchetto.json"));
    man["impostazioni"]["porta_a_porta_sensibile"] = false;   // il legame deve venire dalla scuola del gruppo, non dal porta a porta
    m.file["pacchetto.json"] = man.dump(2);
    const Lettura l = leggi_pacchetto(m);
    ASSERT_TRUE(l.forma_valida());
    const Letto x = leggi_h(l, fogli_k023());
    ASSERT_EQ(x.m.outcomes.size(), 3u);
    for (const CardOutcome& o : x.m.outcomes) EXPECT_EQ(o.kind, CardOutcome::Kind::New) << o.nr;
    std::vector<const Problem*> avvisi;
    for (const Problem& p : x.m.problems)
        if (p.check == Check::CardEndsAtTransfer) avvisi.push_back(&p);
    ASSERT_EQ(avvisi.size(), 1u) << ::testing::PrintToString(messaggi(x.dopo, x.m.problems));
    EXPECT_EQ(avvisi[0]->variant, 0);
    EXPECT_EQ(avvisi[0]->where.line, 3);   // la riga della scheda 4000502/1
    const std::vector<std::string> testi = messaggi(x.dopo, {*avvisi[0]});
    EXPECT_NE(testi[0].find("Scheda 4000502/1: la corsa finisce al punto di trasbordo L-TRASB-PONTE"), std::string::npos) << testi[0];
    EXPECT_NE(testi[0].find("scuola SC-CENTRO"), std::string::npos) << testi[0];
    const Index idx(x.dopo.pacchetto);
    const PassengerGroup* g = idx.group("G-4000502-1");
    ASSERT_NE(g, nullptr);
    ASSERT_TRUE(g->school.has_value());
    EXPECT_EQ(g->school->code, "SC-CENTRO");
    ASSERT_EQ(g->stops.size(), 2u);
    EXPECT_EQ(g->stops[1].place.code, "L-TRASB-PONTE");
    EXPECT_EQ(g->stops[1].alighting, 1);
    EXPECT_EQ(errori_di(x.dopo.pacchetto), 0) << ::testing::PrintToString(messaggi(x.dopo, check_package(x.dopo.pacchetto)));
    EXPECT_TRUE(schede::controlla_elementi(x.dopo).empty());
    EXPECT_TRUE(LegameSensibile(&idx).casa("PAP-202"));
    EXPECT_TRUE(LegameSensibile(&idx).casa("PAP-203"));
}

// Punto 2 (K-023): la regola a-b-c della coincidenza e i casi che fermano la scheda come oggi (variante 32): nessun punto di trasbordo;
// il punto di trasbordo in mezzo alla corsa ma non alla fine; nessuno che scende li' (o solo l'accompagnatore); due scuole
// nell'intestazione (D1); un'andata che comincia al punto di trasbordo. Il ritorno che comincia al punto di trasbordo si legge (variante
// 1); un punto autorizzato solo a voce da' le varianti 2 e 3. Il messaggio della variante 32 dice, al condizionale, come si scrive una
// coincidenza. Sabotaggi: scuola senza la sua riga accettata anche senza trasbordo; accettata con il trasbordo in mezzo ma non alla fine.
TEST(Schede, CoincidenzaSoloAllaFineOAllInizio) {
    const Lettura l = lettura_k023();
    const std::string centro = "Centro diurno Il Faro", ponte = "Ponte di Sorgenta, piazzale", croce = "Croce di Pradello, parcheggio";
    const std::string andata = "Andata Mattino", ritorno = "Ritorno Pomeriggio";
    const auto variante_176 = [&](const Fogli& f) {
        const Letto x = leggi_h(l, f);
        for (const Problem& p : x.m.problems)
            if (p.check == Check::CardEndsAtTransfer) return p.variant;
        return -1;
    };
    // Si legge: finisce al Ponte (0), comincia al Ponte al ritorno (1), finisce o comincia alla Croce, autorizzata solo a voce (2, 3).
    EXPECT_EQ(variante_176(una_h(andata, centro, {{"07:30", "", "PAP-202", "#1", "", "", "4,0", ""}, {"07:50", ponte, "", "", "#1", "7,2", "", ""}})), 0);
    EXPECT_EQ(variante_176(una_h(ritorno, centro, {{"16:00", ponte, "", "#1", "", "", "3,0", ""}, {"16:20", "", "PAP-202", "", "#1", "7,2", "", ""}})), 1);
    EXPECT_EQ(variante_176(una_h(andata, centro, {{"07:30", "", "PAP-202", "#1", "", "", "4,0", ""}, {"07:50", croce, "", "", "#1", "7,2", "", ""}})), 2);
    EXPECT_EQ(variante_176(una_h(ritorno, centro, {{"16:00", croce, "", "#1", "", "", "3,0", ""}, {"16:20", "", "PAP-202", "", "#1", "7,2", "", ""}})), 3);
    // Con l'accompagnatore, che scende al Ponte con la persona: chi prosegue e' uno (accompagnatore escluso), si legge.
    EXPECT_EQ(variante_176(una_h(andata, centro, {{"07:30", "", "PAP-202", "#2", "", "", "4,0", "SALE"}, {"07:50", ponte, "", "", "#2", "7,2", "", "SCENDE"}})), 0);
    const int scuola_senza_riga = static_cast<int>(CardFault::SchoolRowMissing);
    // Nessun punto di trasbordo alla fine (Pradello e' una fermata senza il blocco "trasbordo").
    EXPECT_EQ(motivo_h(l, una_h(andata, centro, {{"07:30", "", "PAP-202", "#1", "", "", "4,0", ""}, {"07:50", "PRADELLO PIAZZA", "", "", "#1", "7,2", "", ""}})),
              scuola_senza_riga);
    // Il punto di trasbordo in mezzo alla corsa, non alla fine.
    EXPECT_EQ(motivo_h(l, una_h(andata, centro, {{"07:30", "", "PAP-202", "#1", "", "", "4,0", ""}, {"07:35", "", "PAP-204", "#1", "", "", "1,0", ""},
                                                 {"07:40", ponte, "", "", "#1", "5,0", "", ""}, {"07:50", "PRADELLO PIAZZA", "", "", "#1", "2,2", "", ""}})),
              scuola_senza_riga);
    // Alla fine il Ponte, ma nessuno scende li' (o scende solo l'accompagnatore).
    EXPECT_EQ(motivo_h(l, una_h(andata, centro, {{"07:30", "", "PAP-202", "#1", "", "", "4,0", ""}, {"07:45", "PRADELLO PIAZZA", "", "", "#1", "5,0", "", ""},
                                                 {"07:50", ponte, "", "", "", "2,2", "", ""}})),
              scuola_senza_riga);
    EXPECT_EQ(motivo_h(l, una_h(andata, centro, {{"07:30", "", "PAP-202", "#2", "", "", "4,0", "SALE"}, {"07:45", "PRADELLO PIAZZA", "", "", "#1", "5,0", "", ""},
                                                 {"07:50", ponte, "", "", "#1", "2,2", "", "SCENDE"}})),
              scuola_senza_riga);
    // Due scuole nell'intestazione (D1): mai la coincidenza.
    EXPECT_EQ(motivo_h(l, una_h(andata, centro + "; Scuola primaria Valfonda",
                                {{"07:30", "", "PAP-202", "#2", "", "", "4,0", ""}, {"07:40", "Scuola primaria Valfonda", "", "", "#1", "5,0", "", ""},
                                 {"07:50", ponte, "", "", "#1", "2,2", "", ""}})),
              scuola_senza_riga);
    // Un'andata che comincia al Ponte (e finisce a casa), un ritorno che finisce al Ponte: il verso conta.
    EXPECT_EQ(motivo_h(l, una_h(andata, centro, {{"07:30", ponte, "", "#1", "", "", "4,0", ""}, {"07:50", "", "PAP-202", "", "#1", "7,2", "", ""}})),
              scuola_senza_riga);
    EXPECT_EQ(motivo_h(l, una_h(ritorno, centro, {{"16:00", "", "PAP-202", "#1", "", "", "3,0", ""}, {"16:20", ponte, "", "", "#1", "7,2", "", ""}})),
              scuola_senza_riga);
    // Il messaggio della variante 32 dice come si scrive una coincidenza.
    const Letto y = leggi_h(l, una_h(andata, centro, {{"07:30", "", "PAP-202", "#1", "", "", "4,0", ""}, {"07:50", "PRADELLO PIAZZA", "", "", "#1", "7,2", "", ""}}));
    ASSERT_FALSE(y.m.problems.empty());
    EXPECT_NE(messaggi(y.dopo, y.m.problems)[0].find("Se chi viaggia passasse su un altro mezzo"), std::string::npos) << messaggi(y.dopo, y.m.problems)[0];
}

// K-023 (fuzzing, file schede_servizi.json): due schede del registro, o due profili, con lo stesso codice danno K-CTR-009 con il punto
// in cui il codice compare la prima volta, come gli altri codici doppi. Prima il messaggio non si poteva scrivere (mancava "{prima}") e il
// programma si fermava con un errore interno. Il caso del fuzzing e' anche in tests/fuzz/casi/pacchetto-schede_servizi.json.
TEST(Schede, CodiciDoppiNelRegistroENeiProfili) {
    PacchettoInMemoria m = pacchetto_con_schede();
    JsonDati r = JsonDati::parse(m.file.at("schede_servizi.json"));
    r.at("schede").at(1)["codice"] = r.at("schede").at(0).at("codice");
    m.file["schede_servizi.json"] = r.dump(2);
    JsonDati p = JsonDati::parse(m.file.at("profili_schede.json"));
    p.at("profili").push_back(p.at("profili").at(0));
    m.file["profili_schede.json"] = p.dump(2);
    const Lettura l = leggi_pacchetto(m);
    ASSERT_TRUE(l.forma_valida());
    std::vector<Problem> v;
    check_cards(l.pacchetto, Index(l.pacchetto), v);
    std::vector<std::string> doppi;
    for (const std::string& t : messaggi(l, v))
        if (t.rfind("K-CTR-009", 0) == 0) doppi.push_back(t);
    ASSERT_EQ(doppi.size(), 2u) << ::testing::PrintToString(messaggi(l, v));
    EXPECT_NE(doppi[0].find("la prima volta in profili_schede.json /profili/0"), std::string::npos) << doppi[0];
    EXPECT_NE(doppi[1].find("la prima volta in schede_servizi.json /schede/0"), std::string::npos) << doppi[1];
    const std::vector<Problem> tutti = check_package(l.pacchetto);   // e kzero controlla scrive tutti i messaggi
    EXPECT_NO_THROW(messaggi(l, tutti));
}

// K-024, punto 0 (S7, sabotaggio dell'arbitro sul K-023 non preso): con la variante 47 (la scheda si ferma) gli avvisi della lettura di
// quella scheda si tolgono, come dice DECISIONI.md (K-023, "Una versione senza giorni non si scrive"); quelli delle altre schede del file
// restano. La scheda 4000502/1 (profilo PS-CVA-H) da sola, con il Tot Giorni sbagliato e "Modifica" per un ID mai visto, ha tre avvisi della
// lettura: K-CTR-163, K-CTR-176 (la coincidenza al Ponte di Sorgenta) e K-CTR-165 (variante 1). Con la data scelta dal 28/06/2027 e
// un'interruzione dal 28 al 30/06 (la disattivazione) non ha nessun giorno: si ferma (variante 47) e dei suoi avvisi non ne resta nessuno.
// La 4000501/1 dello stesso file, con il Tot Giorni sbagliato, tiene il suo K-CTR-163. Sabotaggi: avvisi della scheda ferma non tolti;
// tolti anche quelli delle altre schede.
TEST(Schede, Variante47TogliSoloGliAvvisiDiQuellaScheda) {
    const Lettura l = lettura_k023();
    const Fogli k = fogli_k023();
    Fogli f;
    f.schede = {k.schede[0], k.schede[1], k.schede[2]};
    f.schede[1][16] = "#1";            // 4000501/1: Tot Giorni sbagliato (K-CTR-163)
    f.schede[2][16] = "#1";            // 4000502/1: Tot Giorni sbagliato (K-CTR-163)
    f.schede[2][21] = "Modifica";      // 4000502/1: modifica di un ID mai visto (K-CTR-165, variante 1)
    f.fermate = {k.fermate[0], k.fermate[1], k.fermate[2], k.fermate[3], k.fermate[4]};
    f.date = k.date;
    f.interruzioni = k.interruzioni;
    f.interruzioni.push_back({"4000502 / 1", "28/06/2027", "30/06/2027"});
    const auto scelta = [](Day d) {
        DateChoices s = attivazione();
        s.by_nr["4000502/1"] = DateChoice{DateChoice::Kind::Date, d};
        return s;
    };
    const auto della = [](const Letto& x, const std::string& nr) {
        std::vector<std::string> out;
        for (const Problem& p : x.m.problems)
            for (const Param& q : p.params)
                if (q.name == "scheda" && q.text == nr) out.push_back(definizione_controllo(p.check).codice + " v" + std::to_string(p.variant));
        std::sort(out.begin(), out.end());
        return out;
    };
    // Prima: con un giorno di servizio (dal 25/06, un venerdi') la 4000502/1 si legge, con i suoi tre avvisi.
    const Letto con = leggi(l, f, scelta(day_from_ymd(2027, 6, 25)), kPresa, false, "PS-CVA-H");
    ASSERT_EQ(con.m.outcomes.at(1).kind, CardOutcome::Kind::New) << ::testing::PrintToString(messaggi(con.dopo, con.m.problems));
    EXPECT_EQ(della(con, "4000502/1"), (std::vector<std::string>{"K-CTR-163 v1", "K-CTR-165 v1", "K-CTR-176 v0"}))
        << ::testing::PrintToString(messaggi(con.dopo, con.m.problems));
    EXPECT_EQ(della(con, "4000501/1"), (std::vector<std::string>{"K-CTR-163 v1"}));
    // Dal 28/06: nessun giorno, la scheda si ferma e restano solo l'errore e l'avviso dell'altra scheda.
    const Letto senza = leggi(l, f, scelta(day_from_ymd(2027, 6, 28)), kPresa, false, "PS-CVA-H");
    EXPECT_EQ(senza.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    ASSERT_EQ(senza.m.outcomes.at(1).kind, CardOutcome::Kind::Stopped);
    EXPECT_EQ(senza.m.outcomes.at(1).fault, CardFault::VersionWithoutDays);
    EXPECT_EQ(della(senza, "4000502/1"), (std::vector<std::string>{"K-CTR-160 v47"})) << ::testing::PrintToString(messaggi(senza.dopo, senza.m.problems));
    EXPECT_EQ(della(senza, "4000501/1"), (std::vector<std::string>{"K-CTR-163 v1"}));
    EXPECT_EQ(senza.m.problems.size(), 2u) << ::testing::PrintToString(messaggi(senza.dopo, senza.m.problems));
}

// --- K-024: tipo di servizio per scheda -------------------------------------------------------------------------------------------

namespace {
Lettura lettura_k024() {
    Lettura l = leggi_pacchetto(pacchetto_k024());
    EXPECT_TRUE(l.forma_valida()) << ::testing::PrintToString(messaggi(l, l.problemi));
    return l;
}
// Il pacchetto separato come lo chiude kzero chiudi: i file normali (manifesto con la cassaforte) e i valori tolti.
struct Separato {
    PacchettoInMemoria normali;
    std::vector<Voce> voci;
};
Separato separato(const PacchettoInMemoria& m) {
    const Lettura l = leggi_pacchetto(m);
    chiusura::Separazione s = chiusura::separa(l);
    Separato x;
    for (auto& [file, doc] : s.normali)
        x.normali.file[file] = chiusura::testo_file(file == "pacchetto.json" ? chiusura::manifesto_chiuso(doc, std::string(32, 'a'), 1) : doc);
    x.normali.file["cassaforte.kzc"] = "finta";
    for (auto* v : {&s.contenuto.personali, &s.contenuto.particolari, &s.contenuto.legati})
        for (Voce& y : *v) x.voci.push_back(std::move(y));
    return x;
}
}  // namespace

// K-024 (D1 A): il profilo alla versione 3 ha le regole del tipo (codice, colonna, valori, tipo) e la colonna dei disabili, tutte e due
// facoltative; il tipo del profilo e quello di ogni regola sono dati particolari: chiudendo il pacchetto vanno nella cassaforte, e chi
// legge solo i file normali non li conosce (non noti, mai un altro tipo). Colonna e valori delle regole restano comuni. Il registro alla
// versione 3 ha nella copia della scheda il tipo scelto e il numero dei disabili, particolari anche loro. Le versioni 1 e 2 non hanno i
// campi nuovi. Sabotaggio: il tipo del profilo comune nella versione 3.
TEST(Schede, ProfiliERegistroAllaVersioneTre) {
    const Lettura l = lettura_k024();
    EXPECT_EQ(l.versioni.at("profili_schede.json"), 3);
    ASSERT_EQ(l.pacchetto.card_profiles.size(), 1u);
    const CardProfile p = profilo(l);
    EXPECT_EQ(p.card_cols.disabled, "Disabili");
    ASSERT_TRUE(p.kind.has_value());
    EXPECT_EQ(p.kind->kind, ServiceKind::School);
    EXPECT_EQ(p.kind->group, GroupType::Students);
    ASSERT_EQ(p.type_rules.size(), 1u);
    EXPECT_EQ(p.type_rules[0].code, "RT-1");
    EXPECT_EQ(p.type_rules[0].column, CardRuleColumn::Calendar);
    EXPECT_EQ(p.type_rules[0].values, std::vector<std::string>{"SPECIALE-A"});
    ASSERT_TRUE(p.type_rules[0].kind.has_value());
    EXPECT_EQ(p.type_rules[0].kind->kind, ServiceKind::Disability);
    EXPECT_EQ(p.type_rules[0].kind->group, GroupType::Disabled);
    // I profili di oggi (versioni 1 e 2): senza regole e senza la colonna, il tipo noto e comune.
    for (const Lettura& vecchia : {lettura_di_prova(), lettura_k023()}) {
        const CardProfile q = profilo(vecchia);
        EXPECT_TRUE(q.type_rules.empty());
        EXPECT_TRUE(q.card_cols.disabled.empty());
        ASSERT_TRUE(q.kind.has_value());
        EXPECT_EQ(q.kind->kind, ServiceKind::School);
    }
    // Le regole e la colonna non si leggono alla versione 2.
    for (const char* campo : {"regole_tipo", "disabili"}) {
        PacchettoInMemoria m = pacchetto_k024();
        JsonDati d = JsonDati::parse(m.file.at("profili_schede.json"));
        d["schema_versione"] = 2;
        if (std::string(campo) == "regole_tipo") d.at("profili").at(0).at("colonne").at("schede").erase("disabili");
        else d.at("profili").at(0).erase("regole_tipo");
        m.file["profili_schede.json"] = d.dump(2);
        EXPECT_FALSE(leggi_pacchetto(m).forma_valida()) << campo;
    }
    // Una colonna che una regola non puo' guardare (D3: solo servizio, scuole, calendario, cadenza).
    for (const char* colonna : {"nr", "totali_alunni", "attrezzato", "disabili"}) {
        PacchettoInMemoria m = pacchetto_k024();
        JsonDati d = JsonDati::parse(m.file.at("profili_schede.json"));
        d.at("profili").at(0).at("regole_tipo").at(0)["colonna"] = colonna;
        m.file["profili_schede.json"] = d.dump(2);
        EXPECT_FALSE(leggi_pacchetto(m).forma_valida()) << colonna;
    }
    // Chiuso: il tipo del profilo e quello della regola vanno nella cassaforte; colonna, valori e liberta' oraria restano comuni.
    const Separato s = separato(pacchetto_k024());
    const JsonDati normale = JsonDati::parse(s.normali.file.at("profili_schede.json"));
    const JsonDati& pn = normale.at("profili").at(0);
    for (const char* k : {"tipo", "tipo_gruppo"}) EXPECT_FALSE(pn.at("servizio").contains(k)) << k;
    EXPECT_TRUE(pn.at("servizio").contains("liberta_oraria"));
    EXPECT_FALSE(pn.at("regole_tipo").at(0).contains("servizio"));
    EXPECT_EQ(pn.at("regole_tipo").at(0).at("colonna"), "calendario");
    EXPECT_EQ(pn.at("regole_tipo").at(0).at("valori"), JsonDati::array({"SPECIALE-A"}));
    int nella_cassaforte = 0;
    for (const Voce& v : s.voci)
        if (v.file == "profili_schede.json") {
            ++nella_cassaforte;
            EXPECT_EQ(v.classe, Classe::Particolare) << v.campo;
        }
    EXPECT_EQ(nella_cassaforte, 3);   // tipo e tipo del gruppo del profilo, il tipo della regola
    const Lettura senza = leggi_pacchetto(s.normali);
    ASSERT_TRUE(senza.forma_valida()) << ::testing::PrintToString(messaggi(senza, senza.problemi));
    EXPECT_FALSE(profilo(senza).kind.has_value());
    EXPECT_FALSE(profilo(senza).type_rules.at(0).kind.has_value());
    EXPECT_EQ(profilo(senza).type_rules.at(0).values, std::vector<std::string>{"SPECIALE-A"});
    // Il registro alla versione 3: tipo scelto e numero dei disabili nella copia della scheda, particolari; alla 2 non si leggono.
    PacchettoInMemoria m = pacchetto_con_schede();
    JsonDati r = JsonDati::parse(m.file.at("schede_servizi.json"));
    EXPECT_EQ(r.at("schema_versione"), 3);
    JsonDati& copia = r.at("schede").at(0).at("versioni").at(0).at("scheda");
    copia["disabili"] = 1;
    copia["tipo"] = {{"tipo", "disabili"}, {"tipo_gruppo", "disabili"}, {"da", "regola"}, {"regola", "RT-1"}};
    m.file["schede_servizi.json"] = r.dump(2);
    const Lettura lr = leggi_pacchetto(m);
    ASSERT_TRUE(lr.forma_valida()) << ::testing::PrintToString(messaggi(lr, lr.problemi));
    const Card& k = lr.pacchetto.service_cards.at(0).versions.at(0).card;
    ASSERT_TRUE(k.disabled.has_value());
    EXPECT_EQ(*k.disabled, 1);
    ASSERT_TRUE(k.chosen.has_value());
    EXPECT_EQ(k.chosen->kind.kind, ServiceKind::Disability);
    EXPECT_TRUE(k.chosen->from_rule);
    EXPECT_EQ(k.chosen->rule, "RT-1");
    const Separato sr = separato(m);
    const JsonDati rn = JsonDati::parse(sr.normali.file.at("schede_servizi.json"));
    for (const char* campo : {"disabili", "tipo", "individualizzato", "attrezzato"})
        EXPECT_FALSE(rn.at("schede").at(0).at("versioni").at(0).at("scheda").contains(campo)) << campo;
    r["schema_versione"] = 2;
    m.file["schede_servizi.json"] = r.dump(2);
    EXPECT_FALSE(leggi_pacchetto(m).forma_valida());
}

namespace {
// K-024: il pacchetto del K-024 con il profilo cambiato da una funzione (sul documento JSON del profilo PS-CVA).
Lettura lettura_k024_con(const std::function<void(JsonDati&)>& cambia) {
    PacchettoInMemoria m = pacchetto_k024();
    JsonDati d = JsonDati::parse(m.file.at("profili_schede.json"));
    cambia(d.at("profili").at(0));
    m.file["profili_schede.json"] = d.dump(2);
    Lettura l = leggi_pacchetto(m);
    EXPECT_TRUE(l.forma_valida()) << ::testing::PrintToString(messaggi(l, l.problemi));
    return l;
}
JsonDati regola(const std::string& codice, const std::string& colonna, const std::vector<std::string>& valori, const std::string& tipo,
                const std::string& gruppo) {
    return {{"codice", codice}, {"colonna", colonna}, {"valori", valori}, {"servizio", {{"tipo", tipo}, {"tipo_gruppo", gruppo}}}};
}
// Le schede del K-024 (la colonna dei disabili e' l'ultima, 22) e le loro fermate: solo quelle date (righe del foglio delle schede).
Fogli schede_k024(const std::vector<std::size_t>& righe) {
    const Fogli k = fogli_k024();
    Fogli f;
    f.schede = {k.schede[0]};
    f.fermate = {k.fermate[0]};
    f.date = k.date;
    f.interruzioni = k.interruzioni;
    for (std::size_t i : righe) {
        f.schede.push_back(k.schede[i]);
        for (std::size_t j = 1; j < k.fermate.size(); ++j)
            if (k.fermate[j][0] == k.schede[i][0]) f.fermate.push_back(k.fermate[j]);
    }
    return f;
}
// I fogli di prima del K-024 con la colonna dei disabili in fondo (vuota), per leggerli con il profilo del K-024 che la nomina.
Fogli con_colonna_disabili(Fogli f) {
    f.schede[0].push_back("Disabili");
    for (std::size_t i = 1; i < f.schede.size(); ++i) f.schede[i].resize(23);
    return f;
}
const ServiceCard& in_registro(const Letto& x, const std::string& codice) {
    for (const ServiceCard& s : x.m.cards)
        if (s.code == codice) return s;
    throw std::logic_error("scheda che non c'e': " + codice);
}
std::vector<std::string> codici_di(const Letto& x, const std::string& nr) {
    std::vector<std::string> out;
    for (const Problem& p : x.m.problems)
        for (const Param& q : p.params)
            if (q.name == "scheda" && q.text == nr) out.push_back(definizione_controllo(p.check).codice + " v" + std::to_string(p.variant));
    std::sort(out.begin(), out.end());
    return out;
}
}  // namespace

// K-024, punto 1: il tipo per scheda. Le quattro schede di prova (profilo PS-CVA alla versione 3 con la regola RT-1: calendario SPECIALE-A
// = disabili): la scolastica e la promiscua prendono il tipo del profilo (scolastico, studenti), le due con SPECIALE-A quello della regola
// (disabili, disabili). Il tipo va nel servizio e nel gruppo, e nella copia della scheda con l'origine. Il numero dei disabili sta solo
// nella copia. K-CTR-177: variante 1 sulla promiscua (1 disabile su 8), variante 0 sulla scheda per disabili con 0. Sabotaggi: regole
// ignorate (tutte le schede del tipo del profilo); sempre la prima regola anche se il valore non c'e'; avviso del tipo disabili con 0 che
// manca.
TEST(Schede, TipoPerSchedaConLaRegola) {
    const Lettura l = lettura_k024();
    const Letto x = leggi(l, fogli_k024(), attivazione());
    for (const std::string& m : messaggi(x.dopo, x.m.problems)) std::cout << m << "\n";
    ASSERT_EQ(x.m.outcomes.size(), 4u);
    for (const CardOutcome& o : x.m.outcomes) EXPECT_EQ(o.kind, CardOutcome::Kind::New) << o.nr;
    const Index idx(x.dopo.pacchetto);
    const auto tipi = [&](const std::string& n) {
        const Service* s = idx.service("S-" + n);
        const PassengerGroup* g = idx.group("G-" + n);
        EXPECT_TRUE(s && g) << n;
        return std::make_pair(s ? s->kind.value() : ServiceKind::Other, g ? g->type.value() : GroupType::Other);
    };
    EXPECT_EQ(tipi("4000601-1"), std::make_pair(ServiceKind::School, GroupType::Students));
    EXPECT_EQ(tipi("4000602-1"), std::make_pair(ServiceKind::Disability, GroupType::Disabled));
    EXPECT_EQ(tipi("4000603-1"), std::make_pair(ServiceKind::School, GroupType::Students));
    EXPECT_EQ(tipi("4000604-1"), std::make_pair(ServiceKind::Disability, GroupType::Disabled));
    const auto copia = [&](const std::string& codice) { return in_registro(x, codice).versions.at(0).card; };
    ASSERT_TRUE(copia("SCH-4000601-1").chosen.has_value());
    EXPECT_FALSE(copia("SCH-4000601-1").chosen->from_rule);
    ASSERT_TRUE(copia("SCH-4000602-1").chosen.has_value());
    EXPECT_TRUE(copia("SCH-4000602-1").chosen->from_rule);
    EXPECT_EQ(copia("SCH-4000602-1").chosen->rule, "RT-1");
    EXPECT_EQ(copia("SCH-4000601-1").disabled, std::optional<int>(0));
    EXPECT_EQ(copia("SCH-4000602-1").disabled, std::optional<int>(1));
    EXPECT_EQ(codici_di(x, "4000601/1"), std::vector<std::string>{});
    EXPECT_EQ(codici_di(x, "4000602/1"), std::vector<std::string>{});
    EXPECT_EQ(codici_di(x, "4000603/1"), std::vector<std::string>{"K-CTR-177 v1"});
    EXPECT_EQ(codici_di(x, "4000604/1"), std::vector<std::string>{"K-CTR-177 v0"});
    EXPECT_EQ(x.m.problems.size(), 2u);
    const std::vector<std::string> testi = messaggi(x.dopo, x.m.problems);
    bool promiscua = false;
    for (const std::string& t : testi)
        promiscua = promiscua || t.find("a bordo ci sono 1 disabile su 8 persone, ma il servizio e' scolastico") != std::string::npos;
    EXPECT_TRUE(promiscua) << ::testing::PrintToString(testi);
    EXPECT_EQ(definizione_controllo(Check::CardDisabledMismatch).rivela_variante(0), Classe::Particolare);
    // Il pacchetto scritto: niente errori; il registro tiene il tipo e il numero.
    EXPECT_EQ(errori_di(x.dopo.pacchetto), 0) << ::testing::PrintToString(messaggi(x.dopo, check_package(x.dopo.pacchetto)));
    EXPECT_TRUE(schede::controlla_elementi(x.dopo).empty());
    // I valori si confrontano come le parole del profilo (maiuscole, punteggiatura), ma tutta la cella: "speciale a" scatta, un valore
    // che e' solo un pezzo della cella no.
    Fogli f = schede_k024({1});
    f.schede[1][19] = "speciale a";
    EXPECT_TRUE(leggi(l, f, attivazione()).r.cards.at(0).card.chosen->from_rule);
    const Lettura pezzo = lettura_k024_con([](JsonDati& p) { p.at("regole_tipo").at(0)["valori"] = JsonDati::array({"SPECIALE"}); });
    EXPECT_FALSE(leggi(pezzo, schede_k024({2}), attivazione()).r.cards.at(0).card.chosen->from_rule);
    // Una regola sulle scuole: scatta se una delle scuole della cella e' uno dei valori (la 4000124/1 del K-021 ha due scuole).
    const Lettura scuole = lettura_k024_con([](JsonDati& p) {
        p["regole_tipo"] = JsonDati::array({regola("RT-S", "scuole", {"Scuola primaria Valfonda"}, "altro", "studenti")});
        p.at("regole_tipo").at(0).at("servizio")["tipo_descrizione"] = "doposcuola di prova";
    });
    const Letto y = leggi(scuole, con_colonna_disabili(una(2)), attivazione());
    ASSERT_FALSE(y.r.cards.at(0).fault);
    ASSERT_TRUE(y.r.cards.at(0).card.chosen.has_value());
    EXPECT_EQ(y.r.cards.at(0).card.chosen->rule, "RT-S");
    EXPECT_EQ(y.r.cards.at(0).card.chosen->kind.kind, ServiceKind::Other);
    EXPECT_EQ(y.r.cards.at(0).card.chosen->kind.kind_description, "doposcuola di prova");
}

// K-024, D2: due regole con tipi diversi sulla stessa scheda la fermano (K-CTR-160 variante 48, con i codici delle regole, mai i tipi;
// rivela un dato particolare); due regole con lo stesso tipo no (origine: la prima nell'ordine del profilo). Nel profilo: la stessa parola
// in due regole della stessa colonna e' K-CTR-173 variante 6 (comune); il codice di una regola due volte e' K-CTR-009. Sabotaggio: due
// regole con tipi diversi accettate.
TEST(Schede, DueRegoleConTipiDiversiFermanoLaScheda) {
    const Lettura l = lettura_k024_con([](JsonDati& p) {
        p.at("regole_tipo").push_back(regola("RT-2", "servizio", {"Andata Mattino"}, "linea", "utenti_linea"));
        p.at("regole_tipo").push_back(regola("RT-3", "scuole", {"Centro diurno Il Faro"}, "disabili", "disabili"));
    });
    const Letto x = leggi(l, fogli_k024(), attivazione());
    ASSERT_EQ(x.m.outcomes.size(), 4u);
    EXPECT_EQ(x.m.outcomes[0].kind, CardOutcome::Kind::New);   // 4000601: Andata Mattino, solo RT-2
    EXPECT_EQ(x.r.cards[0].card.chosen->rule, "RT-2");
    EXPECT_EQ(x.r.cards[0].card.chosen->kind.kind, ServiceKind::Line);
    for (std::size_t i : {1u, 3u}) {   // 4000602 e 4000604: RT-1 e RT-3 (disabili) e RT-2 (linea)
        EXPECT_EQ(x.m.outcomes[i].kind, CardOutcome::Kind::Stopped) << i;
        EXPECT_EQ(x.m.outcomes[i].fault, CardFault::RulesDisagree) << i;
        EXPECT_EQ(x.r.cards[i].fault_rules, (std::vector<Code>{"RT-1", "RT-2", "RT-3"})) << i;
    }
    EXPECT_EQ(x.m.outcomes[2].kind, CardOutcome::Kind::New);   // 4000603: Ritorno Pomeriggio, nessuna regola
    EXPECT_FALSE(x.r.cards[2].card.chosen->from_rule);
    std::string testo;
    for (const std::string& m : messaggi(x.dopo, x.m.problems))
        if (m.find("v48") != std::string::npos) testo = m;
    EXPECT_NE(testo.find("le regole del tipo RT-1, RT-2, RT-3 del profilo scattano tutte"), std::string::npos) << testo;
    for (const char* tipo : {"disabili", "linea", "scolastico"}) EXPECT_EQ(testo.find(tipo), std::string::npos) << testo;
    EXPECT_EQ(definizione_controllo(Check::CardUnreadable).rivela_variante(48), Classe::Particolare);
    // Stesso tipo (RT-1 e RT-3 sulle schede per disabili): si legge, l'origine e' la prima regola.
    const Lettura stesso = lettura_k024_con([](JsonDati& p) {
        p.at("regole_tipo").push_back(regola("RT-3", "scuole", {"Centro diurno Il Faro"}, "disabili", "disabili"));
    });
    const Letto y = leggi(stesso, schede_k024({2}), attivazione());
    ASSERT_EQ(y.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    EXPECT_EQ(y.r.cards.at(0).card.chosen->rule, "RT-1");
    // Il profilo: la stessa parola in due regole della stessa colonna; lo stesso codice due volte.
    const Lettura doppia = lettura_k024_con([](JsonDati& p) {
        p.at("regole_tipo").push_back(regola("RT-4", "calendario", {"speciale a"}, "disabili", "disabili"));
        p.at("regole_tipo").push_back(regola("RT-1", "cadenza", {"Ogni 15 giorni"}, "disabili", "disabili"));
    });
    std::vector<Problem> v;
    check_cards(doppia.pacchetto, Index(doppia.pacchetto), v);
    std::vector<std::string> attesi;
    for (const std::string& m : messaggi(doppia, v))
        if (m.rfind("K-CTR-173", 0) == 0 || m.rfind("K-CTR-009", 0) == 0) attesi.push_back(m);
    ASSERT_EQ(attesi.size(), 2u) << ::testing::PrintToString(messaggi(doppia, v));
    std::sort(attesi.begin(), attesi.end());
    EXPECT_NE(attesi[0].find("K-CTR-009 v0: Il codice RT-1 compare due volte tra le regole del tipo"), std::string::npos) << attesi[0];
    EXPECT_NE(attesi[1].find("K-CTR-173 v6"), std::string::npos) << attesi[1];
    EXPECT_NE(attesi[1].find("\"speciale a\""), std::string::npos) << attesi[1];
    EXPECT_EQ(definizione_controllo(Check::CardProfileInvalid).rivela_variante(6), Classe::Comune);
}

// K-024, punto 2 (D5, D7): il numero dei disabili. Cella vuota = non scritto (nessun controllo, mai 0); scritto male (decimale, negativo,
// testo, cella con un errore di Excel) = la scheda si ferma (variante 49); piu' disabili delle persone a bordo, accompagnatore escluso
// (Totali alunni lo comprende) = si ferma (variante 50); tipo disabili con a bordo anche persone che non sono disabili = K-CTR-177
// variante 2. Le varianti 49 e 50 rivelano un dato particolare. Sabotaggio: numero piu' grande dei Totali accettato.
TEST(Schede, NumeroDeiDisabili) {
    const Lettura l = lettura_k024();
    const auto con = [&](std::size_t riga, const std::string& disabili) {
        Fogli f = schede_k024({riga});
        f.schede[1][22] = disabili;
        return leggi(l, f, attivazione());
    };
    const auto motivo = [](const Letto& x) { return x.r.cards.at(0).fault ? static_cast<int>(*x.r.cards.at(0).fault) : -1; };
    for (const std::string& male : {std::string("1,5"), std::string("-1"), std::string("uno"), std::string("!")})
        EXPECT_EQ(motivo(con(1, male)), 49) << male;
    const Letto vuota = con(1, "");
    EXPECT_EQ(motivo(vuota), -1);
    EXPECT_FALSE(vuota.r.cards.at(0).card.disabled.has_value());
    EXPECT_TRUE(vuota.m.problems.empty());
    EXPECT_EQ(motivo(con(1, "#9")), -1);    // 9 su 9
    EXPECT_EQ(motivo(con(1, "#10")), 50);   // 10 su 9
    EXPECT_EQ(motivo(con(2, "#2")), 50);    // 2 su 1
    // Con l'accompagnatore: la scheda dell'infanzia ha Totali alunni 6, accompagnatore compreso: 5 persone a bordo.
    const Lettura k = lettura_k024_con([](JsonDati&) {});
    Fogli inf = con_colonna_disabili(infanzia());
    inf.schede[1][22] = "#6";
    EXPECT_EQ(motivo(leggi(k, inf, attivazione())), 50);
    inf.schede[1][22] = "#5";
    EXPECT_EQ(motivo(leggi(k, inf, attivazione())), -1);
    // Tipo disabili (RT-1) con 3 disabili su 9: a bordo anche 6 persone che non sono disabili.
    Fogli g = schede_k024({1});
    g.schede[1][19] = "SPECIALE-A";
    g.schede[1][22] = "#3";
    const Letto due = leggi(l, g, attivazione());
    std::vector<std::string> c177;
    for (const std::string& m : messaggi(due.dopo, due.m.problems))
        if (m.rfind("K-CTR-177", 0) == 0) c177.push_back(m);
    ASSERT_EQ(c177.size(), 1u) << ::testing::PrintToString(messaggi(due.dopo, due.m.problems));
    EXPECT_NE(c177[0].find("K-CTR-177 v2"), std::string::npos) << c177[0];
    EXPECT_NE(c177[0].find("anche 6 persone che non sono disabili (3 disabili su 9 persone"), std::string::npos) << c177[0];
    for (int v : {49, 50}) EXPECT_EQ(definizione_controllo(Check::CardUnreadable).rivela_variante(v), Classe::Particolare) << v;
    // Un profilo senza la colonna non la legge (anche se il file la ha): numero non scritto.
    const Lettura senza = lettura_k024_con([](JsonDati& p) { p.at("colonne").at("schede").erase("disabili"); });
    EXPECT_FALSE(leggi(senza, schede_k024({1}), attivazione()).r.cards.at(0).card.disabled.has_value());
}

// K-024, D4 e D6: il tipo fa parte della versione; il numero dei disabili conta solo tra due numeri scritti. Letto senza la regola (tutte
// scolastiche), lo stesso file con la regola: le due schede per disabili diventano versioni nuove (dalla data scelta), con il tipo tra le
// cose che cambiano (dato particolare); le altre restano uguali. Una versione di prima del K-024 (senza il tipo nel registro) ha il tipo
// del suo servizio: con lo stesso profilo resta uguale, con la regola cambia; senza il servizio il tipo non si confronta. Numero: 1 -> 2
// e' una versione nuova; un numero che prima non c'era (colonna aggiunta al profilo) no.
TEST(Schede, TipoEDisabiliNelleVersioni) {
    const Lettura senza_regola = lettura_k024_con([](JsonDati& p) { p.erase("regole_tipo"); });
    const Letto prima = leggi(senza_regola, fogli_k024(), attivazione());
    for (const CardOutcome& o : prima.m.outcomes) ASSERT_EQ(o.kind, CardOutcome::Kind::New) << o.nr;
    DateChoices dal23;
    dal23.file = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 11, 23)};
    // Con la regola (il registro e il pacchetto di prima, il profilo con la regola).
    const auto con_regola = [](const Lettura& base) {
        PacchettoInMemoria m;
        for (const auto& [f, d] : base.documenti) m.file[f] = chiusura::testo_file(d);
        m.file["profili_schede.json"] = kzero::prove::elenco_a_mano::leggi_testo(kCartella24 + "/profili_schede.json");
        return leggi_pacchetto(m);
    };
    const Letto dopo = leggi(con_regola(prima.dopo), fogli_k024(), dal23);
    ASSERT_EQ(dopo.m.outcomes.size(), 4u);
    EXPECT_EQ(dopo.m.outcomes[0].kind, CardOutcome::Kind::Same);
    EXPECT_EQ(dopo.m.outcomes[2].kind, CardOutcome::Kind::Same);
    for (std::size_t i : {1u, 3u}) {
        ASSERT_EQ(dopo.m.outcomes[i].kind, CardOutcome::Kind::NewVersion) << i;
        ASSERT_EQ(dopo.m.outcomes[i].changes.size(), 1u) << i;
        const CardChange& c = dopo.m.outcomes[i].changes[0];
        EXPECT_EQ(c.what, "tipo");
        EXPECT_EQ(c.before, "scolastico (gruppo studenti)");
        EXPECT_EQ(c.after, "disabili (gruppo disabili)");
        EXPECT_TRUE(c.particular);
    }
    // Il registro di prima del K-024: le copie senza il tipo.
    const auto senza_tipo = [](const Lettura& base, bool togli_servizio) {
        PacchettoInMemoria m;
        for (const auto& [f, d] : base.documenti) m.file[f] = chiusura::testo_file(d);
        JsonDati r = JsonDati::parse(m.file.at("schede_servizi.json"));
        for (JsonDati& s : r.at("schede"))
            for (JsonDati& v : s.at("versioni")) v.at("scheda").erase("tipo");
        m.file["schede_servizi.json"] = r.dump(2);
        if (togli_servizio) {
            JsonDati sv = JsonDati::parse(m.file.at("servizi.json"));
            JsonDati resta = JsonDati::array();
            for (const JsonDati& x : sv.at("servizi"))
                if (x.at("codice") != "S-4000602-1") resta.push_back(x);
            sv["servizi"] = resta;
            m.file["servizi.json"] = sv.dump(2);
        }
        return leggi_pacchetto(m);
    };
    const Letto vecchio = leggi(senza_tipo(prima.dopo, false), fogli_k024(), dal23, kPresa, false);
    for (const CardOutcome& o : vecchio.m.outcomes) EXPECT_EQ(o.kind, CardOutcome::Kind::Same) << o.nr;   // senza regola: come oggi
    const Letto vecchio_con_regola = leggi(con_regola(senza_tipo(prima.dopo, false)), fogli_k024(), dal23);
    EXPECT_EQ(vecchio_con_regola.m.outcomes[1].kind, CardOutcome::Kind::NewVersion);
    const Letto senza_servizio = leggi(con_regola(senza_tipo(prima.dopo, true)), fogli_k024(), dal23);
    EXPECT_EQ(senza_servizio.m.outcomes[1].kind, CardOutcome::Kind::Same);         // senza il servizio non si confronta
    EXPECT_EQ(senza_servizio.m.outcomes[3].kind, CardOutcome::Kind::NewVersion);
    // Il numero dei disabili: 1 -> 2 (sulla 4000602) e' una versione nuova; vuoto da una parte no.
    const Letto base = leggi(lettura_k024(), fogli_k024(), attivazione());
    Fogli due = fogli_k024();
    due.schede[2][22] = "#1";   // 4000603: 1 -> 1, uguale
    due.schede[1][22] = "";     // 4000601: 0 -> non scritto, uguale
    Fogli tre = due;
    tre.schede[3][22] = "#0";   // 4000603: 1 -> 0, versione nuova
    const Letto n1 = leggi(base.dopo, due, dal23);
    for (const CardOutcome& o : n1.m.outcomes) EXPECT_EQ(o.kind, CardOutcome::Kind::Same) << o.nr;
    const Letto n2 = leggi(base.dopo, tre, dal23);
    ASSERT_EQ(n2.m.outcomes[2].kind, CardOutcome::Kind::NewVersion);
    ASSERT_EQ(n2.m.outcomes[2].changes.size(), 1u);
    EXPECT_EQ(n2.m.outcomes[2].changes[0].what, "disabili");
    EXPECT_TRUE(n2.m.outcomes[2].changes[0].particular);
    // Il profilo senza la colonna, poi con: le schede gia' lette restano uguali.
    const Lettura senza_colonna = lettura_k024_con([](JsonDati& p) { p.at("colonne").at("schede").erase("disabili"); });
    const Letto c1 = leggi(senza_colonna, fogli_k024(), attivazione());
    const Letto c2 = leggi(con_regola(c1.dopo), fogli_k024(), dal23);
    for (const CardOutcome& o : c2.m.outcomes) EXPECT_EQ(o.kind, CardOutcome::Kind::Same) << o.nr;
}

// K-024, D7: K-CTR-177 anche in kzero controlla, con il tipo del servizio nel pacchetto (non quello della lettura): la promiscua (v1) e la
// scheda per disabili con 0 (v0); un servizio cambiato a mano in "altro" non ha l'avviso. Il controllo richiede i dati particolari.
TEST(Schede, DisabiliControllatiSulRegistro) {
    const Letto x = leggi(lettura_k024(), fogli_k024(), attivazione());
    std::vector<Problem> v;
    check_cards(x.dopo.pacchetto, Index(x.dopo.pacchetto), v);
    std::vector<std::string> c177;
    for (const std::string& m : messaggi(x.dopo, v))
        if (m.rfind("K-CTR-177", 0) == 0) c177.push_back(m);
    ASSERT_EQ(c177.size(), 2u) << ::testing::PrintToString(messaggi(x.dopo, v));
    EXPECT_NE(c177[0].find("Scheda SCH-4000603-1"), std::string::npos) << c177[0];
    EXPECT_NE(c177[1].find("Scheda SCH-4000604-1"), std::string::npos) << c177[1];
    PacchettoInMemoria m;
    for (const auto& [f, d] : x.dopo.documenti) m.file[f] = chiusura::testo_file(d);
    JsonDati sv = JsonDati::parse(m.file.at("servizi.json"));
    for (JsonDati& s : sv.at("servizi"))
        if (s.at("codice") == "S-4000603-1") {
            s["tipo"] = "altro";
            s["tipo_descrizione"] = "navetta di prova";
        }
    m.file["servizi.json"] = sv.dump(2);
    const Lettura l = leggi_pacchetto(m);
    std::vector<Problem> w;
    check_cards(l.pacchetto, Index(l.pacchetto), w);
    EXPECT_EQ(std::count_if(w.begin(), w.end(), [](const Problem& p) { return p.check == Check::CardDisabledMismatch; }), 1);
}

// K-025, punto 0 (sabotaggio A8 dell'arbitro, che nessuna prova prendeva): in kzero controlla K-CTR-177 guarda ogni versione della scheda,
// non solo l'ultima (DECISIONI.md, K-024). La 4000603/1 letta dall'attivazione con 1 disabile su 8 in un servizio scolastico (variante 1),
// poi una versione nuova dal 23/11 con 0 disabili (giusta): l'avviso c'e' sulla prima versione (la sua posizione nel registro) e non sulla
// seconda. Sabotaggio: solo l'ultima versione.
TEST(Schede, DisabiliControllatiSuOgniVersione) {
    const Lettura l = lettura_k024();
    const Letto prima = leggi(l, schede_k024({3}), attivazione());
    ASSERT_EQ(prima.m.outcomes.at(0).kind, CardOutcome::Kind::New);
    Fogli f = schede_k024({3});
    f.schede[1][22] = "#0";
    DateChoices dal23;
    dal23.file = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 11, 23)};
    const Letto dopo = leggi(prima.dopo, f, dal23);
    ASSERT_EQ(dopo.m.outcomes.at(0).kind, CardOutcome::Kind::NewVersion) << ::testing::PrintToString(messaggi(dopo.dopo, dopo.m.problems));
    ASSERT_EQ(dopo.m.outcomes.at(0).changes.size(), 1u);
    EXPECT_EQ(dopo.m.outcomes.at(0).changes.at(0).what, "disabili");
    const ServiceCard* s = nullptr;
    for (const ServiceCard& x : dopo.dopo.pacchetto.service_cards)
        if (x.code == "SCH-4000603-1") s = &x;
    ASSERT_NE(s, nullptr);
    ASSERT_EQ(s->versions.size(), 2u);
    ASSERT_NE(s->versions[0].src.pointer, s->versions[1].src.pointer);
    std::vector<Problem> v;
    check_cards(dopo.dopo.pacchetto, Index(dopo.dopo.pacchetto), v);
    std::vector<const Problem*> c177;
    for (const Problem& p : v)
        if (p.check == Check::CardDisabledMismatch) c177.push_back(&p);
    ASSERT_EQ(c177.size(), 1u) << ::testing::PrintToString(messaggi(dopo.dopo, v));
    EXPECT_EQ(c177[0]->variant, 1);
    EXPECT_EQ(c177[0]->where.file, "schede_servizi.json");
    EXPECT_EQ(c177[0]->where.pointer, s->versions[0].src.pointer);
    const std::vector<std::string> testi = messaggi(dopo.dopo, {*c177[0]});
    EXPECT_NE(testi.at(0).find("Scheda SCH-4000603-1: a bordo ci sono 1 disabile su 8 persone, ma il servizio e' scolastico"), std::string::npos)
        << testi.at(0);
}

// K-024, D8 B: K-CTR-178 per ogni profilo alla versione 1 o 2, di qualsiasi tipo (anche PS-CVA, scolastico, del K-023); il messaggio non
// dice il tipo; nessun avviso alla versione 3. Sabotaggio: l'avviso solo per i profili con il tipo disabili.
TEST(Schede, ProfiliAllaVersioneUnoODueAvviso) {
    const auto avvisi = [](const Lettura& l) {
        std::vector<std::string> out;
        for (const std::string& m : messaggi(l, schede::controlla_profili(l))) out.push_back(m);
        return out;
    };
    const std::vector<std::string> v1 = avvisi(lettura_di_prova()), v2 = avvisi(lettura_k023()), v3 = avvisi(lettura_k024());
    ASSERT_EQ(v1.size(), 1u);
    ASSERT_EQ(v2.size(), 2u);
    EXPECT_TRUE(v3.empty());
    EXPECT_NE(v1[0].find("K-CTR-178 v0: Il profilo delle schede PS-CVA e' in profili_schede.json alla versione 1"), std::string::npos) << v1[0];
    EXPECT_NE(v2[0].find("PS-CVA e' in profili_schede.json alla versione 2"), std::string::npos) << v2[0];
    EXPECT_NE(v2[1].find("PS-CVA-H e' in profili_schede.json alla versione 2"), std::string::npos) << v2[1];
    for (const std::vector<std::string>* v : {&v1, &v2})
        for (const std::string& m : *v)
            for (const char* tipo : {"disabili", "scolastico", "studenti"}) EXPECT_EQ(m.find(tipo), std::string::npos) << m;
    EXPECT_EQ(definizione_controllo(Check::CardProfileOldVersion).rivela_variante(0), Classe::Comune);
}

// --- K-025: rilettura delle schede -----------------------------------------------------------------------------------------------

namespace {
DateChoices scelta_file(DateChoice::Kind k, Day d = 0) {
    DateChoices s;
    s.file = DateChoice{k, d};
    return s;
}
const Day kMeseDopo = day_from_ymd(2026, 12, 16);
// Il Nr di una riga del foglio delle fermate (per cambiare una riga di una scheda).
std::size_t riga_fermata(const Fogli& f, const std::string& nr, std::size_t n) {
    std::size_t visti = 0;
    for (std::size_t i = 1; i < f.fermate.size(); ++i)
        if (f.fermate[i][0] == nr && visti++ == n) return i;
    throw std::logic_error("riga che non c'e'");
}
}  // namespace

// K-025, punto 1: il committente manda ogni volta il file intero. Le quattro schede lette dall'attivazione il 16/11 (con i loro avvisi:
// K-CTR-163 sulla 4000703/1, K-CTR-176 sulla 4000702/1), poi lo stesso file riletto lo stesso giorno e un mese dopo, senza scelta, con
// --validita attivazione e con --validita presa-in-carico: sempre quattro "uguali", nessun problema (niente K-CTR-162, niente fermata per
// la data, nessun avviso della lettura, niente K-CTR-165), niente scritto. Sabotaggi: la scelta della data prima del confronto (le uguali
// di nuovo ferme per K-CTR-162); la scheda gia' finita fermata (variante 39 con presa-in-carico); avvisi della lettura ripetuti sulle
// uguali; K-CTR-165 su una uguale.
TEST(Schede, RiletturaDelFileInteroUguale) {
    const Letto prima = leggi(lettura_k024(), fogli_k025(), attivazione());
    ASSERT_EQ(prima.m.outcomes.size(), 4u);
    for (const CardOutcome& o : prima.m.outcomes) ASSERT_EQ(o.kind, CardOutcome::Kind::New) << o.nr;
    EXPECT_EQ(codici_di(prima, "4000701/1"), std::vector<std::string>{});
    EXPECT_EQ(codici_di(prima, "4000702/1"), std::vector<std::string>{"K-CTR-176 v0"});
    EXPECT_EQ(codici_di(prima, "4000703/1"), std::vector<std::string>{"K-CTR-163 v0"});
    EXPECT_EQ(codici_di(prima, "4000704/1"), std::vector<std::string>{});
    for (const Day presa : {kPresa, kMeseDopo})
        for (const DateChoices& s : {DateChoices{}, attivazione(), scelta_file(DateChoice::Kind::TakenInCharge)}) {
            const Letto x = leggi(prima.dopo, fogli_k025(), s, presa);
            const std::string caso = text_from_day(presa) + " scelta " + std::to_string(static_cast<int>(s.file.kind));
            ASSERT_EQ(x.m.outcomes.size(), 4u) << caso;
            for (std::size_t i = 0; i < 4; ++i) {
                const CardOutcome& o = x.m.outcomes[i];
                EXPECT_EQ(o.kind, CardOutcome::Kind::Same) << caso << " " << o.nr;
                EXPECT_EQ(o.version, 1) << caso << " " << o.nr;
                EXPECT_FALSE(o.choice_unused) << caso << " " << o.nr;
                EXPECT_EQ(o.valid.from, prima.m.outcomes[i].valid.from) << caso << " " << o.nr;
                EXPECT_EQ(o.valid.to, prima.m.outcomes[i].valid.to) << caso << " " << o.nr;
            }
            EXPECT_TRUE(x.m.problems.empty()) << caso << ::testing::PrintToString(messaggi(x.dopo, x.m.problems));
            EXPECT_TRUE(x.m.added.empty() && x.m.removed.empty() && x.m.cuts.empty() && x.m.new_homes.empty()) << caso;
            EXPECT_EQ(x.dopo.documenti.at("schede_servizi.json"), prima.dopo.documenti.at("schede_servizi.json")) << caso;
        }
}

// K-025, D1 A: una scelta --validita-scheda per una scheda uguale non fa niente (anche una data prima dell'attivazione, che per una scheda
// cambiata la fermerebbe con la variante 38), e l'uscita lo dice (choice_unused, solo per le schede nominate). Sabotaggio: la scelta
// guardata prima del confronto.
TEST(Schede, SceltaPerUnaSchedaUguale) {
    const Letto prima = leggi(lettura_k024(), fogli_k025(), attivazione());
    DateChoices s = scelta_file(DateChoice::Kind::TakenInCharge);
    s.by_nr["4000703/1"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2027, 1, 11)};
    s.by_nr["4000701/1"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 1, 1)};
    const Letto x = leggi(prima.dopo, fogli_k025(), s, kMeseDopo);
    ASSERT_EQ(x.m.outcomes.size(), 4u);
    for (const CardOutcome& o : x.m.outcomes) {
        EXPECT_EQ(o.kind, CardOutcome::Kind::Same) << o.nr;
        EXPECT_EQ(o.choice_unused, o.nr == "4000703/1" || o.nr == "4000701/1") << o.nr;
    }
    EXPECT_TRUE(x.m.problems.empty()) << ::testing::PrintToString(messaggi(x.dopo, x.m.problems));
}

// K-025: le schede cambiate si leggono come oggi. Un orario cambiato sulla 4000704/1: senza scelta si ferma (K-CTR-162), con la scelta
// e' una versione nuova con K-CTR-165 (segnata "scheda nuova": ora e' davvero una versione nuova). Il numero dei disabili cambiato sulla
// 4000703/1 (0 -> 1): versione nuova, con gli avvisi della lettura di una scheda scritta (K-CTR-163, K-CTR-165, K-CTR-177). La 4000701/1
// gia' finita e cambiata, con presa-in-carico: ferma per la variante 39, come oggi (una versione nuova non avrebbe giorni). Le altre:
// uguali, senza avvisi. Sabotaggio: una scheda cambiata (orario, numero dei disabili) letta come uguale.
TEST(Schede, SchedaCambiataSiLeggeComeOggi) {
    const Letto prima = leggi(lettura_k024(), fogli_k025(), attivazione());
    Fogli f = fogli_k025();
    f.fermate[riga_fermata(f, "4000704 / 1", 0)][1] = "16:12";
    const Letto senza = leggi(prima.dopo, f, {}, kMeseDopo);
    EXPECT_EQ(senza.m.outcomes.at(3).kind, CardOutcome::Kind::ChoiceMissing);
    for (std::size_t i : {0u, 1u, 2u}) EXPECT_EQ(senza.m.outcomes.at(i).kind, CardOutcome::Kind::Same) << i;
    EXPECT_EQ(codici_di(senza, "4000704/1"), std::vector<std::string>{"K-CTR-162 v0"});
    EXPECT_EQ(senza.m.problems.size(), 1u) << ::testing::PrintToString(messaggi(senza.dopo, senza.m.problems));
    f.schede[3][22] = "#1";   // 4000703/1: 0 -> 1 disabile, in un servizio scolastico
    f.fermate[riga_fermata(f, "4000701 / 1", 0)][1] = "07:21";
    DateChoices s = scelta_file(DateChoice::Kind::TakenInCharge);
    s.by_nr["4000704/1"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 12, 21)};
    const Letto x = leggi(prima.dopo, f, s, kMeseDopo);
    ASSERT_EQ(x.m.outcomes.size(), 4u);
    EXPECT_EQ(x.m.outcomes[0].kind, CardOutcome::Kind::Stopped);
    EXPECT_EQ(x.m.outcomes[0].fault, CardFault::ChoiceAfterEnd);
    EXPECT_EQ(x.m.outcomes[1].kind, CardOutcome::Kind::Same);
    for (std::size_t i : {2u, 3u}) {
        ASSERT_EQ(x.m.outcomes[i].kind, CardOutcome::Kind::NewVersion) << i;
        EXPECT_EQ(x.m.outcomes[i].version, 2) << i;
        ASSERT_EQ(x.m.outcomes[i].changes.size(), 1u) << i;
    }
    EXPECT_EQ(x.m.outcomes[2].changes[0].what, "disabili");
    EXPECT_EQ(x.m.outcomes[2].valid.from, kMeseDopo);
    EXPECT_EQ(x.m.outcomes[3].changes[0].what, "orari");
    EXPECT_EQ(x.m.outcomes[3].valid.from, day_from_ymd(2026, 12, 21));
    EXPECT_FALSE(x.m.outcomes[3].choice_unused);
    EXPECT_EQ(codici_di(x, "4000701/1"), std::vector<std::string>{"K-CTR-160 v39"});
    EXPECT_EQ(codici_di(x, "4000702/1"), std::vector<std::string>{});
    EXPECT_EQ(codici_di(x, "4000703/1"), (std::vector<std::string>{"K-CTR-163 v0", "K-CTR-165 v0", "K-CTR-177 v1"}));
    EXPECT_EQ(codici_di(x, "4000704/1"), std::vector<std::string>{"K-CTR-165 v0"});
}

// K-025, D2 B: una scheda uguale a una versione di prima, non all'ultima, si legge come versione nuova (tornare com'era e' un cambio vero),
// con l'avviso K-CTR-179 (forse il file e' vecchio) solo nella lettura. La 4000704/1 con un orario cambiato dal 21/12 (versione 2), poi il
// file di prima riletto con la scelta dall'11/01: versione 3, uguale alla versione 1. Sabotaggio: l'avviso che manca.
TEST(Schede, UgualeAUnaVersioneDiPrima) {
    const Letto prima = leggi(lettura_k024(), fogli_k025(), attivazione());
    Fogli f = fogli_k025();
    f.fermate[riga_fermata(f, "4000704 / 1", 0)][1] = "16:12";
    DateChoices s21;
    s21.by_nr["4000704/1"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2026, 12, 21)};
    const Letto due = leggi(prima.dopo, f, s21, kMeseDopo);
    ASSERT_EQ(due.m.outcomes.at(3).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(codici_di(due, "4000704/1"), std::vector<std::string>{"K-CTR-165 v0"});   // non e' uguale a una versione di prima
    DateChoices s11;
    s11.by_nr["4000704/1"] = DateChoice{DateChoice::Kind::Date, day_from_ymd(2027, 1, 11)};
    const Letto tre = leggi(due.dopo, fogli_k025(), s11, day_from_ymd(2027, 1, 11));
    ASSERT_EQ(tre.m.outcomes.at(3).kind, CardOutcome::Kind::NewVersion);
    EXPECT_EQ(tre.m.outcomes.at(3).version, 3);
    EXPECT_EQ(codici_di(tre, "4000704/1"), (std::vector<std::string>{"K-CTR-165 v0", "K-CTR-179 v0"}));
    std::string testo;
    for (const std::string& m : messaggi(tre.dopo, tre.m.problems))
        if (m.rfind("K-CTR-179", 0) == 0) testo = m;
    EXPECT_EQ(testo, "K-CTR-179 v0: Scheda 4000704/1: e' uguale alla versione 1 della scheda SCH-4000704-1, non all'ultima (la 2): si legge "
                     "come versione nuova (tornare com'era e' un cambio vero). Se il file e' vecchio, rileggere quello giusto prima di "
                     "scrivere.");
    EXPECT_EQ(definizione_controllo(Check::CardSameAsOlderVersion).rivela_variante(0), Classe::Comune);
    for (std::size_t i : {0u, 1u, 2u}) EXPECT_EQ(tre.m.outcomes.at(i).kind, CardOutcome::Kind::Same) << i;
}
