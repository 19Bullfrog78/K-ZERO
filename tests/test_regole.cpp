// K-Zero - prove automatiche delle regole del mestiere (REGOLE.md) sui dati del pacchetto demo (esempi/demo).
// Ogni TEST viene trovato da solo da CTest (gtest_discover_tests): non va elencato altrove.
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "domain_models.hpp"
#include "pacchetto.hpp"
#include "pacchetto_demo.hpp"

using namespace kzero;
using namespace kzero::prove;

namespace {

const Vehicle& minibus16() { return per_codice(demo().vehicles, "ALFA-01"); }    // 16 posti, senza pedana
const Vehicle& attrezzato() { return per_codice(demo().vehicles, "ALFA-03"); }   // 12 posti, 2 carrozzine, 3 posti persi l'una

std::optional<ServiceType> categoria(const Package& p, const std::string& corsa) {
    return Index(p).category(per_codice(p.trips, corsa));
}

const ConditionsRule* regola_assistente(const std::string& regolamento) {
    const Regulation& r = per_codice(demo().regulations, regolamento);
    return r.assistant ? &*r.assistant : nullptr;
}

OnBoard a_bordo_grado(Grade g) {
    OnBoard b;
    b.grades = {g};
    return b;
}

}  // namespace

// La domanda del servizio S-SCU-AM: 38 ragazzi; su un 16 posti servono 3 giri (come le tre corse del demo).
TEST(Regole, giri_38_su_16) {
    int ragazzi = 0;
    for (const PassengerGroup& g : per_codice(demo().services, "S-SCU-AM").groups) ragazzi += g.total_boarding();
    EXPECT_EQ(ragazzi, 38);
    EXPECT_EQ(minibus16().seats, 16);
    EXPECT_EQ(trips_needed(minibus16(), ragazzi), 3);
    EXPECT_EQ(trips_needed(minibus16(), 16), 1);
    EXPECT_EQ(trips_needed(minibus16(), 17), 2);
}

// L'assistente occupa un posto.
TEST(Regole, assistente_occupa_un_posto) {
    EXPECT_TRUE(fits(minibus16(), 16, 0, false));    // 16 bambini da soli: ci stanno
    EXPECT_FALSE(fits(minibus16(), 16, 0, true));    // 16 bambini + assistente: no
    EXPECT_TRUE(fits(minibus16(), 15, 0, true));     // 15 bambini + assistente: si'
}

// Ogni carrozzina toglie posti a sedere (ALFA-03: 12 posti, 3 persi per carrozzina, al massimo 2).
TEST(Regole, carrozzina_toglie_posti) {
    EXPECT_TRUE(fits(attrezzato(), 12, 0));
    EXPECT_FALSE(fits(attrezzato(), 10, 1));   // 12 - 3 = 9 posti
    EXPECT_TRUE(fits(attrezzato(), 9, 1));
    EXPECT_FALSE(fits(attrezzato(), 7, 2));    // 12 - 6 = 6 posti
    EXPECT_TRUE(fits(attrezzato(), 6, 2));
    EXPECT_FALSE(fits(attrezzato(), 0, 3));    // oltre le carrozzine ammesse
}

// Senza pedana nessuna carrozzina.
TEST(Regole, senza_pedana_nessuna_carrozzina) {
    EXPECT_FALSE(fits(minibus16(), 0, 1));
    Vehicle senza_pedana = attrezzato();
    senza_pedana.has_ramp = false;
    EXPECT_FALSE(fits(senza_pedana, 0, 1));
}

// Gruppi di tipi diversi sulla stessa corsa = Ibrido. La corsa C-AM-2 porta due gruppi di studenti
// (medie e primaria): basta cambiare il tipo di uno dei due.
TEST(Regole, gruppi_diversi_ibrido) {
    EXPECT_EQ(categoria(demo(), "C-AM-2"), ServiceType::School);
    const auto con_tipi = [](GroupType medie, GroupType primaria) {
        Package p = demo();
        gruppo(p, "G-AM-MEDIE").type = medie;
        gruppo(p, "G-AM-PRIM").type = primaria;
        return categoria(p, "C-AM-2");
    };
    EXPECT_EQ(con_tipi(GroupType::Students, GroupType::Disabled), ServiceType::Hybrid);    // studenti + disabili
    EXPECT_EQ(con_tipi(GroupType::Students, GroupType::LineUsers), ServiceType::Hybrid);   // studenti + linea
    EXPECT_EQ(con_tipi(GroupType::Disabled, GroupType::LineUsers), ServiceType::Hybrid);   // disabili + linea
    EXPECT_EQ(con_tipi(GroupType::Disabled, GroupType::Disabled), ServiceType::Disability);
}

// Studenti verso un centro (non una scuola) = Ibrido.
TEST(Regole, studenti_centro_esterno_ibrido) {
    Package p = demo();
    gruppo(p, "G-AM-PRIM").school = Ref{"SC-CENTRO", {}};
    EXPECT_EQ(categoria(p, "C-AM-3"), ServiceType::Hybrid);
    EXPECT_EQ(categoria(demo(), "C-AM-3"), ServiceType::School);
}

// Solo studenti verso scuola = Scolastico.
TEST(Regole, solo_studenti_scolastico) {
    EXPECT_EQ(categoria(demo(), "C-AM-1"), ServiceType::School);
    EXPECT_EQ(categoria(demo(), "C-ASILO-1"), ServiceType::School);
}

// Solo disabili = Handicap (anche sulla corsa che consegna un trasbordo).
TEST(Regole, solo_handicap) {
    EXPECT_EQ(categoria(demo(), "C-DIS-1"), ServiceType::Disability);
    EXPECT_EQ(categoria(demo(), "C-DIS-R"), ServiceType::Disability);
}

// Solo utenti di linea = Linea. La corsa non nomina gruppi (conteggi non noti): vale il gruppo anonimo del servizio.
TEST(Regole, solo_linea) {
    EXPECT_EQ(categoria(demo(), "C-LIN-533"), ServiceType::Line);
    EXPECT_EQ(categoria(demo(), "C-LIN-560"), ServiceType::Line);
}

// Piu' servizi sulla stessa corsa = Ibrido: C-RM-1 (superiori del contratto delle linee + medie del lotto) e,
// come nell'esempio del mandato, utenti di linea + studenti sulla stessa corsa.
TEST(Regole, piu_servizi_ibrido) {
    EXPECT_EQ(categoria(demo(), "C-RM-1"), ServiceType::Hybrid);
    Package p = demo();
    per_codice(p.trips, "C-LIN-533").services.push_back(Ref{"S-SUP-RM", {}});
    EXPECT_EQ(categoria(p, "C-LIN-533"), ServiceType::Hybrid);
}

// Gli adulti autorizzati viaggiano con il servizio e non cambiano la categoria; senza gruppi vale il tipo dichiarato.
TEST(Regole, adulti_autorizzati_e_tipo_dichiarato) {
    Package p = demo();
    gruppo(p, "G-AM-PRIM").type = GroupType::AuthorizedAdults;
    EXPECT_EQ(categoria(p, "C-AM-2"), ServiceType::School);

    Package q = demo();
    Trip& t = per_codice(q.trips, "C-AM-1");
    for (Passage& x : t.passages) x.persons.clear();
    per_codice(q.services, "S-SCU-AM").groups.clear();
    EXPECT_EQ(categoria(q, "C-AM-1"), ServiceType::School);   // tipo dichiarato dal committente
    per_codice(q.services, "S-SCU-AM").kind = ServiceKind::Other;
    EXPECT_EQ(categoria(q, "C-AM-1"), ServiceType::Other);
    t.services.clear();
    EXPECT_EQ(categoria(q, "C-AM-1"), std::nullopt);          // ne' servizi ne' gruppi: non si sa
}

// Infanzia sui mezzi con piu' di 8 posti per i passeggeri = assistente obbligatorio (regolamento R-TN, 1.C1.9).
// Il valore sta nel regolamento: il programma non lo conosce.
TEST(Regole, infanzia_assistente_obbligatorio) {
    const ConditionsRule* tn = regola_assistente("R-TN");
    ASSERT_NE(tn, nullptr);
    EXPECT_TRUE(needs_assistant(tn, a_bordo_grado(Grade::Kindergarten), minibus16().seats));
    EXPECT_FALSE(needs_assistant(tn, a_bordo_grado(Grade::Kindergarten), per_codice(demo().vehicles, "ALFA-04").seats));
    EXPECT_FALSE(needs_assistant(tn, a_bordo_grado(Grade::LowerSecondary), minibus16().seats));   // medie: nessuna richiesta
    // Regolamento R-INV (valori inventati): assistente per l'infanzia con qualunque mezzo.
    EXPECT_TRUE(needs_assistant(regola_assistente("R-INV"), a_bordo_grado(Grade::Kindergarten),
                                per_codice(demo().vehicles, "ALFA-04").seats));
}

// Passeggero disabile che chiede l'assistente (P-D1) = assistente obbligatorio con ogni regolamento;
// un disabile che non lo chiede, da solo, no con R-TN e si' con R-INV (che lo chiede per tutti i disabili).
TEST(Regole, handicap_con_accompagnatore_assistente_obbligatorio) {
    const Passenger d1 = per_codice(gruppo(demo(), "G-DIS").passengers, "P-D1");
    const Passenger d3 = per_codice(gruppo(demo(), "G-DIS").passengers, "P-D3");
    ASSERT_TRUE(d1.needs.assistant);
    ASSERT_FALSE(d3.needs.assistant);
    OnBoard con_d1;
    con_d1.group_types = {GroupType::Disabled};
    con_d1.assistant_requested = d1.needs.assistant;
    OnBoard con_d3;
    con_d3.group_types = {GroupType::Disabled};
    con_d3.assistant_requested = d3.needs.assistant;
    const int posti = attrezzato().seats;
    EXPECT_TRUE(needs_assistant(regola_assistente("R-TN"), con_d1, posti));
    EXPECT_TRUE(needs_assistant(nullptr, con_d1, posti));
    EXPECT_FALSE(needs_assistant(regola_assistente("R-TN"), con_d3, posti));
    EXPECT_TRUE(needs_assistant(regola_assistente("R-INV"), con_d3, posti));
}

// Pause: sotto i 60 minuti non contano; quella lunga lontano da casa viene segnalata.
// Luoghi e coordinate del demo: L-MEDIE (Valfonda) e F-LAGOBELLO, a circa 6 km.
namespace {

Trip tratto(const std::string& da, Minutes partenza, const std::string& a, Minutes arrivo) {
    Trip t;
    t.passages.resize(2);
    t.passages[0].place.code = da;
    t.passages[0].time = partenza;
    t.passages[1].place.code = a;
    t.passages[1].time = arrivo;
    return t;
}

Driver autista_di_valfonda() {
    Driver d;
    d.home = *per_codice(demo().places, "L-MEDIE").coord;
    return d;   // pausa da casa: almeno 60 minuti, entro 5 km
}

BreakReport pause(const Schedule& s) {
    const Index idx(demo());
    return check_home_breaks(s, autista_di_valfonda(), [&](const Code& c) { return idx.place(c); });
}

}  // namespace

TEST(Regole, pausa_breve_non_conta) {
    Schedule s;
    s.routes = {tratto("L-MEDIE", ora("07:00"), "F-LAGOBELLO", ora("07:30")),
                tratto("F-LAGOBELLO", ora("08:29"), "L-MEDIE", ora("09:00"))};   // 59 minuti fermo a Lagobello
    const BreakReport br = pause(s);
    EXPECT_EQ(br.near_home, 0);
    EXPECT_EQ(br.far_from_home, 0);
    EXPECT_EQ(br.unknown, 0);
}

TEST(Regole, pausa_lunga_lontano_segnalata) {
    Schedule lontano;
    lontano.routes = {tratto("L-MEDIE", ora("07:00"), "F-LAGOBELLO", ora("07:30")),
                      tratto("F-LAGOBELLO", ora("08:30"), "L-MEDIE", ora("09:00"))};   // 60 minuti fermo a Lagobello
    const BreakReport br = pause(lontano);
    EXPECT_EQ(br.far_from_home, 1);
    EXPECT_EQ(br.near_home, 0);

    // Controprova: la stessa pausa a Valfonda (vicino a casa) non e' segnalata.
    Schedule vicino;
    vicino.routes = {tratto("F-LAGOBELLO", ora("07:00"), "L-MEDIE", ora("07:30")),
                     tratto("L-MEDIE", ora("09:00"), "F-LAGOBELLO", ora("09:30"))};
    const BreakReport bv = pause(vicino);
    EXPECT_EQ(bv.far_from_home, 0);
    EXPECT_EQ(bv.near_home, 1);

    // Un luogo senza coordinate (F-CASTELROVO-ALTO): la pausa non si puo' giudicare.
    Schedule senza;
    senza.routes = {tratto("L-MEDIE", ora("07:00"), "F-CASTELROVO-ALTO", ora("07:30")),
                    tratto("F-CASTELROVO-ALTO", ora("09:00"), "L-MEDIE", ora("09:30"))};
    EXPECT_EQ(pause(senza).unknown, 1);
}
