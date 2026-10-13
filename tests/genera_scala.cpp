// K-Zero - prova di scala: genera un pacchetto dati inventato e grande, sempre uguale (seme fisso).
// 10 aziende, 600 mezzi, 5.000 fermate, 200 scuole, 2.000 servizi con la loro domanda, 10.000 corse.
// I dati sono coerenti: "kzero controlla" deve finire senza errori (e senza avvisi).
// Uso: genera_scala <cartella>   (la cartella viene creata; i file del pacchetto vengono sostituiti)
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

using Json = nlohmann::ordered_json;

namespace {

constexpr int kAziende = 10;
constexpr int kMezziPerAzienda = 60;    // 600 mezzi
constexpr int kComuni = 250;
constexpr int kFermatePerComune = 20;   // 5.000 fermate
constexpr int kScuole = 200;
constexpr int kServizi = 2000;
constexpr int kCorsePerServizio = 5;    // 10.000 corse
constexpr int kFermatePerCorsa = 4;
constexpr int kFermate = kComuni * kFermatePerComune;

// Numeri pseudo-casuali con seme fisso (splitmix64): gli stessi su ogni macchina e con ogni compilatore.
class Caso {
public:
    explicit Caso(std::uint64_t seme) : stato_(seme) {}
    std::uint64_t prossimo() {
        std::uint64_t z = (stato_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    // Intero tra minimo e massimo compresi.
    int tra(int minimo, int massimo) {
        return minimo + static_cast<int>(prossimo() % static_cast<std::uint64_t>(massimo - minimo + 1));
    }

private:
    std::uint64_t stato_;
};

std::string cifre(int n, std::size_t quante) {
    std::string s = std::to_string(n);
    while (s.size() < quante) s = "0" + s;
    return s;
}

std::string orario(int minuti) { return cifre(minuti / 60, 2) + ":" + cifre(minuti % 60, 2); }

double arrotonda(double x, int decimali) {
    const double f = std::pow(10.0, decimali);
    return std::round(x * f) / f;
}

Json documento(const char* schema) {
    Json j = Json::object();
    j["schema"] = schema;
    j["schema_versione"] = 1;
    return j;
}

Json fonte(const char* punto) { return Json{{"punto", punto}}; }

const char* const kGiorniScuola[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi"};
const char* const kGradi[] = {"infanzia", "primaria", "secondaria_primo_grado", "secondaria_secondo_grado"};

std::string azienda(int a) { return "AZ-" + cifre(a + 1, 2); }
std::string fermata(int f) { return "F-" + cifre(f + 1, 5); }
std::string comune(int c) { return "Comune " + cifre(c + 1, 3); }

void scrivi(const std::filesystem::path& cartella, const std::string& nome, const Json& j) {
    std::ofstream f(cartella / nome, std::ios::binary);
    f << j.dump(1) << "\n";
    f.close();
    if (!f) throw std::runtime_error("non riesco a scrivere " + (cartella / nome).string());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Uso: genera_scala <cartella>\n";
        return 2;
    }
    try {
        const std::filesystem::path cartella(argv[1]);
        std::filesystem::create_directories(cartella);
        Caso caso(20261013);

        // Manifesto.
        Json pacchetto = documento("pacchetto");
        pacchetto["codice"] = "SCALA-1";
        pacchetto["nome"] = "Prova di scala (dati inventati)";
        pacchetto["dati_inventati"] = true;
        pacchetto["aziende"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            pacchetto["aziende"].push_back({{"codice", azienda(a)}, {"nome", "Azienda di prova " + cifre(a + 1, 2)}});
        pacchetto["periodo"] = {{"dal", "2026-09-01"}, {"al", "2027-08-31"}};
        pacchetto["origine"] = {{"tipo", "scenario"}, {"nota", "Generato da tests/genera_scala.cpp con un seme fisso."}};
        pacchetto["livello"] = "solo_numeri";
        pacchetto["fuso_orario"] = "Europe/Rome";
        pacchetto["valuta"] = "EUR";
        pacchetto["impostazioni"] = {{"margine_trasbordo_minuti", 3}};
        scrivi(cartella, "pacchetto.json", pacchetto);

        // Luoghi: rimesse, fermate (20 per comune), scuole.
        Json luoghi = documento("luoghi");
        Json& elenco_luoghi = luoghi["luoghi"] = Json::array();
        const auto coordinate = [](int c, double dx, double dy) {
            return Json{{"lat", arrotonda(45.5 + (c / 25) * 0.05 + dy, 5)}, {"lon", arrotonda(10.5 + (c % 25) * 0.05 + dx, 5)}};
        };
        for (int a = 0; a < kAziende; ++a)
            elenco_luoghi.push_back({{"codice", "L-RIM-" + cifre(a + 1, 2)},
                                     {"tipo", "rimessa"},
                                     {"nome", comune(a * 25) + ", rimessa " + cifre(a + 1, 2)},
                                     {"comune", comune(a * 25)},
                                     {"coordinate", coordinate(a * 25, 0.0, 0.0)}});
        for (int f = 0; f < kFermate; ++f) {
            const int c = f / kFermatePerComune;
            elenco_luoghi.push_back({{"codice", fermata(f)},
                                     {"tipo", "fermata"},
                                     {"nome", comune(c) + ", fermata " + cifre(f % kFermatePerComune + 1, 2)},
                                     {"comune", comune(c)},
                                     {"coordinate", coordinate(c, caso.tra(0, 400) / 10000.0, caso.tra(0, 400) / 10000.0)},
                                     {"codici_esterni", Json::array({{{"sistema", "Rete di prova"}, {"tipo", "fermata"},
                                                                      {"codice", std::to_string(10000 + f)}}})}});
        }
        for (int s = 0; s < kScuole; ++s) {
            const int c = s * kComuni / kScuole;
            elenco_luoghi.push_back({{"codice", "L-SC-" + cifre(s + 1, 3)},
                                     {"tipo", "scuola"},
                                     {"nome", comune(c) + ", scuola " + cifre(s + 1, 3)},
                                     {"comune", comune(c)},
                                     {"coordinate", coordinate(c, 0.02, 0.02)}});
        }
        scrivi(cartella, "luoghi.json", luoghi);

        // Regolamento, calendario, scuole, committenti, contratti.
        Json regolamenti = documento("regolamenti");
        regolamenti["regolamenti"] = Json::array({{
            {"codice", "R-SCALA"},
            {"ente", "Ente di prova (inventato)"},
            {"titolo", "Regolamento della prova di scala"},
            {"documento", "Regolamento di prova (inventato)"},
            {"valido_dal", "2026-01-01"},
            {"parametri",
             {{"attesa_massima_prima_minuti", {{"valore", 25}, {"fonte", fonte("1")}}},
              {"attesa_massima_dopo_minuti", {{"valore", 20}, {"fonte", fonte("1")}}},
              {"durata_massima_a_bordo_minuti",
               {{"valori", Json::array({{{"gradi", Json::array({kGradi[0], kGradi[1], kGradi[2], kGradi[3]})}, {"valore", 90}}})},
                {"fonte", fonte("2")}}}}},
        }});
        scrivi(cartella, "regolamenti.json", regolamenti);

        Json calendari = documento("calendari");
        calendari["calendari"] = Json::array({{
            {"codice", "CAL-SCUOLA"},
            {"nome", "Giorni di scuola"},
            {"dal", "2026-09-14"},
            {"al", "2027-06-12"},
            {"giorni", kGiorniScuola},
            {"date_tolte", Json::array({"2026-12-08", "2027-06-02"})},
            {"sospensioni", Json::array({{{"dal", "2026-12-23"}, {"al", "2027-01-06"}, {"motivo", "Vacanze di Natale"}}})},
        }});
        scrivi(cartella, "calendari.json", calendari);

        Json scuole = documento("scuole");
        Json& elenco_scuole = scuole["scuole"] = Json::array();
        for (int s = 0; s < kScuole; ++s)
            elenco_scuole.push_back(
                {{"codice", "SC-" + cifre(s + 1, 3)},
                 {"nome", "Scuola di prova " + cifre(s + 1, 3)},
                 {"luogo", "L-SC-" + cifre(s + 1, 3)},
                 {"tipo", "scuola"},
                 {"grado", kGradi[s % 4]},
                 {"orari", Json::array({{{"dal", "2026-09-14"},
                                         {"al", "2027-06-12"},
                                         {"giorni", kGiorniScuola},
                                         {"inizio", "08:30"},
                                         {"fine", "13:00"},
                                         {"attesa_concordata",
                                          {{"prima_minuti", 20}, {"dopo_minuti", 15}, {"accordo", "Accordo n. " + std::to_string(s + 1)}}}}})}});
        scrivi(cartella, "scuole.json", scuole);

        Json committenti = documento("committenti");
        committenti["committenti"] = Json::array({{{"codice", "CM-1"}, {"nome", "Committente di prova 1"}},
                                                  {{"codice", "CM-2"}, {"nome", "Committente di prova 2"}}});
        scrivi(cartella, "committenti.json", committenti);

        Json contratti = documento("contratti");
        Json& elenco_contratti = contratti["contratti"] = Json::array();
        Json gruppi_servizi = documento("gruppi_servizi");
        Json& lotti = gruppi_servizi["gruppi_servizi"] = Json::array();
        for (int a = 0; a < kAziende; ++a) {
            elenco_contratti.push_back({{"codice", "CT-" + cifre(a + 1, 2)},
                                        {"committente", a % 2 == 0 ? "CM-1" : "CM-2"},
                                        {"azienda", azienda(a)},
                                        {"regolamento", "R-SCALA"},
                                        {"valido_dal", "2026-09-01"},
                                        {"valido_al", "2027-08-31"},
                                        {"assistente_fornito_da", "committente"}});
            lotti.push_back({{"codice", "LOTTO-" + cifre(a + 1, 2)}, {"contratto", "CT-" + cifre(a + 1, 2)}, {"servizi", Json::array()}});
        }
        scrivi(cartella, "contratti.json", contratti);

        // Servizi con la domanda e le loro corse: la domanda di ogni fermata e' la somma delle sue corse.
        Json servizi = documento("servizi");
        Json& elenco_servizi = servizi["servizi"] = Json::array();
        Json corse = documento("corse");
        Json& elenco_corse = corse["corse"] = Json::array();
        int numero_corsa = 0;
        for (int i = 0; i < kServizi; ++i) {
            const int a = i % kAziende;
            const int s = i % kScuole;
            const std::string codice_servizio = "S-" + cifre(i + 1, 4);
            const std::string codice_gruppo = "G-" + cifre(i + 1, 4);
            const std::string scuola = "L-SC-" + cifre(s + 1, 3);
            std::vector<int> salgono;
            int totale = 0;
            Json fermate = Json::array();
            for (int k = 0; k < kFermatePerCorsa; ++k) {
                salgono.push_back(caso.tra(1, 3));
                totale += salgono.back();
                fermate.push_back({{"luogo", fermata((kFermatePerCorsa * i + k) % kFermate)},
                                   {"salgono", salgono.back() * kCorsePerServizio}});
            }
            fermate.push_back({{"luogo", scuola}, {"scendono", totale * kCorsePerServizio}});
            elenco_servizi.push_back(
                {{"codice", codice_servizio},
                 {"nome", "Servizio di prova " + cifre(i + 1, 4)},
                 {"tipo", "scolastico"},
                 {"contratto", "CT-" + cifre(a + 1, 2)},
                 {"calendario", "CAL-SCUOLA"},
                 {"liberta_oraria", {{"tipo", "libera"}, {"soglia_da_concordare_minuti", 10}}},
                 {"domanda",
                  {{"gruppi", Json::array({{{"codice", codice_gruppo},
                                            {"tipo", "studenti"},
                                            {"verso", "andata"},
                                            {"scuola", "SC-" + cifre(s + 1, 3)},
                                            {"finestra", {{"dalle", "07:30"}, {"alle", "08:30"}}},
                                            {"conteggi", "esatti"},
                                            {"fermate", fermate}}})}}}});
            lotti[static_cast<std::size_t>(a)]["servizi"].push_back(codice_servizio);
            for (int g = 0; g < kCorsePerServizio; ++g) {
                int t = 6 * 60 + 20 * g + caso.tra(0, 9);
                Json passaggi = Json::array();
                passaggi.push_back({{"luogo", "L-RIM-" + cifre(a + 1, 2)}, {"orario", orario(t)}});
                for (int k = 0; k < kFermatePerCorsa; ++k) {
                    t += caso.tra(4, 9);
                    passaggi.push_back({{"luogo", fermata((kFermatePerCorsa * i + k) % kFermate)},
                                        {"orario", orario(t)},
                                        {"persone", Json::array({{{"gruppo", codice_gruppo}, {"salgono", salgono[static_cast<std::size_t>(k)]}}})},
                                        {"km", {{k == 0 ? "vuoti" : "pieni", caso.tra(10, 99) / 10.0}}}});
                }
                t += caso.tra(5, 12);
                passaggi.push_back({{"luogo", scuola},
                                    {"orario", orario(t)},
                                    {"persone", Json::array({{{"gruppo", codice_gruppo}, {"scendono", totale}}})},
                                    {"km", {{"pieni", caso.tra(10, 99) / 10.0}}}});
                elenco_corse.push_back({{"codice", "C-" + cifre(++numero_corsa, 5)},
                                        {"servizi", Json::array({codice_servizio})},
                                        {"calendario", "CAL-SCUOLA"},
                                        {"passaggi", passaggi}});
            }
        }
        scrivi(cartella, "gruppi_servizi.json", gruppi_servizi);
        scrivi(cartella, "servizi.json", servizi);
        scrivi(cartella, "corse.json", corse);

        // Mezzi e rimesse.
        Json mezzi = documento("mezzi");
        Json& elenco_mezzi = mezzi["mezzi"] = Json::array();
        Json rimesse = documento("rimesse");
        Json& elenco_rimesse = rimesse["rimesse"] = Json::array();
        const int posti[] = {16, 19, 22, 30, 54};
        for (int a = 0; a < kAziende; ++a) {
            for (int m = 0; m < kMezziPerAzienda; ++m) {
                const int p = posti[m % 5];
                Json mezzo = {{"codice", "V-" + cifre(a + 1, 2) + "-" + cifre(m + 1, 3)},
                              {"azienda", azienda(a)},
                              {"targa", "PR " + cifre(a * kMezziPerAzienda + m + 1, 3) + " SC"},
                              {"categoria", p <= 22 ? "M2" : "M3"},
                              {"posti", p}};
                if (m % 10 == 9)
                    mezzo["alimentazione"] = {{"tipo", "elettrico"}, {"batteria_kwh", 300}, {"consumo_kwh_km", 1.1},
                                              {"carica_minima_percento", 20}};
                else
                    mezzo["alimentazione"] = {{"tipo", "gasolio"}};
                mezzo["costo_fisso_giornaliero"] = 40.0 + caso.tra(0, 50);
                mezzo["costo_al_km"] = caso.tra(50, 100) / 100.0;
                elenco_mezzi.push_back(mezzo);
            }
            elenco_rimesse.push_back({{"codice", "RIM-" + cifre(a + 1, 2)},
                                      {"luogo", "L-RIM-" + cifre(a + 1, 2)},
                                      {"azienda", azienda(a)},
                                      {"ricarica", true},
                                      {"posti_mezzi", kMezziPerAzienda}});
        }
        scrivi(cartella, "mezzi.json", mezzi);
        scrivi(cartella, "rimesse.json", rimesse);

        Json tempi = documento("tempi_sosta");
        tempi["tempi"] = {{"fissa_per_fermata_secondi", 15},
                          {"per_passeggero", Json::array({{{"tipo", "studente"}, {"salita_secondi", 3}, {"discesa_secondi", 3}},
                                                          {{"tipo", "infanzia"}, {"salita_secondi", 8}, {"discesa_secondi", 8}},
                                                          {{"tipo", "disabile"}, {"salita_secondi", 30}, {"discesa_secondi", 30}}})},
                          {"scarico_a_scuola_secondi", 120}};
        scrivi(cartella, "tempi_sosta.json", tempi);

        std::cout << "Pacchetto di scala in " << cartella.string() << ": " << kAziende * kMezziPerAzienda << " mezzi, "
                  << kFermate << " fermate, " << kServizi << " servizi, " << numero_corsa << " corse.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "genera_scala: " << e.what() << "\n";
        return 3;
    }
}
