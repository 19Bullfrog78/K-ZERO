// K-Zero - prove interne della separazione e dell'unione dei dati con la cassaforte (K-010), senza cifratura: il pacchetto
// demo separato in memoria in file normali e valori, poi riletto da utenti con permessi diversi.
// - la separazione segue la stessa regola delle classi con cui kzero controlla conta i valori;
// - l'unione e' per codice (o per la chiave dichiarata nello schema), mai per posizione: un riordino non cambia nulla, un
//   elemento tolto o cambiato e' un errore K-CTR-094, mai un valore sull'elemento sbagliato;
// - per chi non ha un permesso i campi di quella classe valgono come presenti ma non leggibili.
#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "chiusura.hpp"
#include "classi.hpp"
#include "controllo.hpp"
#include "formato_dati.hpp"
#include "controlli.hpp"
#include "lettore.hpp"
#include "riservatezza.hpp"

using namespace kzero::api;
using kzero::Check;

namespace {

// Un pacchetto in memoria (file -> testo).
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
    std::map<std::string, std::string> file;
};

InMemoria demo_in_chiaro() {
    InMemoria m;
    const PacchettoIncorporato demo("esempi/demo");
    for (const auto& v : demo.voci()) m.file[v.nome] = demo.leggi(v.nome, kMassimoByteFile).testo;
    return m;
}

const std::string kId = "00112233445566778899aabbccddeeff";

// Il pacchetto chiuso in memoria: i file normali (con il manifesto che dichiara la cassaforte e un file della cassaforte
// finto: il lettore guarda solo che ci sia) e i valori, come li darebbe la cassaforte aperta.
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

ValoriCassaforte valori(const Chiuso& c, Visibilita leggibili) {
    ValoriCassaforte v;
    v.leggibili = leggibili;
    v.id = kId;
    v.generazione = 1;
    // Come la cassaforte vera: i valori legati a un luogo sensibile solo a chi ha tutti e due i permessi.
    for (const Voce& x : c.voci) {
        const bool apre = x.legato                         ? leggibili.personali && leggibili.particolari
                          : x.classe == Classe::Particolare ? leggibili.particolari
                                                            : leggibili.personali;
        if (apre) v.voci.push_back(&x);
    }
    return v;
}

std::string elenco(const std::vector<kzero::Problem>& problemi) {
    std::string s;
    for (const auto& p : problemi)
    {
        s += definizione_controllo(p.check).codice + " " + p.where.file + " " + p.where.pointer;
        for (const auto& x : p.params) s += " [" + x.name + ": " + x.text + "]";
        s += "\n";
    }
    return s;
}

}  // namespace

// Separare e riunire il demo da' gli stessi dati; nei file normali non resta nessun valore personale o particolare.
TEST(Unione, DemoChiusoERiapertoUguale) {
    const InMemoria chiaro = demo_in_chiaro();
    const Lettura originale = leggi_pacchetto(chiaro);
    const Chiuso c = chiudi(chiaro);
    const ValoriCassaforte tutti = valori(c, {true, true});
    const Lettura riaperta = leggi_pacchetto(c.normali, &tutti);
    EXPECT_TRUE(riaperta.forma_valida()) << elenco(riaperta.problemi);
    std::string differenza;
    EXPECT_TRUE(chiusura::stessi_dati(originale, riaperta, differenza)) << differenza;
    EXPECT_EQ(riaperta.in_chiaro.personali + riaperta.in_chiaro.particolari, 0u);
}

// Esperimento: lettura per ogni tipo di utente (nessuna eccezione, nessun errore di forma).
TEST(Unione, LetturaConPermessiParziali) {
    const Chiuso c = chiudi(demo_in_chiaro());
    for (const Visibilita v : {Visibilita{true, false}, Visibilita{false, true}, Visibilita{false, false}}) {
        const ValoriCassaforte x = valori(c, v);
        const Lettura l = leggi_pacchetto(c.normali, &x);
        EXPECT_TRUE(l.forma_valida()) << v.personali << v.particolari << "\n" << elenco(l.problemi);
    }
}

// --- Controlli che dipendono da una classe di dati ------------------------------------------------------------------------

namespace {

// Le modifiche del demo usate dalle prove sul programma (tests/CMakeLists.txt, MODIFICA IMPOSTA e TOGLI): un corpo di
// pacchetti rotti in tanti modi diversi, gia' scelti per far scattare i controlli.
struct Modifica {
    std::string operazione, file, puntatore, valore;
};

std::vector<Modifica> modifiche_delle_prove() {
    std::ifstream f(KZERO_CARTELLA_PROVE "/CMakeLists.txt", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string t = ss.str();
    std::vector<Modifica> out;
    const auto token = [&](std::size_t& i) {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r')) ++i;
        std::string s;
        if (i < t.size() && t[i] == '"') {
            for (++i; i < t.size() && t[i] != '"'; ++i) {
                if (t[i] == '\\' && i + 1 < t.size()) ++i;
                s += t[i];
            }
            ++i;
            return s;
        }
        while (i < t.size() && t[i] != ' ' && t[i] != '\n' && t[i] != ')') s += t[i++];
        return s;
    };
    for (std::size_t i = t.find("MODIFICA "); i != std::string::npos; i = t.find("MODIFICA ", i + 1)) {
        std::size_t k = i + 9;
        Modifica m;
        m.operazione = token(k);
        if (m.operazione != "IMPOSTA" && m.operazione != "TOGLI") continue;
        m.file = token(k);
        m.puntatore = token(k);
        if (m.operazione == "IMPOSTA") m.valore = token(k);
        out.push_back(m);
    }
    return out;
}

std::optional<InMemoria> modificato(const InMemoria& base, const Modifica& m) {
    InMemoria x = base;
    const auto it = x.file.find(m.file);
    if (it == x.file.end()) return std::nullopt;
    JsonDati d = JsonDati::parse(it->second);
    try {
        const JsonDati::json_pointer p(m.puntatore);
        if (m.operazione == "TOGLI") {
            JsonDati& padre = d.at(p.parent_pointer());
            if (padre.is_array()) padre.erase(std::stoul(p.back()));
            else padre.erase(p.back());
        } else {
            JsonDati& padre = d.at(p.parent_pointer());
            const JsonDati valore = JsonDati::parse(m.valore);
            if (padre.is_array() && p.back() != "-" && std::stoul(p.back()) == padre.size()) padre.push_back(valore);
            else d[p] = valore;
        }
    } catch (const std::exception&) {
        return std::nullopt;
    }
    it->second = d.dump(2);
    return x;
}

// Un problema come testo, per confrontare due letture (controllo, variante, posizione e valori).
std::string come_testo(const kzero::Problem& p) {
    std::string s = definizione_controllo(p.check).codice + "/" + std::to_string(p.variant) + " " + p.where.file + " " +
                    p.where.pointer;
    for (const auto& x : p.params) {
        s += " " + x.name + "=" + x.text + ":" + std::to_string(x.number);
        for (const kzero::Day d : x.days) s += "," + std::to_string(d);
    }
    return s;
}

std::vector<kzero::Problem> tutti_i_problemi(const Lettura& l) {
    std::vector<kzero::Problem> tutti = l.problemi;
    if (l.forma_valida()) {
        const auto contenuto = kzero::check_package(l.pacchetto);
        tutti.insert(tutti.end(), contenuto.begin(), contenuto.end());
    }
    return tutti;
}

// I problemi di una lettura come testi; senza quelli che stanno in un dato (o rivelano un dato) che "leggibili" non apre:
// li puo' vedere solo chi ha quel permesso.
std::set<std::string> problemi_di(const Lettura& l, const Visibilita& leggibili) {
    std::set<std::string> s;
    for (const auto& p : tutti_i_problemi(l)) {
        if (!verificabile(definizione_controllo(p.check), p.variant, classe_della_posizione(p.where, l), in_trasportato(p.where, l),
                          leggibili))
            continue;
        s.insert(come_testo(p));
    }
    return s;
}

}  // namespace

namespace {

// Modifiche mirate ai dati personali e particolari del demo (le modifiche delle prove sul programma toccano soprattutto i
// dati comuni): tipi dei servizi e dei gruppi, esigenze, assistente, carrozzine, competenze, ruoli, abilitazioni,
// autorizzazioni, disponibilita', preferenze, assenze, coordinate dei luoghi privati.
std::vector<Modifica> modifiche_dei_dati_riservati(const InMemoria& demo) {
    std::vector<Modifica> out;
    const auto imposta = [&](const std::string& file, const std::string& p, const std::string& v) {
        out.push_back({"IMPOSTA", file, p, v});
    };
    const JsonDati servizi = JsonDati::parse(demo.file.at("servizi.json"));
    for (std::size_t i = 0; i < servizi.at("servizi").size(); ++i) {
        const std::string ps = "/servizi/" + std::to_string(i);
        for (const char* t : {"\"linea\"", "\"scolastico\"", "\"disabili\""}) imposta("servizi.json", ps + "/tipo", t);
        imposta("servizi.json", ps + "/competenze_richieste", "[\"PEDANA\"]");
        imposta("servizi.json", ps + "/competenze_richieste", "[\"INFANZIA\"]");
        const JsonDati& gruppi = servizi.at("servizi")[i].at("domanda").at("gruppi");
        for (std::size_t k = 0; k < gruppi.size(); ++k) {
            const std::string pg = ps + "/domanda/gruppi/" + std::to_string(k);
            for (const char* t : {"\"studenti\"", "\"disabili\"", "\"utenti_linea\"", "\"adulti_autorizzati\""})
                imposta("servizi.json", pg + "/tipo", t);
            imposta("servizi.json", pg + "/assistente_richiesto", "true");
            imposta("servizi.json", pg + "/assistente_richiesto", "false");
            if (!gruppi[k].at("fermate").empty()) {
                imposta("servizi.json", pg + "/fermate/0/carrozzine", "2");
                imposta("servizi.json", pg + "/fermate/0/ausili_piegati", "3");
            }
            const JsonDati& pass = gruppi[k].contains("passeggeri") ? gruppi[k].at("passeggeri") : JsonDati::array();
            for (std::size_t q = 0; q < pass.size(); ++q) {
                const std::string pp = pg + "/passeggeri/" + std::to_string(q);
                imposta("servizi.json", pp + "/esigenze", "{\"carrozzina\": \"sulla_carrozzina\", \"assistente\": true}");
                imposta("servizi.json", pp + "/esigenze", "{\"ausili_piegati\": 2}");
            }
        }
    }
    const JsonDati turni = JsonDati::parse(demo.file.at("turni.json"));
    for (std::size_t i = 0; i < turni.at("turni").size(); ++i)
        imposta("turni.json", "/turni/" + std::to_string(i) + "/competenze_richieste", "[\"PEDANA\"]");
    const JsonDati persone = JsonDati::parse(demo.file.at("persone.json"));
    for (std::size_t i = 0; i < persone.at("persone").size(); ++i) {
        const std::string pp = "/persone/" + std::to_string(i);
        imposta("persone.json", pp + "/ruoli", "[\"autista\"]");
        imposta("persone.json", pp + "/ruoli", "[\"assistente\"]");
        imposta("persone.json", pp + "/abilitazioni", "[{\"tipo\": \"B\"}]");
        imposta("persone.json", pp + "/abilitazioni", "[{\"tipo\": \"D\", \"scadenza\": \"2026-11-15\"}, {\"tipo\": \"CQC-P\", \"scadenza\": \"2027-01-31\"}]");
        imposta("persone.json", pp + "/autorizzazioni", "[]");
        imposta("persone.json", pp + "/competenze", "[]");
        imposta("persone.json", pp + "/competenze", "[\"PEDANA\", \"INFANZIA\"]");
        imposta("persone.json", pp + "/disponibilita", "[{\"giorni\": [\"lunedi\", \"martedi\"], \"dalle\": \"08:00\", \"alle\": \"12:00\"}]");
        imposta("persone.json", pp + "/preferenze", "[{\"tipo\": \"giorni_liberi\", \"giorni\": [\"lunedi\", \"giovedi\"]}]");
        const std::string codice = persone.at("persone")[i].at("codice").get<std::string>();
        imposta("assenze.json", "/assenze/0", "{\"persona\": \"" + codice + "\", \"tipo\": \"FERIE\", \"dal\": \"2026-10-19\", \"al\": \"2026-11-30\"}");
    }
    const JsonDati luoghi = JsonDati::parse(demo.file.at("luoghi.json"));
    for (std::size_t i = 0; i < luoghi.at("luoghi_privati").size(); ++i)
        out.push_back({"TOGLI", "luoghi.json", "/luoghi_privati/" + std::to_string(i) + "/coordinate", ""});
    return out;
}

}  // namespace

namespace {

// Per un tipo di utente: su ogni pacchetto del corpo (il demo, le modifiche delle prove sul programma e quelle mirate ai
// dati riservati) i problemi che l'utente puo' verificare sono esattamente quelli che trova chi ha tutti i permessi. Se un
// controllo usasse una classe che l'utente non legge senza dichiararlo nel catalogo, qui un problema mancherebbe o sarebbe
// in piu'. Restituisce le differenze (vuoto = tutto bene).
std::map<std::string, std::string> differenze_per(const Visibilita& v, int& usati) {
    const InMemoria demo = demo_in_chiaro();
    std::vector<std::optional<InMemoria>> corpo{demo};
    for (const Modifica& m : modifiche_delle_prove()) corpo.push_back(modificato(demo, m));
    for (const Modifica& m : modifiche_dei_dati_riservati(demo)) corpo.push_back(modificato(demo, m));
    std::map<std::string, std::string> diversi;   // controllo -> un esempio
    usati = 0;
    for (const auto& x : corpo) {
        if (!x) continue;
        const Lettura chiaro = leggi_pacchetto(*x);
        if (!chiaro.forma_valida()) continue;
        const Chiuso c = chiudi(*x);
        const ValoriCassaforte tutti = valori(c, {true, true});
        const Lettura piena = leggi_pacchetto(c.normali, &tutti);
        std::string differenza;
        if (!chiusura::stessi_dati(chiaro, piena, differenza)) continue;   // non si potrebbe chiudere (es. codici ripetuti)
        ++usati;
        const std::set<std::string> pieno = problemi_di(piena, v);
        const ValoriCassaforte vv = valori(c, v);
        const std::set<std::string> ridotto = problemi_di(leggi_pacchetto(c.normali, &vv), v);
        for (const std::string& p : pieno)
            if (!ridotto.count(p)) diversi[p.substr(0, p.find('/')) + " (manca)"] = p;
        for (const std::string& p : ridotto)
            if (!pieno.count(p)) diversi[p.substr(0, p.find('/')) + " (in piu')"] = p;
    }
    return diversi;
}

std::string testo(const std::map<std::string, std::string>& d) {
    std::string t;
    for (const auto& [k, esempio] : d) t += "   " + k + "   es. " + esempio.substr(0, 200) + "\n";
    return t;
}

}  // namespace

TEST(Unione, NonVerificatiDichiaratiSenzaParticolari) {
    int usati = 0;
    const auto d = differenze_per({true, false}, usati);
    EXPECT_GT(usati, 300);
    EXPECT_TRUE(d.empty()) << testo(d);
}

TEST(Unione, NonVerificatiDichiaratiSenzaPersonali) {
    int usati = 0;
    const auto d = differenze_per({false, true}, usati);
    EXPECT_GT(usati, 300);
    EXPECT_TRUE(d.empty()) << testo(d);
}

TEST(Unione, NonVerificatiDichiaratiSenzaPermessi) {
    int usati = 0;
    const auto d = differenze_per({false, false}, usati);
    EXPECT_GT(usati, 300);
    EXPECT_TRUE(d.empty()) << testo(d);
}

// --- Unione per codice, mai per posizione -------------------------------------------------------------------------------

namespace {

JsonDati documento(const InMemoria& m, const std::string& file) { return JsonDati::parse(m.file.at(file)); }
void metti(InMemoria& m, const std::string& file, const JsonDati& d) { m.file[file] = d.dump(2); }

std::set<std::string> codici(const Lettura& l) {
    std::set<std::string> c;
    for (const auto& p : l.problemi) c.insert(definizione_controllo(p.check).codice + " " + p.where.file);
    return c;
}

const JsonDati& per_codice(const JsonDati& elenco, const std::string& codice) {
    for (const JsonDati& x : elenco)
        if (x.at("codice") == codice) return x;
    throw std::logic_error("codice non trovato: " + codice);
}

}  // namespace

// Riordinare le persone nel file normale: ogni nome resta con il suo codice.
TEST(Unione, RiordinoNonCambiaNulla) {
    const InMemoria chiaro = demo_in_chiaro();
    const Lettura originale = leggi_pacchetto(chiaro);
    Chiuso c = chiudi(chiaro);
    JsonDati persone = documento(c.normali, "persone.json");
    std::reverse(persone["persone"].begin(), persone["persone"].end());
    metti(c.normali, "persone.json", persone);
    JsonDati assegnazioni = documento(c.normali, "assegnazioni.json");
    std::reverse(assegnazioni["assegnazioni"].begin(), assegnazioni["assegnazioni"].end());
    metti(c.normali, "assegnazioni.json", assegnazioni);
    JsonDati servizi = documento(c.normali, "servizi.json");
    for (JsonDati& sv : servizi["servizi"])
        for (JsonDati& g : sv["domanda"]["gruppi"]) std::reverse(g["fermate"].begin(), g["fermate"].end());
    metti(c.normali, "servizi.json", servizi);
    const ValoriCassaforte tutti = valori(c, {true, true});
    const Lettura l = leggi_pacchetto(c.normali, &tutti);
    ASSERT_TRUE(l.forma_valida()) << elenco(l.problemi);
    // Nomi e note seguono i codici e le chiavi.
    const JsonDati& prima = originale.documenti.at("persone.json").at("persone");
    const JsonDati& dopo = l.documenti.at("persone.json").at("persone");
    for (const JsonDati& x : prima) EXPECT_EQ(per_codice(dopo, x.at("codice").get<std::string>()), x);
    std::multiset<std::string> note_prima, note_dopo;
    for (const JsonDati& a : originale.documenti.at("assegnazioni.json").at("assegnazioni"))
        note_prima.insert(a.at("turno").get<std::string>() + a.at("data").get<std::string>() + a.value("nota", ""));
    for (const JsonDati& a : l.documenti.at("assegnazioni.json").at("assegnazioni"))
        note_dopo.insert(a.at("turno").get<std::string>() + a.at("data").get<std::string>() + a.value("nota", ""));
    EXPECT_EQ(note_prima, note_dopo);
    // Le carrozzine seguono la fermata (luogo), non la posizione.
    const auto carrozzine = [](const Lettura& x) {
        std::multiset<std::string> m;
        for (const JsonDati& sv : x.documenti.at("servizi.json").at("servizi"))
            for (const JsonDati& g : sv.at("domanda").at("gruppi"))
                for (const JsonDati& f : g.at("fermate"))
                    m.insert(g.at("codice").get<std::string>() + f.at("luogo").get<std::string>() + std::to_string(f.value("carrozzine", -1)));
        return m;
    };
    EXPECT_EQ(carrozzine(originale), carrozzine(l));
}

// Un elemento tolto o con il codice cambiato dopo la chiusura: i suoi valori non trovano posto (K-CTR-094) e il file non si
// usa; mai il nome di una persona su un'altra.
TEST(Unione, ElementoToltoOCambiato) {
    for (const std::string& come : {std::string("tolto"), std::string("cambiato")}) {
        Chiuso c = chiudi(demo_in_chiaro());
        JsonDati persone = documento(c.normali, "persone.json");
        if (come == "tolto") persone["persone"].erase(1);
        else persone["persone"][1]["codice"] = "P-NUOVO";
        metti(c.normali, "persone.json", persone);
        const ValoriCassaforte tutti = valori(c, {true, true});
        const Lettura l = leggi_pacchetto(c.normali, &tutti);
        EXPECT_FALSE(l.forma_valida()) << come;
        EXPECT_TRUE(codici(l).count("K-CTR-094 persone.json")) << come << "\n" << elenco(l.problemi);
        EXPECT_FALSE(l.usato("persone.json"));
    }
}

// Una riga senza codice (assegnazione del giorno) con la data cambiata: la sua nota non trova posto.
TEST(Unione, RigaSenzaCodiceCambiata) {
    Chiuso c = chiudi(demo_in_chiaro());
    JsonDati a = documento(c.normali, "assegnazioni.json");
    a["assegnazioni"][0]["data"] = "2027-01-04";
    metti(c.normali, "assegnazioni.json", a);
    const ValoriCassaforte tutti = valori(c, {true, true});
    const Lettura l = leggi_pacchetto(c.normali, &tutti);
    EXPECT_TRUE(codici(l).count("K-CTR-094 assegnazioni.json")) << elenco(l.problemi);
}

// Un valore sia nella cassaforte sia in chiaro: K-CTR-095. Un valore nuovo solo in chiaro accanto alla cassaforte: avviso
// K-CTR-096 (dati in chiaro).
TEST(Unione, ValoreDoppioEValoreInChiaro) {
    Chiuso c = chiudi(demo_in_chiaro());
    JsonDati persone = documento(c.normali, "persone.json");
    persone["persone"][0]["nome"] = "Nome scritto in chiaro";
    metti(c.normali, "persone.json", persone);
    const ValoriCassaforte tutti = valori(c, {true, true});
    Lettura l = leggi_pacchetto(c.normali, &tutti);
    EXPECT_TRUE(codici(l).count("K-CTR-095 persone.json")) << elenco(l.problemi);

    Chiuso d = chiudi(demo_in_chiaro());
    JsonDati assegnazioni = documento(d.normali, "assegnazioni.json");
    JsonDati nuova = assegnazioni["assegnazioni"][0];
    nuova["data"] = "2027-01-05";
    nuova["nota"] = "Nota scritta dopo la chiusura.";
    assegnazioni["assegnazioni"].push_back(nuova);
    metti(d.normali, "assegnazioni.json", assegnazioni);
    const ValoriCassaforte tutti_d = valori(d, {true, true});
    l = leggi_pacchetto(d.normali, &tutti_d);
    EXPECT_TRUE(codici(l).count("K-CTR-096 ")) << elenco(l.problemi);
    EXPECT_EQ(l.in_chiaro.personali, 1u);
}

// Le assenze (elementi tutti personali) vanno intere nella cassaforte: nel file normale l'elenco resta vuoto. I dati
// personali dei passeggeri legati al centro diurno sono particolari e stanno nella sezione dei legati: li legge solo chi ha
// tutti e due i permessi (aggiunta 1 del K-010).
TEST(Unione, AssenzeIntereEPasseggeriLegati) {
    const Chiuso c = chiudi(demo_in_chiaro());
    EXPECT_EQ(documento(c.normali, "assenze.json").at("assenze"), JsonDati::array());
    bool nome_legato = false, nome_libero = false;
    for (const Voce& v : c.voci) {
        if (v.file != "servizi.json" || v.campo != "nome" || v.percorso.size() < 7) continue;
        const std::string codice = v.percorso.back().value("codice", "");
        if (codice == "P-D1") nome_legato = v.classe == Classe::Particolare && v.legato;
        if (codice == "P-A1") nome_libero = v.classe == Classe::Personale && !v.legato;
    }
    EXPECT_TRUE(nome_legato);
    EXPECT_TRUE(nome_libero);
    // Chi ha i soli dati personali non vede il nome di P-D1, ma vede quello di P-A1.
    const ValoriCassaforte pers = valori(c, {true, false});
    const Lettura l = leggi_pacchetto(c.normali, &pers);
    ASSERT_TRUE(l.forma_valida()) << elenco(l.problemi);
    const std::string servizi = l.documenti.at("servizi.json").dump();
    EXPECT_EQ(servizi.find("Bianchi Luca"), std::string::npos);
    EXPECT_NE(servizi.find("Rossi Giulia"), std::string::npos);
    EXPECT_EQ(l.documenti.at("assenze.json").at("assenze").size(), 3u);
    // Chi ha i soli particolari: niente assenze, e nemmeno il nome di P-D1 (e' un dato personale).
    const ValoriCassaforte part = valori(c, {false, true});
    const Lettura lp = leggi_pacchetto(c.normali, &part);
    ASSERT_TRUE(lp.forma_valida()) << elenco(lp.problemi);
    EXPECT_EQ(lp.documenti.at("assenze.json").at("assenze"), JsonDati::array());
    EXPECT_EQ(lp.documenti.at("servizi.json").dump().find("Bianchi Luca"), std::string::npos);
    EXPECT_EQ(lp.documenti.at("servizi.json").dump().find("Rossi Giulia"), std::string::npos);
    // Chi ha tutti e due i permessi legge tutti e due i nomi.
    const ValoriCassaforte tutti = valori(c, {true, true});
    const Lettura lt = leggi_pacchetto(c.normali, &tutti);
    ASSERT_TRUE(lt.forma_valida()) << elenco(lt.problemi);
    EXPECT_NE(lt.documenti.at("servizi.json").dump().find("Bianchi Luca"), std::string::npos);
    EXPECT_NE(lt.documenti.at("servizi.json").dump().find("Rossi Giulia"), std::string::npos);
}

// Cassaforte mancante, non dichiarata, di un'altra chiusura; valori per un file che non c'e'.
TEST(Unione, CassaforteMancanteNonDichiarataOAltraChiusura) {
    Chiuso c = chiudi(demo_in_chiaro());
    InMemoria senza = c.normali;
    senza.file.erase("cassaforte.kzc");
    Lettura l = leggi_pacchetto(senza);
    EXPECT_TRUE(codici(l).count("K-CTR-091 pacchetto.json")) << elenco(l.problemi);
    EXPECT_FALSE(l.forma_valida());

    InMemoria non_dichiarata = c.normali;
    JsonDati m = documento(non_dichiarata, "pacchetto.json");
    m["cassaforte"] = false;
    metti(non_dichiarata, "pacchetto.json", m);
    l = leggi_pacchetto(non_dichiarata);
    EXPECT_TRUE(codici(l).count("K-CTR-092 pacchetto.json")) << elenco(l.problemi);

    ValoriCassaforte tutti = valori(c, {true, true});
    tutti.generazione = 2;
    l = leggi_pacchetto(c.normali, &tutti);
    EXPECT_TRUE(codici(l).count("K-CTR-093 pacchetto.json")) << elenco(l.problemi);

    InMemoria senza_assegnazioni = c.normali;
    senza_assegnazioni.file.erase("assegnazioni.json");
    const ValoriCassaforte t2 = valori(c, {true, true});
    l = leggi_pacchetto(senza_assegnazioni, &t2);
    EXPECT_TRUE(codici(l).count("K-CTR-094 assegnazioni.json")) << elenco(l.problemi);
}

// Dati personali in chiaro in un pacchetto che non dice "dati_inventati": avviso K-CTR-096; il demo (dati inventati) no.
TEST(Unione, DatiInChiaroSenzaDatiInventati) {
    InMemoria chiaro = demo_in_chiaro();
    EXPECT_FALSE(codici(leggi_pacchetto(chiaro)).count("K-CTR-096 "));
    JsonDati m = documento(chiaro, "pacchetto.json");
    m.erase("dati_inventati");
    metti(chiaro, "pacchetto.json", m);
    const Lettura l = leggi_pacchetto(chiaro);
    EXPECT_TRUE(codici(l).count("K-CTR-096 ")) << elenco(l.problemi);
    EXPECT_TRUE(l.forma_valida());   // e' un avviso
}

// Il conteggio dei valori della separazione e' quello di kzero controlla (una regola sola).
TEST(Unione, StessaRegolaDelConteggio) {
    const Lettura l = leggi_pacchetto(demo_in_chiaro());
    const chiusura::Separazione s = chiusura::separa(l);
    std::size_t p = 0, q = 0;
    for (const auto& [file, x] : l.riservati) {
        p += x.personali;
        q += x.particolari;
    }
    EXPECT_EQ(s.contenuto.valori.personali, p);
    EXPECT_EQ(s.contenuto.valori.particolari, q);
    for (const auto& [file, doc] : s.normali) {
        if (file == "pacchetto.json") continue;
        const FileDati* f = file_dati_per_nome(file);
        const ValoriPerClasse rimasti = conta_valori(doc, f->schema, l.versioni.at(file));
        EXPECT_EQ(rimasti.personali + rimasti.particolari, 0u) << file;
    }
}
