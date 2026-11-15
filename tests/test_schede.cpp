// K-Zero - prove delle schede dei servizi (K-021): lettura dei tre fogli con il profilo, totali, accompagnatore, km scritti solo su
// alcune righe, case porta a porta, cadenze, scelta della data, versioni, togliere una scheda, controlli sul registro. I comandi e le
// uscite: test_schede_comandi.cpp. Dati inventati (tests/esempi/k021-schede).
#include <gtest/gtest.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "controlli_catalogo.hpp"
#include "excel.hpp"
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
