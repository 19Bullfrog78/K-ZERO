// K-Zero - prove dell'avviso sui nomi liberi (K-018, src/api/testi_liberi.hpp): nomi di persone e di case e parole sulla
// salute nei testi comuni, luoghi forse sensibili, falsi allarmi, e chi vede cosa con la cassaforte (i tre utenti: tutti e
// due i permessi, solo i dati personali, solo i particolari). Pacchetti in memoria fatti dal demo (dati inventati).
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "chiusura.hpp"
#include "classi.hpp"
#include "controlli_catalogo.hpp"
#include "controllo.hpp"
#include "formato_dati.hpp"
#include "lettore.hpp"
#include "testi_liberi.hpp"

using namespace kzero::api;
using kzero::Check;

namespace {

class InMemoria : public Sorgente {
public:
    std::vector<Sorgente::Voce> voci() const override {
        std::vector<Sorgente::Voce> v;
        for (const auto& [nome, testo] : file) v.push_back({nome, false});
        return v;
    }
    FileLetto leggi(const std::string& nome, std::uintmax_t massimo) const override {
        FileLetto out;
        const auto it = file.find(nome);
        if (it == file.end()) return out;
        if (it->second.size() > massimo) {
            out.esito = FileLetto::Esito::TroppoGrande;
            return out;
        }
        out.esito = FileLetto::Esito::Letto;
        out.testo = it->second;
        return out;
    }
    JsonDati json(const std::string& f) const { return JsonDati::parse(file.at(f)); }
    void scrivi(const std::string& f, const JsonDati& d) { file[f] = d.dump(2); }
    std::map<std::string, std::string> file;
};

InMemoria demo() {
    InMemoria m;
    const PacchettoIncorporato d("esempi/demo");
    for (const auto& v : d.voci()) m.file[v.nome] = d.leggi(v.nome, kMassimoByteFile).testo;
    return m;
}

JsonDati& per_codice(JsonDati& elenco, const std::string& codice) {
    for (JsonDati& x : elenco)
        if (x.value("codice", "") == codice) return x;
    throw std::logic_error("codice non trovato: " + codice);
}

// Cambia un campo di un elemento con il codice in un elenco principale.
void imposta(InMemoria& m, const std::string& file, const std::string& elenco, const std::string& codice, const std::string& campo,
             const JsonDati& valore) {
    JsonDati d = m.json(file);
    per_codice(d.at(elenco), codice)[campo] = valore;
    m.scrivi(file, d);
}

// I problemi K-018 calcolati (prima del filtro delle uscite), come "codice variante file puntatore".
std::vector<std::string> grezzi(const InMemoria& m) {
    const Lettura l = leggi_pacchetto(m);
    EXPECT_TRUE(l.forma_valida());
    std::vector<std::string> out;
    for (const kzero::Problem& p : controlla_testi_liberi(l))
        out.push_back(definizione_controllo(p.check).codice + " " + std::to_string(p.variant) + " " + p.where.file + " " + p.where.pointer);
    std::sort(out.begin(), out.end());
    return out;
}

// I problemi K-018 come li mostra kzero controlla con quella visibilita' (codice e messaggio), e il resoconto.
struct Visti {
    std::vector<std::string> righe;
    Resoconto r;
};
Visti visti(const InMemoria& m, Visibilita v) {
    Visti out{{}, controlla_pacchetto(m, v)};
    for (const ProblemaPresentato& p : out.r.problemi)
        if (p.codice >= "K-CTR-136" && p.codice <= "K-CTR-138") out.righe.push_back(p.codice + " " + p.messaggio);
    return out;
}

bool contiene(const std::vector<std::string>& v, const std::string& testo) {
    return std::any_of(v.begin(), v.end(), [&](const std::string& x) { return x.find(testo) != std::string::npos; });
}

// Il demo con il caso "Matrimonio Rossi": il committente privato CP-01 si chiama Rossi Marta e il suo servizio "Matrimonio Rossi".
InMemoria matrimonio_rossi() {
    InMemoria m = demo();
    JsonDati c = m.json("committenti.json");
    per_codice(c.at("committenti_privati"), "CP-01")["nome"] = "Rossi Marta";
    m.scrivi("committenti.json", c);
    imposta(m, "servizi.json", "servizi", "S-MATRIMONIO", "nome", "Matrimonio Rossi");
    return m;
}

const std::string kId = "00112233445566778899aabbccddeeff";

struct Chiuso {
    InMemoria normali;
    std::vector<Voce> voci;
};

Chiuso chiudi(const InMemoria& chiaro) {
    const Lettura l = leggi_pacchetto(chiaro);
    if (!l.forma_valida()) throw std::logic_error("pacchetto con errori di forma");
    chiusura::Separazione s = chiusura::separa(l);
    Chiuso c;
    for (auto& [file, doc] : s.normali) {
        const JsonDati d = file == "pacchetto.json" ? chiusura::manifesto_chiuso(doc, kId, 1) : doc;
        c.normali.file[file] = chiusura::testo_file(d);
    }
    c.normali.file["cassaforte.kzc"] = "finta";
    for (Voce& v : s.contenuto.personali) c.voci.push_back(std::move(v));
    for (Voce& v : s.contenuto.particolari) c.voci.push_back(std::move(v));
    for (Voce& v : s.contenuto.legati) c.voci.push_back(std::move(v));
    return c;
}

// Come la cassaforte vera: i valori legati a un luogo sensibile solo a chi ha tutti e due i permessi.
ValoriCassaforte valori(const Chiuso& c, Visibilita leggibili) {
    ValoriCassaforte v;
    v.leggibili = leggibili;
    v.id = kId;
    v.generazione = 1;
    for (const Voce& x : c.voci) {
        const bool apre = x.legato                         ? leggibili.personali && leggibili.particolari
                          : x.classe == Classe::Particolare ? leggibili.particolari
                                                            : leggibili.personali;
        if (apre) v.voci.push_back(&x);
    }
    return v;
}

}  // namespace

// Le parole dei confronti: la normalizzazione della ricerca (maiuscole, accenti, punteggiatura), e lettere e cifre separate.
TEST(TestiLiberi, ParoleDelTesto) {
    EXPECT_EQ(parole_del_testo("Nicolò D'Àngelo, via Rossi2"),
              (std::vector<std::string>{"NICOLO", "D", "ANGELO", "VIA", "ROSSI", "2"}));
    EXPECT_EQ(parole_del_testo("S-VAL-3"), (std::vector<std::string>{"S", "VAL", "3"}));
    EXPECT_TRUE(parole_del_testo(" - , ").empty());
}

// Il caso del mandato: "Matrimonio Rossi" e' un testo delle cose del committente privato CP-01 (il nome del suo servizio) e
// contiene una parola del suo nome. Il messaggio dice il codice CP-01 solo con i dati personali, mai il nome come sta nei dati.
TEST(TestiLiberi, MatrimonioRossi) {
    const InMemoria m = matrimonio_rossi();
    const std::vector<std::string> g = grezzi(m);
    EXPECT_EQ(g, (std::vector<std::string>{"K-CTR-136 1 servizi.json /servizi/9/nome", "K-CTR-137 0 blocchi.json /blocchi/0/nota"}));
    const Visti tutto = visti(m, Visibilita{true, true});
    ASSERT_TRUE(contiene(tutto.righe, "K-CTR-136 Il testo \"Matrimonio Rossi\" (servizi[].nome di S-MATRIMONIO) e' un testo delle cose "
                                      "del committente privato CP-01 e contiene una parola del suo nome (ROSSI)."))
        << ::testing::PrintToString(tutto.righe);
    EXPECT_FALSE(contiene(tutto.righe, "Rossi Marta"));
    // Di base (senza --includi-dati-personali): il testo e dove sta, mai il codice della persona.
    const Visti base = visti(m, Visibilita{false, false});
    ASSERT_TRUE(contiene(base.righe, "K-CTR-136 Il testo \"Matrimonio Rossi\" (servizi[].nome di S-MATRIMONIO) e' un testo delle cose di "
                                     "una persona o di una casa dei dati (quale: dati personali, riservati) e contiene una parola del suo "
                                     "nome (riservata)."))
        << ::testing::PrintToString(base.righe);
    EXPECT_FALSE(contiene(base.righe, "CP-01"));
    // Una parola sola del nome conta solo nei testi delle cose della persona: "Rossi" nel nome di un calendario no (e
    // nemmeno per P-A "Rossi Mario" del personale e P-A1 "Rossi Giulia" passeggero: il calendario non e' delle loro cose).
    InMemoria cal = matrimonio_rossi();
    JsonDati c = cal.json("calendari.json");
    c.at("calendari")[0]["nome"] = "Giorni di via Rossi";
    cal.scrivi("calendari.json", c);
    EXPECT_EQ(grezzi(cal), g);
}

// Il nome intero: tutte le sue parole, una dopo l'altra in qualsiasi ordine, o attaccate in una parola sola; senza maiuscole ne'
// accenti. In qualunque testo comune.
TEST(TestiLiberi, NomeInteroInQualsiasiOrdineEAccenti) {
    InMemoria m = demo();
    JsonDati p = m.json("persone.json");
    per_codice(p.at("persone"), "P-A")["nome"] = "Rossi Nicolò";
    m.scrivi("persone.json", p);
    JsonDati c = m.json("calendari.json");
    c.at("calendari")[0]["nome"] = "Calendario di NICOLO ROSSI";
    c.at("calendari")[1]["nome"] = "Calendario di Rossi, nicolo";
    c.at("calendari")[2]["codice"] = "CAL-NICOLOROSSI";
    c.at("calendari")[3]["nome"] = "Calendario di Nicolo e di Rossi";   // non una dopo l'altra: no
    m.scrivi("calendari.json", c);
    // CAL-NICOLOROSSI e' usato altrove: basta il nome nei testi, la forma dei dati non conta (si guarda solo il calendario).
    const Lettura l = leggi_pacchetto(m);
    std::vector<std::string> trovati;
    for (const kzero::Problem& x : controlla_testi_liberi(l))
        if (x.check == Check::NameInCommonText) trovati.push_back(x.where.pointer + " " + std::to_string(x.variant));
    std::sort(trovati.begin(), trovati.end());
    EXPECT_EQ(trovati, (std::vector<std::string>{"/calendari/0/nome 0", "/calendari/1/nome 0", "/calendari/2/codice 0"}));
}

// Confronto senza accenti anche nei testi: "Nicolò" nel nome e "NICOLO" nel testo sono la stessa parola (sabotaggio: confronto
// senza la normalizzazione degli accenti).
TEST(TestiLiberi, AccentiNelTestoENelNome) {
    InMemoria m = demo();
    JsonDati p = m.json("persone.json");
    per_codice(p.at("persone"), "P-B")["nome"] = "Bianco Nicolo";
    m.scrivi("persone.json", p);
    imposta(m, "turni.json", "turni", "T-PRIMO", "nome", "Primo di Nicolò");   // T-PRIMO e' nella rotazione di P-B
    const std::vector<std::string> g = grezzi(m);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-136 1 turni.json /turni/0/nome") == 1) << ::testing::PrintToString(g);
}

// Nomi di passeggeri, case e committenti legati a un luogo sensibile: varianti particolari (2, 3). Il demo: P-D1 Bianchi Luca va
// al centro diurno; la casa PP-02 "Casa Verdi" e' legata; CP-02 noleggia verso il centro.
TEST(TestiLiberi, LegatiVariantiParticolari) {
    InMemoria m = demo();
    imposta(m, "calendari.json", "calendari", "CAL-SCUOLA", "nome", "Scuola di Luca Bianchi");
    imposta(m, "calendari.json", "calendari", "CAL-FERIALI", "nome", "Feriali di Casa Verdi");
    imposta(m, "servizi.json", "servizi", "S-PRIVATO", "nome", "Noleggio Ferri");   // una parola di "Ferri Bruno" (CP-02)
    const std::vector<std::string> g = grezzi(m);
    for (const char* x : {"K-CTR-136 2 calendari.json /calendari/0/nome", "K-CTR-136 2 calendari.json /calendari/4/nome",
                          "K-CTR-136 3 servizi.json /servizi/10/nome"})
        EXPECT_TRUE(std::count(g.begin(), g.end(), x) == 1) << x << "\n" << ::testing::PrintToString(g);
    const DefinizioneControllo& d = definizione_controllo(Check::NameInCommonText);
    EXPECT_EQ(d.rivela_variante(kNomeIntero), Classe::Personale);
    EXPECT_EQ(d.rivela_variante(kNomeParola), Classe::Personale);
    EXPECT_EQ(d.rivela_variante(kNomeInteroLegato), Classe::Particolare);
    EXPECT_EQ(d.rivela_variante(kNomeParolaLegato), Classe::Particolare);
    EXPECT_EQ(d.rivela_variante(kNomeFalsoAllarme), Classe::Personale);
    EXPECT_TRUE(d.richiede_personali && d.richiede_particolari);
    // Il titolare senza i parametri: i legati si contano soltanto; con i soli dati personali il nome non legato con il codice e i
    // legati ancora contati soltanto (rivelano il legame con il centro).
    const Visti base = visti(m, Visibilita{false, false});
    EXPECT_FALSE(contiene(base.righe, "P-D1"));
    EXPECT_FALSE(contiene(base.righe, "Scuola di Luca Bianchi"));
    EXPECT_GE(base.r.avvisi_riservati, 3);
    const Visti personali = visti(m, Visibilita{true, false});
    EXPECT_FALSE(contiene(personali.righe, "P-D1"));
    EXPECT_FALSE(contiene(personali.righe, "CP-02"));
    EXPECT_FALSE(contiene(personali.righe, "PP-02"));
    const Visti tutto = visti(m, Visibilita{true, true});
    EXPECT_TRUE(contiene(tutto.righe, "contiene il nome intero del passeggero P-D1, che e' legato a un luogo sensibile"));
    EXPECT_TRUE(contiene(tutto.righe, "contiene il nome intero della casa PP-02, che e' legato a un luogo sensibile"));
    EXPECT_TRUE(contiene(tutto.righe, "e' un testo delle cose del committente privato CP-02, che e' legato a un luogo sensibile"));
}

// Le cose di una persona del personale: il suo codice, i suoi servizi fissi e i turni dei suoi servizi fissi e delle rotazioni
// in cui sta. P-E (Galli Sara) ha il servizio fisso SF-SCUOLA-1 su T-SCUOLA-1.
TEST(TestiLiberi, CoseDelPersonale) {
    InMemoria m = demo();
    imposta(m, "turni.json", "turni", "T-SCUOLA-1", "nome", "Scuola di Sara");
    imposta(m, "turni.json", "turni", "T-SCUOLA-2", "nome", "Scuola di Sara");   // di P-F: non delle cose di P-E
    imposta(m, "servizi_fissi.json", "servizi_fissi", "SF-SCUOLA-1", "codice", "SF-SARA");   // la nota e' personale: non si controlla
    const std::vector<std::string> g = grezzi(m);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-136 1 turni.json /turni/5/nome") == 1) << ::testing::PrintToString(g);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-136 1 servizi_fissi.json /servizi_fissi/2/codice") == 1) << ::testing::PrintToString(g);
    EXPECT_FALSE(std::count(g.begin(), g.end(), "K-CTR-136 1 turni.json /turni/7/nome")) << ::testing::PrintToString(g);
}

// Chi non ha tutti e due i permessi non legge mai un nome da un avviso, nemmeno dal codice della persona accanto al testo: per
// lui il controllo dei nomi e' "non verificato", e non c'e' nessuna riga K-CTR-136. Il titolare (tutti e due i permessi) vede i
// codici con i parametri.
TEST(TestiLiberi, TreUtentiDellaCassaforte) {
    InMemoria m = matrimonio_rossi();
    imposta(m, "servizi.json", "servizi", "S-PRIVATO", "nome", "Noleggio Ferri");   // CP-02, legato
    const Chiuso c = chiudi(m);
    for (const Visibilita v : {Visibilita{true, true}, Visibilita{true, false}, Visibilita{false, true}}) {
        const ValoriCassaforte vc = valori(c, v);
        const Resoconto r = controlla_lettura(leggi_pacchetto(c.normali, &vc), v);
        std::vector<std::string> righe;
        std::string tutto;
        for (const ProblemaPresentato& p : r.problemi) {
            tutto += p.codice + " " + p.messaggio + "\n";
            if (p.codice == "K-CTR-136") righe.push_back(p.messaggio);
        }
        bool non_verificato = false;
        for (const Resoconto::NonVerificati& nv : r.non_verificati)
            non_verificato = non_verificato || std::count(nv.controlli.begin(), nv.controlli.end(), "K-CTR-136") > 0;
        const std::string chi = v.personali && v.particolari ? "tutti e due" : v.personali ? "solo personali" : "solo particolari";
        if (v.personali && v.particolari) {
            EXPECT_FALSE(non_verificato) << chi;
            EXPECT_TRUE(contiene(righe, "delle cose del committente privato CP-01 e contiene una parola del suo nome (ROSSI)")) << tutto;
            EXPECT_TRUE(contiene(righe, "delle cose del committente privato CP-02, che e' legato a un luogo sensibile")) << tutto;
        } else {
            EXPECT_TRUE(non_verificato) << chi;
            EXPECT_TRUE(righe.empty()) << chi << "\n" << tutto;
            // Nessuna riga lega un codice di persona a "Matrimonio Rossi" o "Noleggio Ferri".
            for (const ProblemaPresentato& p : r.problemi) {
                if (p.messaggio.find("Matrimonio Rossi") != std::string::npos || p.messaggio.find("Noleggio Ferri") != std::string::npos) {
                    EXPECT_TRUE(p.messaggio.find("CP-0") == std::string::npos) << chi << ": " << p.messaggio;
                }
            }
        }
        // Le parole sulla salute non leggono dati personali: tutti la vedono.
        EXPECT_NE(tutto.find("K-CTR-137 Il testo \"Il centro diurno vuole sempre il mezzo con la pedana.\""), std::string::npos) << chi;
    }
}

// Gli elenchi delle parole sulla salute sono dati: senza elenco (norme della versione 3, nessuna parola delle aziende) nessun
// avviso (sabotaggio: elenco scritto nel codice).
TEST(TestiLiberi, SenzaElencoNessunAvviso) {
    InMemoria m = demo();
    JsonDati n = m.json("norme.json");
    n["schema_versione"] = 3;
    n.at("norme")[0].erase("parole_sulla_salute");
    m.scrivi("norme.json", n);
    JsonDati p = m.json("pacchetto.json");
    p.erase("falsi_allarmi");   // senza elenco non spegnerebbe piu' niente
    m.scrivi("pacchetto.json", p);
    imposta(m, "turni.json", "turni", "T-PRIMO", "nome", "Disabili e handicap, terapia");               // di un'azienda
    imposta(m, "calendari.json", "calendari", "CAL-SCUOLA", "nome", "Disabili e handicap, terapia");   // di nessuna azienda
    EXPECT_EQ(grezzi(m), std::vector<std::string>{});
}

// Un testo di un'azienda si confronta con le parole delle sue norme e le sue; un testo che non e' di un'azienda con tutti gli
// elenchi del pacchetto.
TEST(TestiLiberi, ElenchiPerAzienda) {
    InMemoria m = demo();
    JsonDati a = m.json("aziende.json");
    a["schema_versione"] = 4;
    a.at("aziende")[1]["parole_sulla_salute"] = JsonDati::array({"Faro"});   // BETA
    m.scrivi("aziende.json", a);
    imposta(m, "turni.json", "turni", "T-PRIMO", "nome", "Primo del Faro");        // ALFA: non e' una parola sua
    imposta(m, "turni.json", "turni", "T-B-PRIMO", "nome", "Primo del Faro");      // BETA: si'
    imposta(m, "calendari.json", "calendari", "CAL-SCUOLA", "nome", "Giorni del Faro");   // di nessuna azienda: tutti gli elenchi
    const std::vector<std::string> g = grezzi(m);
    EXPECT_EQ(g, (std::vector<std::string>{"K-CTR-137 0 blocchi.json /blocchi/0/nota", "K-CTR-137 0 calendari.json /calendari/0/nome",
                                           "K-CTR-137 0 turni.json /turni/12/nome"}));
    const Visti v = visti(m, Visibilita{false, false});
    EXPECT_TRUE(contiene(v.righe, "contiene parole sulla salute (\"Faro\" dell'elenco dell'azienda BETA)")) << ::testing::PrintToString(v.righe);
}

// Norme e regolamenti sono regole pubbliche (marcatura dei loro schemi, scelta del supervisore): le parole sulla salute nei loro
// testi non danno avvisi. I nomi delle persone si cercano anche li'.
TEST(TestiLiberi, RegolePubblicheSenzaParoleSullaSalute) {
    InMemoria m = demo();
    JsonDati r = m.json("regolamenti.json");
    r.at("regolamenti")[0]["nota"] = "Servizi per disabili e di terapia, con Rossi Mario.";
    m.scrivi("regolamenti.json", r);
    const std::vector<std::string> g = grezzi(m);
    EXPECT_EQ(g, (std::vector<std::string>{"K-CTR-136 0 regolamenti.json /regolamenti/0/nota", "K-CTR-137 0 blocchi.json /blocchi/0/nota"}));
    EXPECT_TRUE(regole_pubbliche("norme", 4) && regole_pubbliche("norme", 1) && regole_pubbliche("regolamenti", 3));
    EXPECT_FALSE(regole_pubbliche("servizi", 3) || regole_pubbliche("aziende", 4) || regole_pubbliche("luoghi", 2));
}

// Un luogo pubblico non segnato sensibile con una parola dell'elenco nel nome: K-CTR-138 (luoghi versione 1: la variante che dice
// di passare alla 2). Un luogo segnato sensibile, e la scuola o il centro che sta li', possono avere quelle parole (sabotaggio:
// luogo sensibile con l'avviso).
TEST(TestiLiberi, LuoghiForseSensibili) {
    InMemoria m = demo();
    JsonDati p = m.json("pacchetto.json");
    p.erase("falsi_allarmi");
    m.scrivi("pacchetto.json", p);
    imposta(m, "luoghi.json", "luoghi", "F-CASTELROVO", "nome", "Castelrovo, Riabilitazione");
    std::vector<std::string> g = grezzi(m);
    EXPECT_EQ(g, (std::vector<std::string>{"K-CTR-137 0 blocchi.json /blocchi/0/nota", "K-CTR-138 0 luoghi.json /luoghi/2/nomi_alternativi/0",
                                           "K-CTR-138 0 luoghi.json /luoghi/4/nome"}));
    // L-CENTRO "Valfonda, Centro diurno Il Faro" e SC-CENTRO "Centro diurno Il Faro" sono sensibili: nessun avviso, nemmeno nella nota.
    imposta(m, "luoghi.json", "luoghi", "L-CENTRO", "nota", "Centro diurno per disabili.");
    imposta(m, "scuole.json", "scuole", "SC-CENTRO", "nota", "Centro diurno per disabili.");
    EXPECT_EQ(grezzi(m), g);
    // Senza "sensibile" il centro ha l'avviso.
    JsonDati l = m.json("luoghi.json");
    per_codice(l.at("luoghi"), "L-CENTRO").erase("sensibile");
    m.scrivi("luoghi.json", l);
    g = grezzi(m);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-138 0 luoghi.json /luoghi/15/nome") == 1) << ::testing::PrintToString(g);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-137 0 luoghi.json /luoghi/15/nota") == 1) << ::testing::PrintToString(g);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-137 0 scuole.json /scuole/4/nome") == 1) << ::testing::PrintToString(g);
    // Luoghi della versione 1 (senza "sensibile"): la variante 1.
    l["schema_versione"] = 1;
    m.scrivi("luoghi.json", l);
    g = grezzi(m);
    EXPECT_TRUE(std::count(g.begin(), g.end(), "K-CTR-138 1 luoghi.json /luoghi/15/nome") == 1) << ::testing::PrintToString(g);
}

// Il falso allarme spegne solo quel controllo, quel campo, quell'elemento e quel testo: se il testo cambia, l'avviso torna e il
// falso allarme non spegne piu' niente (la sua variante, con la riservatezza del suo controllo). Sabotaggio: falso allarme che vale
// anche con il testo cambiato.
TEST(TestiLiberi, FalsoAllarme) {
    InMemoria m = matrimonio_rossi();
    EXPECT_FALSE(contiene(grezzi(m), "K-CTR-138"));   // il demo: il falso allarme spegne il parcheggio del centro diurno
    JsonDati p = m.json("pacchetto.json");
    p.at("falsi_allarmi").push_back({{"controllo", "K-CTR-136"}, {"file", "servizi.json"}, {"campo", "servizi[].nome"},
                                     {"elemento", "S-MATRIMONIO"}, {"testo", "Matrimonio Rossi"},
                                     {"motivo", "Prova: e' il nome del ristorante."}, {"quando", "2026-11-12"}});
    p.at("falsi_allarmi").push_back({{"controllo", "K-CTR-137"}, {"file", "blocchi.json"}, {"campo", "blocchi[].nota"},
                                     {"elemento", "B-VAL-3"}, {"testo", "Il centro diurno vuole sempre il mezzo con la pedana"},
                                     {"motivo", "Prova: un testo senza il punto finale."}, {"quando", "2026-11-12"}});
    m.scrivi("pacchetto.json", p);
    // Il primo spegne il nome; il secondo ha un testo diverso (senza il punto): non spegne la nota e non spegne niente.
    EXPECT_EQ(grezzi(m), (std::vector<std::string>{"K-CTR-137 0 blocchi.json /blocchi/0/nota", "K-CTR-137 1 pacchetto.json /falsi_allarmi/2"}));
    // Il testo del servizio cambia: l'avviso torna e il falso allarme non spegne piu' niente.
    imposta(m, "servizi.json", "servizi", "S-MATRIMONIO", "nome", "Matrimonio Rossi, andata");
    EXPECT_EQ(grezzi(m), (std::vector<std::string>{"K-CTR-136 1 servizi.json /servizi/9/nome", "K-CTR-136 4 pacchetto.json /falsi_allarmi/1",
                                                   "K-CTR-137 0 blocchi.json /blocchi/0/nota", "K-CTR-137 1 pacchetto.json /falsi_allarmi/2"}));
    // Un falso allarme di un altro controllo, di un altro campo o di un altro elemento non spegne.
    imposta(m, "servizi.json", "servizi", "S-MATRIMONIO", "nome", "Matrimonio Rossi");
    p = m.json("pacchetto.json");
    p.at("falsi_allarmi")[1]["controllo"] = "K-CTR-137";
    m.scrivi("pacchetto.json", p);
    EXPECT_TRUE(contiene(grezzi(m), "K-CTR-136 1 servizi.json /servizi/9/nome"));
    p.at("falsi_allarmi")[1]["controllo"] = "K-CTR-136";
    p.at("falsi_allarmi")[1]["elemento"] = "S-GITA";
    m.scrivi("pacchetto.json", p);
    EXPECT_TRUE(contiene(grezzi(m), "K-CTR-136 1 servizi.json /servizi/9/nome"));
    // Il motivo e' un testo comune: entra anche lui nei controlli.
    p.at("falsi_allarmi")[1]["elemento"] = "S-MATRIMONIO";
    p.at("falsi_allarmi")[1]["motivo"] = "Non e' il cliente: e' la via del centro diurno.";
    m.scrivi("pacchetto.json", p);
    EXPECT_TRUE(contiene(grezzi(m), "K-CTR-137 0 pacchetto.json /falsi_allarmi/1/motivo"));
    EXPECT_FALSE(contiene(grezzi(m), "K-CTR-136 1 servizi.json"));
}

// La tabella dei campi controllati di docs/controlli.md coincide con quella ricavata dagli schemi (ultime versioni).
TEST(TestiLiberi, TabellaDeiCampiNeiDocumenti) {
    std::vector<std::string> dal_programma;
    std::string attesa;
    for (const FileDati& x : file_dati()) {
        std::string campi;
        for (const std::string& c : campi_testi_comuni(x.schema, x.versioni.back())) {
            dal_programma.push_back(x.file + " " + c);
            campi += (campi.empty() ? "" : ", ") + ("`" + c + "`");
        }
        if (!campi.empty()) attesa += "| `" + x.file + "` | " + campi + " |\n";
    }
    std::ifstream f(std::string(KZERO_CARTELLA_PROVE) + "/../docs/controlli.md");
    ASSERT_TRUE(f.good());
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string md = ss.str();
    const std::string inizio = "<!-- campi dei testi comuni: inizio -->", fine = "<!-- campi dei testi comuni: fine -->";
    const auto a = md.find(inizio), b = md.find(fine);
    ASSERT_TRUE(a != std::string::npos && b != std::string::npos && a < b) << "La tabella attesa:\n" << attesa;
    std::vector<std::string> nei_documenti;
    const std::regex riga(R"(\n\| `([a-z_]+\.json)` \| (.*) \|)");
    const std::string pezzo = md.substr(a, b - a);
    for (auto it = std::sregex_iterator(pezzo.begin(), pezzo.end(), riga); it != std::sregex_iterator(); ++it) {
        const std::string file = (*it)[1];
        const std::string campi = (*it)[2];
        const std::regex campo("`([^`]+)`");
        for (auto c = std::sregex_iterator(campi.begin(), campi.end(), campo); c != std::sregex_iterator(); ++c)
            nei_documenti.push_back(file + " " + (*c)[1].str());
    }
    EXPECT_EQ(nei_documenti, dal_programma) << "La tabella attesa:\n" << attesa;
}
