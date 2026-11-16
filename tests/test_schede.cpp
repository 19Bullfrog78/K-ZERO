// K-Zero - prove delle schede dei servizi (K-021): lettura dei tre fogli con il profilo, totali, accompagnatore, km scritti solo su
// alcune righe, case porta a porta, cadenze, scelta della data, versioni, togliere una scheda, controlli sul registro. I comandi e le
// uscite: test_schede_comandi.cpp. Dati inventati (tests/esempi/k021-schede).
#include <gtest/gtest.h>

#include <algorithm>
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
const CardProfile& profilo(const Lettura& l) {
    for (const CardProfile& p : l.pacchetto.card_profiles)
        if (p.code == "PS-CVA") return p;
    throw std::logic_error("profilo che non c'e'");
}

const Day kPresa = day_from_ymd(2026, 11, 16);

struct Letto {
    CardsRead r;
    CardsMerge m;
    Lettura dopo;
};
Letto leggi(const Lettura& l, const Fogli& f, DateChoices scelte = {}, Day presa = kPresa, bool date1904 = false) {
    const Index idx(l.pacchetto);
    Letto x;
    x.r = read_cards(CardTables{tabella(f.schede), tabella(f.fermate), tabella(f.date)}, profilo(l), idx, date1904);
    EXPECT_EQ(x.r.fault, TableFault::None) << x.r.fault_title;
    x.m = merge_cards(l.pacchetto, profilo(l), "CT-SCU", presa, "mail del committente", scelte, x.r);
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
    const CardsRead r = read_cards(CardTables{tabella(una(1).schede), tabella(una(1).fermate), {}}, profilo(v1.dopo), i1, false);
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
// passaggio). Il confronto con le corse (K-CTR-042) da' due avvisi falsi; l'avviso nuovo K-CTR-175 lo dice, sulla seconda voce, una
// volta per luogo; il confronto con la scheda (K-CTR-170) vede che il gruppo non e' quello che la scheda da'. Sabotaggio: l'avviso
// che manca.
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
              "le voci (salgono e scendono sommati): finche' sono due, il confronto con le corse (K-CTR-042) non e' affidabile.");
    EXPECT_EQ(sui_gruppi(v, {"G-4000300-1"}, Check::DemandDiffersFromTrips).size(), 2u);   // gli avvisi falsi che K-CTR-175 spiega
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
