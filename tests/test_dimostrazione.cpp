// K-Zero - prove del nucleo sul pacchetto dimostrativo esempi/demo (dati inventati).
// Controllano i risultati che il comando "kzero dimostrazione" mostra in testo e in JSON.
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dimostrazione.hpp"
#include "pacchetto.hpp"
#include "pacchetto_demo.hpp"
#include "turni.hpp"

using namespace kzero;
using namespace kzero::prove;

// La categoria di ogni corsa la ricava il nucleo: superiori + medie di due contratti = promiscuo,
// centro diurno = handicap (anche la corsa del giovedi' con la carrozzina piegata), solo utenti di linea = linea,
// la gita della Pro Loco (servizio NCC dal K-012, gruppi di tipo "altro") = NCC.
TEST(Dimostrazione, categorie_ricavate) {
    std::vector<std::pair<std::string, ServiceType>> attese = {
        {"C-AM-1", ServiceType::School},      {"C-AM-2", ServiceType::School},     {"C-AM-3", ServiceType::School},
        {"C-RM-1", ServiceType::Mixed},       {"C-VAL-3R", ServiceType::Disability}, {"C-VAL-3", ServiceType::Disability},
        {"C-ASILO-1", ServiceType::School},   {"C-LIN-533", ServiceType::Line},     {"C-LIN-560", ServiceType::Line},
        {"C-VAL-4", ServiceType::Disability}};
    for (const char* c : {"C-LIN-537", "C-LIN-545", "C-LIN-549", "C-LIN-553", "C-B620-01", "C-B620-02", "C-B620-03", "C-B620-04",
                          "C-B620-05", "C-B620-06", "C-B620-11", "C-B620-12", "C-B620-13", "C-B620-14"})
        attese.push_back({c, ServiceType::Line});
    attese.push_back({"C-GITA-A", ServiceType::Ncc});
    attese.push_back({"C-GITA-R", ServiceType::Ncc});
    // K-012: il matrimonio e il noleggio privato verso il centro diurno (un gruppo di disabili): servizi NCC.
    for (const char* c : {"C-MATRIMONIO-A", "C-MATRIMONIO-R", "C-PRIVATO"}) attese.push_back({c, ServiceType::Ncc});
    const Index idx(demo());
    ASSERT_EQ(demo().trips.size(), attese.size());
    for (std::size_t i = 0; i < attese.size(); ++i) {
        EXPECT_EQ(demo().trips[i].code, attese[i].first);
        EXPECT_EQ(idx.category(demo().trips[i]).value(), attese[i].second) << attese[i].first;
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
    EXPECT_NEAR(pieni, 393.2, 1e-9);   // K-012: 349,2 + 32 del matrimonio + 12 del noleggio privato
    EXPECT_NEAR(vuoti, 98.2, 1e-9);    // 63,7 + 19 + 15,5

    // Un turno con l'andata del mattino e il ritorno di mezzogiorno (K-008: il turno del giorno al posto della
    // vecchia "giornata del mezzo"): km dei dati e minuti fermi tra le due corse.
    Package p = demo();
    Shift s;
    s.code = "T-PROVA";
    s.company = Ref{"ALFA", {}};
    s.calendar = Ref{"CAL-SCUOLA-LV", {}};
    s.vehicle_mode = ShiftVehicleMode::Specific;
    s.vehicle = Ref{"ALFA-01", {}};
    for (const char* c : {"C-AM-1", "C-RM-1"}) {
        ShiftItem it;
        it.trip.trip = Ref{c, {}};
        s.items.push_back(it);
    }
    p.shifts.clear();   // il demo ha i suoi turni (K-008): qui basta questo
    p.shifts.push_back(s);
    const Planner piano(p);
    const auto giornata = piano.day(giorno("2026-10-15"));
    ASSERT_EQ(giornata.size(), 1u);
    EXPECT_NEAR(loaded_km(giornata[0]), 29.9, 1e-9);
    EXPECT_NEAR(empty_km(giornata[0]), 12.8, 1e-9);
    EXPECT_EQ(idle_minutes(giornata[0]), 263);
}

// Posti, carrozzine, costi e assistente, con i valori del pacchetto demo.
TEST(Dimostrazione, posti_costi_assistente) {
    const Dimostrazione d = calcola_dimostrazione(demo());

    // 38 ragazzi del servizio S-SCU-AM: non stanno sul mezzo piu' grande dell'azienda in flotta (35 posti, K-008).
    // La gita (30 persone all'andata e 30 al ritorno, K-008) e' di 30 persone: ci sta.
    ASSERT_EQ(d.giri.size(), 1u);
    EXPECT_EQ(d.giri[0].servizio, "S-SCU-AM");
    EXPECT_EQ(d.giri[0].persone, 38);
    ASSERT_EQ(d.giri[0].mezzi.size(), 8u);
    EXPECT_EQ(d.giri[0].mezzi[0].giri, 3);   // ALFA-01, 16 posti
    EXPECT_EQ(d.giri[0].mezzi[2].giri, 4);   // ALFA-03, 12 posti
    EXPECT_EQ(d.giri[0].mezzi[4].giri, 2);   // ALFA-X30, 30 posti (ipotetico)
    EXPECT_EQ(d.giri[0].mezzi[5].giri, 2);   // ALFA-05, 35 posti

    // ALFA-03: 12 posti, 3 persi per carrozzina, al massimo 2 carrozzine.
    ASSERT_EQ(d.carrozzine.size(), 1u);
    EXPECT_EQ(d.carrozzine[0].mezzo, "ALFA-03");
    ASSERT_EQ(d.carrozzine[0].casi.size(), 4u);
    EXPECT_EQ(d.carrozzine[0].casi[1].posti_a_sedere, 9);
    EXPECT_EQ(d.carrozzine[0].casi[2].posti_a_sedere, 6);
    EXPECT_TRUE(d.carrozzine[0].casi[2].ammesso);
    EXPECT_FALSE(d.carrozzine[0].casi[3].ammesso);

    // Ausili piegati. ALFA-02 (16 posti, nessun bagagliaio): tra i sedili, 2 posti per ausilio, solo se liberi.
    // ALFA-04 (auto, 4 posti): bagagliaio per 1 ausilio, tra i sedili no.
    ASSERT_EQ(d.ausili_piegati.size(), 2u);
    const AusiliMezzo& minibus = d.ausili_piegati[0];
    EXPECT_EQ(minibus.mezzo, "ALFA-02");
    EXPECT_EQ(minibus.bagagliaio, 0);
    EXPECT_EQ(minibus.posti_tra_i_sedili, 2);
    ASSERT_EQ(minibus.casi.size(), 3u);
    EXPECT_TRUE(minibus.casi[0].ammesso);              // 1 ausilio a mezzo vuoto
    EXPECT_EQ(minibus.casi[0].tra_i_sedili, 1);
    EXPECT_EQ(minibus.casi[0].posti_liberi, 14);
    EXPECT_EQ(minibus.casi[1].posti_liberi, 12);       // 2 ausili
    EXPECT_EQ(minibus.casi[2].seduti, 16);             // 1 ausilio a mezzo pieno: non ammesso
    EXPECT_FALSE(minibus.casi[2].ammesso);
    const AusiliMezzo& auto_ = d.ausili_piegati[1];
    EXPECT_EQ(auto_.mezzo, "ALFA-04");
    EXPECT_EQ(auto_.bagagliaio, 1);
    EXPECT_FALSE(auto_.posti_tra_i_sedili.has_value());
    ASSERT_EQ(auto_.casi.size(), 3u);
    EXPECT_TRUE(auto_.casi[0].ammesso);                // 1 ausilio: nel bagagliaio, 4 posti liberi
    EXPECT_EQ(auto_.casi[0].nel_bagagliaio, 1);
    EXPECT_EQ(auto_.casi[0].posti_liberi, 4);
    EXPECT_FALSE(auto_.casi[1].ammesso);               // 2 ausili: non ammesso
    EXPECT_TRUE(auto_.casi[2].ammesso);                // 1 ausilio con 4 seduti: nel bagagliaio
    EXPECT_EQ(auto_.casi[2].posti_liberi, 0);

    // La linea chiede un mezzo da almeno 30 posti: sono ammessi l'autobus di linea (K-008) e il mezzo ipotetico.
    // Sei corse da 15,1 km al giorno (K-008: quattro corse in piu').
    ASSERT_EQ(d.costi.size(), 2u);
    const CostiServizio& linea = d.costi[1];
    EXPECT_EQ(linea.servizio, "S-LIN-B610");
    EXPECT_EQ(linea.posti_minimi, 30);
    EXPECT_EQ(linea.posti_massimi, 54);
    EXPECT_NEAR(linea.km_al_giorno, 90.6, 1e-9);
    ASSERT_EQ(linea.mezzi.size(), 8u);
    EXPECT_FALSE(linea.mezzi[0].ammesso);
    EXPECT_NEAR(*linea.mezzi[0].costo, 94.83, 1e-9);    // 45 + 90,6 km x 0,55
    EXPECT_TRUE(linea.mezzi[4].ammesso);
    EXPECT_TRUE(linea.mezzi[4].ipotetico);
    EXPECT_NEAR(*linea.mezzi[4].costo, 176.07, 1e-9);   // 90 + 90,6 km x 0,95
    EXPECT_TRUE(linea.mezzi[5].ammesso);
    EXPECT_FALSE(linea.mezzi[5].ipotetico);
    EXPECT_NEAR(*linea.mezzi[5].costo, 190.13, 1e-9);   // 95 + 90,6 km x 1,05

    // Infanzia di Lagobello con un mezzo da 19 posti: il regolamento R-TN chiede l'assistente.
    ASSERT_EQ(d.assistente.size(), 1u);
    EXPECT_EQ(d.assistente[0].corsa, "C-ASILO-1");
    EXPECT_EQ(d.assistente[0].regolamento, "R-TN");
    ASSERT_EQ(d.assistente[0].mezzi.size(), 2u);   // K-008: anche l'autobus di linea di Beta
    EXPECT_EQ(d.assistente[0].mezzi[0].mezzo, "BETA-01");
    EXPECT_TRUE(d.assistente[0].mezzi[0].serve);
}
