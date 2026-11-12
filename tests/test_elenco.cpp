// K-Zero - prove dell'elenco utenti del committente (K-019): lettura delle celle con due profili diversi (il lettore non conosce
// il file vero), lettura delle righe e confronto (nuovi, cambiati, usciti, omonimi, parziale, righe illeggibili, sblocchi, codici),
// controlli sui dati, e i comandi kzero leggi-elenco e kzero sblocco con i tre utenti della cassaforte. Dati inventati
// (tests/esempi/k019-elenco); i file Excel si costruiscono da testo (tests/xlsx_prove.hpp).
#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "cassaforte_prove.hpp"
#include "chiusura.hpp"
#include "controllo.hpp"
#include "elenco.hpp"
#include "errori.hpp"
#include "excel.hpp"
#include "lettore.hpp"
#include "ricerca.hpp"
#include "xlsx_prove.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove;
namespace fs = std::filesystem;

namespace {

const std::string kCartella = KZERO_CARTELLA_PROVE "/esempi/k019-elenco";

std::string leggi_testo(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

// Il pacchetto di prova in memoria: il demo, i profili di prova e i codici esterni di scuole e fermate.
PacchettoInMemoria pacchetto_di_prova() {
    PacchettoInMemoria m;
    const PacchettoIncorporato demo("esempi/demo");
    const JsonDati codici = JsonDati::parse(leggi_testo(kCartella + "/codici_esterni.json"));
    for (const auto& v : demo.voci()) {
        std::string t = demo.leggi(v.nome, kMassimoByteFile).testo;
        if (v.nome == "scuole.json" || v.nome == "luoghi.json") {
            JsonDati d = JsonDati::parse(t);
            const char* elenco = v.nome == "scuole.json" ? "scuole" : "luoghi";
            for (JsonDati& x : d.at(elenco)) {
                const std::string c = x.at("codice").get<std::string>();
                if (!codici.at(elenco).contains(c)) continue;
                if (!x.contains("codici_esterni")) x["codici_esterni"] = JsonDati::array();
                for (const JsonDati& e : codici.at(elenco).at(c)) x["codici_esterni"].push_back(e);
            }
            t = d.dump(2);
        }
        m.file[v.nome] = t;
    }
    m.file["profili_elenchi.json"] = leggi_testo(kCartella + "/profili_elenchi.json");
    return m;
}

Lettura lettura_di_prova() {
    Lettura l = leggi_pacchetto(pacchetto_di_prova());
    EXPECT_TRUE(l.forma_valida());
    return l;
}

const ReadingProfile& profilo(const Lettura& l, const char* codice) {
    for (const ReadingProfile& p : l.pacchetto.reading_profiles)
        if (p.code == codice) return p;
    throw std::logic_error(std::string("profilo che non c'e': ") + codice);
}

// Una tabella dalle righe di testo (come le dara' il lettore del file).
Table tabella(const std::vector<std::vector<std::string>>& righe) {
    Table t;
    for (std::size_t r = 0; r < righe.size(); ++r) {
        TableRow x;
        x.number = static_cast<int>(r + 1);
        for (std::size_t c = 0; c < righe[r].size(); ++c)
            if (!righe[r][c].empty()) x.cells.push_back({static_cast<int>(c + 1), righe[r][c], righe[r][c] == "!"});
        t.rows.push_back(x);
    }
    return t;
}

std::vector<std::vector<std::string>> da_tsv(const std::string& file) {
    std::vector<std::vector<std::string>> out;
    std::istringstream in(leggi_testo(kCartella + "/" + file));
    std::string riga;
    while (std::getline(in, riga)) {
        std::vector<std::string> celle;
        std::size_t a = 0;
        for (;;) {
            const std::size_t b = riga.find('\t', a);
            std::string c = riga.substr(a, b == std::string::npos ? std::string::npos : b - a);
            if (!c.empty() && c[0] == '#') c = c.substr(1);
            celle.push_back(c);
            if (b == std::string::npos) break;
            a = b + 1;
        }
        out.push_back(celle);
    }
    return out;
}

const std::vector<std::string> kTitoli = {"SCUOLA", "FERMATA", "COMUNE", "FRAZIONE", "VIA", "CIVICO", "UTENTE", "CLASSE",
                                          "SEZ.", "STATO", "PERCORSI", "ORARI MATTINA", "ORARI POMERIGGIO"};

std::vector<std::string> riga(const std::string& scuola, const std::string& nome, const std::string& stato = "ATTIVO",
                              const std::string& percorsi = "AM/ALFA RM/ALFA", const std::string& mattina = "",
                              const std::string& pomeriggio = "", const std::string& via = "via Roma", const std::string& civico = "3") {
    return {scuola, "", "Pradello", "", via, civico, nome, "3", "A", stato, percorsi, mattina, pomeriggio};
}

std::string giorno(Weekday w) {
    static const char* n[] = {"lu", "ma", "me", "gi", "ve", "sa", "do"};
    return n[static_cast<int>(w)];
}

std::vector<std::string> orari(const std::vector<ListUserTime>& v) {
    std::vector<std::string> out;
    for (const ListUserTime& t : v)
        out.push_back(t.leg + " " + giorno(t.day) + " " + text_from_minutes(t.time) + " " + t.period + " " + text_from_day(t.dates.from) +
                      " " + text_from_day(t.dates.to));
    return out;
}

// Applica una lettura al pacchetto in memoria (come fa il comando con --scrivi, senza la cassaforte) e lo rilegge.
Lettura applica(const Lettura& prima, const ListMerge& m) {
    PacchettoInMemoria x;
    for (const auto& [f, d] : prima.documenti) x.file[f] = chiusura::testo_file(d);
    JsonDati doc = prima.documenti.count("elenco_utenti.json")
                       ? prima.documenti.at("elenco_utenti.json")
                       : JsonDati{{"schema", "elenco_utenti"}, {"schema_versione", 1}, {"ultimo_numero", 0}, {"elenchi", JsonDati::array()}};
    // Lo stesso documento del comando: lo si ottiene dal comando stesso nelle prove dei comandi; qui un elenco semplice.
    JsonDati utenti = JsonDati::array();
    for (const ListUser& u : m.list.users) {
        JsonDati j = {{"codice", u.code}, {"nome", u.name}, {"scuola", u.school.code}, {"stato", {{"tipo", u.active ? "attivo" : "sospeso"}}}};
        if (u.home) {
            JsonDati c = JsonDati::object();
            if (!u.home->town.empty()) c["comune"] = u.home->town;
            if (!u.home->hamlet.empty()) c["frazione"] = u.home->hamlet;
            if (!u.home->street.empty()) c["via"] = u.home->street;
            if (!u.home->number.empty()) c["civico"] = u.home->number;
            j["casa"] = c;
        }
        if (!u.unlocks.empty()) {
            j["sblocchi"] = JsonDati::array();
            for (const ListUserUnlock& s : u.unlocks)
                j["sblocchi"].push_back({{"tratta", s.leg}, {"riferimento", s.reference}, {"data", text_from_day(s.date)}});
        }
        if (!u.legs.empty()) {
            j["tratte"] = JsonDati::array();
            for (const ListUserLeg& l : u.legs) j["tratte"].push_back({{"sigla", l.leg}, {"azienda", "ALFA"}});
        }
        if (u.out_of_list) j["fuori_elenco"] = true;
        utenti.push_back(j);
    }
    doc["ultimo_numero"] = m.last_number;
    doc["elenchi"] = JsonDati::array({{{"committente", m.list.client.code}, {"profilo", m.list.profile.code},
                                       {"anno_scolastico", "2026-2027"}, {"utenti", utenti}}});
    x.file["elenco_utenti.json"] = doc.dump(2);
    Lettura l = leggi_pacchetto(x);
    EXPECT_TRUE(l.forma_valida());
    return l;
}

ListMerge leggi_e_confronta(const Lettura& l, const std::vector<std::vector<std::string>>& righe, bool completo,
                            const char* codice_profilo = "PR-CVA", ListRead* letto = nullptr) {
    const ReadingProfile& p = profilo(l, codice_profilo);
    const Index idx(l.pacchetto);
    const ClientList* prima = nullptr;
    for (const ClientList& c : l.pacchetto.client_lists)
        if (c.client.code == p.client.code) prima = &c;
    ListRead r = read_list(tabella(righe), p, idx, 2026, prima);
    EXPECT_EQ(r.fault, TableFault::None);
    ListMerge m = merge_list(l.pacchetto, p, 2026, r, completo, std::string(64, 'a'));
    if (letto) *letto = r;
    return m;
}

int conta(const ListMerge& m, ListChange::Kind k) {
    return static_cast<int>(std::count_if(m.changes.begin(), m.changes.end(), [&](const ListChange& c) { return c.kind == k; }));
}

}  // namespace

// --- Celle ------------------------------------------------------------------------------------------------------------------

TEST(Elenco, PulituraEAnnoScolastico) {
    EXPECT_EQ(clean_cell("  ROSSI   ANNA  \t\r\n"), "ROSSI ANNA");
    EXPECT_EQ(clean_cell("A\xC2\xA0" "B"), "A B");
    // Gennaio e' del secondo anno, settembre del primo (sabotaggio: anno sbagliato per le date di gennaio).
    EXPECT_EQ(text_from_day(*school_date(2026, 9, 23, 1)), "2027-01-23");
    EXPECT_EQ(text_from_day(*school_date(2026, 9, 17, 9)), "2026-09-17");
    EXPECT_EQ(text_from_day(*school_date(2026, 9, 31, 8)), "2027-08-31");
    EXPECT_FALSE(school_date(2026, 9, 29, 2).has_value());   // 2027 non e' bisestile
    EXPECT_TRUE(school_date(2027, 9, 29, 2).has_value());    // 2028 si'
    EXPECT_FALSE(school_date(2026, 9, 31, 4).has_value());
    const DateRange a = school_year(2026, 9);
    EXPECT_EQ(text_from_day(a.from), "2026-09-01");
    EXPECT_EQ(text_from_day(a.to), "2027-08-31");
}

TEST(Elenco, PercorsiConDitteConSpazi) {
    const Lettura l = lettura_di_prova();
    const ReadingProfile& p = profilo(l, "PR-CVA");
    const RoutesParse r = parse_routes("AM/ALFA RM/DITTA GAMMA RP/LINEA C SCOL.", p);
    ASSERT_FALSE(r.fault);
    ASSERT_EQ(r.legs.size(), 3u);
    EXPECT_EQ(r.legs[0].leg, "AM");
    ASSERT_TRUE(r.legs[0].company);
    EXPECT_EQ(r.legs[0].company->code, "ALFA");
    EXPECT_EQ(r.legs[1].other_company, "DITTA GAMMA");
    EXPECT_EQ(r.legs[2].other_company, "LINEA C SCOL.");
    EXPECT_EQ(parse_routes("AM/BETA TRASPORTI RP/ALFA", p).legs[0].company->code, "BETA");   // ditta con uno spazio nel profilo
    EXPECT_EQ(parse_routes("AM/ALFA RP/ALFA", p).legs.size(), 2u);                              // infanzia
    EXPECT_EQ(parse_routes("xx AM/ALFA", p).fault, RowFault::RoutesForm);
    EXPECT_EQ(parse_routes("AM/ RM/ALFA", p).fault, RowFault::RoutesForm);
    EXPECT_EQ(parse_routes("AM/ALFA AM/BETA", p).fault, RowFault::RoutesLegTwice);
    EXPECT_TRUE(parse_routes("", p).legs.empty());
}

TEST(Elenco, StatoConNota) {
    const Lettura l = lettura_di_prova();
    const ReadingProfile& p = profilo(l, "PR-CVA");
    const StatusParse s = parse_status("SOSPESO - da attivare solo per ritorno su fermata via Roma", p);
    EXPECT_TRUE(s.ok);
    EXPECT_FALSE(s.active);
    EXPECT_EQ(s.note, "da attivare solo per ritorno su fermata via Roma");
    EXPECT_TRUE(parse_status("attivo", p).active);
    EXPECT_FALSE(parse_status("SOSPESA", p).ok);
}

// Il caso vero: l'ora attaccata alla voce dopo ("12.00f2"), le date senza anno (gennaio del secondo anno), voci senza date.
TEST(Elenco, OrariPerGiornoEOraAttaccata) {
    const Lettura l = lettura_di_prova();
    const ReadingProfile& p = profilo(l, "PR-CVA");
    const ProfileTimes& pomeriggio = p.times[1];
    TimesParse t = parse_times("//ME rm 12.00f2 15.50 [27/01-04/06]", pomeriggio, p, 2026);
    ASSERT_FALSE(t.fault);
    EXPECT_EQ(orari(t.times), (std::vector<std::string>{"RM me 12:00 rm 2026-09-01 2027-08-31", "RP me 15:50 f2 2027-01-27 2027-06-04"}));
    t = parse_times("//LU o 15.50 [15/09-09/06] //ME f1 15.50 [17/09-23/01] - f2 15.50 [28/01-05/06] //VE rm 12.00 ", pomeriggio, p, 2026);
    ASSERT_FALSE(t.fault);
    EXPECT_EQ(orari(t.times),
              (std::vector<std::string>{"RP lu 15:50 o 2026-09-15 2027-06-09", "RP me 15:50 f1 2026-09-17 2027-01-23",
                                        "RP me 15:50 f2 2027-01-28 2027-06-05", "RM ve 12:00 rm 2026-09-01 2027-08-31"}));
    EXPECT_EQ(parse_times("//XX o 15.50", pomeriggio, p, 2026).fault, RowFault::TimesDayUnknown);
    EXPECT_EQ(parse_times("//LU z 15.50", pomeriggio, p, 2026).fault, RowFault::TimesPeriodUnknown);
    EXPECT_EQ(parse_times("//LU o 25.50", pomeriggio, p, 2026).fault, RowFault::TimesHour);
    EXPECT_EQ(parse_times("//LU o 15.50 [31/04-09/06]", pomeriggio, p, 2026).fault, RowFault::TimesDate);
    EXPECT_EQ(parse_times("//LU o 15.50 [09/06-15/09]", pomeriggio, p, 2026).fault, RowFault::TimesDatesReversed);
    EXPECT_EQ(parse_times("LU o 15.50", pomeriggio, p, 2026).fault, RowFault::TimesForm);
    EXPECT_EQ(parse_times("//LU o 15.50 [15/09-09/06", pomeriggio, p, 2026).fault, RowFault::TimesForm);
    const ProfileTimes& mattina = p.times[0];
    t = parse_times("AM: 08:00 LU-MA | RM: 12:00 VE", mattina, p, 2026);
    ASSERT_FALSE(t.fault);
    EXPECT_EQ(orari(t.times), (std::vector<std::string>{"AM lu 08:00  2026-09-01 2027-08-31", "AM ma 08:00  2026-09-01 2027-08-31",
                                                        "RM ve 12:00  2026-09-01 2027-08-31"}));
    EXPECT_EQ(parse_times("XX: 08:00 LU", mattina, p, 2026).fault, RowFault::TimesLegUnknown);
    EXPECT_EQ(parse_times("AM: 08:00 LU-XX", mattina, p, 2026).fault, RowFault::TimesDayUnknown);
    EXPECT_EQ(parse_times("AM 08:00 LU", mattina, p, 2026).fault, RowFault::TimesForm);
}

// Un secondo profilo, con altri titoli, separatori, sigle e mese d'inizio: lo stesso lettore, nessuna parola nel programma.
TEST(Elenco, SecondoProfilo) {
    const Lettura l = lettura_di_prova();
    const ReadingProfile& p = profilo(l, "PR-TPV");
    const RoutesParse r = parse_routes("ANDATA=Trasporti Beta RIT2=Pulmini Delta", p);
    ASSERT_FALSE(r.fault);
    EXPECT_EQ(r.legs[0].company->code, "BETA");
    EXPECT_EQ(r.legs[1].other_company, "Pulmini Delta");
    TimesParse t = parse_times("ANDATA> 7h55 lun,mer ; RIT1>13h10 ven", p.times[0], p, 2026);
    ASSERT_FALSE(t.fault);
    EXPECT_EQ(orari(t.times).size(), 3u);
    // Mese d'inizio agosto: il 20/08 e' del primo anno.
    t = parse_times("#mar T 16:30 (20.08~30.06) + X 12:40", p.times[1], p, 2026);
    ASSERT_FALSE(t.fault);
    EXPECT_EQ(orari(t.times), (std::vector<std::string>{"RIT2 ma 16:30 T 2026-08-20 2027-06-30", "RIT1 ma 12:40 X 2026-08-01 2027-07-31"}));
    const StatusParse s = parse_status("In attesa: vedi la mail", p);
    EXPECT_TRUE(s.ok);
    EXPECT_FALSE(s.active);
    EXPECT_EQ(s.note, "vedi la mail");
    // Le parole del primo profilo non valgono per il secondo.
    EXPECT_FALSE(parse_status("ATTIVO", p).ok);
    EXPECT_EQ(parse_routes("AM/ALFA", p).fault, RowFault::RoutesForm);
    // Una tabella con i titoli del secondo profilo, e il foglio senza le colonne del primo.
    const Index idx(l.pacchetto);
    const ListRead letto = read_list(tabella({{"Cognome Nome", "Istituto", "Condizione", "Vettori", "Paese", "Indirizzo", "Entrate", "Uscite"},
                                              {"Blu Ugo", "IST. SUPERIORE VALFONDA", "Ok", "ANDATA=Trasporti Beta", "Valfonda", "via Po 1",
                                               "ANDATA> 7h55 lun", ""}}),
                                     p, idx, 2026, nullptr);
    ASSERT_EQ(letto.fault, TableFault::None);
    ASSERT_EQ(letto.users.size(), 1u);
    EXPECT_EQ(letto.users[0].user.school.code, "SC-SUP");
    const ListRead sbagliato = read_list(tabella({kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA")}), p, idx, 2026, nullptr);
    EXPECT_EQ(sbagliato.fault, TableFault::MissingTitle);
}

// --- Righe e confronto ---------------------------------------------------------------------------------------------------------

// I casi veri del mandato, dal testo dei file di prova: spazi in fondo, civico con barra, colonna fermata quasi vuota, nota nello
// stato, ditte con spazi, ora attaccata, infanzia senza orari, colonne in piu' (mai salvate).
TEST(Elenco, PrimaLetturaCasiVeri) {
    const Lettura l = lettura_di_prova();
    ListRead r;
    const ListMerge m = leggi_e_confronta(l, da_tsv("prima-lettura.tsv"), true, "PR-CVA", &r);
    EXPECT_EQ(r.data_rows, 5);
    EXPECT_TRUE(r.unreadable_rows.empty());
    EXPECT_EQ(conta(m, ListChange::Kind::New), 5);
    ASSERT_EQ(m.list.users.size(), 5u);
    const ListUser& rossi = m.list.users[0];
    EXPECT_EQ(rossi.code, "U-000001");
    EXPECT_EQ(rossi.name, "ROSSI ANNA");   // senza gli spazi in fondo
    EXPECT_EQ(rossi.home->number, "3");
    const ListUser& verdi = m.list.users[1];
    EXPECT_EQ(verdi.home->number, "21/C");
    EXPECT_FALSE(verdi.active);
    EXPECT_EQ(verdi.status_note, "da attivare solo per ritorno su fermata via Roma");
    ASSERT_TRUE(verdi.stop);
    EXPECT_EQ(verdi.stop->code, "F-PRADELLO");
    EXPECT_EQ(verdi.stop_match, StopMatch::ExternalCode);
    const ListUser& bianchi = m.list.users[2];
    EXPECT_TRUE(bianchi.times.empty());   // infanzia: nessun orario
    EXPECT_EQ(bianchi.legs.size(), 2u);
    EXPECT_FALSE(bianchi.section.size());
    const ListUser& neri = m.list.users[3];
    ASSERT_TRUE(neri.stop);
    EXPECT_EQ(neri.stop->code, "F-VALFONDA-PIAZZA");   // dal nome, tra i luoghi di Valfonda
    EXPECT_EQ(neri.stop_match, StopMatch::Name);
    const ListUser& gialli = m.list.users[4];
    EXPECT_FALSE(gialli.stop);
    EXPECT_EQ(gialli.stop_text, "Bivio vecchio");
    // Nessun dato delle colonne in piu' nell'elenco.
    for (const ListUser& u : m.list.users) EXPECT_EQ(u.name.find("000"), std::string::npos);
}

// Un nome di fermata vale solo tra i luoghi del comune della casa.
TEST(Elenco, FermataDalNomeSoloNelComune) {
    const Lettura l = lettura_di_prova();
    std::vector<std::string> x = riga("Scuola primaria Valfonda", "ROSSI ANNA");
    x[1] = "Valfonda Piazza Grande";   // casa a Pradello: non si riconosce
    const ListMerge m = leggi_e_confronta(l, {kTitoli, x}, true);
    EXPECT_FALSE(m.list.users[0].stop);
    EXPECT_EQ(m.list.users[0].stop_text, "Valfonda Piazza Grande");
}

TEST(Elenco, SecondaLetturaTreCambiEUnUscito) {
    const Lettura l1 = lettura_di_prova();
    const Lettura l2 = applica(l1, leggi_e_confronta(l1, da_tsv("prima-lettura.tsv"), true));
    // Il confronto vero dei campi vuole i dati interi: si rilegge la prima lettura dal pacchetto di prova con il comando (sotto);
    // qui conta che lo stesso utente si riconosce da nome e scuola e mantiene il codice.
    const ListMerge m = leggi_e_confronta(l2, da_tsv("seconda-lettura.tsv"), true);
    EXPECT_EQ(conta(m, ListChange::Kind::New), 0);
    EXPECT_EQ(conta(m, ListChange::Kind::Exited), 1);
    const auto uscito = std::find_if(m.changes.begin(), m.changes.end(), [](const ListChange& c) { return c.kind == ListChange::Kind::Exited; });
    EXPECT_EQ(uscito->code, "U-000005");
    EXPECT_EQ(m.list.users.size(), 4u);
}

// Il codice non viene dal nome: lo stesso nome in un altro pacchetto, o in un altro ordine, ha un altro codice; i codici non si
// riusano e saltano quelli usati da altri elementi (sabotaggio: codice ricavato dal nome).
TEST(Elenco, CodiciInOrdineMaiDalNome) {
    const Lettura l = lettura_di_prova();
    const ListMerge a = leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA"), riga("Scuola primaria Valfonda", "BIANCHI LUCA")}, true);
    const ListMerge b = leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "BIANCHI LUCA"), riga("Scuola primaria Valfonda", "ROSSI ANNA")}, true);
    const auto codice_di = [](const ListMerge& m, const std::string& nome) {
        for (const ListUser& u : m.list.users)
            if (u.name == nome) return u.code;
        return std::string();
    };
    EXPECT_EQ(codice_di(a, "ROSSI ANNA"), "U-000001");
    EXPECT_EQ(codice_di(b, "ROSSI ANNA"), "U-000002");
    // Un codice gia' usato da un altro elemento si salta.
    PacchettoInMemoria m = pacchetto_di_prova();
    JsonDati c = JsonDati::parse(m.file.at("calendari.json"));
    c.at("calendari")[0]["codice"] = "U-000001";
    m.file["calendari.json"] = c.dump();
    Lettura l2 = leggi_pacchetto(m);
    const ListMerge d = leggi_e_confronta(l2, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA")}, true);
    EXPECT_EQ(d.list.users[0].code, "U-000002");
    // Un uscito non restituisce il suo numero.
    const Lettura l3 = applica(l, a);
    const ListMerge e = leggi_e_confronta(l3, {kTitoli, riga("Scuola primaria Valfonda", "VERDI GINO")}, true);
    EXPECT_EQ(e.list.users.front().code, "U-000003");
    EXPECT_EQ(e.last_number, 3);
}

// Righe con lo stesso nome nella stessa scuola: ferme tutte, mai unite (sabotaggio: omonimi uniti).
TEST(Elenco, OmonimiFermi) {
    const Lettura l = lettura_di_prova();
    ListRead r;
    const ListMerge m = leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA"), riga("Scuola primaria Valfonda", "Rossi Anna", "SOSPESO"),
                                              riga("Scuola media Valfonda", "ROSSI ANNA")},
                                          true, "PR-CVA", &r);
    EXPECT_EQ(r.unreadable_rows, (std::set<int>{2, 3}));
    EXPECT_EQ(m.list.users.size(), 1u);   // solo quella delle medie
    EXPECT_EQ(m.list.users[0].school.code, "SC-MEDIE");
    EXPECT_FALSE(m.exits_applied);
    const bool omonimi = std::any_of(r.problems.begin(), r.problems.end(), [](const Problem& p) { return p.check == Check::ListRowHomonyms; });
    EXPECT_TRUE(omonimi);
    // Anche se una delle due righe e' illeggibile per un altro motivo: il nome e la scuola si leggono, e si fermano tutte e due.
    ListRead r2;
    leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA"), riga("Scuola primaria Valfonda", "ROSSI ANNA", "BOH")}, true, "PR-CVA", &r2);
    EXPECT_EQ(r2.unreadable_rows, (std::set<int>{2, 3}));
}

// Elenco parziale: mai uscite. Completo con righe illeggibili: uscite non applicate, e l'utente della riga illeggibile resta com'e'
// (sabotaggi: elenco parziale che toglie utenti; uscite applicate con righe illeggibili).
TEST(Elenco, ParzialeEIlleggibiliNonFannoUscire) {
    const Lettura l0 = lettura_di_prova();
    const Lettura l = applica(l0, leggi_e_confronta(l0, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA"), riga("Scuola primaria Valfonda", "VERDI LUCA"),
                                                       riga("Scuola primaria Valfonda", "NERI MARCO")},
                                                   true));
    const ListMerge parziale = leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA")}, false);
    EXPECT_EQ(conta(parziale, ListChange::Kind::Exited), 0);
    EXPECT_EQ(conta(parziale, ListChange::Kind::NotInFile), 2);
    EXPECT_EQ(parziale.list.users.size(), 3u);
    ListRead r;
    const ListMerge illeggibili = leggi_e_confronta(
        l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA"), riga("Scuola primaria Valfonda", "VERDI LUCA", "ATTIVO", "AM/ALFA", "AM: 25:00 LU")}, true,
        "PR-CVA", &r);
    EXPECT_EQ(r.unreadable_rows, (std::set<int>{3}));
    EXPECT_FALSE(illeggibili.exits_applied);
    EXPECT_EQ(conta(illeggibili, ListChange::Kind::Exited), 0);
    EXPECT_EQ(conta(illeggibili, ListChange::Kind::Unread), 1);   // VERDI: la sua riga c'e' ma non si legge
    EXPECT_EQ(conta(illeggibili, ListChange::Kind::NotInFile), 1);   // NERI
    EXPECT_EQ(illeggibili.list.users.size(), 3u);
    const bool avviso = std::any_of(illeggibili.warnings.begin(), illeggibili.warnings.end(),
                                    [](const Problem& p) { return p.check == Check::ListExitsNotApplied; });
    EXPECT_TRUE(avviso);
    // Lo stesso file completo e tutto leggibile: NERI e VERDI escono.
    const ListMerge buono = leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA")}, true);
    EXPECT_EQ(conta(buono, ListChange::Kind::Exited), 2);
    EXPECT_EQ(buono.list.users.size(), 1u);
}

// Uno sblocco non si perde mai con una lettura: ritrovato, l'utente lo tiene; uscito, resta "fuori elenco" (sabotaggio: sblocco
// cancellato da una lettura). Senza piu' sblocchi, se ne va alla lettura completa successiva.
TEST(Elenco, SbloccoMaiCancellatoDaUnaLettura) {
    const Lettura l0 = lettura_di_prova();
    ListMerge m0 = leggi_e_confronta(l0, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA", "SOSPESO"), riga("Scuola primaria Valfonda", "VERDI LUCA", "SOSPESO")}, true);
    for (ListUser& u : m0.list.users) u.unlocks.push_back({"AM", "mail del 10/11", day_from_ymd(2026, 11, 10), {}});
    const Lettura l1 = applica(l0, m0);
    const ListMerge m1 = leggi_e_confronta(l1, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA", "SOSPESO")}, true);
    ASSERT_EQ(m1.list.users.size(), 2u);
    EXPECT_EQ(m1.list.users[0].unlocks.size(), 1u);   // ROSSI ritrovata: tiene lo sblocco
    EXPECT_TRUE(m1.list.users[1].out_of_list);        // VERDI uscito: resta per lo sblocco
    EXPECT_EQ(m1.list.users[1].unlocks.size(), 1u);
    EXPECT_EQ(conta(m1, ListChange::Kind::KeptForUnlock), 1);
    // Tolto lo sblocco (kzero sblocco --togli), alla lettura completa successiva se ne va.
    ListMerge m1b = m1;
    m1b.list.users[1].unlocks.clear();
    const Lettura l2 = applica(l1, m1b);
    const ListMerge m2 = leggi_e_confronta(l2, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA", "SOSPESO")}, true);
    EXPECT_EQ(m2.list.users.size(), 1u);
    EXPECT_EQ(conta(m2, ListChange::Kind::Exited), 1);
    // Torna nel file: rientra con il suo codice e lo sblocco.
    const ListMerge m3 = leggi_e_confronta(l1, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA", "SOSPESO"), riga("Scuola primaria Valfonda", "VERDI LUCA", "SOSPESO")}, true);
    EXPECT_FALSE(m3.list.users[1].out_of_list);
    EXPECT_EQ(m3.list.users[1].unlocks.size(), 1u);
}

// Forse lo stesso: un uscito e un nuovo con la stessa casa (stessa scuola, o stesso nome in un'altra scuola). Mai un'unione.
TEST(Elenco, ForseLoStesso) {
    const Lettura l0 = lettura_di_prova();
    const Lettura l = applica(l0, leggi_e_confronta(l0, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA")}, true));
    const ListMerge scritto_diverso = leggi_e_confronta(l, {kTitoli, riga("Scuola primaria Valfonda", "ROSSI ANNA MARIA")}, true);
    EXPECT_EQ(conta(scritto_diverso, ListChange::Kind::New), 1);
    EXPECT_EQ(conta(scritto_diverso, ListChange::Kind::Exited), 1);
    ASSERT_EQ(scritto_diverso.warnings.size(), 1u);
    EXPECT_EQ(scritto_diverso.warnings[0].check, Check::ListMaybeSameUser);
    EXPECT_EQ(scritto_diverso.warnings[0].variant, 0);
    const ListMerge altra_scuola = leggi_e_confronta(l, {kTitoli, riga("Scuola media Valfonda", "ROSSI ANNA")}, true);
    ASSERT_EQ(altra_scuola.warnings.size(), 1u);
    EXPECT_EQ(altra_scuola.warnings[0].variant, 1);
    const ListMerge altra_casa = leggi_e_confronta(l, {kTitoli, riga("Scuola media Valfonda", "ROSSI ANNA", "ATTIVO", "AM/ALFA", "", "", "via Po")}, true);
    EXPECT_TRUE(altra_casa.warnings.empty());
}

TEST(Elenco, RigheIlleggibili) {
    const Lettura l = lettura_di_prova();
    const std::vector<std::pair<std::vector<std::string>, RowFault>> casi = {
        {riga("Scuola primaria Valfonda", ""), RowFault::EmptyRequired},
        {riga("Scuola che non c'e'", "A B"), RowFault::SchoolUnknown},
        {riga("Scuola primaria Valfonda", "A B", "FORSE"), RowFault::StatusUnknown},
        {riga("Scuola primaria Valfonda", "A B", "ATTIVO", "AM/ALFA AM/ALFA"), RowFault::RoutesLegTwice},
        {riga("Scuola primaria Valfonda", "A B", "ATTIVO", "AM/ALFA", "AM: 08:00 XX"), RowFault::TimesDayUnknown},
        {riga("Scuola primaria Valfonda", "!"), RowFault::UnreadableCell},
    };
    for (const auto& [x, atteso] : casi) {
        ListRead r;
        leggi_e_confronta(l, {kTitoli, x}, true, "PR-CVA", &r);
        ASSERT_EQ(r.problems.size(), 1u) << static_cast<int>(atteso);
        EXPECT_EQ(r.problems[0].variant, static_cast<int>(atteso));
        EXPECT_EQ(r.problems[0].where.line, 2);
        // Mai il valore di una cella personale nel problema (il nome della scuola si', e' pubblico).
        for (const Param& p : r.problems[0].params) EXPECT_NE(p.text, "A B");
    }
    std::vector<std::string> classe = riga("Scuola primaria Valfonda", "A B");
    classe[7] = "terza";
    ListRead r;
    leggi_e_confronta(l, {kTitoli, classe}, true, "PR-CVA", &r);
    EXPECT_EQ(r.problems.at(0).variant, static_cast<int>(RowFault::ClassNotNumber));
    // Titolo mancante o ripetuto: il file non si legge.
    const Index idx(l.pacchetto);
    std::vector<std::string> titoli = kTitoli;
    titoli[10] = "TRATTE";
    EXPECT_EQ(read_list(tabella({titoli}), profilo(l, "PR-CVA"), idx, 2026, nullptr).fault, TableFault::MissingTitle);
    titoli = kTitoli;
    titoli.push_back("utente");
    EXPECT_EQ(read_list(tabella({titoli}), profilo(l, "PR-CVA"), idx, 2026, nullptr).fault, TableFault::DuplicateTitle);
}

// --- Controlli sui dati ------------------------------------------------------------------------------------------------------

std::vector<std::string> codici_dei_problemi(const Lettura& l) {
    std::vector<Problem> out;
    check_lists(l.pacchetto, Index(l.pacchetto), out);
    std::vector<std::string> v;
    for (const Problem& p : out) v.push_back(definizione_controllo(p.check).codice + " " + std::to_string(p.variant));
    std::sort(v.begin(), v.end());
    return v;
}

TEST(Elenco, ControlliSuiDati) {
    const Lettura l0 = lettura_di_prova();
    ListMerge m = leggi_e_confronta(l0, da_tsv("prima-lettura.tsv"), true);
    // Avvisi del contenuto: andata senza ritorno in parte dell'anno (ROSSI: il mercoledi' dopo il 23/01 e prima del 28/01, e
    // fuori dai periodi), date fuori dall'anno della scuola, sblocco di un utente attivo.
    ListUser& rossi = m.list.users[0];
    rossi.times.erase(std::remove_if(rossi.times.begin(), rossi.times.end(), [](const ListUserTime& t) { return t.leg == "RM"; }), rossi.times.end());
    rossi.unlocks.push_back({"AM", "mail", day_from_ymd(2026, 11, 10), {}});
    PacchettoInMemoria x;
    const Lettura con = applica(l0, m);
    const std::vector<std::string> c = codici_dei_problemi(con);
    EXPECT_TRUE(std::count(c.begin(), c.end(), "K-CTR-146 0")) << ::testing::PrintToString(c);
    // Il codice di un utente usato da un altro elemento; la stessa tratta due volte; un profilo ambiguo.
    PacchettoInMemoria y;
    for (const auto& [f, d] : con.documenti) y.file[f] = chiusura::testo_file(d);
    JsonDati cal = con.documenti.at("calendari.json");
    cal.at("calendari")[0]["codice"] = "U-000001";
    y.file["calendari.json"] = cal.dump();
    JsonDati pr = con.documenti.at("profili_elenchi.json");
    pr.at("profili")[0]["tratte"].push_back({{"sigla", "am"}, {"verso", "andata"}});
    y.file["profili_elenchi.json"] = pr.dump();
    const std::vector<std::string> d = codici_dei_problemi(leggi_pacchetto(y));
    EXPECT_TRUE(std::count(d.begin(), d.end(), "K-CTR-141 0")) << ::testing::PrintToString(d);
    EXPECT_TRUE(std::count(d.begin(), d.end(), "K-CTR-139 0")) << ::testing::PrintToString(d);
}

TEST(Elenco, AndataSenzaRitornoESovrapposizioni) {
    const Lettura l0 = lettura_di_prova();
    std::vector<std::string> x = riga("Scuola primaria Valfonda", "ROSSI ANNA", "ATTIVO", "AM/ALFA RP/ALFA", "AM: 08:00 ME",
                                      "//ME f1 15.50 [17/09-23/01] - f2 15.50 [28/01-05/06] - f2 16.10 [01/06-05/06]");
    const Lettura l = applica(l0, leggi_e_confronta(l0, {kTitoli, x}, true));
    // applica() tiene solo i campi che servono alle prove del confronto: qui servono anche gli orari, quindi il controllo si fa sul
    // risultato del confronto con il pacchetto di prima.
    ListMerge m = leggi_e_confronta(l0, {kTitoli, x}, true);
    Package p = l0.pacchetto;
    p.client_lists = {m.list};
    std::vector<Problem> out;
    check_lists(p, Index(p), out);
    int andata = 0, sovrapposte = 0;
    for (const Problem& q : out) {
        if (q.check == Check::ListOutboundWithoutReturn) {
            ++andata;
            for (const Param& pr : q.params)
                if (pr.name == "dal") {
                    EXPECT_EQ(text_from_day(static_cast<Day>(pr.number)), "2026-09-02");   // il primo mercoledi'
                }
        }
        if (q.check == Check::ListTimesOverlap) ++sovrapposte;
    }
    EXPECT_EQ(andata, 1);
    EXPECT_EQ(sovrapposte, 1);
    (void)l;
}

// --- I comandi, con i tre utenti della cassaforte ------------------------------------------------------------------------------

namespace {

const std::string kPw = "cavallo pesca lampione verde";
const std::string kPwB = "barca gialla sul lago verde";
const std::string kPwC = "il treno delle sette e mezza";

struct Prova {
    CartellaDiLavoro lavoro;
    AmbienteDiProva amb;
    fs::path chiuso;
    Prova() {
        const PacchettoInMemoria m = pacchetto_di_prova();
        fs::create_directories(lavoro / "chiaro");
        for (const auto& [f, t] : m.file) {
            std::ofstream o(lavoro / "chiaro" / f, std::ios::binary);
            o << t;
        }
        amb.password = {kPw};
        comando({"chiudi", (lavoro / "chiaro").string(), "--scrivi", (lavoro / "chiuso").string(), "--utente", "A", "--conservazione", "2099-12-31"});
        chiuso = lavoro / "chiuso";
        amb.password = {kPw, kPwB};
        comando({"aggiungi-utente", "--scrivi", chiuso.string(), "--utente", "A", "--nuovo-utente", "B", "--permessi", "personali"});
        amb.password = {kPw, kPwC};
        comando({"aggiungi-utente", "--scrivi", chiuso.string(), "--utente", "A", "--nuovo-utente", "C", "--permessi", "particolari"});
    }
    fs::path xlsx(const std::string& nome, const std::vector<std::vector<std::string>>& righe) {
        std::vector<std::vector<std::string>> conv = righe;
        for (auto& r : conv)
            for (auto& c : r)
                if (!c.empty() && std::all_of(c.begin(), c.end(), [](char ch) { return ch >= '0' && ch <= '9'; })) c = "#" + c;
        const std::string f = xlsx::file_righe(conv);
        std::ofstream o(lavoro / nome, std::ios::binary);
        o << f;
        return lavoro / nome;
    }
    Risultato leggi(const fs::path& file, const std::string& utente, const std::string& pw, bool scrivere, std::vector<std::string> altri = {}) {
        amb.password = {pw};
        std::vector<std::string> a = {"leggi-elenco", file.string(), chiuso.string(), "--profilo", "PR-CVA", "--anno-scolastico", "2026-2027",
                                      "--elenco", "completo", "--utente", utente};
        if (scrivere) a.insert(a.end(), {"--scrivi", chiuso.string()});
        a.insert(a.end(), altri.begin(), altri.end());
        return comando(a);
    }
    std::string elenco_normale() const { return leggi_testo(chiuso / "elenco_utenti.json"); }
};

std::string errore_di(const std::function<void()>& f) {
    try {
        f();
    } catch (const ErroreKZero& e) {
        return e.definizione().codice;
    }
    return "nessuno";
}

}  // namespace

TEST(ElencoComandi, TreUtentiDellaCassaforte) {
    Prova p;
    const fs::path f = p.xlsx("prima.xlsx", da_tsv("prima-lettura.tsv"));
    // Chi ha un permesso solo non legge ne' scrive l'elenco (per ora: tutti e due).
    EXPECT_EQ(errore_di([&] { p.leggi(f, "B", kPwB, false); }), "K-ERR-023");
    EXPECT_EQ(errore_di([&] { p.leggi(f, "C", kPwC, true); }), "K-ERR-023");
    EXPECT_FALSE(fs::exists(p.chiuso / "elenco_utenti.json"));
    // Il titolare: anteprima (niente scritto), poi scrittura.
    const Risultato a = p.leggi(f, "A", kPw, false);
    EXPECT_NE(a.uscita.find("5 nuovi"), std::string::npos) << a.uscita;
    EXPECT_FALSE(fs::exists(p.chiuso / "elenco_utenti.json"));
    const Risultato s = p.leggi(f, "A", kPw, true);
    EXPECT_NE(s.uscita.find("scritto nella cassaforte"), std::string::npos) << s.uscita;
    // Nel file normale nessun nome, nessuna casa, nessuna nota, niente delle colonne in piu' (sabotaggio: dati in chiaro su disco).
    const std::string normale = p.elenco_normale();
    for (const char* x : {"ROSSI", "VERDI", "via Roma", "21/C", "da attivare", "000 000", "riga gialla", "Bivio vecchio", "\"classe\"", "\"sezione\""})
        EXPECT_EQ(normale.find(x), std::string::npos) << x;
    EXPECT_NE(normale.find("U-000005"), std::string::npos);
    // Senza --includi-dati-personali nessun nome nell'uscita; con il parametro si' (sabotaggio: nome nell'uscita senza il parametro).
    const fs::path g = p.xlsx("seconda.xlsx", da_tsv("seconda-lettura.tsv"));
    const Risultato base = p.leggi(g, "A", kPw, false);
    EXPECT_EQ(base.uscita.find("ROSSI"), std::string::npos) << base.uscita;
    EXPECT_NE(base.uscita.find("3 cambiati"), std::string::npos) << base.uscita;
    EXPECT_NE(base.uscita.find("1 usciti"), std::string::npos) << base.uscita;
    const Risultato con = p.leggi(g, "A", kPw, false, {"--includi-dati-personali"});
    EXPECT_NE(con.uscita.find("\"ROSSI ANNA\""), std::string::npos) << con.uscita;
    // Il parametro oltre i permessi: errore anche per il titolare? No: il titolare li ha tutti e due.
    EXPECT_EQ(con.codice, 0);
}

TEST(ElencoComandi, LegatiALuogoSensibile) {
    Prova p;
    // Un utente del centro diurno (luogo sensibile): il nome sta tra i legati; nell'uscita solo con tutti e due i parametri.
    const fs::path f = p.xlsx("centro.xlsx", {kTitoli, riga("Centro diurno Il Faro", "ROSA LIA", "ATTIVO", "AM/ALFA")});
    p.leggi(f, "A", kPw, true);
    // (in JSON: l'utente e' invariato, e il testo non elenca gli invariati)
    EXPECT_EQ(p.leggi(f, "A", kPw, false, {"--includi-dati-personali", "--formato", "json"}).uscita.find("ROSA"), std::string::npos);
    EXPECT_NE(p.leggi(f, "A", kPw, false, {"--includi-dati-personali", "--includi-dati-particolari", "--formato", "json"}).uscita.find("\"ROSA LIA\""),
              std::string::npos);
    // B (solo personali) non lo decifra: kzero controlla per B non vede il nome (e' tra i legati).
    p.amb.password = {kPwB};
    const Risultato b = comando({"controlla", p.chiuso.string(), "--utente", "B", "--includi-dati-personali"});
    EXPECT_EQ(b.uscita.find("ROSA"), std::string::npos);
}

TEST(ElencoComandi, ErroriDelComando) {
    Prova p;
    const fs::path f = p.xlsx("prima.xlsx", da_tsv("prima-lettura.tsv"));
    // --scrivi su un'altra cartella.
    p.amb.password = {kPw};
    EXPECT_EQ(errore_di([&] {
                  comando({"leggi-elenco", f.string(), p.chiuso.string(), "--profilo", "PR-CVA", "--anno-scolastico", "2026-2027", "--elenco",
                           "completo", "--utente", "A", "--scrivi", (p.lavoro / "altra").string()});
              }),
              "K-ERR-046");
    // Profilo che non c'e', anno scritto male, --elenco mancante, pacchetto in chiaro.
    EXPECT_EQ(errore_di([&] {
                  comando({"leggi-elenco", f.string(), p.chiuso.string(), "--profilo", "PR-NO", "--anno-scolastico", "2026-2027", "--elenco",
                           "completo", "--utente", "A"});
              }),
              "K-ERR-045");
    EXPECT_EQ(errore_di([&] {
                  comando({"leggi-elenco", f.string(), p.chiuso.string(), "--profilo", "PR-CVA", "--anno-scolastico", "2026-2028", "--elenco",
                           "completo", "--utente", "A"});
              }),
              "K-ERR-014");
    EXPECT_EQ(errore_di([&] {
                  comando({"leggi-elenco", f.string(), p.chiuso.string(), "--profilo", "PR-CVA", "--anno-scolastico", "2026-2027", "--utente", "A"});
              }),
              "K-ERR-013");
    EXPECT_EQ(errore_di([&] {
                  comando({"leggi-elenco", f.string(), (p.lavoro / "chiaro").string(), "--profilo", "PR-CVA", "--anno-scolastico", "2026-2027",
                           "--elenco", "completo", "--utente", "A"});
              }),
              "K-ERR-031");
    // File rotti: troncato, vecchio .xls, non trovato, titolo mancante.
    const std::string buono = leggi_testo(f);
    std::ofstream(p.lavoro / "troncato.xlsx", std::ios::binary) << buono.substr(0, buono.size() / 2);
    std::ofstream(p.lavoro / "vecchio.xls", std::ios::binary) << std::string("\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) << std::string(200, 'x');
    for (const auto& caso : std::vector<std::pair<std::string, std::string>>{
             {"troncato.xlsx", "K-ERR-042"}, {"vecchio.xls", "K-ERR-043"}, {"manca.xlsx", "K-ERR-041"}}) {
        const std::string nome = caso.first;
        EXPECT_EQ(errore_di([&] { p.leggi(p.lavoro / nome, "A", kPw, false); }), caso.second) << nome;
    }
    std::vector<std::string> titoli = kTitoli;
    titoli[6] = "ALUNNO";
    const fs::path senza = p.xlsx("senza.xlsx", {titoli});
    EXPECT_EQ(errore_di([&] { p.leggi(senza, "A", kPw, false); }), "K-ERR-044");
}

// kzero sblocco: aggiunge e toglie per l'utente e la tratta giusti (sabotaggio: sblocco tolto all'utente o alla tratta sbagliati),
// mai in chiaro su disco in un file temporaneo, e una lettura non lo cancella.
TEST(ElencoComandi, Sblocco) {
    Prova p;
    const fs::path f = p.xlsx("prima.xlsx", da_tsv("prima-lettura.tsv"));
    p.leggi(f, "A", kPw, true);
    const auto sblocco = [&](std::vector<std::string> a) {
        p.amb.password = {kPw};
        std::vector<std::string> x = {"sblocco", p.chiuso.string(), "--utente", "A", "--scrivi", p.chiuso.string()};
        x.insert(x.end(), a.begin(), a.end());
        return comando(x);
    };
    // U-000002 (VERDI) e' sospeso: si sblocca il ritorno del pomeriggio.
    EXPECT_EQ(sblocco({"--utente-elenco", "U-000002", "--tratta", "RP", "--riferimento", "mail del consorzio del 10/11", "--data", "2026-11-10"}).codice, 0);
    EXPECT_EQ(errore_di([&] { sblocco({"--utente-elenco", "U-000001", "--tratta", "RP", "--riferimento", "x", "--data", "2026-11-10"}); }), "K-ERR-049");
    EXPECT_EQ(errore_di([&] { sblocco({"--utente-elenco", "U-000002", "--tratta", "XX", "--riferimento", "x", "--data", "2026-11-10"}); }), "K-ERR-049");
    EXPECT_EQ(errore_di([&] { sblocco({"--utente-elenco", "U-000009", "--tratta", "RP", "--togli"}); }), "K-ERR-048");
    EXPECT_EQ(errore_di([&] { sblocco({"--utente-elenco", "U-000002", "--tratta", "AM", "--togli"}); }), "K-ERR-049");
    EXPECT_EQ(errore_di([&] { sblocco({"--utente-elenco", "U-000002", "--tratta", "RP", "--riferimento", "x", "--data", "2026-11-11"}); }), "K-ERR-049");
    EXPECT_EQ(errore_di([&] { sblocco({"--utente-elenco", "U-000002", "--tratta", "AM", "--riferimento", "x", "--togli"}); }), "K-ERR-015");
    const JsonDati d = JsonDati::parse(p.elenco_normale());
    const JsonDati& u = d.at("elenchi")[0].at("utenti")[1];
    EXPECT_EQ(u.at("codice"), "U-000002");
    ASSERT_EQ(u.at("sblocchi").size(), 1u);
    EXPECT_EQ(u.at("sblocchi")[0].at("tratta"), "RP");
    EXPECT_FALSE(d.at("elenchi")[0].at("utenti")[0].contains("sblocchi"));
    // Nessun file temporaneo rimasto nella cartella, nessun dato in chiaro (nomi) accanto alla cassaforte.
    for (const auto& e : fs::directory_iterator(p.chiuso)) {
        EXPECT_NE(e.path().filename().string()[0], '.') << e.path();
        if (e.path().extension() == ".json") {
            EXPECT_EQ(leggi_testo(e.path()).find("VERDI"), std::string::npos) << e.path();
        }
    }
    // La seconda lettura completa non ha VERDI? Si': c'e', e tiene lo sblocco.
    const fs::path g = p.xlsx("seconda.xlsx", da_tsv("seconda-lettura.tsv"));
    p.leggi(g, "A", kPw, true);
    const JsonDati d2 = JsonDati::parse(p.elenco_normale());
    EXPECT_EQ(d2.at("elenchi")[0].at("utenti")[1].at("sblocchi").size(), 1u);
    // Tolto: solo quello di VERDI, solo la tratta RP.
    EXPECT_EQ(sblocco({"--utente-elenco", "U-000002", "--tratta", "RP", "--togli"}).codice, 0);
    EXPECT_FALSE(JsonDati::parse(p.elenco_normale()).at("elenchi")[0].at("utenti")[1].contains("sblocchi"));
}

// Prova di scala: 5.000 utenti, lettura del file e confronto (il tempo si scrive nel DIARIO, dall'uscita della prova).
TEST(ElencoScala, CinquemilaUtenti) {
    const Lettura l = lettura_di_prova();
    std::vector<std::vector<std::string>> righe = {kTitoli};
    const char* scuole[] = {"Scuola primaria Valfonda", "Scuola media Valfonda", "Scuola infanzia Lagobello"};
    for (int i = 0; i < 5000; ++i)
        righe.push_back(riga(scuole[i % 3], "UTENTE " + std::to_string(i) + " DI PROVA", i % 7 ? "ATTIVO" : "SOSPESO - nota",
                             "AM/ALFA RM/DITTA GAMMA RP/LINEA C SCOL.", "AM: 08:00 LU-MA-ME-GI-VE | RM: 12:00 LU-MA-ME-GI-VE",
                             "//LU o 15.50 [15/09-09/06] //ME f1 15.50 [17/09-23/01] - f2 15.50 [28/01-05/06] //VE rm 12.00",
                             "via " + std::to_string(i % 97), std::to_string(i % 50 + 1)));
    const std::string file = xlsx::file_righe(righe);
    const auto t0 = std::chrono::steady_clock::now();
    const excel::Foglio f = excel::leggi_foglio(file, "");
    const auto t1 = std::chrono::steady_clock::now();
    ListRead r;
    const ListMerge m = leggi_e_confronta(l, righe, true, "PR-CVA", &r);
    const auto t2 = std::chrono::steady_clock::now();
    const Lettura l2 = applica(l, m);
    const auto t3 = std::chrono::steady_clock::now();
    const ListMerge m2 = leggi_e_confronta(l2, righe, true);
    const auto t4 = std::chrono::steady_clock::now();
    EXPECT_EQ(f.righe.size(), 5001u);
    EXPECT_TRUE(r.unreadable_rows.empty());
    EXPECT_EQ(conta(m, ListChange::Kind::New), 5000);
    EXPECT_EQ(conta(m2, ListChange::Kind::New), 0);
    const auto ms = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
    std::cout << "[ scala K-019 ] 5.000 utenti: file " << file.size() / 1024 << " KB; lettura del foglio " << ms(t0, t1)
              << " ms; righe e confronto (prima lettura) " << ms(t1, t2) << " ms; rilettura del pacchetto " << ms(t2, t3)
              << " ms; righe e confronto (seconda lettura, tutti ritrovati) " << ms(t3, t4) << " ms\n";
    EXPECT_LT(ms(t0, t4), 60000);
}

// Nessuna parola dei profili nel programma (sabotaggio: una parola del profilo scritta nel codice). Ogni testo tra virgolette
// dei sorgenti (src/) si confronta con le parole dei due profili di prova: titoli delle colonne, parole dello stato, sigle di
// tratte, giorni e periodi, ditte, foglio (di almeno due caratteri; i commenti non contano). Le parole stanno nei dati, il
// programma conosce solo le forme.
TEST(ElencoProfilo, NessunaParolaDelProfiloNelProgramma) {
    const JsonDati profili = JsonDati::parse(leggi_testo(kCartella + "/profili_elenchi.json"));
    std::set<std::string> parole;
    for (const JsonDati& p : profili.at("profili")) {
        for (const auto& [k, v] : p.at("colonne").items()) parole.insert(v.get<std::string>());
        for (const char* s : {"attivo", "sospeso"})
            for (const JsonDati& w : p.at("stato").at(s)) parole.insert(w.get<std::string>());
        for (const char* e : {"tratte", "giorni", "periodi"})
            for (const JsonDati& x : p.value(e, JsonDati::array())) parole.insert(x.at("sigla").get<std::string>());
        for (const JsonDati& x : p.value("ditte", JsonDati::array())) parole.insert(x.at("testo").get<std::string>());
        for (const JsonDati& x : p.value("orari", JsonDati::array())) parole.insert(x.at("colonna").get<std::string>());
        if (p.contains("foglio")) parole.insert(p.at("foglio").get<std::string>());
    }
    ASSERT_GT(parole.size(), 30u);
    std::string trovate;
    for (const auto& e : fs::recursive_directory_iterator(fs::path(KZERO_CARTELLA_PROVE) / ".." / "src")) {
        if (!e.is_regular_file()) continue;
        // Il codice senza i commenti di una riga (gli esempi nei commenti non sono il programma).
        std::string t;
        {
            std::istringstream in(leggi_testo(e.path()));
            std::string riga;
            while (std::getline(in, riga)) {
                bool in_testo = false;
                for (std::size_t k = 0; k < riga.size(); ++k) {
                    if (riga[k] == '\\' && in_testo) {
                        ++k;
                        continue;
                    }
                    if (riga[k] == '"') in_testo = !in_testo;
                    if (!in_testo && riga.compare(k, 2, "//") == 0) {
                        riga.resize(k);
                        break;
                    }
                }
                t += riga + "\n";
            }
        }
        // I testi tra virgolette (senza le sequenze \" dentro).
        for (std::size_t i = t.find('"'); i != std::string::npos; i = t.find('"', i + 1)) {
            std::size_t j = i + 1;
            while (j < t.size() && t[j] != '"' && t[j] != '\n') j += t[j] == '\\' ? 2 : 1;
            if (j >= t.size() || t[j] != '"') {
                i = j;
                continue;
            }
            const std::string s = t.substr(i + 1, j - i - 1);
            if (s.size() >= 2 && parole.count(s)) trovate += e.path().filename().string() + ": \"" + s + "\"\n";
            i = j;
        }
    }
    EXPECT_TRUE(trovate.empty()) << "Parole dei profili di prova scritte nel programma:\n" << trovate;
}
