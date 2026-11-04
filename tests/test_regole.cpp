// K-Zero - prove automatiche delle regole del mestiere (REGOLE.md) sui dati del pacchetto demo (esempi/demo).
// Ogni TEST viene trovato da solo da CTest (gtest_discover_tests): non va elencato altrove.
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "domain_models.hpp"
#include "pacchetto.hpp"
#include "pacchetto_demo.hpp"
#include "orari.hpp"
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

// Gruppi di tipi diversi sulla stessa corsa = Promiscuo (fino al K-011 "ibrido"). La corsa C-AM-2 porta due gruppi di studenti
// (medie e primaria): basta cambiare il tipo di uno dei due.
TEST(Regole, gruppi_diversi_promiscuo) {
    EXPECT_EQ(categoria(demo(), "C-AM-2"), ServiceType::School);
    const auto con_tipi = [](GroupType medie, GroupType primaria) {
        Package p = demo();
        gruppo(p, "G-AM-MEDIE").type = medie;
        gruppo(p, "G-AM-PRIM").type = primaria;
        return categoria(p, "C-AM-2");
    };
    EXPECT_EQ(con_tipi(GroupType::Students, GroupType::Disabled), ServiceType::Mixed);    // studenti + disabili
    EXPECT_EQ(con_tipi(GroupType::Students, GroupType::LineUsers), ServiceType::Mixed);   // studenti + linea
    EXPECT_EQ(con_tipi(GroupType::Disabled, GroupType::LineUsers), ServiceType::Mixed);   // disabili + linea
    EXPECT_EQ(con_tipi(GroupType::Disabled, GroupType::Disabled), ServiceType::Disability);
}

// Studenti verso un centro (non una scuola) = Promiscuo.
TEST(Regole, studenti_centro_esterno_promiscuo) {
    Package p = demo();
    gruppo(p, "G-AM-PRIM").school = Ref{"SC-CENTRO", {}};
    EXPECT_EQ(categoria(p, "C-AM-3"), ServiceType::Mixed);
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

// Piu' servizi sulla stessa corsa = Promiscuo: C-RM-1 (superiori del contratto delle linee + medie del lotto) e,
// come nell'esempio del mandato, utenti di linea + studenti sulla stessa corsa.
TEST(Regole, piu_servizi_promiscuo) {
    EXPECT_EQ(categoria(demo(), "C-RM-1"), ServiceType::Mixed);
    Package p = demo();
    per_codice(p.trips, "C-LIN-533").services.push_back(Ref{"S-SUP-RM", {}});
    EXPECT_EQ(categoria(p, "C-LIN-533"), ServiceType::Mixed);
}

// K-012: per un servizio NCC i gruppi non cambiano la categoria. La gita della Pro Loco (gruppi "altro") e' NCC; una gita
// scolastica (studenti verso una scuola, anche un gruppo di disabili, anche verso un centro) resta NCC: chi sale non cambia
// il tipo. Gli altri tipi come prima: gli stessi gruppi su un servizio scolastico danno scolastico o promiscuo.
TEST(Regole, ncc_i_gruppi_non_cambiano_la_categoria) {
    EXPECT_EQ(categoria(demo(), "C-GITA-A"), ServiceType::Ncc);
    EXPECT_EQ(categoria(demo(), "C-GITA-R"), ServiceType::Ncc);
    Package p = demo();
    per_codice(p.services, "S-SCU-AM").kind = ServiceKind::Ncc;   // gita scolastica: medie e primaria
    EXPECT_EQ(categoria(p, "C-AM-2"), ServiceType::Ncc);
    gruppo(p, "G-AM-PRIM").type = GroupType::Disabled;
    EXPECT_EQ(categoria(p, "C-AM-2"), ServiceType::Ncc);
    gruppo(p, "G-AM-PRIM").school = Ref{"SC-CENTRO", {}};
    EXPECT_EQ(categoria(p, "C-AM-2"), ServiceType::Ncc);
    per_codice(p.services, "S-SCU-AM").kind = ServiceKind::School;   // lo stesso servizio scolastico: promiscuo
    EXPECT_EQ(categoria(p, "C-AM-2"), ServiceType::Mixed);
    Package q = demo();
    per_codice(q.services, "S-SCU-AM").kind = ServiceKind::Ncc;
    EXPECT_EQ(categoria(q, "C-AM-1"), ServiceType::Ncc);   // studenti, un solo gruppo
}

// K-012: piu' servizi sulla stessa corsa = Promiscuo anche se uno e' NCC. Se la corsa non elenca i servizi (errore K-CTR-034)
// valgono i servizi dei suoi gruppi: di un solo servizio NCC = NCC, di piu' servizi con un NCC = Promiscuo.
TEST(Regole, ncc_con_altri_servizi_promiscuo) {
    Package p = demo();
    per_codice(p.trips, "C-GITA-A").services.push_back(Ref{"S-SUP-RM", {}});
    EXPECT_EQ(categoria(p, "C-GITA-A"), ServiceType::Mixed);
    Package q = demo();
    per_codice(q.trips, "C-GITA-A").services.clear();
    EXPECT_EQ(categoria(q, "C-GITA-A"), ServiceType::Ncc);   // i gruppi sono tutti di S-GITA
    Trip& t = per_codice(q.trips, "C-GITA-A");
    t.passages[0].persons.push_back(PassageCount{Ref{"G-RM-SUP", {}}, 2, 0, {}});
    EXPECT_EQ(categoria(q, "C-GITA-A"), ServiceType::Mixed);   // anche un gruppo di S-SUP-RM
    Package r = demo();
    Trip& u = per_codice(r.trips, "C-AM-2");
    u.services.clear();   // gruppi di un solo servizio scolastico: come prima, dai tipi dei gruppi
    EXPECT_EQ(categoria(r, "C-AM-2"), ServiceType::School);
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
    per_codice(q.services, "S-SCU-AM").kind = ServiceKind::Ncc;
    EXPECT_EQ(categoria(q, "C-AM-1"), ServiceType::Ncc);
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

// Rientro (K-011): la pausa fuori dal posto (in rimessa o in un ricovero) e' giustificata con la chiave dell'azienda o della
// persona (Mauro, 20/10): fermo minimo, fattore della guida a vuoto in piu', km a vuoto in piu' al massimo. Nessun valore nel
// codice: la chiave la scrive la prova, come la scriverebbe l'azienda nei dati (es. 30 minuti e fattore 2 per Alfa).
namespace {

ReturnJudgment rientro(int fermo, int guida_trasferimenti, bool stesso_luogo, int fermo_minimo, double fattore,
                       std::optional<double> km_massimi = std::nullopt, double km = 0.0, bool km_noti = true) {
    ReturnJudgment r;
    r.stop = fermo;
    r.transfers_driving = guida_trasferimenti;
    r.same_place = stesso_luogo;
    r.min_stop = fermo_minimo;
    r.factor = fattore;
    r.max_km = km_massimi;
    r.transfers_km = km;
    r.km_known = km_noti;
    judge_return(r);
    return r;
}

}  // namespace

// Esempio a di Mauro: 20 minuti per andare e 20 per tornare, 30 minuti fermo: servono 2 x 40 = 80 minuti: non conviene.
TEST(Regole, rientro_esempio_a_non_giustificato) {
    const ReturnJudgment r = rientro(30, 40, true, 30, 2.0);
    EXPECT_EQ(r.outcome, ReturnJudgment::Outcome::NotJustified);
    EXPECT_EQ(r.why, ReturnJudgment::Why::StopBelowFactor);
}

// Esempio b: arrivo 19:05 e ripartenza 19:50 dallo stesso posto, 15 minuti (20 km) per andare e 15 per tornare: fermo 15
// minuti, sotto il fermo minimo di 30.
TEST(Regole, rientro_esempio_b_sotto_il_fermo_minimo) {
    const ReturnJudgment r = rientro(15, 30, true, 30, 2.0, std::nullopt, 40.0);
    EXPECT_EQ(r.outcome, ReturnJudgment::Outcome::NotJustified);
    EXPECT_EQ(r.why, ReturnJudgment::Why::StopBelowMin);
}

// Esempio c: 9:08 -> 12:00 dallo stesso posto, 18 minuti (12 km) per andare e 18 per tornare: fermo 2 ore e 16 minuti
// contro 2 x 36 = 72 minuti: giustificato.
TEST(Regole, rientro_esempio_c_giustificato) {
    const ReturnJudgment r = rientro(136, 36, true, 30, 2.0, std::nullopt, 24.0);
    EXPECT_EQ(r.outcome, ReturnJudgment::Outcome::Justified);
    EXPECT_EQ(r.why, ReturnJudgment::Why::None);
    // Al limite: 72 minuti bastano, 71 no.
    EXPECT_EQ(rientro(72, 36, true, 30, 2.0).outcome, ReturnJudgment::Outcome::Justified);
    EXPECT_EQ(rientro(71, 36, true, 30, 2.0).outcome, ReturnJudgment::Outcome::NotJustified);
}

// Km a vuoto in piu' oltre il massimo (facoltativo): non giustificato; senza i km dei trasferimenti non si giudica.
TEST(Regole, rientro_km_massimi) {
    EXPECT_EQ(rientro(136, 36, true, 30, 2.0, 20.0, 24.0).why, ReturnJudgment::Why::KmOverMax);
    EXPECT_EQ(rientro(136, 36, true, 30, 2.0, 30.0, 24.0).outcome, ReturnJudgment::Outcome::Justified);
    const ReturnJudgment senza_km = rientro(136, 36, true, 30, 2.0, 30.0, 0.0, false);
    EXPECT_EQ(senza_km.outcome, ReturnJudgment::Outcome::NotJudged);
    EXPECT_EQ(senza_km.why, ReturnJudgment::Why::NeedKm);
}

// Senza le distanze (il luogo dopo e' un altro: la guida diretta non si conosce) il programma giudica solo quando e' certo:
// giustificato se basta anche contando tutti i trasferimenti come guida in piu'; non giustificato solo per il fermo minimo;
// altrimenti non giudicato.
TEST(Regole, rientro_senza_distanze_giudica_solo_se_certo) {
    EXPECT_EQ(rientro(136, 36, false, 30, 2.0).outcome, ReturnJudgment::Outcome::Justified);
    EXPECT_EQ(rientro(20, 36, false, 30, 2.0).why, ReturnJudgment::Why::StopBelowMin);
    const ReturnJudgment r = rientro(60, 36, false, 30, 2.0);
    EXPECT_EQ(r.outcome, ReturnJudgment::Outcome::NotJudged);
    EXPECT_EQ(r.why, ReturnJudgment::Why::NeedDistances);
}

// Tragitto casa -> ricovero: senza coordinate nessun valore.
TEST(Regole, tragitto_casa_senza_coordinate) {
    EXPECT_FALSE(commute_km(Person{}, Coord{46.4, 11.0}).has_value());
    Person p;
    p.home = Home{"", "", per_codice(demo().places, "L-MEDIE").coord};
    EXPECT_GT(*commute_km(p, *per_codice(demo().places, "F-LAGOBELLO").coord), 10.0);
}
