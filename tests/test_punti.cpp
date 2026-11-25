// K-Zero - prove dei punti degli utenti dell'elenco e del confronto con le schede (K-027). Dati inventati (tests/esempi/k027-punti):
// le quattro schede (4000901/1 andata dell'infanzia a due giri, Lagobello Lago due volte; 4000902/1 ritorno del pomeriggio solo il
// mercoledi'; 4000903/1 andata e 4000904/1 ritorno di mezzogiorno della primaria) e gli otto alunni (punti_prove.hpp).
#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "chiusura.hpp"
#include "controlli_catalogo.hpp"
#include "punti_prove.hpp"
#include "riservatezza.hpp"
#include "schede_scrittura.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove;
using namespace kzero::prove::punti;

namespace {

Day g(const char* t) { return *day_from_text(t); }

// "codice vN posizione: messaggio" dei problemi (tutti visibili), in ordine.
std::vector<std::string> messaggi(const Lettura& l, const std::vector<Problem>& v, std::optional<Check> solo = std::nullopt) {
    const Presentatore pr(&l.pacchetto, &l.versioni, Visibilita{true, true});
    std::vector<std::string> out;
    for (const Problem& p : v)
        if (!solo || p.check == *solo)
            out.push_back(definizione_controllo(p.check).codice + " v" + std::to_string(p.variant) + " " + p.where.pointer + ": " + pr.messaggio(p));
    return out;
}
// I giorni di un problema (il parametro dei giorni), vuoto se non c'e'.
std::vector<Day> giorni_di(const Problem& p) {
    for (const Param& x : p.params)
        if (x.kind == Param::Kind::Days) return x.days;
    return {};
}
const Problem* problema(const std::vector<Problem>& v, Check c, const std::string& utente = "", int variante = -1) {
    for (const Problem& p : v) {
        if (p.check != c || (variante >= 0 && p.variant != variante)) continue;
        if (utente.empty()) return &p;
        for (const Param& x : p.params)
            if (x.name == "utente" && x.text == utente) return &p;
    }
    return nullptr;
}

// Il periodo del confronto della scheda (codice del registro) che contiene quel giorno; nullptr se la scheda quel giorno non gira.
const ComparedPeriod* periodo(const PointsComparison& c, const std::string& scheda, Day d) {
    for (const ComparedVersion& v : c.versions)
        if (v.card->code == scheda)
            for (const ComparedPeriod& pe : v.periods)
                if (std::find(pe.days.begin(), pe.days.end(), d) != pe.days.end()) return &pe;
    return nullptr;
}
PointsComparison confronto(const Lettura& l) { return compare_points(l.pacchetto, Index(l.pacchetto), l.pacchetto.manifest.period); }
// Chi sale (o scende) a una riga di una scheda quel giorno.
std::vector<Code> salgono(const PointsComparison& c, const std::string& scheda, Day d, std::size_t riga) {
    const ComparedPeriod* pe = periodo(c, scheda, d);
    return pe ? pe->passages.at(riga).boarding : std::vector<Code>{};
}
std::vector<Code> scendono(const PointsComparison& c, const std::string& scheda, Day d, std::size_t riga) {
    const ComparedPeriod* pe = periodo(c, scheda, d);
    return pe ? pe->passages.at(riga).alighting : std::vector<Code>{};
}

// I codici degli alunni del pacchetto di prova (dati dal programma nell'ordine del file).
struct Alunni {
    Code rossi, bianchi, verdi, neri, gialli, bruni, mori, viola;
};
Alunni alunni(const PacchettoInMemoria& m) {
    const Package p = lettura(m).pacchetto;
    return {utente(p, "ROSSI ANNA").code, utente(p, "BIANCHI SOFIA").code, utente(p, "VERDI LUCA").code, utente(p, "NERI MARCO").code,
            utente(p, "GIALLI PAOLO").code, utente(p, "BRUNI ELSA").code, utente(p, "MORI LIA").code, utente(p, "VIOLA DINO").code};
}

// Una versione nuova di una scheda dal giorno dal (scelta per la scheda), presa in carico il giorno presa.
PacchettoInMemoria con_versione(const PacchettoInMemoria& m, const prove::schede::Fogli& f, const std::string& nr, const std::string& dal,
                                const std::string& presa) {
    const Lettura l = lettura(m);
    const Index idx(l.pacchetto);
    const CardProfile* pr = idx.card_profile("PS-CVA");
    const CardsRead r = read_cards(CardTables{prove::schede::tabella(f.schede), prove::schede::tabella(f.fermate), prove::schede::tabella(f.date), {}}, *pr, idx, false);
    DateChoices scelte;
    scelte.by_nr[nr] = DateChoice{DateChoice::Kind::Date, g(dal.c_str())};
    scelte.file.kind = DateChoice::Kind::Activation;
    const CardsMerge x = merge_cards(l.pacchetto, *pr, "CT-SCU", g(presa.c_str()), "mail del committente", scelte, r);
    PacchettoInMemoria out = m;
    for (const auto& [file, doc] : kzero::api::schede::documenti_dopo(l, x.cards, l.pacchetto.removed_cards, x.added, x.removed, x.cuts, x.new_homes))
        out.file[file] = chiusura::testo_file(doc);
    return out;
}

// Le righe della scheda 4000901/1: 0 scuola (sale l'accompagnatore), 1 Lagobello Lago (1a volta), 2 Castelrovo, 3 scuola (scendono i
// bambini del primo giro), 4 Lagobello Lago (2a volta), 5 Pradello, 6 scuola (scendono quelli del secondo giro e l'accompagnatore).
constexpr std::size_t kLago1 = 1, kScuola1 = 3, kLago2 = 4, kPradello901 = 5, kScuola2 = 6;
// Le righe della 4000903/1: 0 Pradello, 1 Castelrovo, 2 scuola.
constexpr std::size_t kPradello = 0, kCastelrovo = 1, kScuola = 2;

}  // namespace

// --- Formato e cassaforte ----------------------------------------------------------------------------------------------------

// K-027, D1 A: i punti, i periodi "non frequenta" e gli sblocchi per giorni stanno in elenco_utenti.json alla versione 3 (la scrittura
// del programma scrive la 3); tutto comune tranne chi ha detto uno sblocco a voce e la nota di un periodo "non frequenta" (personali: nella
// cassaforte quando il pacchetto si chiude). Riletti, i dati sono gli stessi.
TEST(Punti, FormatoVersioneTreECassaforte) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        ListUser& v = utente(l, "VERDI LUCA");
        ListUserUnlock s("AM", "telefonata all'ufficio trasporti del 3/11", g("2026-11-03"), {});
        s.all_days = false;
        s.days.add(Weekday::Wednesday);
        s.from = g("2026-11-04");
        s.to = g("2027-01-31");
        s.by_voice = true;
        s.who = "Ufficio trasporti, signora Nera";
        v.unlocks.push_back(s);
        punto(v, "AM", "2026-11-04", {regola(p, "SCH-4000903-1", "F-CASTELROVO")});
        ListUser& b = utente(l, "BRUNI ELSA");
        punto(b, "AM", "2026-09-14",
              {regola(p, "SCH-4000903-1", "F-PRADELLO", 1, {Weekday::Monday, Weekday::Tuesday, Weekday::Wednesday}),
               regola(p, "SCH-4000903-1", "F-CASTELROVO", 1, {Weekday::Thursday, Weekday::Friday})},
              PointOrigin::Proposal, "mail del committente del 3/9");
        ListUser& n = utente(l, "NERI MARCO");
        NotAttending x;
        x.from = g("2026-11-02");
        x.reason = NotAttendingReason::Family;
        x.note = "la famiglia lo accompagna";
        x.mark = ManualMark{"telefonata della famiglia del 30/10", g("2026-10-30")};
        n.not_attending.push_back(x);
    });
    const Lettura l = lettura(m);
    EXPECT_EQ(l.versioni.at("elenco_utenti.json"), 3);
    const ListUser v = utente(l.pacchetto, "VERDI LUCA");
    ASSERT_EQ(v.unlocks.size(), 1u);
    EXPECT_FALSE(v.unlocks[0].all_days);
    EXPECT_TRUE(v.unlocks[0].days.has(Weekday::Wednesday));
    EXPECT_FALSE(v.unlocks[0].days.has(Weekday::Monday));
    EXPECT_EQ(v.unlocks[0].from, g("2026-11-04"));
    EXPECT_EQ(v.unlocks[0].to, g("2027-01-31"));
    EXPECT_TRUE(v.unlocks[0].by_voice);
    EXPECT_EQ(v.unlocks[0].who, "Ufficio trasporti, signora Nera");
    ASSERT_EQ(v.points.size(), 1u);
    EXPECT_EQ(v.points[0].leg, "AM");
    ASSERT_EQ(v.points[0].history.size(), 1u);
    EXPECT_EQ(v.points[0].history[0].rules.at(0).place.code, "F-CASTELROVO");
    const ListUser b = utente(l.pacchetto, "BRUNI ELSA");
    ASSERT_EQ(b.points.at(0).history.size(), 1u);
    const PointEntry& e = b.points[0].history[0];
    EXPECT_EQ(e.origin, PointOrigin::Proposal);
    EXPECT_EQ(e.consent, "mail del committente del 3/9");
    EXPECT_TRUE(point_entry_double(e));
    ASSERT_EQ(e.rules.size(), 2u);
    EXPECT_TRUE(e.rules[1].days.has(Weekday::Friday));
    EXPECT_FALSE(e.rules[1].days.has(Weekday::Monday));
    const ListUser n = utente(l.pacchetto, "NERI MARCO");
    ASSERT_EQ(n.not_attending.size(), 1u);
    EXPECT_EQ(n.not_attending[0].reason, NotAttendingReason::Family);
    EXPECT_EQ(n.not_attending[0].note, "la famiglia lo accompagna");
    EXPECT_FALSE(n.not_attending[0].to);
    // Chiudendo il pacchetto: chi e la nota vanno nella cassaforte (personali), il resto resta nel file normale.
    const chiusura::Separazione s = chiusura::separa(l);
    const std::string normale = s.normali.at("elenco_utenti.json").dump();
    EXPECT_EQ(normale.find("signora Nera"), std::string::npos);
    EXPECT_EQ(normale.find("la famiglia lo accompagna"), std::string::npos);
    EXPECT_NE(normale.find("\"a_voce\":true"), std::string::npos);   // il fatto "a voce" e' comune
    EXPECT_NE(normale.find("F-CASTELROVO"), std::string::npos);
    EXPECT_NE(normale.find("\"motivo\":\"famiglia\""), std::string::npos);
    std::set<std::string> personali;
    for (const Voce& x : s.contenuto.personali)
        if (x.file == "elenco_utenti.json" && x.valore.is_string()) personali.insert(x.valore.get<std::string>());
    EXPECT_TRUE(personali.count("Ufficio trasporti, signora Nera"));
    EXPECT_TRUE(personali.count("la famiglia lo accompagna"));
}

// K-027 (D1): una lettura del file del committente non cambia mai i punti ne' i periodi "non frequenta" (non sono nel file). Un utente che
// esce dall'elenco (lettura completa) se ne va con i suoi punti. Sabotaggio: la lettura perde i punti.
TEST(Punti, LetturaDellElencoNonPerdeIPunti) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "GIALLI PAOLO"), "AM", "2026-09-14", {regola(p, "SCH-4000903-1", "F-CASTELROVO")});
        NotAttending x;
        x.from = g("2026-11-02");
        x.mark = ManualMark{"telefonata", g("2026-10-30")};
        utente(l, "NERI MARCO").not_attending.push_back(x);
    });
    const Lettura l = lettura(m);
    const Index idx(l.pacchetto);
    const ReadingProfile& pr = *idx.reading_profile("PR-CVA");
    prove::schede::Righe righe = righe_k027("elenco.tsv");
    righe.erase(righe.begin() + 7);   // MORI LIA (la settima riga dopo i titoli) non c'e' piu'
    const ListRead r = read_list(prove::schede::tabella(righe), pr, idx, 2026, l.pacchetto.client_lists.data());
    const ListMerge x = merge_list(l.pacchetto, pr, 2026, r, true, std::string(64, 'c'));
    const auto trova = [&](const std::string& nome) -> const ListUser* {
        for (const ListUser& u : x.list.users)
            if (u.name == nome) return &u;
        return nullptr;
    };
    ASSERT_TRUE(trova("GIALLI PAOLO"));
    ASSERT_EQ(trova("GIALLI PAOLO")->points.size(), 1u);
    EXPECT_EQ(trova("GIALLI PAOLO")->points[0].history.at(0).rules.at(0).place.code, "F-CASTELROVO");
    ASSERT_TRUE(trova("NERI MARCO"));
    EXPECT_EQ(trova("NERI MARCO")->not_attending.size(), 1u);
    EXPECT_FALSE(trova("MORI LIA"));
}

// --- La storia e il passaggio ----------------------------------------------------------------------------------------------------

// K-027 (D2): il punto punta al passaggio (luogo e volta), non al luogo. La 4000901/1 passa due volte da Lagobello, Lago: ROSSI ANNA sale
// alla 1a volta e scende alla prima scuola dopo (riga 3), MORI LIA sale alla 2a volta e scende alla scuola della fine (riga 6). Sabotaggio:
// il punto al luogo invece che al passaggio (tutte e due alla prima volta).
TEST(Punti, IlPuntoEIlPassaggioNonIlLuogo) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "ROSSI ANNA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-LAGOBELLO", 1)});
        punto(utente(l, "MORI LIA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-LAGOBELLO", 2)});
        punto(utente(l, "BIANCHI SOFIA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-PRADELLO", 1)});
    });
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const PointsComparison c = confronto(l);
    const Day lun = g("2026-11-09");
    EXPECT_EQ(salgono(c, "SCH-4000901-1", lun, kLago1), std::vector<Code>{a.rossi});
    EXPECT_EQ(salgono(c, "SCH-4000901-1", lun, kLago2), std::vector<Code>{a.mori});
    EXPECT_EQ(salgono(c, "SCH-4000901-1", lun, kPradello901), std::vector<Code>{a.bianchi});
    EXPECT_EQ(scendono(c, "SCH-4000901-1", lun, kScuola1), std::vector<Code>{a.rossi});
    EXPECT_EQ(scendono(c, "SCH-4000901-1", lun, kScuola2), (std::vector<Code>{a.bianchi, a.mori}));
    // L'accompagnatore e' tolto dai numeri della scheda: alla scuola della fine scendono 3 sulla scheda, 2 bambini.
    const ComparedPeriod* pe = periodo(c, "SCH-4000901-1", lun);
    ASSERT_TRUE(pe);
    EXPECT_EQ(pe->passages[kScuola2].card_alighting, 2);
    EXPECT_EQ(pe->passages[0].card_boarding, 0);
    EXPECT_TRUE(pe->passages[kScuola2].school);
    // Il passaggio di Castelrovo ha una salita sulla scheda e nessun punto: K-CTR-182; la prima scuola ne ha 2, uno solo con il punto: 181 v1.
    const std::vector<Problem> v = problemi_dei_punti(l);
    const std::vector<std::string> m182 = messaggi(l, v, Check::PointsPassageEmpty);
    ASSERT_FALSE(m182.empty());
    EXPECT_EQ(m182[0].rfind("K-CTR-182 v0 /schede/0/versioni/0: Scheda SCH-4000901-1 (versione 1): al passaggio F-CASTELROVO (Castelrovo, Bivio) "
                            "(1a volta, alle 07:30) sulla scheda sale 1 persona, e nessun utente ha il punto li', ",
                            0),
              0u)
        << m182[0];
    ASSERT_TRUE(problema(v, Check::PointsCountsDiffer, "", 1));
}

// K-027: una voce nuova dal giorno D chiude quella di prima al giorno D-1; i giorni prima di D non cambiano mai. Una voce scritta dopo con
// un dal prima di una voce gia' scritta la sostituisce (resta scritta, segnata "sostituita"). Sabotaggio: una voce nuova che cambia i
// giorni prima.
TEST(Punti, VoceNuovaNonCambiaIGiorniPrima) {
    const PacchettoInMemoria base = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "VIOLA DINO"), "AM", "2026-09-14", {regola(p, "SCH-4000903-1", "F-PRADELLO")});
    });
    const PacchettoInMemoria dopo = con_elenco(base, [](ClientList& l, const Package& p) {
        punto(utente(l, "VIOLA DINO"), "AM", "2026-12-01", {regola(p, "SCH-4000903-1", "F-CASTELROVO")});
    });
    const Alunni a = alunni(base);
    // Le letture restano vive finche' servono: il confronto punta alle loro schede.
    const Lettura l0 = lettura(base), l1 = lettura(dopo);
    const PointsComparison c0 = confronto(l0), c1 = confronto(l1);
    for (Day d = g("2026-09-14"); d < g("2026-12-01"); ++d) {
        if (!periodo(c0, "SCH-4000903-1", d)) continue;
        EXPECT_EQ(salgono(c0, "SCH-4000903-1", d, kPradello), salgono(c1, "SCH-4000903-1", d, kPradello)) << text_from_day(d);
        EXPECT_EQ(salgono(c0, "SCH-4000903-1", d, kCastelrovo), salgono(c1, "SCH-4000903-1", d, kCastelrovo)) << text_from_day(d);
    }
    EXPECT_EQ(salgono(c1, "SCH-4000903-1", g("2026-11-30"), kPradello), std::vector<Code>{a.viola});
    EXPECT_EQ(salgono(c1, "SCH-4000903-1", g("2026-12-01"), kCastelrovo), std::vector<Code>{a.viola});
    EXPECT_TRUE(salgono(c1, "SCH-4000903-1", g("2026-12-01"), kPradello).empty());
    // Una voce dal 20/11 scritta dopo: sostituisce quella dal 01/12 (che resta, segnata), i giorni prima del 20/11 restano.
    const PacchettoInMemoria terza = con_elenco(dopo, [](ClientList& l, const Package& p) {
        punto(utente(l, "VIOLA DINO"), "AM", "2026-11-20", {regola(p, "SCH-4000903-1", "F-CASTELROVO")});
    });
    const ListUser u = utente(lettura(terza).pacchetto, "VIOLA DINO");
    ASSERT_EQ(u.points.at(0).history.size(), 3u);
    EXPECT_FALSE(u.points[0].history[0].replaced);
    EXPECT_EQ(u.points[0].history[1].from, g("2026-11-20"));
    EXPECT_FALSE(u.points[0].history[1].replaced);
    EXPECT_EQ(u.points[0].history[2].from, g("2026-12-01"));
    EXPECT_TRUE(u.points[0].history[2].replaced);
    const Lettura l2 = lettura(terza);
    const PointsComparison c2 = confronto(l2);
    EXPECT_EQ(salgono(c2, "SCH-4000903-1", g("2026-11-19"), kPradello), std::vector<Code>{a.viola});
    EXPECT_EQ(salgono(c2, "SCH-4000903-1", g("2026-11-20"), kCastelrovo), std::vector<Code>{a.viola});
}

// --- Chi conta ----------------------------------------------------------------------------------------------------------------

// K-027: un sospeso conta solo nei giorni in cui e' sbloccato. VERDI LUCA (sospeso) ha il punto a Castelrovo sulla 4000903/1 (da lunedi' a
// venerdi') e lo sblocco AM solo il mercoledi': il mercoledi' conta, il lunedi' no (K-CTR-186, solo nei giorni non sbloccati). Sabotaggio:
// lo sblocco del mercoledi' contato anche il lunedi'.
TEST(Punti, SospesoSbloccatoSoloIlMercoledi) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        ListUser& v = utente(l, "VERDI LUCA");
        ListUserUnlock s("AM", "mail del committente del 10/11", g("2026-11-10"), {});
        s.all_days = false;
        s.days.add(Weekday::Wednesday);
        v.unlocks.push_back(s);
        punto(v, "AM", "2026-09-14", {regola(p, "SCH-4000903-1", "F-CASTELROVO")});
    });
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const PointsComparison c = confronto(l);
    EXPECT_EQ(salgono(c, "SCH-4000903-1", g("2026-11-18"), kCastelrovo), std::vector<Code>{a.verdi});   // mercoledi' (l'11/11 e' il patrono: niente scuola)
    EXPECT_TRUE(salgono(c, "SCH-4000903-1", g("2026-11-16"), kCastelrovo).empty());                      // lunedi'
    const ComparedPeriod* lun = periodo(c, "SCH-4000903-1", g("2026-11-16"));
    ASSERT_TRUE(lun);
    EXPECT_EQ(lun->not_counted, std::vector<Code>{a.verdi});
    const std::vector<Problem> v = problemi_dei_punti(l);
    const Problem* p186 = problema(v, Check::PointsSuspendedNoUnlock, a.verdi);
    ASSERT_TRUE(p186);
    const std::vector<Day> giorni = giorni_di(*p186);
    EXPECT_TRUE(std::find(giorni.begin(), giorni.end(), g("2026-11-16")) != giorni.end());
    EXPECT_TRUE(std::find(giorni.begin(), giorni.end(), g("2026-11-18")) == giorni.end());
    for (const Day d : giorni) EXPECT_NE(weekday_of(d), Weekday::Wednesday) << text_from_day(d);
    // Il mercoledi' ha il punto e lo sblocco: niente "senza punto"; RP (sospeso, senza sblocco) non ha diritto: nessun avviso.
    EXPECT_FALSE(problema(v, Check::PointsMissing, a.verdi));
}

// K-027: chi non frequenta (periodo "non frequenta") non conta e il confronto lo mostra; nessun "senza punto" nel periodo. Sabotaggio: chi non
// viaggia contato.
TEST(Punti, ChiNonFrequentaNonConta) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        ListUser& n = utente(l, "NERI MARCO");
        punto(n, "AM", "2026-09-14", {regola(p, "SCH-4000903-1", "F-PRADELLO")});
        NotAttending x;
        x.from = g("2026-11-02");
        x.reason = NotAttendingReason::Family;
        x.mark = ManualMark{"telefonata della famiglia del 30/10", g("2026-10-30")};
        n.not_attending.push_back(x);
    });
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const PointsComparison c = confronto(l);
    EXPECT_EQ(salgono(c, "SCH-4000903-1", g("2026-10-30"), kPradello), std::vector<Code>{a.neri});
    EXPECT_TRUE(salgono(c, "SCH-4000903-1", g("2026-11-02"), kPradello).empty());
    const ComparedPeriod* pe = periodo(c, "SCH-4000903-1", g("2026-11-02"));
    ASSERT_TRUE(pe);
    EXPECT_EQ(pe->not_attending, std::vector<Code>{a.neri});
    // RM (nessun punto): "senza punto" solo fino al 30/10 (tutte le tratte non frequentano dal 02/11).
    const std::vector<Problem> v = problemi_dei_punti(l);
    const Problem* p183 = nullptr;
    for (const Problem& p : v)
        if (p.check == Check::PointsMissing) {
            bool neri = false, rm = false;
            for (const Param& x : p.params) {
                neri = neri || (x.name == "utente" && x.text == a.neri);
                rm = rm || (x.name == "tratta" && x.text == "RM");
            }
            if (neri && rm) p183 = &p;
        }
    ASSERT_TRUE(p183);
    const std::vector<Day> giorni = giorni_di(*p183);
    ASSERT_FALSE(giorni.empty());
    EXPECT_EQ(giorni.back(), g("2026-10-30"));
}

// K-027 (D5): un attivo con una tratta della nostra azienda senza punto nei giorni in cui viaggia ha l'avviso K-CTR-183 ("grave": ha
// diritto), con i giorni. Con il punto da un giorno, l'avviso resta solo per i giorni prima. Sabotaggio: attivo senza punto non segnalato.
TEST(Punti, AttivoSenzaPuntoSegnalato) {
    const PacchettoInMemoria m = pacchetto_k027();
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const std::vector<Problem> v = problemi_dei_punti(l);
    const Problem* p = problema(v, Check::PointsMissing, a.gialli, 0);
    ASSERT_TRUE(p);
    EXPECT_EQ(giorni_di(*p).size(), 175u);   // da lunedi' a venerdi', dal 14/09/2026 all'11/06/2027, senza le feste (CAL-SCUOLA)
    EXPECT_EQ(definizione_controllo(Check::PointsMissing).livello, Livello::Avviso);
    const PacchettoInMemoria con = con_elenco(m, [](ClientList& l2, const Package& pk) {
        punto(utente(l2, "GIALLI PAOLO"), "AM", "2026-10-01", {regola(pk, "SCH-4000903-1", "F-CASTELROVO")});
    });
    const std::vector<Problem> vq = problemi_dei_punti(lettura(con));   // il problema trovato punta dentro questo elenco
    const Problem* q = problema(vq, Check::PointsMissing, a.gialli, 0);
    ASSERT_TRUE(q);
    EXPECT_EQ(giorni_di(*q).back(), g("2026-09-30"));
    // Niente avviso per i giorni senza scuola (D4: i calendari di base delle schede): l'8 dicembre e' tolto da CAL-SCUOLA.
    const std::vector<Day> gp = giorni_di(*p);
    EXPECT_TRUE(std::find(gp.begin(), gp.end(), g("2026-12-08")) == gp.end());
    EXPECT_TRUE(std::find(gp.begin(), gp.end(), g("2026-12-07")) != gp.end());
}

// K-027: la scuola di scarico. Le discese alla scuola si contano per chi va in quella scuola: un alunno della primaria con il punto sulla
// scheda dell'infanzia non scende all'infanzia (K-CTR-188 variante 3, contato solo dove sale). Sabotaggio: la scuola di scarico ignorata
// nel confronto (scende alla prima scuola dopo, qualunque sia).
TEST(Punti, ScuolaDiScarico) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "ROSSI ANNA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-LAGOBELLO", 1)});
        punto(utente(l, "GIALLI PAOLO"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-CASTELROVO", 1)});
    });
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const PointsComparison c = confronto(l);
    const Day lun = g("2026-11-09");
    EXPECT_EQ(salgono(c, "SCH-4000901-1", lun, 2), std::vector<Code>{a.gialli});
    EXPECT_EQ(scendono(c, "SCH-4000901-1", lun, kScuola1), std::vector<Code>{a.rossi});
    EXPECT_TRUE(scendono(c, "SCH-4000901-1", lun, kScuola2).empty());
    EXPECT_TRUE(problema(problemi_dei_punti(l), Check::PointsInconsistent, a.gialli, 3));
}

// K-027 (D7): doppia tratta. BRUNI ELSA con due luoghi in giorni diversi (con il consenso): il lunedi' a Pradello, il giovedi' a
// Castelrovo, mai "doppia" nei numeri. VIOLA DINO con due luoghi "sempre" (settimane alterne): conta su tutti e due i passaggi, segnata
// "doppio" e "doppio sulla stessa scheda" (stessa versione). Senza consenso: K-CTR-187. Sabotaggio (dell'arbitro): un doppio contato una
// volta sola.
TEST(Punti, DoppiaTratta) {
    const PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "BRUNI ELSA"), "AM", "2026-09-14",
              {regola(p, "SCH-4000903-1", "F-PRADELLO", 1, {Weekday::Monday, Weekday::Tuesday, Weekday::Wednesday}),
               regola(p, "SCH-4000903-1", "F-CASTELROVO", 1, {Weekday::Thursday, Weekday::Friday})},
              PointOrigin::Manual, "mail del committente del 3/9");
        punto(utente(l, "VIOLA DINO"), "AM", "2026-09-14",
              {regola(p, "SCH-4000903-1", "F-PRADELLO"), regola(p, "SCH-4000903-1", "F-CASTELROVO")});
    });
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const PointsComparison c = confronto(l);
    const Day lun = g("2026-11-09"), gio = g("2026-11-12");
    EXPECT_EQ(salgono(c, "SCH-4000903-1", lun, kPradello), (std::vector<Code>{a.bruni, a.viola}));
    EXPECT_EQ(salgono(c, "SCH-4000903-1", lun, kCastelrovo), std::vector<Code>{a.viola});
    EXPECT_EQ(salgono(c, "SCH-4000903-1", gio, kPradello), std::vector<Code>{a.viola});
    EXPECT_EQ(salgono(c, "SCH-4000903-1", gio, kCastelrovo), (std::vector<Code>{a.bruni, a.viola}));
    const ComparedPeriod* pe = periodo(c, "SCH-4000903-1", lun);
    ASSERT_TRUE(pe);
    EXPECT_EQ(pe->passages[kPradello].doubles, std::vector<Code>{a.viola});
    EXPECT_EQ(pe->passages[kCastelrovo].doubles_same_card, std::vector<Code>{a.viola});
    // Scende alla scuola una volta per ogni passaggio contato (il committente lo conta su tutte e due le fermate): VIOLA due volte, alla
    // fermata una volta sola.
    EXPECT_EQ(pe->passages[kScuola].alighting, (std::vector<Code>{a.bruni, a.viola, a.viola}));
    EXPECT_EQ(pe->passages[kCastelrovo].boarding, std::vector<Code>{a.viola});
    const std::vector<Problem> v = problemi_dei_punti(l);
    EXPECT_FALSE(problema(v, Check::PointsDoubleNoConsent, a.bruni));
    EXPECT_TRUE(problema(v, Check::PointsDoubleNoConsent, a.viola));
}

// K-027 (D2): una versione nuova della scheda dove il luogo c'e' un altro numero di volte: il punto non si riaggancia (K-CTR-185 variante 0,
// non contato); confermato di nuovo sulla versione nuova, conta. Una volta che la scheda non ha: K-CTR-184 variante 0.
TEST(Punti, VersioneNuovaDellaScheda) {
    PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "ROSSI ANNA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-LAGOBELLO", 1)});
        punto(utente(l, "MORI LIA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-LAGOBELLO", 2)});
    });
    // La versione 2 dall'11/01/2027: il secondo giro passa da Montebrino invece che da Lagobello, Lago.
    prove::schede::Fogli f = fogli_k027();
    for (auto& r : f.fermate)
        if (r.at(0) == "4000901 / 1" && r.at(1) == "07:53") r.at(2) = "MONTEBRINO CENTRO";
    m = con_versione(m, f, "4000901/1", "2027-01-11", "2027-01-05");
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const PointsComparison c = confronto(l);
    EXPECT_EQ(salgono(c, "SCH-4000901-1", g("2027-01-08"), kLago1), std::vector<Code>{a.rossi});
    EXPECT_TRUE(salgono(c, "SCH-4000901-1", g("2027-01-11"), kLago1).empty());
    const std::vector<Problem> v = problemi_dei_punti(l);
    const Problem* p = problema(v, Check::PointsPassageChanged, a.rossi, 0);
    ASSERT_TRUE(p);
    EXPECT_EQ(giorni_di(*p).front(), g("2027-01-11"));
    EXPECT_TRUE(problema(v, Check::PointsPassageChanged, a.mori, 0));
    // Confermato di nuovo sulla versione 2: conta.
    const PacchettoInMemoria ok = con_elenco(m, [](ClientList& l2, const Package& p2) {
        punto(utente(l2, "ROSSI ANNA"), "AM", "2027-01-11", {regola(p2, "SCH-4000901-1", "F-LAGOBELLO", 1, {}, 2)});
        punto(utente(l2, "MORI LIA"), "AM", "2027-01-11", {regola(p2, "SCH-4000901-1", "F-LAGOBELLO", 2, {}, 2)});
    });
    const Lettura lo = lettura(ok);
    EXPECT_EQ(salgono(confronto(lo), "SCH-4000901-1", g("2027-01-11"), kLago1), std::vector<Code>{a.rossi});
    const std::vector<Problem> w = problemi_dei_punti(lo);
    EXPECT_FALSE(problema(w, Check::PointsPassageChanged, a.rossi));
    // La 2a volta sulla versione 2 non c'e' (Lagobello, Lago una volta sola): K-CTR-184 variante 0.
    EXPECT_TRUE(problema(w, Check::PointsNotValid, a.mori, 0));
}

// --- La proposta -----------------------------------------------------------------------------------------------------------------

// K-027: la proposta. Le fonti in ordine (la fermata indicata, poi lo stradario: la riga del grado infanzia vince su quella di tutti); i
// candidati per giorno della settimana; piu' candidati, nessuna scelta; l'orario dell'utente con le attese (D6). La proposta non scrive
// niente (il pacchetto resta com'e').
TEST(Punti, Proposta) {
    const PacchettoInMemoria m = pacchetto_k027();
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const Index idx(l.pacchetto);
    const std::vector<PointProposal> v = propose_points(l.pacchetto, idx, g("2026-09-21"), "", "", false);
    const auto di = [&](const Code& u, const std::string& tratta) -> const PointProposal* {
        for (const PointProposal& x : v)
            if (x.user->code == u && x.leg == tratta) return &x;
        return nullptr;
    };
    // BIANCHI: la fermata indicata (Pradello): sulla 4000901/1 una volta sola, un candidato per tutti i giorni: una regola "sempre".
    const PointProposal* b = di(a.bianchi, "AM");
    ASSERT_TRUE(b);
    EXPECT_EQ(b->source, PointSource::IndicatedStop);
    ASSERT_EQ(b->ready.size(), 1u);
    EXPECT_TRUE(b->ready[0].all_days);
    EXPECT_EQ(b->ready[0].place.code, "F-PRADELLO");
    // ROSSI: lo stradario, riga dell'infanzia (Lagobello, Lago, non Pradello): la 4000901/1 ci passa due volte, tutte e due compatibili con
    // l'orario (08:00, attesa di 10 minuti): due candidati, nessuna regola pronta.
    const PointProposal* r = di(a.rossi, "AM");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->source, PointSource::StreetTable);
    EXPECT_EQ(r->places, std::vector<Code>{"F-LAGOBELLO"});
    EXPECT_TRUE(r->ready.empty());
    ASSERT_FALSE(r->days.empty());
    EXPECT_EQ(r->days[0].candidates.size(), 2u);
    // NERI, ritorno di mezzogiorno: solo la 4000904/1 (parte alle 12:40, ora 12:35, attesa dopo 10); la 4000902/1 (16:10, il mercoledi') no.
    const PointProposal* n = di(a.neri, "RM");
    ASSERT_TRUE(n);
    for (const PointDayProposal& d : n->days) {
        ASSERT_EQ(d.candidates.size(), 1u);
        EXPECT_EQ(d.candidates[0].card->code, "SCH-4000904-1");
    }
    EXPECT_TRUE(n->ready.at(0).all_days);
    // VERDI e' sospeso senza sblocchi: nessuna proposta.
    EXPECT_FALSE(di(a.verdi, "AM"));
    EXPECT_FALSE(di(a.verdi, "RP"));
    // La proposta non cambia il pacchetto.
    for (const ClientList& x : l.pacchetto.client_lists)
        for (const ListUser& u : x.users) EXPECT_TRUE(u.points.empty());
}

// --- Riservatezza -----------------------------------------------------------------------------------------------------------------

// K-027 (D3 A): l'utente e' legato (nome e casa tra i particolari, nella sezione dei legati) se un suo punto e' in un luogo sensibile o su
// una scheda per disabili o individualizzata; gli altri utenti della stessa scheda no (regola per persona). Sabotaggio: il nome di un
// legato a un luogo sensibile fuori dai particolari.
TEST(Punti, UtenteLegatoDaiPunti) {
    PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        punto(utente(l, "GIALLI PAOLO"), "AM", "2026-09-14", {regola(p, "SCH-4000903-1", "F-CASTELROVO")});
        punto(utente(l, "VIOLA DINO"), "AM", "2026-09-14", {regola(p, "SCH-4000903-1", "F-PRADELLO")});
        punto(utente(l, "ROSSI ANNA"), "AM", "2026-09-21", {regola(p, "SCH-4000901-1", "F-LAGOBELLO", 1)});
    });
    const Alunni a = alunni(m);
    {
        const Lettura l = lettura(m);
        const Index idx(l.pacchetto);
        const LegameSensibile ls(&idx);
        EXPECT_FALSE(ls.utente(a.gialli));
        EXPECT_FALSE(ls.utente(a.viola));
    }
    // Castelrovo, Bivio luogo sensibile: GIALLI (punto li') e' legato, VIOLA (stessa scheda, punto a Pradello) no.
    PacchettoInMemoria s = m;
    JsonDati d = JsonDati::parse(s.file.at("luoghi.json"));
    for (JsonDati& x : d.at("luoghi"))
        if (x.at("codice") == "F-CASTELROVO") x["sensibile"] = true;
    s.file["luoghi.json"] = d.dump(2);
    {
        const Lettura l = lettura(s);
        const Index idx(l.pacchetto);
        const LegameSensibile ls(&idx);
        EXPECT_TRUE(ls.utente(a.gialli));
        EXPECT_FALSE(ls.utente(a.viola));
        // Chiudendo il pacchetto il nome di GIALLI va nella sezione dei legati.
        const chiusura::Separazione sep = chiusura::separa(l);
        bool legato = false;
        for (const Voce& x : sep.contenuto.legati) legato = legato || (x.valore.is_string() && x.valore.get<std::string>() == "GIALLI PAOLO");
        EXPECT_TRUE(legato);
        for (const Voce& x : sep.contenuto.personali) EXPECT_FALSE(x.valore.is_string() && x.valore.get<std::string>() == "GIALLI PAOLO");
    }
    // La scheda dell'infanzia per disabili (il servizio della versione): ROSSI legata; VIOLA (altra scheda) no.
    PacchettoInMemoria t = m;
    JsonDati sv = JsonDati::parse(t.file.at("servizi.json"));
    for (JsonDati& x : sv.at("servizi"))
        if (x.at("codice") == "S-4000901-1") x["tipo"] = "disabili";
    t.file["servizi.json"] = sv.dump(2);
    {
        const Lettura l = lettura(t);
        const Index idx(l.pacchetto);
        const LegameSensibile ls(&idx);
        EXPECT_TRUE(ls.utente(a.rossi));
        EXPECT_FALSE(ls.utente(a.viola));
        EXPECT_FALSE(ls.utente(a.gialli));
    }
    // La scheda individualizzata (la casella della copia): VIOLA legata.
    PacchettoInMemoria u = m;
    JsonDati r = JsonDati::parse(u.file.at("schede_servizi.json"));
    for (JsonDati& x : r.at("schede"))
        if (x.at("codice") == "SCH-4000903-1") x.at("versioni").at(0).at("scheda")["individualizzato"] = true;
    u.file["schede_servizi.json"] = r.dump(2);
    {
        const Lettura l = lettura(u);
        const Index idx(l.pacchetto);
        const LegameSensibile ls(&idx);
        EXPECT_TRUE(ls.utente(a.viola));
        EXPECT_TRUE(ls.utente(a.gialli));
        EXPECT_FALSE(ls.utente(a.rossi));
    }
}

// --- Coerenza dei dati -----------------------------------------------------------------------------------------------------------

// K-027: i giorni non noti (D4: K-CTR-189), i punti incoerenti (K-CTR-188 varianti 0-2), la scheda che non c'e' (K-CTR-184 variante 3),
// l'elenco incoerente (K-CTR-140 varianti 5-10), lo stradario incoerente (K-CTR-190).
TEST(Punti, CoerenzaDeiDati) {
    PacchettoInMemoria m = con_elenco(pacchetto_k027(), [](ClientList& l, const Package& p) {
        // MORI: la scheda del ritorno per una tratta di andata (verso opposto).
        punto(utente(l, "MORI LIA"), "AM", "2026-09-21", {regola(p, "SCH-4000904-1", "F-PRADELLO")});
        // BIANCHI: una scheda che non c'e'.
        PointRule r = regola(p, "SCH-4000901-1", "F-PRADELLO");
        r.card_id = "scheda-che-non-c-e";
        punto(utente(l, "BIANCHI SOFIA"), "AM", "2026-09-21", {r});
        // NERI: senza gli orari (giorni non noti).
        utente(l, "NERI MARCO").times.clear();
        // BRUNI: due sblocchi della stessa tratta negli stessi giorni (anche se attiva).
        ListUser& b = utente(l, "BRUNI ELSA");
        b.unlocks.push_back(ListUserUnlock("AM", "mail 1", g("2026-10-01"), {}));
        b.unlocks.push_back(ListUserUnlock("AM", "mail 2", g("2026-10-02"), {}));
    });
    JsonDati st = JsonDati::parse(m.file.at("stradari.json"));
    JsonDati riga = st.at("stradari").at(0).at("righe").at(2);
    st.at("stradari").at(0).at("righe").push_back(riga);   // la stessa riga due volte
    m.file["stradari.json"] = st.dump(2);
    const Alunni a = alunni(m);
    const Lettura l = lettura(m);
    const std::vector<Problem> v = problemi_dei_punti(l);
    EXPECT_TRUE(problema(v, Check::PointsInconsistent, a.mori, 2));
    EXPECT_TRUE(problema(v, Check::PointsNotValid, a.bianchi, 3));
    EXPECT_TRUE(problema(v, Check::PointsDaysUnknown, a.neri, 0));
    EXPECT_TRUE(problema(v, Check::PointsMissing, a.neri, 2));
    EXPECT_TRUE(problema(v, Check::ListInvalid, a.bruni, 8));
    EXPECT_TRUE(problema(v, Check::StreetTableInvalid, "", 0));
    EXPECT_EQ(definizione_controllo(Check::StreetTableInvalid).livello, Livello::Errore);
}

