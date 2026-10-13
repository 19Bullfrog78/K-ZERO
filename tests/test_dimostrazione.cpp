// K-Zero - prove del nucleo sul pacchetto dimostrativo esempi/demo (dati inventati).
// Controllano i risultati che il comando "kzero dimostrazione" mostra in testo e in JSON.
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dimostrazione.hpp"
#include "pacchetto.hpp"
#include "pacchetto_demo.hpp"

using namespace kzero;
using namespace kzero::prove;

// La categoria di ogni corsa la ricava il nucleo: superiori + medie di due contratti = ibrido,
// centro diurno = handicap, solo utenti di linea = linea.
TEST(Dimostrazione, categorie_ricavate) {
    const std::vector<std::pair<std::string, ServiceType>> attese = {
        {"C-AM-1", ServiceType::School},      {"C-AM-2", ServiceType::School},     {"C-AM-3", ServiceType::School},
        {"C-RM-1", ServiceType::Hybrid},      {"C-DIS-R", ServiceType::Disability}, {"C-DIS-1", ServiceType::Disability},
        {"C-ASILO-1", ServiceType::School},   {"C-LIN-533", ServiceType::Line},     {"C-LIN-560", ServiceType::Line}};
    const Index idx(demo());
    ASSERT_EQ(demo().trips.size(), attese.size());
    for (std::size_t i = 0; i < attese.size(); ++i) {
        EXPECT_EQ(demo().trips[i].code, attese[i].first);
        EXPECT_EQ(idx.category(demo().trips[i]), attese[i].second) << attese[i].first;
    }
}

// Totali e pause. I km a pieno e a vuoto sono quelli scritti sulle schede del committente (un dato, non un calcolo):
// nel demo i totali coincidono con la somma dei tratti. La giornata C-AM-1 + C-RM-1 ha 263 minuti fermi a Valfonda,
// vicino alla rimessa dove abita l'autista: pausa lunga vicino a casa.
TEST(Dimostrazione, totali_e_pause) {
    double pieni = 0, vuoti = 0;
    for (const Trip& t : demo().trips) {
        double tratti_pieni = 0, tratti_vuoti = 0;
        for (const Passage& p : t.passages) {
            tratti_pieni += p.loaded_km.value_or(0.0);
            tratti_vuoti += p.empty_km.value_or(0.0);
        }
        ASSERT_TRUE(t.total_loaded_km && t.total_empty_km) << t.code;
        EXPECT_NEAR(*t.total_loaded_km, tratti_pieni, 1e-9) << t.code;
        EXPECT_NEAR(*t.total_empty_km, tratti_vuoti, 1e-9) << t.code;
        pieni += t.loaded_km();
        vuoti += t.empty_km();
    }
    EXPECT_NEAR(pieni, 122.7, 1e-9);
    EXPECT_NEAR(vuoti, 55.3, 1e-9);

    Schedule giornata;
    giornata.routes = {per_codice(demo().trips, "C-AM-1"), per_codice(demo().trips, "C-RM-1")};
    EXPECT_NEAR(giornata.loaded_km(), 29.9, 1e-9);
    EXPECT_NEAR(giornata.empty_km(), 12.8, 1e-9);
    EXPECT_EQ(giornata.idle_minutes(), 263);
    Driver autista;
    autista.home = *per_codice(demo().places, "L-RIMESSA-ALFA").coord;
    const Index idx(demo());
    const BreakReport br = check_home_breaks(giornata, autista, [&](const Code& c) { return idx.place(c); });
    EXPECT_EQ(br.near_home, 1);
    EXPECT_EQ(br.far_from_home, 0);
}

// Posti, carrozzine, costi e assistente, con i valori del pacchetto demo.
TEST(Dimostrazione, posti_costi_assistente) {
    const Dimostrazione d = calcola_dimostrazione(demo());

    // 38 ragazzi del servizio S-SCU-AM: non stanno sul mezzo piu' grande in flotta (16 posti).
    ASSERT_EQ(d.giri.size(), 1u);
    EXPECT_EQ(d.giri[0].servizio, "S-SCU-AM");
    EXPECT_EQ(d.giri[0].persone, 38);
    ASSERT_EQ(d.giri[0].mezzi.size(), 5u);
    EXPECT_EQ(d.giri[0].mezzi[0].giri, 3);   // ALFA-01, 16 posti
    EXPECT_EQ(d.giri[0].mezzi[2].giri, 4);   // ALFA-03, 12 posti
    EXPECT_EQ(d.giri[0].mezzi[4].giri, 2);   // ALFA-X30, 30 posti (ipotetico)

    // ALFA-03: 12 posti, 3 persi per carrozzina, al massimo 2 carrozzine.
    ASSERT_EQ(d.carrozzine.size(), 1u);
    EXPECT_EQ(d.carrozzine[0].mezzo, "ALFA-03");
    ASSERT_EQ(d.carrozzine[0].casi.size(), 4u);
    EXPECT_EQ(d.carrozzine[0].casi[1].posti_a_sedere, 9);
    EXPECT_EQ(d.carrozzine[0].casi[2].posti_a_sedere, 6);
    EXPECT_TRUE(d.carrozzine[0].casi[2].ammesso);
    EXPECT_FALSE(d.carrozzine[0].casi[3].ammesso);

    // La linea chiede un mezzo da almeno 30 posti: solo il mezzo ipotetico e' ammesso.
    ASSERT_EQ(d.costi.size(), 2u);
    const CostiServizio& linea = d.costi[1];
    EXPECT_EQ(linea.servizio, "S-LIN-B610");
    EXPECT_EQ(linea.posti_minimi, 30);
    EXPECT_EQ(linea.posti_massimi, 54);
    EXPECT_NEAR(linea.km_al_giorno, 30.2, 1e-9);
    ASSERT_EQ(linea.mezzi.size(), 5u);
    EXPECT_FALSE(linea.mezzi[0].ammesso);
    EXPECT_NEAR(*linea.mezzi[0].costo, 61.61, 1e-9);    // 45 + 30,2 km x 0,55
    EXPECT_TRUE(linea.mezzi[4].ammesso);
    EXPECT_TRUE(linea.mezzi[4].ipotetico);
    EXPECT_NEAR(*linea.mezzi[4].costo, 118.69, 1e-9);   // 90 + 30,2 km x 0,95

    // Infanzia di Lagobello con un mezzo da 19 posti: il regolamento R-TN chiede l'assistente.
    ASSERT_EQ(d.assistente.size(), 1u);
    EXPECT_EQ(d.assistente[0].corsa, "C-ASILO-1");
    EXPECT_EQ(d.assistente[0].regolamento, "R-TN");
    ASSERT_EQ(d.assistente[0].mezzi.size(), 1u);
    EXPECT_EQ(d.assistente[0].mezzi[0].mezzo, "BETA-01");
    EXPECT_TRUE(d.assistente[0].mezzi[0].serve);
}
