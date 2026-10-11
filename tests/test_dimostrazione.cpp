// K-Zero - prove del nucleo sullo scenario di dimostrazione (dati inventati).
// Controllano i risultati che il comando "kzero dimostrazione" mostra in testo e in JSON.
#include <gtest/gtest.h>

#include "dimostrazione.hpp"

using namespace kzero;

// La categoria di ogni corsa la ricava il nucleo: studenti + handicap = ibrido,
// solo utenti di linea = linea, studente verso il doposcuola = ibrido.
TEST(Dimostrazione, categorie_ricavate) {
    const Dimostrazione d = crea_dimostrazione();
    ASSERT_EQ(d.turno.routes.size(), 3u);
    EXPECT_EQ(d.turno.routes[0].service, ServiceType::Hybrid);
    EXPECT_EQ(d.turno.routes[1].service, ServiceType::Line);
    EXPECT_EQ(d.turno.routes[2].service, ServiceType::Hybrid);
}

// Totali del turno e pause: 20 minuti tra la corsa 1 e la 2 (pausa breve, non conta),
// 265 minuti tra la 2 e la 3, a Rumo, lontano da casa dell'autista (Cles).
TEST(Dimostrazione, totali_e_pause) {
    const Dimostrazione d = crea_dimostrazione();
    EXPECT_DOUBLE_EQ(d.turno.loaded_km(), 31.8);
    EXPECT_DOUBLE_EQ(d.turno.empty_km(), 15.8);
    EXPECT_EQ(d.turno.idle_minutes(), 285);
    EXPECT_EQ(d.pause.near_home, 0);
    EXPECT_EQ(d.pause.far_from_home, 1);
}

// Posti, carrozzine, costi e assistente.
TEST(Dimostrazione, posti_costi_assistente) {
    const Dimostrazione d = crea_dimostrazione();
    EXPECT_EQ(d.giri_necessari, 3);   // 38 ragazzi su 16 posti

    ASSERT_EQ(d.casi_carrozzine.size(), 4u);   // attrezzato: 12 posti, 3 persi per carrozzina, max 2
    EXPECT_EQ(d.casi_carrozzine[1].posti_a_sedere, 9);
    EXPECT_EQ(d.casi_carrozzine[2].posti_a_sedere, 6);
    EXPECT_TRUE(d.casi_carrozzine[2].ragazzi_ci_stanno);
    EXPECT_FALSE(d.casi_carrozzine[3].ammesso);
    EXPECT_FALSE(d.casi_carrozzine[3].ragazzi_ci_stanno);

    EXPECT_DOUBLE_EQ(d.confronto_costi.costo_flotta, 111.0);     // 45 + 120 km x 0,55
    EXPECT_DOUBLE_EQ(d.confronto_costi.costo_richiesto, 204.0);  // 90 + 120 km x 0,95
    EXPECT_DOUBLE_EQ(d.confronto_costi.differenza, 93.0);

    EXPECT_TRUE(d.assistente_infanzia);
    EXPECT_TRUE(d.assistente_corsa_handicap);
    EXPECT_FALSE(d.bambini_con_assistente_ci_stanno);   // l'assistente occupa un posto
    EXPECT_DOUBLE_EQ(d.costo_assistente_ditta, 27.0);   // 1,5 ore x 18 EUR
}
