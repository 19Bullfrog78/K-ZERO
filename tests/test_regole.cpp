// K-Zero - prove automatiche delle regole del mestiere (REGOLE.md) sui dati del pacchetto demo (esempi/demo).
// Ogni TEST viene trovato da solo da CTest (gtest_discover_tests): non va elencato altrove.
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "domain_models.hpp"
#include "pacchetto.hpp"
#include "pacchetto_demo.hpp"
#include "turni.hpp"

using namespace kzero;
using namespace kzero::prove;

namespace {

const Vehicle& minibus16() { return per_codice(demo().vehicles, "ALFA-01"); }    // 16 posti, senza pedana
const Vehicle& attrezzato() { return per_codice(demo().vehicles, "ALFA-03"); }   // 12 posti, 2 carrozzine, 3 posti persi l'una
const Vehicle& senza_bagagliaio() { return per_codice(demo().vehicles, "ALFA-02"); }  // 16 posti, ausili tra i sedili (2 posti)
const Vehicle& auto_() { return per_codice(demo().vehicles, "ALFA-04"); }        // 4 posti, bagagliaio per 1 ausilio

Load seduti(int n, bool assistente = false) { return {n, 0, 0, assistente}; }
Load con_carrozzine(int seduti, int carrozzine) { return {seduti, carrozzine, 0, false}; }
Load con_ausili(int seduti, int ausili) { return {seduti, 0, ausili, false}; }

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
    EXPECT_TRUE(fits(minibus16(), seduti(16)));          // 16 bambini da soli: ci stanno
    EXPECT_FALSE(fits(minibus16(), seduti(16, true)));   // 16 bambini + assistente: no
    EXPECT_TRUE(fits(minibus16(), seduti(15, true)));    // 15 bambini + assistente: si'
}

// Ogni carrozzina toglie posti a sedere (ALFA-03: 12 posti, 3 persi per carrozzina, al massimo 2).
TEST(Regole, carrozzina_toglie_posti) {
    EXPECT_TRUE(fits(attrezzato(), seduti(12)));
    EXPECT_FALSE(fits(attrezzato(), con_carrozzine(10, 1)));   // 12 - 3 = 9 posti
    EXPECT_TRUE(fits(attrezzato(), con_carrozzine(9, 1)));
    EXPECT_FALSE(fits(attrezzato(), con_carrozzine(7, 2)));    // 12 - 6 = 6 posti
    EXPECT_TRUE(fits(attrezzato(), con_carrozzine(6, 2)));
    EXPECT_FALSE(fits(attrezzato(), con_carrozzine(0, 3)));    // oltre le carrozzine ammesse
}

// Senza pedana nessuna carrozzina.
TEST(Regole, senza_pedana_nessuna_carrozzina) {
    EXPECT_FALSE(fits(minibus16(), con_carrozzine(0, 1)));
    Vehicle senza_pedana = attrezzato();
    senza_pedana.has_ramp = false;
    EXPECT_FALSE(fits(senza_pedana, con_carrozzine(0, 1)));
}

// Auto con la carrozzina piegata nel bagagliaio e il passeggero dietro: ammesso, nessun posto perso.
TEST(Regole, auto_carrozzina_piegata_nel_bagagliaio) {
    EXPECT_EQ(auto_().aids_in_trunk, 1);
    EXPECT_FALSE(auto_().has_ramp);
    const Fit f = fits(auto_(), con_ausili(1, 1));
    EXPECT_TRUE(f.admitted);
    EXPECT_EQ(f.aids_in_trunk, 1);
    EXPECT_EQ(f.aids_between_seats, 0);
    EXPECT_EQ(f.free_seats, 3);
    EXPECT_TRUE(fits(auto_(), con_ausili(4, 1)));    // auto piena: il bagagliaio non toglie posti
    EXPECT_FALSE(fits(auto_(), con_ausili(1, 2)));   // il secondo ausilio non ha posto: tra i sedili non e' ammesso
}

// Minibus senza bagagliaio: l'ausilio va tra i sedili, legato e bloccato, e occupa 2 posti se sono liberi.
TEST(Regole, minibus_senza_bagagliaio_ausilio_tra_i_sedili) {
    EXPECT_EQ(senza_bagagliaio().aids_in_trunk, 0);
    ASSERT_TRUE(senza_bagagliaio().seats_per_aid_between.has_value());
    EXPECT_EQ(*senza_bagagliaio().seats_per_aid_between, 2);
    const Fit f = fits(senza_bagagliaio(), con_ausili(10, 1));
    EXPECT_TRUE(f.admitted);
    EXPECT_EQ(f.aids_in_trunk, 0);
    EXPECT_EQ(f.aids_between_seats, 1);
    EXPECT_EQ(f.free_seats, 4);   // 16 - 2 - 10: due posti in meno
    EXPECT_TRUE(fits(senza_bagagliaio(), con_ausili(14, 1)));
    // Stesso minibus pieno: i posti per l'ausilio non ci sono, non e' ammesso.
    EXPECT_FALSE(fits(senza_bagagliaio(), con_ausili(16, 1)));
    EXPECT_FALSE(fits(senza_bagagliaio(), con_ausili(15, 1)));
    // Un mezzo che non ha bagagliaio e non ammette ausili tra i sedili non porta ausili piegati.
    EXPECT_FALSE(fits(minibus16(), con_ausili(0, 1)));
}

// Minibus con il bagagliaio: gli ausili vanno li' e non si perde nessun posto; prima il bagagliaio, poi tra i sedili.
TEST(Regole, minibus_con_bagagliaio_nessun_posto_perso) {
    Vehicle con_bagagliaio = minibus16();
    con_bagagliaio.aids_in_trunk = 2;
    const Fit f = fits(con_bagagliaio, con_ausili(16, 2));
    EXPECT_TRUE(f.admitted);
    EXPECT_EQ(f.aids_in_trunk, 2);
    EXPECT_EQ(f.free_seats, 0);
    EXPECT_FALSE(fits(con_bagagliaio, con_ausili(0, 3)));   // il terzo non entra e tra i sedili non e' ammesso

    Vehicle tutti_e_due = senza_bagagliaio();
    tutti_e_due.aids_in_trunk = 1;
    const Fit g = fits(tutti_e_due, con_ausili(10, 2));
    EXPECT_TRUE(g.admitted);
    EXPECT_EQ(g.aids_in_trunk, 1);
    EXPECT_EQ(g.aids_between_seats, 1);
    EXPECT_EQ(g.free_seats, 4);   // solo il secondo ausilio occupa posti
}

// La carrozzina piegata non chiede la pedana: su un mezzo senza pedana e' ammessa, la persona seduta sulla
// carrozzina no. Carrozzina, assistente e ausilio insieme si sommano.
TEST(Regole, carrozzina_piegata_senza_pedana) {
    Vehicle senza_pedana = minibus16();
    senza_pedana.aids_in_trunk = 1;
    EXPECT_FALSE(senza_pedana.has_ramp);
    EXPECT_TRUE(fits(senza_pedana, con_ausili(1, 1)));         // seduta su un posto, carrozzina piegata nel bagagliaio
    EXPECT_FALSE(fits(senza_pedana, con_carrozzine(0, 1)));    // seduta sulla carrozzina: serve la pedana

    Vehicle tutto = attrezzato();   // 12 posti, 3 persi per carrozzina
    tutto.seats_per_aid_between = 2;
    EXPECT_TRUE(fits(tutto, {6, 1, 1, true}));    // 12 - 3 - 1 (assistente) - 2 (ausilio) - 6 = 0
    EXPECT_FALSE(fits(tutto, {7, 1, 1, true}));
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

// Pause a casa (K-008): la regola (minuti e km) non e' ancora nei dati e non e' scritta nel codice; qui la scrive la
// prova (60 minuti, 5 km). Senza regola, o senza le coordinate di casa, la funzione non giudica.
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

// Turno del giorno fatto di corse gia' pronte (le corse devono restare in vita finche' si usa il turno).
ShiftDay turno_con(const std::vector<Trip>& corse) {
    ShiftDay sd;
    for (const Trip& t : corse) {
        PlannedItem it;
        it.trip = &t;
        it.from = 0;
        it.to = t.passages.size() - 1;
        it.start = t.start();
        it.end = t.end();
        sd.items.push_back(it);
    }
    return sd;
}

Person autista_di_valfonda() {
    Person p;
    p.home = Home{"", "", per_codice(demo().places, "L-MEDIE").coord};
    return p;
}

const HomeBreakRule kRegolaDiProva{60, 5.0, std::nullopt};   // scritta dalla prova, non dal programma

BreakReport pause(const std::vector<Trip>& corse) {
    const Index idx(demo());
    const auto r = check_home_breaks(turno_con(corse), autista_di_valfonda(), kRegolaDiProva, [&](const Code& c) { return idx.place(c); });
    return r ? *r : BreakReport{-1, -1, -1};
}

}  // namespace

TEST(Regole, pausa_breve_non_conta) {
    const BreakReport br = pause({tratto("L-MEDIE", ora("07:00"), "F-LAGOBELLO", ora("07:30")),
                                  tratto("F-LAGOBELLO", ora("08:29"), "L-MEDIE", ora("09:00"))});   // 59 minuti fermo a Lagobello
    EXPECT_EQ(br.near_home, 0);
    EXPECT_EQ(br.far_from_home, 0);
    EXPECT_EQ(br.unknown, 0);
}

TEST(Regole, pausa_lunga_lontano_segnalata) {
    const BreakReport br = pause({tratto("L-MEDIE", ora("07:00"), "F-LAGOBELLO", ora("07:30")),
                                  tratto("F-LAGOBELLO", ora("08:30"), "L-MEDIE", ora("09:00"))});   // 60 minuti fermo a Lagobello
    EXPECT_EQ(br.far_from_home, 1);
    EXPECT_EQ(br.near_home, 0);

    // Controprova: la stessa pausa a Valfonda (vicino a casa) non e' segnalata.
    const BreakReport bv = pause({tratto("F-LAGOBELLO", ora("07:00"), "L-MEDIE", ora("07:30")),
                                  tratto("L-MEDIE", ora("09:00"), "F-LAGOBELLO", ora("09:30"))});
    EXPECT_EQ(bv.far_from_home, 0);
    EXPECT_EQ(bv.near_home, 1);

    // Un luogo senza coordinate (F-CASTELROVO-ALTO): la pausa non si puo' giudicare.
    EXPECT_EQ(pause({tratto("L-MEDIE", ora("07:00"), "F-CASTELROVO-ALTO", ora("07:30")),
                     tratto("F-CASTELROVO-ALTO", ora("09:00"), "L-MEDIE", ora("09:30"))})
                  .unknown,
              1);
}

// Senza regola della pausa (non ancora nei dati) o senza le coordinate di casa la funzione non giudica: nessun valore
// di 5 km, 60 minuti o 10 km e' scritto nel codice.
TEST(Regole, pausa_senza_dati_non_giudica) {
    const std::vector<Trip> corse = {tratto("L-MEDIE", ora("07:00"), "F-LAGOBELLO", ora("07:30")),
                                     tratto("F-LAGOBELLO", ora("09:00"), "L-MEDIE", ora("09:30"))};
    const Index idx(demo());
    const auto luogo = [&](const Code& c) { return idx.place(c); };
    EXPECT_FALSE(check_home_breaks(turno_con(corse), autista_di_valfonda(), std::nullopt, luogo).has_value());
    EXPECT_FALSE(check_home_breaks(turno_con(corse), Person{}, kRegolaDiProva, luogo).has_value());
    EXPECT_FALSE(commute_km(Person{}, Coord{46.4, 11.0}).has_value());
    EXPECT_GT(*commute_km(autista_di_valfonda(), *per_codice(demo().places, "F-LAGOBELLO").coord), 10.0);
}
