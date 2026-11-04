// K-Zero - prova di scala: genera un pacchetto dati inventato e grande, sempre uguale (seme fisso).
// 10 aziende, 600 mezzi (piu' un mezzo di scorta per azienda), 5.000 fermate, 200 scuole, 20 centri diurni, circa 2.000
// servizi con la loro domanda e 10.000 corse (K-007); dal K-008 anche 1.320 persone, 1.200 turni del giorno, una rotazione per
// azienda (lunghezze da 3 a 28 giorni e una a passi di un mese), un anno di festivita', assenze coperte da assegnazioni del
// singolo giorno.
// Dal K-012 la prova e' mista, come un'azienda vera: in ogni azienda 4 autobus di linea (linee fino a 50 km e linee oltre i 50
// km, che fanno valere il Reg. 561/2006), 2 auto NCC da 8 posti (noleggi insieme a piccoli servizi scolastici), 4 minibus
// attrezzati (servizi per disabili verso i centri diurni, luoghi sensibili, e corse promiscue scolastico + disabili), 4
// pullman NCC (561 certo) e 46 mezzi dello scolastico. Le aziende scelgono il modo delle corse per tipo di servizio (OUT o
// tachigrafo) e per le linee lunghe il tachigrafo per servizio; alcune assenze sono coperte da persone in prestito da
// un'altra azienda e un mezzo in revisione dal mezzo di scorta prestato da un'altra azienda.
// Ogni mezzo fa le sue corse una dopo l'altra (fasce di 40 minuti dalle 06:00; una linea lunga occupa due fasce): due turni al
// giorno per mezzo, il mattino e il pomeriggio, ognuno con una pausa di almeno 45 minuti a meta'. Le righe di ogni rotazione
// si danno il cambio sui turni della loro meta' della giornata (il riposo scorre): chi fa il pomeriggio non fa il mattino dopo.
// Dal K-011 norme, aziende e persone sono alle versioni nuove: le leggi sugli orari sono quelle del pacchetto dimostrativo
// (incorporate nel programma, cosi' la prova misura le regole vere), con le categorie dei tempi e le scelte delle aziende.
// I dati sono coerenti: "kzero controlla" deve finire senza errori; gli avvisi che restano sono spiegati in tests/CMakeLists.txt.
// Uso: genera_scala <cartella>   (la cartella viene creata; i file del pacchetto vengono sostituiti)
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>

#include "risorse.hpp"

using Json = nlohmann::ordered_json;

namespace {

constexpr int kAziende = 10;
constexpr int kMezziPerAzienda = 60;    // 600 mezzi
constexpr int kComuni = 250;
constexpr int kFermatePerComune = 20;   // 5.000 fermate
constexpr int kScuole = 200;
constexpr int kCentri = 20;              // centri diurni (luoghi sensibili), K-012
constexpr int kCorsePerServizio = 5;
constexpr int kFermatePerCorsa = 4;
constexpr int kFermate = kComuni * kFermatePerComune;
constexpr int kFasce = 17;                       // fasce 0-16 in ogni giornata del mezzo
constexpr int kFasciaMinuti = 40;                // ogni corsa sta in una fascia: dura al massimo 36 minuti (una linea lunga, due fasce: 76)
constexpr int kFascePerMattino = 8;              // fasce 0-7 il mattino, 8-16 il pomeriggio
constexpr int kTurniPerAzienda = 2 * kMezziPerAzienda;   // 120
constexpr int kRiposiPerGiorno = 12;             // righe della rotazione a riposo ogni giorno
constexpr int kPersonePerAzienda = kTurniPerAzienda + kRiposiPerGiorno;   // 132
constexpr int kAssenzePerAzienda = 10;
constexpr int kPausaDiMezzogiorno = 40;   // minuti tra le fasce del mattino e quelle del pomeriggio
constexpr int kPausaNelTurno = 45;        // minuti in piu' prima delle fasce 4 e 12, a meta' di ogni turno
constexpr int kMetaDelleRighe = kPersonePerAzienda / 2;   // 66 righe per il mattino, 66 per il pomeriggio
static_assert(kMetaDelleRighe > kMezziPerAzienda, "in ogni meta' della giornata qualcuno riposa");

// Inizio della fascia k: dalle 06:00 ogni 40 minuti, con la pausa di mezzogiorno prima della fascia 8 e quella a meta'
// turno prima delle fasce 4 e 12. Il turno del mattino finisce (rientro e fine servizio compresi) prima che cominci
// quello del pomeriggio. Con corse di al massimo 36 minuti ogni turno resta sotto le 6 ore e 30 di lavoro e le 4 ore e 30
// di guida senza pausa, e la pausa a meta' turno dura almeno 49 minuti: le leggi sugli orari dei dati sono rispettate
// anche nella lettura prudente.
int inizio_fascia(int k) {
    return 6 * 60 + kFasciaMinuti * k + (k >= kFascePerMattino ? kPausaDiMezzogiorno : 0) + (k >= 4 ? kPausaNelTurno : 0) +
           (k >= 12 ? kPausaNelTurno : 0);
}

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

Json documento(const char* schema, int versione = 1) {
    Json j = Json::object();
    j["schema"] = schema;
    j["schema_versione"] = versione;
    return j;
}

// Le leggi sugli orari e i cambi dell'ora del pacchetto dimostrativo, incorporato nel programma.
Json norme_della_dimostrazione() {
    const auto testo = kzero::api::risorse::trova("esempi/demo/norme.json");
    if (!testo) throw std::runtime_error("manca esempi/demo/norme.json tra i file incorporati");
    const Json demo = Json::parse(testo->begin(), testo->end());
    return demo.at("norme").at(0);
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
        // K-012: i centri diurni dei servizi per disabili, luoghi sensibili (luoghi versione 2).
        for (int c = 0; c < kCentri; ++c) {
            const int k = c * kComuni / kCentri + 5;
            elenco_luoghi.push_back({{"codice", "L-CEN-" + cifre(c + 1, 2)},
                                     {"tipo", "centro"},
                                     {"nome", comune(k) + ", centro diurno " + cifre(c + 1, 2)},
                                     {"comune", comune(k)},
                                     {"coordinate", coordinate(k, 0.01, 0.03)},
                                     {"sensibile", true}});
        }
        luoghi["schema_versione"] = 2;
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
        for (int c = 0; c < kCentri; ++c)
            elenco_scuole.push_back({{"codice", "SC-CEN-" + cifre(c + 1, 2)},
                                     {"nome", "Centro diurno di prova " + cifre(c + 1, 2)},
                                     {"luogo", "L-CEN-" + cifre(c + 1, 2)},
                                     {"tipo", "centro"},
                                     {"tipo_centro", {{"valore", "diurno"}}},
                                     {"orari", Json::array({{{"dal", "2026-09-14"},
                                                             {"al", "2027-06-12"},
                                                             {"giorni", kGiorniScuola},
                                                             {"inizio", "08:30"},
                                                             {"fine", "16:00"}}})},
                                     {"durata_massima_a_bordo_minuti", 60}});
        scrivi(cartella, "scuole.json", scuole);

        Json committenti = documento("committenti");
        committenti["committenti"] = Json::array({{{"codice", "CM-1"}, {"nome", "Committente di prova 1"}},
                                                  {{"codice", "CM-2"}, {"nome", "Committente di prova 2"}},
                                                  {{"codice", "CM-AGENZIA"}, {"nome", "Agenzia di viaggi di prova"}}});
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
            // K-012: i noleggi con conducente per un'agenzia, con un contratto a parte (fuori dal lotto).
            elenco_contratti.push_back({{"codice", "CT-NCC-" + cifre(a + 1, 2)},
                                        {"committente", "CM-AGENZIA"},
                                        {"azienda", azienda(a)},
                                        {"regolamento", "R-SCALA"},
                                        {"valido_dal", "2026-09-01"},
                                        {"valido_al", "2027-08-31"},
                                        {"assistente_fornito_da", "committente"}});
            lotti.push_back({{"codice", "LOTTO-" + cifre(a + 1, 2)}, {"contratto", "CT-" + cifre(a + 1, 2)}, {"servizi", Json::array()}});
        }
        scrivi(cartella, "contratti.json", contratti);

        // Servizi con la domanda e le loro corse (K-012: tipi di servizio diversi, secondo il mezzo che li fa). In ogni azienda le
        // fasce dei mezzi sono i posti delle corse: un servizio ha 5 corse in 5 posti dello stesso tipo di mezzo (di solito la
        // stessa fascia di 5 mezzi diversi). Sulle auto e sui minibus attrezzati i tipi si alternano lungo la giornata, cosi' ogni
        // turno ne ha piu' d'uno. La domanda di ogni fermata e' la somma delle sue corse.
        enum class TipoMezzo { Linea, Auto, Attrezzato, Pullman, Scuola };
        const auto tipo_mezzo = [](int m) {
            return m < 4    ? TipoMezzo::Linea
                   : m < 6  ? TipoMezzo::Auto
                   : m < 10 ? TipoMezzo::Attrezzato
                   : m < 14 ? TipoMezzo::Pullman
                            : TipoMezzo::Scuola;
        };
        struct Posto {
            int m = 0, fascia = 0;
            bool lunga = false;   // linea oltre i 50 km: due fasce
        };
        enum class Tipo { Scuola, ScuolaPiccola, Linea, LineaLunga, NccPullman, NccAuto, Disabili, Promiscuo };
        Json servizi = documento("servizi", 3);
        Json& elenco_servizi = servizi["servizi"] = Json::array();
        Json corse = documento("corse");
        Json& elenco_corse = corse["corse"] = Json::array();
        int numero_corsa = 0, numero_servizio = 0, servizi_scritti = 0, corse_promiscue = 0, linee_lunghe_scritte = 0;
        // Corse di ogni mezzo: codice, fascia, ora di fine, luogo dove finisce.
        struct CorsaDelMezzo {
            std::string codice, arrivo;
            int fascia = 0, fine = 0;
        };
        std::vector<std::vector<std::vector<CorsaDelMezzo>>> del_mezzo(
            kAziende, std::vector<std::vector<CorsaDelMezzo>>(kMezziPerAzienda));
        std::vector<std::vector<std::string>> servizi_lunghi(kAziende);   // linee oltre i 50 km: il tachigrafo per servizio
        for (int a = 0; a < kAziende; ++a) {
            std::vector<Posto> linee_lunghe, linee, auto_ncc, attrezzati, pullman, scolastici;
            for (int f = 0; f < kFasce; ++f)
                for (int m = 0; m < kMezziPerAzienda; ++m) switch (tipo_mezzo(m)) {
                        case TipoMezzo::Linea:
                            // Le linee lunghe nelle fasce 0-1, 2-3, 4-5, 8-9 e 12-13 (mai a cavallo di una pausa); le altre
                            // fasce, linee corte.
                            if (f == 0 || f == 2 || f == 4 || f == 8 || f == 12) linee_lunghe.push_back({m, f, true});
                            else if (!(f == 1 || f == 3 || f == 5 || f == 9 || f == 13)) linee.push_back({m, f, false});
                            break;
                        case TipoMezzo::Auto: auto_ncc.push_back({m, f, false}); break;
                        case TipoMezzo::Attrezzato: attrezzati.push_back({m, f, false}); break;
                        case TipoMezzo::Pullman: pullman.push_back({m, f, false}); break;
                        case TipoMezzo::Scuola: scolastici.push_back({m, f, false}); break;
                    }
            std::vector<std::pair<Tipo, std::vector<Posto>>> piano;
            const auto a_blocchi = [&](Tipo t, const std::vector<Posto>& v) {
                for (std::size_t k = 0; k + kCorsePerServizio <= v.size(); k += kCorsePerServizio)
                    piano.push_back({t, std::vector<Posto>(v.begin() + static_cast<std::ptrdiff_t>(k),
                                                           v.begin() + static_cast<std::ptrdiff_t>(k + kCorsePerServizio))});
            };
            // Tipi che si alternano lungo la giornata di ogni mezzo: la fascia f del mezzo m e' del tipo tipi[(f + m) % n]; poi i
            // posti di ogni tipo, a blocchi di 5, fanno i servizi (cosi' ogni turno ha piu' tipi, e ogni servizio piu' mezzi).
            const auto alternati = [&](const std::vector<Tipo>& tipi, const std::vector<Posto>& v) {
                const std::size_t n = tipi.size();
                std::vector<std::vector<Posto>> per_tipo(n);
                for (const Posto& q : v) per_tipo[static_cast<std::size_t>(q.fascia + q.m) % n].push_back(q);
                std::vector<Posto> unici;
                std::vector<std::pair<Tipo, std::vector<Posto>>> raccolti;
                for (std::size_t k = 0; k < n; ++k) {
                    // Tipi ripetuti nell'elenco (per le proporzioni) mettono insieme i loro posti.
                    auto it = std::find_if(raccolti.begin(), raccolti.end(), [&](const auto& x) { return x.first == tipi[k]; });
                    if (it == raccolti.end()) raccolti.push_back({tipi[k], per_tipo[k]});
                    else it->second.insert(it->second.end(), per_tipo[k].begin(), per_tipo[k].end());
                }
                for (auto& [t, posti_tipo] : raccolti) {
                    std::sort(posti_tipo.begin(), posti_tipo.end(),
                              [](const Posto& x, const Posto& y) { return std::tie(x.fascia, x.m) < std::tie(y.fascia, y.m); });
                    a_blocchi(t, posti_tipo);
                }
            };
            a_blocchi(Tipo::LineaLunga, linee_lunghe);   // 20 corse: 4 linee oltre i 50 km
            a_blocchi(Tipo::Linea, linee);                // 5 linee corte
            alternati({Tipo::NccAuto, Tipo::ScuolaPiccola}, auto_ncc);
            alternati({Tipo::Disabili, Tipo::Disabili, Tipo::Disabili, Tipo::Disabili, Tipo::Disabili, Tipo::Disabili, Tipo::Promiscuo,
                       Tipo::Promiscuo, Tipo::Promiscuo, Tipo::Promiscuo, Tipo::Promiscuo, Tipo::Promiscuo, Tipo::Promiscuo},
                      attrezzati);
            a_blocchi(Tipo::NccPullman, pullman);
            a_blocchi(Tipo::Scuola, scolastici);

            for (const auto& [tipo, posti] : piano) {
                const int i = numero_servizio++;
                const bool lunga = tipo == Tipo::LineaLunga, piccola = tipo == Tipo::NccAuto || tipo == Tipo::ScuolaPiccola;
                const bool promiscuo = tipo == Tipo::Promiscuo;
                const bool a_scuola = tipo == Tipo::Scuola || tipo == Tipo::ScuolaPiccola || promiscuo;
                const std::string scuola = "SC-" + cifre(i % kScuole + 1, 3), centro = "SC-CEN-" + cifre(i % kCentri + 1, 2);
                const std::string arrivo = a_scuola                ? "L-SC-" + cifre(i % kScuole + 1, 3)
                                           : tipo == Tipo::Disabili ? "L-CEN-" + cifre(i % kCentri + 1, 2)
                                                                    : fermata((kFermatePerCorsa * i + 2500) % kFermate);
                // I gruppi: uno per servizio; nella corsa promiscua gli studenti salgono alle prime due fermate e i disabili
                // (verso la stessa scuola) alle altre due, con due servizi sulla stessa corsa.
                struct Gruppo {
                    std::string servizio, codice;
                    int da = 0, a = kFermatePerCorsa;   // fermate di salita [da, a)
                    std::vector<int> salgono;
                    int totale = 0;
                };
                std::vector<Gruppo> gruppi;
                const std::string codice_servizio = "S-" + cifre(i + 1, 4);
                gruppi.push_back({codice_servizio, "G-" + cifre(i + 1, 4), 0, promiscuo ? 2 : kFermatePerCorsa, {}, 0});
                if (promiscuo) gruppi.push_back({codice_servizio + "-D", "G-" + cifre(i + 1, 4) + "-D", 2, kFermatePerCorsa, {}, 0});
                for (Gruppo& g : gruppi)
                    for (int k = g.da; k < g.a; ++k) {
                        const bool disabile = tipo == Tipo::Disabili || g.servizio != codice_servizio;
                        g.salgono.push_back(disabile ? 1 : piccola ? caso.tra(1, 2) : caso.tra(1, 3));
                        g.totale += g.salgono.back();
                    }
                for (std::size_t n = 0; n < gruppi.size(); ++n) {
                    const Gruppo& g = gruppi[n];
                    const bool disabili = tipo == Tipo::Disabili || n == 1;
                    const bool linea = tipo == Tipo::Linea || tipo == Tipo::LineaLunga;
                    const bool ncc = tipo == Tipo::NccAuto || tipo == Tipo::NccPullman;
                    Json fermate = Json::array();
                    for (int k = g.da; k < g.a; ++k)
                        fermate.push_back({{"luogo", fermata((kFermatePerCorsa * i + k) % kFermate)},
                                           {"salgono", g.salgono[static_cast<std::size_t>(k - g.da)] * kCorsePerServizio}});
                    fermate.push_back({{"luogo", arrivo}, {"scendono", g.totale * kCorsePerServizio}});
                    Json gruppo = {{"codice", g.codice}};
                    if (disabili) gruppo["tipo"] = "disabili";
                    else if (linea) gruppo["tipo"] = "utenti_linea";
                    else if (ncc) gruppo.update({{"tipo", "altro"}, {"tipo_descrizione", "Clienti del noleggio"}});
                    else gruppo["tipo"] = "studenti";
                    gruppo["verso"] = "andata";
                    if (a_scuola) gruppo["scuola"] = scuola;
                    if (tipo == Tipo::Disabili) gruppo["scuola"] = centro;
                    if (a_scuola || tipo == Tipo::Disabili) gruppo["finestra"] = {{"dalle", "07:30"}, {"alle", "08:30"}};
                    gruppo["conteggi"] = linea ? "stimati" : "esatti";
                    gruppo["fermate"] = fermate;
                    Json servizio = {{"codice", g.servizio},
                                     {"nome", std::string(linea ? "Linea" : ncc ? "Noleggio" : disabili ? "Trasporto" : "Scuola") +
                                                  " di prova " + cifre(i + 1, 4) + (n == 1 ? " (disabili)" : "")},
                                     {"tipo", disabili ? "disabili" : linea ? "linea" : ncc ? "ncc" : "scolastico"},
                                     {"contratto", (ncc ? "CT-NCC-" : "CT-") + cifre(a + 1, 2)},
                                     {"calendario", "CAL-SCUOLA"}};
                    servizio["liberta_oraria"] = linea || ncc ? Json{{"tipo", "fissa"}} : Json{{"tipo", "libera"}, {"soglia_da_concordare_minuti", 10}};
                    servizio["domanda"] = {{"gruppi", Json::array({gruppo})}};
                    elenco_servizi.push_back(servizio);
                    ++servizi_scritti;
                    if (!ncc) lotti[static_cast<std::size_t>(a)]["servizi"].push_back(g.servizio);
                }
                if (lunga) {
                    servizi_lunghi[static_cast<std::size_t>(a)].push_back(codice_servizio);
                    ++linee_lunghe_scritte;
                }
                if (promiscuo) corse_promiscue += kCorsePerServizio;
                for (const Posto& q : posti) {
                    int t = inizio_fascia(q.fascia) + caso.tra(0, 4);
                    Json passaggi = Json::array();
                    passaggi.push_back({{"luogo", "L-RIM-" + cifre(a + 1, 2)}, {"orario", orario(t)}});
                    for (int k = 0; k < kFermatePerCorsa; ++k) {
                        t += lunga ? caso.tra(11, 13) : caso.tra(3, 6);
                        Json persone = Json::array();
                        for (const Gruppo& g : gruppi)
                            if (k >= g.da && k < g.a)
                                persone.push_back({{"gruppo", g.codice}, {"salgono", g.salgono[static_cast<std::size_t>(k - g.da)]}});
                        passaggi.push_back({{"luogo", fermata((kFermatePerCorsa * i + k) % kFermate)},
                                            {"orario", orario(t)},
                                            {"persone", persone},
                                            {"km", {{k == 0 ? "vuoti" : "pieni", (lunga ? caso.tra(105, 130) : caso.tra(10, 99)) / 10.0}}}});
                    }
                    t += lunga ? caso.tra(11, 13) : caso.tra(4, 8);
                    Json scendono = Json::array();
                    for (const Gruppo& g : gruppi) scendono.push_back({{"gruppo", g.codice}, {"scendono", g.totale}});
                    passaggi.push_back({{"luogo", arrivo},
                                        {"orario", orario(t)},
                                        {"persone", scendono},
                                        {"km", {{"pieni", (lunga ? caso.tra(105, 130) : caso.tra(10, 99)) / 10.0}}}});
                    Json servizi_corsa = Json::array();
                    for (const Gruppo& g : gruppi) servizi_corsa.push_back(g.servizio);
                    const std::string codice_corsa = "C-" + cifre(++numero_corsa, 5);
                    elenco_corse.push_back({{"codice", codice_corsa}, {"servizi", servizi_corsa}, {"calendario", "CAL-SCUOLA"}, {"passaggi", passaggi}});
                    del_mezzo[static_cast<std::size_t>(a)][static_cast<std::size_t>(q.m)].push_back({codice_corsa, arrivo, q.fascia, t});
                }
            }
            for (auto& v : del_mezzo[static_cast<std::size_t>(a)])
                std::sort(v.begin(), v.end(), [](const CorsaDelMezzo& x, const CorsaDelMezzo& y) { return x.fascia < y.fascia; });
        }
        scrivi(cartella, "gruppi_servizi.json", gruppi_servizi);
        scrivi(cartella, "servizi.json", servizi);
        scrivi(cartella, "corse.json", corse);

        // Mezzi e rimesse: il tipo del mezzo secondo il suo lavoro (K-012); un mezzo di scorta per azienda, che si presta
        // all'azienda dopo (mezzi.json versione 3).
        Json mezzi = documento("mezzi", 3);
        Json& elenco_mezzi = mezzi["mezzi"] = Json::array();
        Json rimesse = documento("rimesse");
        Json& elenco_rimesse = rimesse["rimesse"] = Json::array();
        const int posti[] = {16, 19, 22, 30, 54};
        for (int a = 0; a < kAziende; ++a) {
            for (int m = 0; m <= kMezziPerAzienda; ++m) {
                const bool scorta = m == kMezziPerAzienda;
                const TipoMezzo tm = scorta ? TipoMezzo::Scuola : tipo_mezzo(m);
                int p = scorta ? 22 : posti[m % 5];
                std::string uso = "NCC";
                if (tm == TipoMezzo::Linea) {
                    p = 54;
                    uso = "LINEA";
                } else if (tm == TipoMezzo::Auto) {
                    p = 8;
                } else if (tm == TipoMezzo::Attrezzato) {
                    p = 19;
                } else if (tm == TipoMezzo::Pullman) {
                    p = 50;
                }
                Json mezzo = {{"codice", ::mezzo(a, m)},
                              {"uso", uso},
                              {"azienda", azienda(a)},
                              {"targa", "PR " + cifre(a * (kMezziPerAzienda + 1) + m + 1, 3) + " SC"},
                              {"categoria", tm == TipoMezzo::Auto ? "M1" : p <= 22 ? "M2" : "M3"},
                              {"posti", p}};
                if (tm == TipoMezzo::Linea) mezzo["posti_in_piedi"] = 20;
                if (tm == TipoMezzo::Attrezzato)
                    mezzo.update({{"carrozzine", 2}, {"posti_persi_per_carrozzina", 3}, {"pedana", true}, {"tempo_pedana_secondi", 120}});
                if (m % 10 == 9)
                    mezzo["alimentazione"] = {{"tipo", "elettrico"}, {"batteria_kwh", 300}, {"consumo_kwh_km", 1.1},
                                              {"carica_minima_percento", 20}};
                else
                    mezzo["alimentazione"] = {{"tipo", "gasolio"}};
                mezzo["costo_fisso_giornaliero"] = 40.0 + caso.tra(0, 50);
                mezzo["costo_al_km"] = caso.tra(50, 100) / 100.0;
                if (scorta) {
                    mezzo["scorta"] = true;
                    mezzo["prestiti"] = Json::array({{{"azienda", azienda((a + 1) % kAziende)},
                                                      {"accordo", "Accordo consortile di prova"},
                                                      {"dal", "2026-09-01"},
                                                      {"al", "2027-08-31"}}});
                }
                elenco_mezzi.push_back(mezzo);
            }
            elenco_rimesse.push_back({{"codice", "RIM-" + cifre(a + 1, 2)},
                                      {"luogo", "L-RIM-" + cifre(a + 1, 2)},
                                      {"azienda", azienda(a)},
                                      {"ricarica", true},
                                      {"posti_mezzi", kMezziPerAzienda + 1}});
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

        // --- K-008: norme, aziende, persone, turni, rotazioni, assenze e sostituzioni -------------------------------
        Json norme = documento("norme", 3);
        const Json fonte_norma = {{"documento", "Norme di prova (inventate)"}};
        const Json demo = norme_della_dimostrazione();
        norme["norme"] = Json::array({{
            {"codice", "N-SC"},
            {"paese", "IT"},
            {"nome", "Norme della prova di scala"},
            {"abilitazioni", Json::array({{{"codice", "B"}, {"nome", "Patente B"}, {"fonte", fonte_norma}},
                                         {{"codice", "D"}, {"nome", "Patente D"}, {"fonte", fonte_norma}},
                                         {{"codice", "CQC-P"}, {"nome", "Carta di qualificazione"}, {"fonte", fonte_norma}}})},
            {"usi", Json::array({{{"codice", "NCC"}, {"nome", "Noleggio con conducente"}, {"fonte", fonte_norma}},
                                 {{"codice", "LINEA"}, {"nome", "Servizio di linea"}, {"fonte", fonte_norma}}})},
            // K-012: le linee con l'uso di linea, l'NCC con l'uso NCC; scolastico e disabili senza un uso richiesto.
            {"usi_per_servizio", Json::array({{{"tipi_servizio", Json::array({"linea"})}, {"usi", Json::array({"LINEA"})}, {"fonte", fonte_norma}},
                                              {{"tipi_servizio", Json::array({"ncc"})}, {"usi", Json::array({"NCC"})}, {"fonte", fonte_norma}}})},
            {"requisiti_guida", Json::array({{{"categorie", Json::array({"M2", "M3"})},
                                               {"alternative", Json::array({{{"abilitazioni", Json::array({"D", "CQC-P"})}}})},
                                               {"fonte", fonte_norma}}})},
            {"regimi", demo.at("regimi")},
            {"cambi_ora", demo.at("cambi_ora")},
        }});
        scrivi(cartella, "norme.json", norme);

        Json aziende = documento("aziende", 3);
        Json& elenco_aziende = aziende["aziende"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            elenco_aziende.push_back(
                {{"azienda", azienda(a)},
                 {"norme", "N-SC"},
                 {"impostazioni",
                  {{"ricovero_vicino_a_casa", "ammesso"},
                   {"abbinamento_autista_mezzo", "nessuno"},
                   {"soste_non_scritte", {{"categoria", "disponibilita"}, {"oltre_minuti", 30}, {"categoria_oltre", "interruzione"}}}}},
                 {"attivita", Json::array({{{"codice", "PRESA"}, {"nome", "Presa servizio"}, {"usa_il_mezzo", true}, {"tempo", "altre_mansioni"}},
                                           {{"codice", "VUOTO"}, {"nome", "Trasferimento a vuoto"}, {"usa_il_mezzo", true}, {"tempo", "guida"}},
                                           {{"codice", "FINE"}, {"nome", "Fine servizio"}, {"usa_il_mezzo", true}, {"tempo", "altre_mansioni"}}})},
                 {"tipi_assenza", Json::array({{{"codice", "FERIE"}, {"nome", "Ferie"}}})},
                 {"scelte_orari", Json::array({{{"tipo", "ampiezza_massima"}, {"massimo_minuti", 600}, {"forza", "preferenza"}},
                                               {{"tipo", "giorni_consecutivi"}, {"massimo_giorni", 6}, {"forza", "limite"}}})},
                 {"rientro", {{"fermo_minimo_minuti", 45}, {"fattore_guida", 2}, {"forza", "preferenza"}}}});
        // K-012: il modo delle corse (OUT o tachigrafo). Le aziende pari lavorano OUT linee e scolastico dove la legge lo permette,
        // con il tachigrafo per l'NCC e, per servizio, per le linee oltre i 50 km (dove vale comunque la legge); le dispari
        // registrano con il tachigrafo linee e NCC, come preferenza. Un tipo di fermo per la revisione dei mezzi.
        for (int a = 0; a < kAziende; ++a) {
            Json& az = elenco_aziende[static_cast<std::size_t>(a)];
            az["tipi_fermo"] = Json::array({{{"codice", "REVISIONE"}, {"nome", "Revisione"}}});
            Json modo = {{"regime", "R-561"}};
            if (a % 2 == 0) {
                modo["per_tipo_servizio"] = Json::array({{{"tipo_servizio", "linea"}, {"modo", "out"}},
                                                         {{"tipo_servizio", "scolastico"}, {"modo", "out"}},
                                                         {{"tipo_servizio", "ncc"}, {"modo", "tachigrafo"}}});
                Json per_servizio = Json::array();
                for (const std::string& sl : servizi_lunghi[static_cast<std::size_t>(a)])
                    per_servizio.push_back({{"servizio", sl}, {"modo", "tachigrafo"}, {"nota", "Linea oltre i 50 km."}});
                modo["servizi"] = per_servizio;
            } else {
                modo["forza"] = "preferenza";
                modo["per_tipo_servizio"] = Json::array({{{"tipo_servizio", "linea"}, {"modo", "tachigrafo"}},
                                                         {{"tipo_servizio", "ncc"}, {"modo", "tachigrafo"}}});
            }
            az["modo_corse"] = modo;
        }
        scrivi(cartella, "aziende.json", aziende);

        Json persone = documento("persone", 2);
        Json& elenco_persone = persone["persone"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            for (int i = 0; i < kPersonePerAzienda; ++i)
                elenco_persone.push_back({{"codice", persona(a, i)},
                                          {"azienda", azienda(a)},
                                          {"ruoli", Json::array({"autista"})},
                                          {"presa_servizio", "L-RIM-" + cifre(a + 1, 2)},
                                          {"rapporto", {{"tipo", "dipendente_tempo_pieno"}}},
                                          {"ore", Json::array({{{"previste", {{"settimana", 39}}}, {"massime", {{"giorno", 10}, {"settimana", 48}}}}})},
                                          {"abilitazioni", Json::array({{{"tipo", "B"}}, {{"tipo", "D"}}, {{"tipo", "CQC-P"}}})}});

        // Turni: per ogni mezzo il mattino (fasce 0-7) e il pomeriggio (fasce 8-16), con presa servizio, corse,
        // rientro a vuoto e fine servizio.
        Json turni = documento("turni");
        Json& elenco_turni = turni["turni"] = Json::array();
        for (int a = 0; a < kAziende; ++a)
            for (int t = 0; t < kTurniPerAzienda; ++t) {
                const int m = t % kMezziPerAzienda;
                const bool mattino = t < kMezziPerAzienda;
                const std::string rimessa = "L-RIM-" + cifre(a + 1, 2);
                // Le corse del mezzo nelle fasce della sua meta' della giornata (K-012: le corse sono in ordine di fascia; una
                // linea lunga occupa due fasce e qualche fascia resta libera).
                std::vector<const CorsaDelMezzo*> sue;
                for (const CorsaDelMezzo& c : del_mezzo[static_cast<std::size_t>(a)][static_cast<std::size_t>(m)])
                    if ((c.fascia < kFascePerMattino) == mattino) sue.push_back(&c);
                if (sue.empty()) throw std::runtime_error("mezzo senza corse in un turno");
                const int presa = inizio_fascia(sue.front()->fascia) - 15;
                Json elementi = Json::array();
                elementi.push_back({{"attivita", "PRESA"}, {"luogo", rimessa}, {"dalle", orario(presa)}, {"alle", orario(presa + 13)}});
                for (const CorsaDelMezzo* c : sue) elementi.push_back({{"corsa", c->codice}});
                const CorsaDelMezzo& ultima = *sue.back();
                elementi.push_back({{"attivita", "VUOTO"}, {"luogo", ultima.arrivo}, {"verso", rimessa},
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

        // Rotazioni: le righe 0-65 fanno i turni del mattino (0-59), le righe 66-131 quelli del pomeriggio (60-119). La
        // riga i al passo k fa il turno (i + k * salto) mod 66 della sua meta', oppure riposa se il numero e' 60 o piu'
        // (salto = 66 / lunghezza: ogni riga lavora in quasi tutti i passi). Ogni giorno tutti i 120 turni hanno una
        // persona e 12 persone riposano; nessuno passa dal pomeriggio al mattino del giorno dopo (riposo troppo corto).
        // Lunghezze diverse per azienda; l'ultima a passi di un mese.
        const int lunghezze[kAziende] = {3, 5, 6, 7, 8, 10, 12, 14, 21, 4};
        const int partenza = giorno(2026, 8, 31);
        const auto passo_del_giorno = [&](int a, int d) {
            if (a == kAziende - 1) return (mese_di(d) - mese_di(giorno(2026, 9, 1))) % lunghezze[a];
            return ((d - partenza) % lunghezze[a] + lunghezze[a]) % lunghezze[a];
        };
        const auto turno_della_riga = [&](int a, int i, int k) {   // >= 120: riposo
            const int meta = i / kMetaDelleRighe;
            const int n = (i % kMetaDelleRighe + k * (kMetaDelleRighe / lunghezze[a])) % kMetaDelleRighe;
            return n >= kMezziPerAzienda ? kTurniPerAzienda : meta * kMezziPerAzienda + n;
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

        // Assenze di un giorno, ognuna coperta da un'assegnazione del singolo giorno a una persona della stessa meta' della
        // giornata che quel giorno riposa (cosi' nessun turno resta scoperto, nessuno fa due turni e i riposi restano).
        std::vector<int> giorni_di_scuola;
        for (int d = giorno(2026, 9, 14); d <= giorno(2027, 6, 11); ++d) {
            const int dow = ((d % 7) + 7 + 3) % 7;   // 0 = lunedi' (il 1970-01-01 era un giovedi')
            const bool festa = std::find(feste.begin(), feste.end(), data(d)) != feste.end();
            const bool natale = d >= giorno(2026, 12, 23) && d <= giorno(2027, 1, 6);
            if (dow < 5 && !festa && !natale) giorni_di_scuola.push_back(d);
        }
        // K-012: due assenze per azienda sono coperte da una persona in prestito dall'azienda dopo (anche lei a riposo quel
        // giorno, nella stessa meta' della giornata), con il prestito dichiarato nelle sue persone.
        Json assenze = documento("assenze");
        Json& elenco_assenze = assenze["assenze"] = Json::array();
        Json assegnazioni = documento("assegnazioni");
        Json& elenco_assegnazioni = assegnazioni["assegnazioni"] = Json::array();
        std::set<std::string> impegnati;   // "azienda|persona|giorno" e "azienda|turno|giorno" gia' usati
        const auto chiave = [](int a, int x, int d) { return std::to_string(a) + "|" + std::to_string(x) + "|" + std::to_string(d); };
        int prestiti_persone = 0, prestiti_mezzi = 0;
        for (int a = 0; a < kAziende; ++a)
            for (int n = 0; n < kAssenzePerAzienda; ++n) {
                const int d = giorni_di_scuola[static_cast<std::size_t>(17 * n + 3 * a) % giorni_di_scuola.size()];
                const int k = passo_del_giorno(a, d);
                int assente = (13 * n + a) % kPersonePerAzienda;
                while (turno_della_riga(a, assente, k) >= kTurniPerAzienda) assente = (assente + 1) % kPersonePerAzienda;
                const int turno_assente = turno_della_riga(a, assente, k);
                const bool prestito = n % 5 == 0;
                const int b = prestito ? (a + 1) % kAziende : a;   // l'azienda di chi sostituisce
                const int kb = passo_del_giorno(b, d);
                int sostituto = assente / kMetaDelleRighe * kMetaDelleRighe;
                while (sostituto / kMetaDelleRighe == assente / kMetaDelleRighe &&
                       (turno_della_riga(b, sostituto, kb) < kTurniPerAzienda || impegnati.count("p" + chiave(b, sostituto, d))))
                    ++sostituto;
                if (sostituto / kMetaDelleRighe != assente / kMetaDelleRighe) throw std::runtime_error("sostituto mancante");
                impegnati.insert("p" + chiave(b, sostituto, d));
                impegnati.insert("t" + chiave(a, turno_assente, d));
                elenco_assenze.push_back({{"persona", persona(a, assente)}, {"tipo", "FERIE"}, {"dal", data(d)}, {"al", data(d)}});
                elenco_assegnazioni.push_back({{"data", data(d)}, {"turno", turno(a, turno_assente)}, {"persona", persona(b, sostituto)}});
                if (prestito) {
                    Json& chi = elenco_persone[static_cast<std::size_t>(b * kPersonePerAzienda + sostituto)];
                    if (!chi.contains("prestiti")) chi["prestiti"] = Json::array();
                    Json voce = {{"azienda", azienda(a)}, {"accordo", "Accordo consortile di prova"}, {"dal", "2026-09-01"}, {"al", "2027-08-31"}};
                    if (std::find(chi["prestiti"].begin(), chi["prestiti"].end(), voce) == chi["prestiti"].end()) chi["prestiti"].push_back(voce);
                    ++prestiti_persone;
                }
            }
        // K-012: un mezzo per azienda va in revisione per un giorno di scuola; i suoi due turni prendono il mezzo di scorta
        // dell'azienda prima, prestato con l'accordo (mezzi.json, prestiti).
        Json fermi = documento("fermi");
        Json& elenco_fermi = fermi["fermi"] = Json::array();
        for (int a = 0; a < kAziende; ++a) {
            const int b = (a + 1) % kAziende, m = 20;
            std::size_t g = static_cast<std::size_t>(29 * a + 11);
            while (impegnati.count("t" + chiave(b, m, giorni_di_scuola[g % giorni_di_scuola.size()])) ||
                   impegnati.count("t" + chiave(b, m + kMezziPerAzienda, giorni_di_scuola[g % giorni_di_scuola.size()])))
                ++g;
            const int d = giorni_di_scuola[g % giorni_di_scuola.size()];
            elenco_fermi.push_back({{"mezzo", ::mezzo(b, m)}, {"tipo", "REVISIONE"}, {"dal", data(d)}, {"al", data(d)}});
            for (int t : {m, m + kMezziPerAzienda})
                elenco_assegnazioni.push_back({{"data", data(d)}, {"turno", turno(b, t)}, {"mezzo", ::mezzo(a, kMezziPerAzienda)}});
            ++prestiti_mezzi;
        }
        scrivi(cartella, "persone.json", persone);
        scrivi(cartella, "assenze.json", assenze);
        scrivi(cartella, "assegnazioni.json", assegnazioni);
        scrivi(cartella, "fermi.json", fermi);

        std::cout << "Pacchetto di scala in " << cartella.string() << ": " << kAziende * (kMezziPerAzienda + 1) << " mezzi ("
                  << kAziende << " di scorta), " << kFermate << " fermate, " << servizi_scritti << " servizi, " << numero_corsa
                  << " corse (" << linee_lunghe_scritte << " linee oltre i 50 km, " << corse_promiscue << " corse promiscue), "
                  << kAziende * kPersonePerAzienda << " persone, " << kAziende * kTurniPerAzienda << " turni, " << prestiti_persone
                  << " prestiti di persone e " << prestiti_mezzi << " di mezzi.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "genera_scala: " << e.what() << "\n";
        return 3;
    }
}
