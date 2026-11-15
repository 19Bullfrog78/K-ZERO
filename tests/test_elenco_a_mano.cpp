// K-Zero - prove delle modifiche a mano dell'elenco utenti (K-020): i campi scritti a mano letti con il profilo, le regole perche'
// una lettura del file non disfi mai in silenzio una modifica a mano (i tre casi, nome corretto, aggiunti, tolti, rimessi,
// cancellati, colonne degli orari, date). I campi dall'ingresso e i comandi: test_elenco_a_mano_comandi.cpp.
// Dati inventati (il demo con i profili e i codici esterni di tests/esempi/k019-elenco).
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "chiusura.hpp"
#include "elenco.hpp"
#include "elenco_a_mano_prove.hpp"
#include "elenco_scrittura.hpp"
#include "lettore.hpp"

using namespace kzero;
using namespace kzero::api;
using namespace kzero::prove::elenco_a_mano;
namespace fs = std::filesystem;

namespace {

Lettura lettura_di_prova() {
    Lettura l = leggi_pacchetto(pacchetto_di_prova());
    EXPECT_TRUE(l.forma_valida());
    return l;
}

const ReadingProfile& profilo(const Lettura& l) {
    for (const ReadingProfile& p : l.pacchetto.reading_profiles)
        if (p.code == "PR-CVA") return p;
    throw std::logic_error("profilo che non c'e'");
}

const std::vector<std::string> kTitoli = {"SCUOLA", "FERMATA", "COMUNE", "FRAZIONE", "VIA", "CIVICO", "UTENTE", "CLASSE",
                                          "SEZ.", "STATO", "PERCORSI", "ORARI MATTINA", "ORARI POMERIGGIO"};
const std::string kMattina = "AM: 08:00 LU-MA-ME-GI-VE | RM: 12:00 LU-MA-ME-GI-VE";
const std::string kPomeriggio = "//LU o 15.50 //VE rm 12.00";

std::vector<std::string> riga(const std::string& scuola, const std::string& nome, const std::string& via = "via Roma",
                              const std::string& civico = "3", const std::string& stato = "ATTIVO", const std::string& mattina = kMattina,
                              const std::string& pomeriggio = kPomeriggio) {
    return {scuola, "", "Pradello", "", via, civico, nome, "3", "A", stato, "AM/ALFA RM/ALFA RP/ALFA", mattina, pomeriggio};
}

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

const ClientList* lista_cva(const Lettura& l) {
    for (const ClientList& c : l.pacchetto.client_lists)
        if (c.client.code == "CVA") return &c;
    return nullptr;
}
const ListUser& utente(const Lettura& l, const std::string& codice) {
    for (const ListUser& u : lista_cva(l)->users)
        if (u.code == codice) return u;
    throw std::logic_error("utente che non c'e': " + codice);
}
bool c_e(const Lettura& l, const std::string& codice) {
    const ClientList* e = lista_cva(l);
    return e && std::any_of(e->users.begin(), e->users.end(), [&](const ListUser& u) { return u.code == codice; });
}

// Il documento come lo scrivono i comandi (elenco_scrittura.hpp), riletto: il giro intero dei dati, valori di prima compresi.
Lettura scrivi(const Lettura& prima, const ClientList& lista, int ultimo) {
    Lettura l = elenco::con_documento(prima, elenco::kFile, elenco::documento_con(prima, lista, ultimo));
    EXPECT_TRUE(l.forma_valida());
    return l;
}

struct Letto {
    ListRead r;
    ListMerge m;
    Lettura dopo;
};
Letto leggi(const Lettura& l, const std::vector<std::vector<std::string>>& righe, bool completo = true) {
    const Index idx(l.pacchetto);
    Letto x;
    x.r = read_list(tabella(righe), profilo(l), idx, 2026, lista_cva(l));
    EXPECT_EQ(x.r.fault, TableFault::None);
    x.m = merge_list(l.pacchetto, profilo(l), 2026, x.r, completo, std::string(64, 'a'));
    x.dopo = scrivi(l, x.m.list, x.m.last_number);
    return x;
}

ManualMark segno(int giorno) { return ManualMark{"mail del committente del " + std::to_string(giorno) + "/11", day_from_ymd(2026, 11, giorno)}; }

ManualParse campi_scritti(const Lettura& l, const std::vector<std::string>& righe, const ListUser* attuale, bool aggiungere = false) {
    const Index idx(l.pacchetto);
    return parse_manual(righe, profilo(l), idx, 2026, attuale, aggiungere);
}

// Una modifica a mano (cambio): il nucleo e la scrittura del documento.
Lettura cambia(const Lettura& l, const std::string& codice, const std::vector<std::string>& righe, int giorno,
               ManualOutcome* esito = nullptr) {
    ClientList lista = *lista_cva(l);
    const ManualParse in = campi_scritti(l, righe, &utente(l, codice));
    EXPECT_EQ(in.error.fault, ManualFault::None) << in.error.field;
    const ManualOutcome o = manual_change(lista, codice, in, segno(giorno));
    if (esito) *esito = o;
    return o.refusal == ManualRefusal::None ? scrivi(l, lista, l.pacchetto.list_last_number) : l;
}

std::vector<int> varianti(const std::vector<Problem>& v, Check c) {
    std::vector<int> out;
    for (const Problem& p : v)
        if (p.check == c) out.push_back(p.variant);
    return out;
}
std::vector<Problem> controlli(const Lettura& l) {
    std::vector<Problem> out;
    const Index idx(l.pacchetto);
    check_lists(l.pacchetto, idx, out);
    return out;
}
const ListChange& esito_di(const ListMerge& m, const std::string& codice) {
    for (const ListChange& c : m.changes)
        if (c.code == codice) return c;
    throw std::logic_error("esito che non c'e': " + codice);
}

// La prima lettura: ROSSI ANNA (U-000001), VERDI LUCA (U-000002), NERI MARCO (U-000003).
Lettura con_tre_utenti() {
    const Lettura l = lettura_di_prova();
    return leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")})
        .dopo;
}

}  // namespace

// --- I campi scritti a mano --------------------------------------------------------------------------------------------------

// Le notazioni del file, lette con il profilo: nessuna sintassi nuova. I nomi dei campi senza maiuscole; gli orari con il titolo della
// colonna.
TEST(ElencoAMano, CampiConLeNotazioniDelFile) {
    const Lettura l = con_tre_utenti();
    const ManualParse in = campi_scritti(l, {"Stato: SOSPESO - da attivare per il ritorno", "percorsi: AM/ALFA RM/DITTA GAMMA",
                                     "orari ORARI POMERIGGIO: //ME f1 15.50 [17/09-23/01] - f2 15.50 [28/01-05/06]", "fermata: PRADELLO PIAZZA"},
                                 &utente(l, "U-000001"));
    ASSERT_EQ(in.error.fault, ManualFault::None);
    EXPECT_FALSE(in.values.active);
    EXPECT_EQ(in.values.status_note, "da attivare per il ritorno");
    ASSERT_EQ(in.values.legs.size(), 2u);
    EXPECT_EQ(in.values.legs[1].other_company, "DITTA GAMMA");
    ASSERT_EQ(in.values.times.size(), 2u);
    EXPECT_EQ(in.values.times[0].column, "ORARI POMERIGGIO");
    ASSERT_TRUE(in.values.stop);
    EXPECT_EQ(in.values.stop->code, "F-PRADELLO");
    EXPECT_EQ(in.fields.size(), 4u);
}

// Campi sbagliati: niente cambia, e l'errore dice la riga e il campo, mai il valore (sabotaggio: parte della casa non scritta che si
// svuota).
TEST(ElencoAMano, CampiSbagliati) {
    const Lettura l = con_tre_utenti();
    const ListUser* u = &utente(l, "U-000001");
    const auto guasto = [&](const std::vector<std::string>& righe, bool aggiungere = false) {
        return campi_scritti(l, righe, aggiungere ? nullptr : u, aggiungere).error;
    };
    EXPECT_EQ(guasto({"telefono: 000 0000001"}).fault, ManualFault::UnknownField);
    EXPECT_EQ(guasto({"nome: A B", "NOME: A C"}).fault, ManualFault::RepeatedField);
    EXPECT_EQ(guasto({"orari POMERIGGIO: //LU o 15.50"}).fault, ManualFault::NotInProfile);
    EXPECT_EQ(guasto({"via: via dei Larici", "civico: 23"}).fault, ManualFault::HomePartMissing);   // mancano comune e frazione
    EXPECT_EQ(guasto({"via: via dei Larici", "civico: 23", "comune: Pradello"}).field, "frazione");
    EXPECT_EQ(guasto({"nome:"}).fault, ManualFault::RequiredEmpty);
    EXPECT_EQ(guasto({"nome: ROSA DEMO", "scuola: " + kPrim}, true).fault, ManualFault::RequiredMissing);
    EXPECT_EQ(guasto({}).fault, ManualFault::NoFields);
    const ManualError e = guasto({"stato: ATTIVO", "percorsi: AM/ALFA AM/BETA TRASPORTI"});
    EXPECT_EQ(e.fault, ManualFault::Value);
    EXPECT_EQ(e.line, 2);
    EXPECT_EQ(e.field, "percorsi");
    EXPECT_EQ(e.row_fault, RowFault::RoutesLegTwice);
    EXPECT_EQ(guasto({"scuola: Scuola che non c'e'"}).row_fault, RowFault::SchoolUnknown);
    EXPECT_EQ(guasto({"orari ORARI MATTINA: AM: 25:00 LU"}).row_fault, RowFault::TimesHour);
    // Una parte della casa vuota si scrive apposta: allora vale vuota.
    const ManualParse buono = campi_scritti(l, {"comune: Pradello", "frazione:", "via: via dei Larici", "civico: 23"}, u);
    ASSERT_EQ(buono.error.fault, ManualFault::None);
    EXPECT_EQ(buono.values.home->hamlet, "");
}

// --- I tre casi del punto 2 ------------------------------------------------------------------------------------------------------

// Cambio a mano della casa, poi tre file diversi. Sabotaggi: lettura che rimette il valore di prima sopra un valore a mano; terzo
// valore del file che non vince; valore di prima che resta dopo la conferma.
TEST(ElencoAMano, TreCasiDopoUnCambioAMano) {
    const Lettura l0 = con_tre_utenti();
    const Lettura l = cambia(l0, "U-000001", kCasaNuova, 12);
    const ManualField* m = utente(l, "U-000001").manual_field(ListField::Home);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->before->home->street, "via Roma");   // il valore di prima: quello del file
    EXPECT_EQ(utente(l, "U-000001").home->street, "via dei Larici");
    EXPECT_EQ(varianti(controlli(l), Check::ListManualPending), std::vector<int>{0});
    // 1. Il file ha ancora il valore di prima: non e' aggiornato, resta il valore a mano.
    const Letto vecchio = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(utente(vecchio.dopo, "U-000001").home->street, "via dei Larici");
    EXPECT_NE(utente(vecchio.dopo, "U-000001").manual_field(ListField::Home), nullptr);
    EXPECT_EQ(varianti(vecchio.m.warnings, Check::ListFileNotUpdated), std::vector<int>{0});
    EXPECT_EQ(esito_di(vecchio.m, "U-000001").kind, ListChange::Kind::Unchanged);
    // 2. Il file ha il valore a mano: confermato, la modifica diventa normale e il valore di prima si cancella.
    std::vector<std::string> nuova = riga(kPrim, "ROSSI ANNA", "via dei Larici", "23");
    nuova[3] = "Croce";
    const Letto conferma = leggi(l, {kTitoli, nuova, riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(utente(conferma.dopo, "U-000001").home->street, "via dei Larici");
    EXPECT_TRUE(utente(conferma.dopo, "U-000001").manual.empty());
    EXPECT_EQ(esito_di(conferma.m, "U-000001").confirmed, std::vector<std::string>{"casa"});
    EXPECT_TRUE(varianti(conferma.m.warnings, Check::ListFileNotUpdated).empty());
    EXPECT_TRUE(varianti(controlli(conferma.dopo), Check::ListManualPending).empty());
    EXPECT_EQ(conferma.dopo.documenti.at("elenco_utenti.json").dump().find("via Roma"), std::string::npos);
    // 3. Il file ha un terzo valore: il committente l'ha cambiato dopo, vince il file.
    const Letto terzo = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA", "via del Sole", "1"), riga(kPrim, "VERDI LUCA", "via Larga", "5"),
                                  riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(utente(terzo.dopo, "U-000001").home->street, "via del Sole");
    EXPECT_TRUE(utente(terzo.dopo, "U-000001").manual.empty());
    EXPECT_EQ(varianti(terzo.m.warnings, Check::ListFileNewerValue), std::vector<int>{0});
    EXPECT_EQ(esito_di(terzo.m, "U-000001").fields, std::vector<std::string>{"casa"});
}

// Piu' modifiche a mano dello stesso campo: il valore di prima resta l'ultimo del file; rimesso a mano il valore di prima, la
// modifica si toglie. Date: una modifica prima dell'ultima dello stesso campo e' un errore, lo stesso giorno va bene (sabotaggio:
// modifica con una data prima dell'ultima accettata).
TEST(ElencoAMano, PiuModificheEDate) {
    const Lettura l0 = con_tre_utenti();
    const Lettura l1 = cambia(l0, "U-000001", kCasaNuova, 12);
    ManualOutcome o;
    const Lettura l2 = cambia(l1, "U-000001", {"comune: Pradello", "frazione:", "via: via del Mulino", "civico: 4"}, 13, &o);
    ASSERT_EQ(o.refusal, ManualRefusal::None);
    EXPECT_EQ(o.changes.at(0).memory, ManualFieldChange::Memory::Updated);
    EXPECT_EQ(utente(l2, "U-000001").manual_field(ListField::Home)->before->home->street, "via Roma");
    EXPECT_EQ(utente(l2, "U-000001").manual_field(ListField::Home)->mark.date, day_from_ymd(2026, 11, 13));
    // Prima dell'ultima: errore, niente cambia; il messaggio ha la data dell'ultima.
    cambia(l2, "U-000001", kCasaNuova, 11, &o);
    EXPECT_EQ(o.refusal, ManualRefusal::DateBeforeLast);
    EXPECT_EQ(o.last_date, day_from_ymd(2026, 11, 13));
    EXPECT_EQ(o.field, "casa");
    // Lo stesso giorno va bene; un altro campo con una data qualunque anche.
    cambia(l2, "U-000001", kCasaNuova, 13, &o);
    EXPECT_EQ(o.refusal, ManualRefusal::None);
    cambia(l2, "U-000001", {"sezione: B"}, 1, &o);
    EXPECT_EQ(o.refusal, ManualRefusal::None);
    // Rimesso il valore di prima: la modifica si toglie.
    const Lettura l3 = cambia(l2, "U-000001", {"comune: Pradello", "frazione:", "via: via Roma", "civico: 3"}, 14, &o);
    EXPECT_EQ(o.changes.at(0).memory, ManualFieldChange::Memory::Dropped);
    EXPECT_TRUE(utente(l3, "U-000001").manual.empty());
    // Niente da cambiare.
    cambia(l3, "U-000001", {"sezione: A"}, 14, &o);
    EXPECT_EQ(o.refusal, ManualRefusal::NothingChanges);
}

// Gli orari una colonna alla volta (D1): una colonna non scritta resta com'e'; cambio a mano del pomeriggio, poi un file con la sola
// mattina cambiata: la mattina vale il file, il pomeriggio a mano resta (sabotaggio: colonna degli orari non scritta che si svuota).
TEST(ElencoAMano, OrariColonnaPerColonna) {
    const Lettura l0 = con_tre_utenti();
    const Lettura l = cambia(l0, "U-000001", {"orari ORARI POMERIGGIO: //LU o 16.10 //VE rm 12.00"}, 12);
    const ListUser u = utente(l, "U-000001");
    std::size_t mattina = 0, pomeriggio = 0;
    for (const ListUserTime& t : u.times) (t.column == "ORARI MATTINA" ? mattina : pomeriggio)++;
    EXPECT_EQ(mattina, 10u);   // la mattina non scritta resta com'e'
    EXPECT_EQ(pomeriggio, 2u);
    // Il file: mattina cambiata dal committente, pomeriggio ancora quello di prima.
    const Letto f = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA", "via Roma", "3", "ATTIVO", "AM: 07:55 LU-MA-ME-GI-VE | RM: 12:00 LU-MA-ME-GI-VE"),
                              riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    const ListUser v = utente(f.dopo, "U-000001");
    bool sedici_e_dieci = false, sette_e_55 = false;
    for (const ListUserTime& t : v.times) {
        sedici_e_dieci = sedici_e_dieci || t.time == 16 * 60 + 10;
        sette_e_55 = sette_e_55 || t.time == 7 * 60 + 55;
    }
    EXPECT_TRUE(sedici_e_dieci);   // il pomeriggio a mano, piu' nuovo, resta
    EXPECT_TRUE(sette_e_55);       // la mattina del file
    EXPECT_NE(v.manual_field(ListField::Times, "ORARI POMERIGGIO"), nullptr);
    ASSERT_EQ(f.m.warnings.size(), 1u);
    EXPECT_EQ(f.m.warnings[0].check, Check::ListFileNotUpdated);
    // Una colonna che il profilo non ha piu': vince il file, con l'avviso (variante 1).
    ClientList lista = *lista_cva(l);
    for (ListUser& x : lista.users)
        for (ManualField& m : x.manual) m.column = "ORARI VECCHI";
    const Lettura l2 = scrivi(l, lista, l.pacchetto.list_last_number);
    const Letto g = leggi(l2, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(varianti(g.m.warnings, Check::ListFileNewerValue), std::vector<int>{1});
    EXPECT_TRUE(utente(g.dopo, "U-000001").manual.empty());
}

// Orari letti con la versione 1 (senza colonna): una colonna sola non si cambia fino alla prossima lettura; gli altri campi si'.
TEST(ElencoAMano, OrariDellaVersione1) {
    const Lettura l0 = con_tre_utenti();
    ClientList lista = *lista_cva(l0);
    for (ListUser& u : lista.users)
        for (ListUserTime& t : u.times) t.column.clear();
    const Lettura l = scrivi(l0, lista, l0.pacchetto.list_last_number);
    ManualOutcome o;
    cambia(l, "U-000001", {"orari ORARI MATTINA: AM: 07:55 LU"}, 12, &o);
    EXPECT_EQ(o.refusal, ManualRefusal::TimesWithoutColumn);
    EXPECT_EQ(o.field, "orari ORARI MATTINA");
    cambia(l, "U-000001", kCasaNuova, 12, &o);
    EXPECT_EQ(o.refusal, ManualRefusal::None);
    // Dopo una lettura le colonne ci sono, e senza nessun campo "cambiato" in piu' (la colonna dice solo da dove vengono).
    const Letto r = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(esito_di(r.m, "U-000001").kind, ListChange::Kind::Unchanged);
    for (const ListUserTime& t : utente(r.dopo, "U-000001").times) EXPECT_FALSE(t.column.empty());
}

// --- Nome corretto a mano ----------------------------------------------------------------------------------------------------------

// La lettura ritrova l'utente anche con il nome di prima: mai "uscito + nuovo" per una correzione (sabotaggio). Due righe, una con il
// nome di oggi e una con quello di prima, si fermano tutte e due e contano come illeggibili (D4; sabotaggio: le uscite passano).
TEST(ElencoAMano, NomeCorrettoAMano) {
    const Lettura l0 = con_tre_utenti();
    const Lettura l = cambia(l0, "U-000002", {"nome: VERDI LUCIO"}, 12);
    // Il file non e' aggiornato: ha ancora il nome di prima.
    const Letto vecchio = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(esito_di(vecchio.m, "U-000002").kind, ListChange::Kind::Unchanged);
    EXPECT_EQ(std::count_if(vecchio.m.changes.begin(), vecchio.m.changes.end(), [](const ListChange& c) { return c.kind == ListChange::Kind::New; }), 0);
    EXPECT_EQ(utente(vecchio.dopo, "U-000002").name, "VERDI LUCIO");
    EXPECT_EQ(varianti(vecchio.m.warnings, Check::ListFileNotUpdated), std::vector<int>{0});
    // Il file e' aggiornato: confermato.
    const Letto nuovo = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCIO", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(esito_di(nuovo.m, "U-000002").confirmed, std::vector<std::string>{"nome"});
    EXPECT_TRUE(utente(nuovo.dopo, "U-000002").manual.empty());
    // Tutte e due le righe: ferme, illeggibili, nessuna uscita (NERI non c'e' nel file e resta).
    const Letto due = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "VERDI LUCIO", "via Larga", "5")});
    EXPECT_EQ(due.r.unreadable_rows, (std::set<int>{3, 4}));
    EXPECT_EQ(varianti(due.r.problems, Check::ListRowHomonyms), std::vector<int>{1});
    EXPECT_FALSE(due.m.exits_applied);
    EXPECT_EQ(esito_di(due.m, "U-000002").kind, ListChange::Kind::Unread);
    EXPECT_TRUE(c_e(due.dopo, "U-000003"));
    EXPECT_EQ(utente(due.dopo, "U-000002").name, "VERDI LUCIO");
    // Un utente nuovo con il nome di prima di un'altra correzione: errore (omonimo).
    ClientList lista = *lista_cva(l);
    const ManualParse in = campi_scritti(l, {"nome: VERDI LUCA", "scuola: " + kPrim, "stato: ATTIVO"}, nullptr, true);
    const ManualOutcome o = manual_add(l.pacchetto, lista, in, segno(13));
    EXPECT_EQ(o.refusal, ManualRefusal::Homonym);
    EXPECT_EQ(o.other, "U-000002");
}

// --- Utenti aggiunti, tolti, rimessi, cancellati --------------------------------------------------------------------------------

// Aggiunto a mano: codice nuovo in ordine (mai dal nome); resta anche se una lettura completa non lo ha; quando il file lo porta vale
// il file, tranne i campi cambiati a mano dopo l'aggiunta (D2). Sabotaggi: aggiunto a mano tolto da una lettura completa; codice
// nuovo ricavato dal nome.
TEST(ElencoAMano, AggiuntoAMano) {
    const Lettura l0 = con_tre_utenti();
    ClientList lista = *lista_cva(l0);
    const ManualParse in = campi_scritti(l0, {"nome: ROSA DEMO", "scuola: " + kPrim, "stato: ATTIVO", "percorsi: AM/ALFA", "comune: Pradello", "frazione:",
                                      "via: via dei Mille", "civico: 7"},
                                 nullptr, true);
    ASSERT_EQ(in.error.fault, ManualFault::None);
    const ManualOutcome o = manual_add(l0.pacchetto, lista, in, segno(10));
    ASSERT_EQ(o.refusal, ManualRefusal::None);
    EXPECT_EQ(o.code, "U-000004");
    EXPECT_EQ(o.last_number, 4);
    const Lettura l = scrivi(l0, lista, o.last_number);
    EXPECT_EQ(varianti(controlli(l), Check::ListAddedByHand), std::vector<int>{0});
    // Un altro nome: il codice dopo, non dal nome.
    ClientList lista2 = *lista_cva(l);
    const ManualOutcome o2 = manual_add(l.pacchetto, lista2, campi_scritti(l, {"nome: ABATE ZENO", "scuola: " + kPrim, "stato: ATTIVO"}, nullptr, true), segno(10));
    EXPECT_EQ(o2.code, "U-000005");
    // Una lettura completa senza di lui: resta.
    const std::vector<std::string> r1 = riga(kPrim, "ROSSI ANNA"), r2 = riga(kPrim, "VERDI LUCA", "via Larga", "5"), r3 = riga(kPrim, "NERI MARCO", "via Alta", "9");
    const Letto senza = leggi(l, {kTitoli, r1, r2, r3});
    EXPECT_TRUE(senza.m.exits_applied);
    EXPECT_TRUE(c_e(senza.dopo, "U-000004"));
    EXPECT_EQ(esito_di(senza.m, "U-000004").kind, ListChange::Kind::ManualNotInFile);
    // Cambio della casa dopo l'aggiunta (D2), poi il file con la casa dell'aggiunta e un'altra sezione.
    const Lettura l2 = cambia(l, "U-000004", {"comune: Pradello", "frazione:", "via: via Nuova", "civico: 1"}, 12);
    std::vector<std::string> nel_file = riga(kPrim, "ROSA DEMO", "via dei Mille", "7");
    nel_file[8] = "B";
    const Letto con = leggi(l2, {kTitoli, r1, r2, r3, nel_file});
    const ListUser u = utente(con.dopo, "U-000004");
    EXPECT_FALSE(u.added);
    EXPECT_EQ(u.home->street, "via Nuova");   // cambiato dopo l'aggiunta: il file ha il valore dell'aggiunta, non e' aggiornato
    EXPECT_EQ(u.section, "B");                 // non cambiato dopo l'aggiunta: vale il file
    EXPECT_EQ(varianti(con.m.warnings, Check::ListFileNotUpdated), std::vector<int>{0});
    EXPECT_FALSE(varianti(con.m.warnings, Check::ListAddedNowInFile).empty());
    // Una data prima dell'aggiunta: errore.
    ManualOutcome rif;
    cambia(l, "U-000004", {"sezione: C"}, 9, &rif);
    EXPECT_EQ(rif.refusal, ManualRefusal::DateBeforeLast);
}

// Tolto a mano: subito fuori dal piano; il file che lo ha ancora non lo rimette (ne' ne aggiorna i campi); una lettura parziale o con
// righe illeggibili non lo toglie; la prima completa senza di lui lo toglie davvero, con gli sblocchi. Sabotaggi: tolto a mano
// rimesso nel piano dal file; tolto a mano mai tolto davvero.
TEST(ElencoAMano, ToltoAMano) {
    const Lettura l0 = con_tre_utenti();
    ClientList lista = *lista_cva(l0);
    for (ListUser& u : lista.users)
        if (u.code == "U-000003") u.unlocks.push_back({"AM", "mail del 2/11", day_from_ymd(2026, 11, 2), {}});
    const ManualOutcome o = manual_remove(lista, "U-000003", segno(12));
    EXPECT_EQ(o.unlocks, 1u);
    const Lettura l = scrivi(l0, lista, l0.pacchetto.list_last_number);
    EXPECT_FALSE(utente(l, "U-000003").in_plan());
    EXPECT_EQ(varianti(controlli(l), Check::ListRemovedByHand), std::vector<int>{1});
    const std::vector<std::string> r1 = riga(kPrim, "ROSSI ANNA"), r2 = riga(kPrim, "VERDI LUCA", "via Larga", "5");
    // Il file lo ha ancora (con una casa nuova): resta fuori, com'era.
    const Letto ancora = leggi(l, {kTitoli, r1, r2, riga(kPrim, "NERI MARCO", "via Bassa", "1")});
    EXPECT_FALSE(utente(ancora.dopo, "U-000003").in_plan());
    EXPECT_EQ(utente(ancora.dopo, "U-000003").home->street, "via Alta");
    EXPECT_EQ(esito_di(ancora.m, "U-000003").kind, ListChange::Kind::RemovedInFile);
    EXPECT_EQ(varianti(ancora.m.warnings, Check::ListRemovedStillInFile), std::vector<int>{0});
    // Parziale senza di lui: resta (tolto).
    const Letto parziale = leggi(l, {kTitoli, r1}, false);
    EXPECT_TRUE(c_e(parziale.dopo, "U-000003"));
    // Completo con una riga illeggibile: resta.
    const Letto illeggibile = leggi(l, {kTitoli, r1, riga(kPrim, "VERDI LUCA", "via Larga", "5", "BOH")});
    EXPECT_TRUE(c_e(illeggibile.dopo, "U-000003"));
    // Completo e leggibile senza di lui: se ne va davvero, con lo sblocco.
    const Letto via = leggi(l, {kTitoli, r1, r2});
    EXPECT_FALSE(c_e(via.dopo, "U-000003"));
    EXPECT_EQ(esito_di(via.m, "U-000003").kind, ListChange::Kind::RemovedGone);
    // Annullare: torna nel piano; e dopo una lettura con lui e' un utente normale.
    ClientList lista2 = *lista_cva(l);
    EXPECT_EQ(manual_restore(lista2, "U-000003", segno(11)).refusal, ManualRefusal::DateBeforeLast);
    EXPECT_EQ(manual_restore(lista2, "U-000003", segno(13)).refusal, ManualRefusal::None);
    const Lettura l3 = scrivi(l, lista2, l.pacchetto.list_last_number);
    EXPECT_TRUE(utente(l3, "U-000003").in_plan());
    // Cambiare un utente tolto: prima rimetterlo.
    ManualOutcome rif;
    cambia(l, "U-000003", {"sezione: C"}, 13, &rif);
    EXPECT_EQ(rif.refusal, ManualRefusal::UserRemoved);
}

// Un utente fuori elenco (uscito, tenuto per uno sblocco) rimesso a mano nell'elenco (D3): una lettura completa senza di lui non lo
// toglie (il file non e' aggiornato) e l'avviso resta anche in kzero controlla; il file che lo porta conferma.
TEST(ElencoAMano, FuoriElencoRimessoAMano) {
    const Lettura l0 = con_tre_utenti();
    ClientList lista = *lista_cva(l0);
    for (ListUser& u : lista.users)
        if (u.code == "U-000003") {
            u.out_of_list = true;
            u.unlocks.push_back({"AM", "mail del 2/11", day_from_ymd(2026, 11, 2), {}});
        }
    EXPECT_EQ(manual_restore(lista, "U-000001", segno(12)).refusal, ManualRefusal::NotRemovedNorOut);
    const ManualOutcome o = manual_restore(lista, "U-000003", segno(12));
    ASSERT_EQ(o.refusal, ManualRefusal::None);
    const Lettura l = scrivi(l0, lista, l0.pacchetto.list_last_number);
    EXPECT_TRUE(utente(l, "U-000003").in_plan());
    EXPECT_EQ(varianti(controlli(l), Check::ListManualPending), std::vector<int>{1});
    EXPECT_TRUE(varianti(controlli(l), Check::ListUserOutOfList).empty());
    const std::vector<std::string> r1 = riga(kPrim, "ROSSI ANNA"), r2 = riga(kPrim, "VERDI LUCA", "via Larga", "5");
    const Letto senza = leggi(l, {kTitoli, r1, r2});
    EXPECT_TRUE(utente(senza.dopo, "U-000003").in_plan());
    EXPECT_EQ(utente(senza.dopo, "U-000003").unlocks.size(), 1u);
    EXPECT_EQ(varianti(senza.m.warnings, Check::ListFileNotUpdated), std::vector<int>{1});
    EXPECT_EQ(varianti(controlli(senza.dopo), Check::ListManualPending), std::vector<int>{1});
    const Letto con = leggi(l, {kTitoli, r1, r2, riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(esito_di(con.m, "U-000003").confirmed, std::vector<std::string>{"fuori_elenco"});
    EXPECT_TRUE(utente(con.dopo, "U-000003").manual.empty());
}

// Cancellato: subito da tutto il pacchetto, con gli sblocchi; il suo codice non si da' mai a un utente nuovo (sabotaggio).
TEST(ElencoAMano, CancellatoIlCodiceNonTorna) {
    const Lettura l0 = con_tre_utenti();
    ClientList lista = *lista_cva(l0);
    manual_delete(lista, "U-000003");
    const Lettura l = scrivi(l0, lista, l0.pacchetto.list_last_number);
    EXPECT_FALSE(c_e(l, "U-000003"));
    EXPECT_EQ(l.pacchetto.list_last_number, 3);
    ClientList lista2 = *lista_cva(l);
    const ManualOutcome o = manual_add(l.pacchetto, lista2, campi_scritti(l, {"nome: NERI MARCO", "scuola: " + kPrim, "stato: ATTIVO"}, nullptr, true), segno(13));
    EXPECT_EQ(o.code, "U-000004");
    // Anche la lettura: il file che lo ha ancora lo aggiunge come utente nuovo, con un codice nuovo.
    const Letto r = leggi(l, {kTitoli, riga(kPrim, "ROSSI ANNA"), riga(kPrim, "VERDI LUCA", "via Larga", "5"), riga(kPrim, "NERI MARCO", "via Alta", "9")});
    EXPECT_EQ(esito_di(r.m, "U-000004").kind, ListChange::Kind::New);
    EXPECT_FALSE(c_e(r.dopo, "U-000003"));
}

// Caso trovato dal fuzzing: l'aggiunta salta anche i codici dell'elenco che cambia, se quell'elenco non e' ancora nel pacchetto.
TEST(ElencoAMano, AggiuntaSaltaICodiciDellElencoCheCambia) {
    const Lettura l = lettura_di_prova();   // nessun elenco nel pacchetto
    ClientList lista;
    lista.client = Ref{"CVA", {}};
    lista.profile = Ref{"PR-CVA", {}};
    lista.first_year = 2026;
    ListUser u;
    u.code = "U-000001";
    u.name = "PROVA UNO";
    u.school = Ref{"SC-PRIM", {}};
    lista.users.push_back(u);
    const ManualOutcome o = manual_add(l.pacchetto, lista, campi_scritti(l, {"nome: ROSA DEMO", "scuola: " + kPrim, "stato: ATTIVO"}, nullptr, true), segno(10));
    ASSERT_EQ(o.refusal, ManualRefusal::None);
    EXPECT_EQ(o.code, "U-000002");
}
