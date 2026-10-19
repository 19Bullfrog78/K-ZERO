// K-Zero - prova di scala: genera un pacchetto dati inventato e grande, sempre uguale (seme fisso).
// 10 aziende, 600 mezzi, 5.000 fermate, 200 scuole, 2.000 servizi con la loro domanda, 10.000 corse (K-007);
// dal K-008 anche 1.320 persone, 1.200 turni del giorno, una rotazione per azienda (lunghezze da 3 a 28 giorni e una a
// passi di un mese), un anno di festivita', assenze coperte da assegnazioni del singolo giorno.
// Ogni mezzo fa le sue corse una dopo l'altra (fasce di 55 minuti dalle 06:00): due turni al giorno per mezzo, il
// mattino e il pomeriggio. Le righe di ogni rotazione si danno il cambio sui turni (il riposo scorre).
// I dati sono coerenti: "kzero controlla" deve finire senza errori (e senza avvisi).
// Uso: genera_scala <cartella>   (la cartella viene creata; i file del pacchetto vengono sostituiti)
#include <algorithm>
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
constexpr int kCorsePerAzienda = kServizi / kAziende * kCorsePerServizio;
static_assert(kCorsePerAzienda == 1000, "la prova di scala ha 1.000 corse per azienda (DIARIO.md)");
constexpr int kFasciaMinuti = 55;                // ogni corsa sta in una fascia: dura al massimo 52 minuti
constexpr int kFascePerMattino = 8;              // fasce 0-7 il mattino, 8-16 il pomeriggio
constexpr int kTurniPerAzienda = 2 * kMezziPerAzienda;   // 120
constexpr int kRiposiPerGiorno = 12;             // righe della rotazione a riposo ogni giorno
constexpr int kPersonePerAzienda = kTurniPerAzienda + kRiposiPerGiorno;   // 132
constexpr int kAssenzePerAzienda = 10;
constexpr int kPausaDiMezzogiorno = 40;   // minuti tra le fasce del mattino e quelle del pomeriggio

// Inizio della fascia k: dalle 06:00 ogni 55 minuti, con la pausa di mezzogiorno prima della fascia 8. Il turno del
// mattino finisce (rientro e fine servizio compresi) prima che cominci quello del pomeriggio.
int inizio_fascia(int k) { return 6 * 60 + kFasciaMinuti * k + (k >= kFascePerMattino ? kPausaDiMezzogiorno : 0); }

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

// Giorni dal 1970-01-01 per una data del calendario gregoriano (algoritmo di H. Hinnant) e il contrario.
int giorno(int a, int m, int g) {
    a -= m <= 2;
    const int era = (a >= 0 ? a : a - 399) / 400;
    const int ya = a - era * 400;
    const int dy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + g - 1;
    const int de = ya * 365 + ya / 4 - ya / 100 + dy;
    return era * 146097 + de - 719468;
}
std::string data(int z) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const int doe = z - era * 146097;
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    const int g = doy - (153 * mp + 2) / 5 + 1;
    const int m = mp + (mp < 10 ? 3 : -9);
    return cifre(yoe + era * 400 + (m <= 2), 4) + "-" + cifre(m, 2) + "-" + cifre(g, 2);
}
int mese_di(int z) { return std::stoi(data(z).substr(0, 4)) * 12 + std::stoi(data(z).substr(5, 2)); }

const char* const kGiorniScuola[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi"};
const char* const kGradi[] = {"infanzia", "primaria", "secondaria_primo_grado", "secondaria_secondo_grado"};

std::string azienda(int a) { return "AZ-" + cifre(a + 1, 2); }
std::string persona(int a, int i) { return "P-" + cifre(a + 1, 2) + "-" + cifre(i + 1, 3); }
std::string mezzo(int a, int m) { return "V-" + cifre(a + 1, 2) + "-" + cifre(m + 1, 3); }
// Turni dell'azienda: 0-59 il mattino del mezzo, 60-119 il pomeriggio.
std::string turno(int a, int t) {
    return "T-" + cifre(a + 1, 2) + (t < kMezziPerAzienda ? "-M-" : "-P-") + cifre(t % kMezziPerAzienda + 1, 3);
}
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

        // Un anno di festivita' (inventate, sulle date delle feste italiane) e i calendari che le tolgono.
        Json festivita = documento("festivita");
        const std::vector<std::string> feste = {"2026-11-01", "2026-12-08", "2026-12-25", "2026-12-26", "2027-01-01", "2027-01-06",
                                                "2027-03-28", "2027-03-29", "2027-04-25", "2027-05-01", "2027-06-02", "2027-08-15"};
        Json date_feste = Json::array();
        for (const std::string& f : feste) date_feste.push_back({{"data", f}, {"nome", "Festa del " + f}});
        festivita["festivita"] = Json::array({{{"codice", "FEST-SC"},
                                               {"nome", "Festivita' della prova di scala"},
                                               {"paese", "IT"},
                                               {"fonte", {{"documento", "Calendario di prova (inventato)"}}},
                                               {"date", date_feste}}});
        scrivi(cartella, "festivita.json", festivita);

        Json calendari = documento("calendari");
        calendari["schema_versione"] = 2;
        calendari["calendari"] = Json::array({{
            {"codice", "CAL-SCUOLA"},
            {"nome", "Giorni di scuola"},
            {"dal", "2026-09-14"},
            {"al", "2027-06-12"},
            {"giorni", kGiorniScuola},
            {"toglie", {{"festivita", Json::array({"FEST-SC"})}}},
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
        std::vector<int> corse_azienda(kAziende, 0);
        // Corse di ogni mezzo, in ordine di fascia: codice, ora di fine, scuola dove finisce.
        struct CorsaDelMezzo {
            std::string codice, scuola;
            int fine = 0;
        };
        std::vector<std::vector<std::vector<CorsaDelMezzo>>> del_mezzo(
            kAziende, std::vector<std::vector<CorsaDelMezzo>>(kMezziPerAzienda));
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
                // La j-esima corsa dell'azienda va al mezzo j % 60, nella fascia j / 60.
                const int j = corse_azienda[static_cast<std::size_t>(a)]++;
                const int m = j % kMezziPerAzienda;
                int t = inizio_fascia(j / kMezziPerAzienda) + caso.tra(0, 4);
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
                const std::string codice_corsa = "C-" + cifre(++numero_corsa, 5);
                elenco_corse.push_back({{"codice", codice_corsa},
                                        {"servizi", Json::array({codice_servizio})},
                                        {"calendario", "CAL-SCUOLA"},
                                        {"passaggi", passaggi}});
                del_mezzo[static_cast<std::size_t>(a)][static_cast<std::size_t>(m)].push_back({codice_corsa, scuola, t});
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
                Json mezzo = {{"codice", ::mezzo(a, m)},
                              {"uso", "NCC"},
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
        mezzi["schema_versione"] = 2;
        scrivi(cartella, "mezzi.json", mezzi);
        scrivi(cartella, "rimesse.json", rimesse);

        Json tempi = documento("tempi_sosta");
        tempi["tempi"] = {{"fissa_per_fermata_secondi", 15},
                          {"per_passeggero", Json::array({{{"tipo", "studente"}, {"salita_secondi", 3}, {"discesa_secondi", 3}},
                                                          {{"tipo", "infanzia"}, {"salita_secondi", 8}, {"discesa_secondi", 8}},
                                                          {{"tipo", "disabile"}, {"salita_secondi", 30}, {"discesa_secondi", 30}}})},
                          {"scarico_a_scuola_secondi", 120}};
        scrivi(cartella, "tempi_sosta.json", tempi);

        // --- K-008: norme, aziende, persone, turni, rotazioni, assenze e sostituzioni -------------------------------
        Json norme = documento("norme");
        const Json fonte_norma = {{"documento", "Norme di prova (inventate)"}};
        norme["norme"] = Json::array({{
            {"codice", "N-SC"},
            {"paese", "IT"},
            {"nome", "Norme della prova di scala"},
            {"abilitazioni", Json::array({{{"codice", "B"}, {"nome", "Patente B"}, {"fonte", fonte_norma}},
                                         {{"codice", "D"}, {"nome", "Patente D"}, {"fonte", fonte_norma}},
                                         {{"codice", "CQC-P"}, {"nome", "Carta di qualificazione"}, {"fonte", fonte_norma}}})},
            {"usi", Json::array({{{"codice", "NCC"}, {"nome", "Noleggio con conducente"}, {"fonte", fonte_norma}}})},
            {"requisiti_guida", Json::array({{{"categorie", Json::array({"M2", "M3"})},
                                               {"alternative", Json::array({{{"abilitazioni", Json::array({"D", "CQC-P"})}}})},
                                               {"fonte", fonte_norma}}})},
        }});
        scrivi(cartella, "norme.json", norme);

        Json aziende = documento("aziende");
        Json& elenco_aziende = aziende["aziende"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            elenco_aziende.push_back(
                {{"azienda", azienda(a)},
                 {"norme", "N-SC"},
                 {"impostazioni", {{"ricovero_vicino_a_casa", "ammesso"}, {"abbinamento_autista_mezzo", "nessuno"}}},
                 {"attivita", Json::array({{{"codice", "PRESA"}, {"nome", "Presa servizio"}, {"usa_il_mezzo", true}},
                                           {{"codice", "VUOTO"}, {"nome", "Trasferimento a vuoto"}, {"usa_il_mezzo", true}},
                                           {{"codice", "FINE"}, {"nome", "Fine servizio"}, {"usa_il_mezzo", true}}})},
                 {"tipi_assenza", Json::array({{{"codice", "FERIE"}, {"nome", "Ferie"}}})}});
        scrivi(cartella, "aziende.json", aziende);

        Json persone = documento("persone");
        Json& elenco_persone = persone["persone"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            for (int i = 0; i < kPersonePerAzienda; ++i)
                elenco_persone.push_back({{"codice", persona(a, i)},
                                          {"azienda", azienda(a)},
                                          {"ruoli", Json::array({"autista"})},
                                          {"presa_servizio", "L-RIM-" + cifre(a + 1, 2)},
                                          {"rapporto", {{"tipo", "dipendente_tempo_pieno"}}},
                                          {"abilitazioni", Json::array({{{"tipo", "B"}}, {{"tipo", "D"}}, {{"tipo", "CQC-P"}}})}});
        scrivi(cartella, "persone.json", persone);

        // Turni: per ogni mezzo il mattino (fasce 0-7) e il pomeriggio (fasce 8-16), con presa servizio, corse,
        // rientro a vuoto e fine servizio.
        Json turni = documento("turni");
        Json& elenco_turni = turni["turni"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            for (int t = 0; t < kTurniPerAzienda; ++t) {
                const int m = t % kMezziPerAzienda;
                const bool mattino = t < kMezziPerAzienda;
                const std::string rimessa = "L-RIM-" + cifre(a + 1, 2);
                const auto& corse_mezzo = del_mezzo[static_cast<std::size_t>(a)][static_cast<std::size_t>(m)];
                const std::size_t da = mattino ? 0 : kFascePerMattino;
                const std::size_t a_ = mattino ? std::min<std::size_t>(kFascePerMattino, corse_mezzo.size()) : corse_mezzo.size();
                if (da >= a_) throw std::runtime_error("mezzo senza corse in un turno");
                const int presa = inizio_fascia(static_cast<int>(da)) - 15;
                Json elementi = Json::array();
                elementi.push_back({{"attivita", "PRESA"}, {"luogo", rimessa}, {"dalle", orario(presa)}, {"alle", orario(presa + 13)}});
                for (std::size_t k = da; k < a_; ++k) elementi.push_back({{"corsa", corse_mezzo[k].codice}});
                const CorsaDelMezzo& ultima = corse_mezzo[a_ - 1];
                elementi.push_back({{"attivita", "VUOTO"}, {"luogo", ultima.scuola}, {"verso", rimessa},
                                    {"dalle", orario(ultima.fine + 1)}, {"alle", orario(ultima.fine + 15)}, {"km_vuoti", 5.0}});
                elementi.push_back({{"attivita", "FINE"}, {"luogo", rimessa}, {"dalle", orario(ultima.fine + 15)},
                                    {"alle", orario(ultima.fine + 20)}});
                elenco_turni.push_back({{"codice", turno(a, t)},
                                        {"azienda", azienda(a)},
                                        {"ruolo", "autista"},
                                        {"calendario", "CAL-SCUOLA"},
                                        {"mezzo", {{"modo", "preciso"}, {"codice", ::mezzo(a, m)}}},
                                        {"inizio", rimessa},
                                        {"fine", rimessa},
                                        {"elementi", elementi}});
            }
        scrivi(cartella, "turni.json", turni);

        // Rotazioni: la riga i al passo k fa il turno (i + k * salto) mod 132, oppure riposa se il numero e' 120 o piu'
        // (salto = 132 / lunghezza: ogni riga lavora in quasi tutti i passi). Ogni giorno tutti i 120 turni hanno una
        // persona e 12 persone riposano. Lunghezze diverse per azienda; l'ultima a passi di un mese.
        const int lunghezze[kAziende] = {3, 5, 6, 7, 8, 10, 12, 14, 21, 4};
        const int partenza = giorno(2026, 8, 31);
        const auto passo_del_giorno = [&](int a, int d) {
            if (a == kAziende - 1) return (mese_di(d) - mese_di(giorno(2026, 9, 1))) % lunghezze[a];
            return ((d - partenza) % lunghezze[a] + lunghezze[a]) % lunghezze[a];
        };
        const auto turno_della_riga = [&](int a, int i, int k) {   // >= 120: riposo
            return (i + k * (kPersonePerAzienda / lunghezze[a])) % kPersonePerAzienda;
        };
        Json rotazioni = documento("rotazioni");
        Json& elenco_rotazioni = rotazioni["rotazioni"] = Json::array();
        for (int a = 0; a < kAziende; ++a) {
            Json righe = Json::array();
            for (int i = 0; i < kPersonePerAzienda; ++i) {
                Json passi = Json::array();
                for (int k = 0; k < lunghezze[a]; ++k) {
                    const int t = turno_della_riga(a, i, k);
                    if (t >= kTurniPerAzienda) passi.push_back("riposo");
                    else passi.push_back(Json::array({turno(a, t)}));
                }
                righe.push_back({{"persona", persona(a, i)}, {"passi", passi}});
            }
            const bool mensile = a == kAziende - 1;
            elenco_rotazioni.push_back({{"codice", "ROT-" + cifre(a + 1, 2)},
                                        {"passo", mensile ? "mese" : "giorno"},
                                        {"lunghezza", lunghezze[a]},
                                        {"partenza", mensile ? "2026-09-01" : "2026-08-31"},
                                        {"righe", righe}});
        }
        scrivi(cartella, "rotazioni.json", rotazioni);

        // Assenze di un giorno, ognuna coperta da un'assegnazione del singolo giorno a una persona che quel giorno
        // riposa (cosi' nessun turno resta scoperto e nessuno fa due turni).
        std::vector<int> giorni_di_scuola;
        for (int d = giorno(2026, 9, 14); d <= giorno(2027, 6, 11); ++d) {
            const int dow = ((d % 7) + 7 + 3) % 7;   // 0 = lunedi' (il 1970-01-01 era un giovedi')
            const bool festa = std::find(feste.begin(), feste.end(), data(d)) != feste.end();
            const bool natale = d >= giorno(2026, 12, 23) && d <= giorno(2027, 1, 6);
            if (dow < 5 && !festa && !natale) giorni_di_scuola.push_back(d);
        }
        Json assenze = documento("assenze");
        Json& elenco_assenze = assenze["assenze"] = Json::array();
        Json assegnazioni = documento("assegnazioni");
        Json& elenco_assegnazioni = assegnazioni["assegnazioni"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            for (int n = 0; n < kAssenzePerAzienda; ++n) {
                const int d = giorni_di_scuola[static_cast<std::size_t>(17 * n + 3 * a) % giorni_di_scuola.size()];
                const int k = passo_del_giorno(a, d);
                int assente = (13 * n + a) % kPersonePerAzienda;
                while (turno_della_riga(a, assente, k) >= kTurniPerAzienda) assente = (assente + 1) % kPersonePerAzienda;
                int sostituto = 0;
                while (turno_della_riga(a, sostituto, k) < kTurniPerAzienda) ++sostituto;
                elenco_assenze.push_back({{"persona", persona(a, assente)}, {"tipo", "FERIE"}, {"dal", data(d)}, {"al", data(d)}});
                elenco_assegnazioni.push_back(
                    {{"data", data(d)}, {"turno", turno(a, turno_della_riga(a, assente, k))}, {"persona", persona(a, sostituto)}});
            }
        scrivi(cartella, "assenze.json", assenze);
        scrivi(cartella, "assegnazioni.json", assegnazioni);

        std::cout << "Pacchetto di scala in " << cartella.string() << ": " << kAziende * kMezziPerAzienda << " mezzi, "
                  << kFermate << " fermate, " << kServizi << " servizi, " << numero_corsa << " corse, "
                  << kAziende * kPersonePerAzienda << " persone, " << kAziende * kTurniPerAzienda << " turni.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "genera_scala: " << e.what() << "\n";
        return 3;
    }
}
