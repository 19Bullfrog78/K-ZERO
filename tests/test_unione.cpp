// K-Zero - prove interne della separazione e dell'unione dei dati con la cassaforte (K-010), senza cifratura: il pacchetto
// demo separato in memoria in file normali e valori, poi riletto da utenti con permessi diversi.
// - la separazione segue la stessa regola delle classi con cui kzero controlla conta i valori;
// - l'unione e' per codice (o per la chiave dichiarata nello schema), mai per posizione: un riordino non cambia nulla, un
//   elemento tolto o cambiato e' un errore K-CTR-094, mai un valore sull'elemento sbagliato;
// - per chi non ha un permesso i campi di quella classe valgono come presenti ma non leggibili.
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "chiusura.hpp"
#include "classi.hpp"
#include "comandi.hpp"
#include "controllo.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "formato_dati.hpp"
#include "controlli.hpp"
#include "lettore.hpp"
#include "riservatezza.hpp"
#include "orari.hpp"
#include "schemi_dati.hpp"
#include "tempo.hpp"
#include "turni.hpp"
#include "vista.hpp"

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

// I problemi di una lettura, giorno per giorno (un testo per problema e giorno: un problema certo puo' coprire solo una parte dei
// giorni, K-017), divisi come li tratta il resoconto (controllo.cpp) per un utente che apre le classi "leggibili": mostrati;
// non verificati (K-017: dei controlli dei turni, per chi apre i personali ma non i particolari, se dipendono dal tipo di
// servizio: si contano soltanto); gli altri, che stanno in un dato (o rivelano un dato) che "leggibili" non apre, mancano.
struct ProblemiPerGiorno {
    std::set<std::string> mostrati;
    std::set<std::string> non_verificati;   // con la chiave larga (chiave_larga)
};

// Un problema in un giorno, con la chiave larga: il controllo, il turno, la persona o il mezzo e il giorno. Un problema non
// verificato puo' nominare un'altra causa (es. un altro servizio) di quello di chi ha tutti i permessi.
std::string chiave_larga(const std::string& codice, const kzero::Problem& p, const std::string& giorno) {
    std::string s = codice;
    for (const char* nome : {"turno", "persona", "mezzo"})
        for (const auto& x : p.params)
            if (x.name == nome) {
                s += " " + x.name + "=" + x.text;
                break;
            }
    return s + " " + giorno;
}

ProblemiPerGiorno problemi_per_giorno(const Lettura& l, const Visibilita& leggibili) {
    ProblemiPerGiorno out;
    for (const auto& p : tutti_i_problemi(l)) {
        const DefinizioneControllo& d = definizione_controllo(p.check);
        const bool non_verificato = d.turni && leggibili.personali && !leggibili.particolari && (p.unverified || p.by_service_kind);
        if (!non_verificato &&
            !verificabile(d, p, classe_della_posizione(p.where, l), in_trasportato(p.where, l), leggibili))
            continue;
        std::vector<kzero::Day> giorni;
        kzero::Problem senza = p;
        for (auto& x : senza.params)
            if (x.kind == kzero::Param::Kind::Days) {
                giorni = x.days;
                x.days.clear();
            }
        std::vector<std::string> per_giorno;
        if (giorni.empty()) per_giorno.push_back("");
        for (const kzero::Day g : giorni) per_giorno.push_back(kzero::text_from_day(g));
        for (const std::string& g : per_giorno) {
            if (non_verificato) out.non_verificati.insert(chiave_larga(d.codice, p, g));
            else out.mostrati.insert(come_testo(senza) + " giorno=" + g);
        }
    }
    return out;
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

// I problemi di chi ha tutti i dati (pieno) e quelli dello stesso pacchetto per un utente con meno permessi (ridotto), visti
// come li vede quell'utente (problemi_per_giorno): ogni giorno di un problema mostrato e' vero, lo trova anche chi ha tutti i
// dati (K-017: anche quelli certi dei turni senza i dati particolari); ogni problema di chi ha tutti i dati, che l'utente puo'
// verificare, e' mostrato oppure (K-017) contato tra i non verificati. Le differenze vanno in diversi (controllo -> esempio).
void confronta_problemi(const ProblemiPerGiorno& pieno, const ProblemiPerGiorno& ridotto, std::map<std::string, std::string>& diversi) {
    for (const std::string& p : ridotto.mostrati)
        if (!pieno.mostrati.count(p)) diversi[p.substr(0, p.find('/')) + " (in piu')"] = p;
    const auto larga = [](const std::string& testo) {
        // testo = "K-CTR-nnn/v file punt k=v:n ... giorno=AAAA-MM-GG": la chiave larga dello stesso problema.
        std::string k = testo.substr(0, testo.find('/'));
        for (const char* nome : {" turno=", " persona=", " mezzo="}) {
            const std::size_t i = testo.find(nome);
            if (i != std::string::npos) k += testo.substr(i, testo.find(':', i) - i);
        }
        return k + " " + testo.substr(testo.rfind("giorno=") + 7);
    };
    for (const std::string& p : pieno.mostrati)
        if (!ridotto.mostrati.count(p) && !ridotto.non_verificati.count(larga(p))) diversi[p.substr(0, p.find('/')) + " (manca)"] = p;
    for (const std::string& p : pieno.non_verificati)
        if (!ridotto.non_verificati.count(p)) diversi[p.substr(0, p.find(' ')) + " (non verificato, manca)"] = p;
}

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
        const ValoriCassaforte vv = valori(c, v);
        confronta_problemi(problemi_per_giorno(piena, v), problemi_per_giorno(leggi_pacchetto(c.normali, &vv), v), diversi);
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

// --- K-011 aggiunta 2: i comandi con i permessi parziali, su tutti i pacchetti delle prove --------------------------------
// La separazione dei dati segue gli schemi (classi.hpp): ogni campo personale o particolare, anche quelli che verranno,
// arriva a un utente con permessi parziali solo se ne ha il permesso. La prova legge ogni pacchetto delle prove come lo
// riceve chi ha solo i dati personali, chi ha solo i particolari (niente legati) e chi li ha tutti, e lancia kzero controlla,
// kzero giornata e kzero ore: mai un errore interno. Perche' valga anche per i campi futuri, controlla prima che ogni campo
// personale o particolare degli schemi compaia in almeno uno dei pacchetti (altrimenti: aggiungerlo al pacchetto di prova
// dei servizi del K-012, tests/strumenti/genera_k012.py, che comprende quello degli orari del K-011).

namespace {

InMemoria da_cartella(const std::string& cartella) {
    InMemoria m;
    const CartellaSuDisco d(cartella);
    for (const Sorgente::Voce& v : d.voci())
        if (!v.cartella) m.file[v.nome] = d.leggi(v.nome, kMassimoByteFile).testo;
    return m;
}

struct PacchettoDiProva {
    std::string nome;
    InMemoria chiaro;
};

std::vector<PacchettoDiProva> pacchetti_delle_prove() {
    const std::string prove = KZERO_CARTELLA_PROVE;
    std::vector<PacchettoDiProva> out;
    for (const char* c : {"/../esempi/demo", "/../esempi/demo-numeri", "/esempi/k011-orari", "/esempi/k012-servizi", "/esempi/k007-demo",
                          "/esempi/k007-demo-numeri"})
        out.push_back({c, da_cartella(prove + c)});
    return out;
}

// I campi marcati personali o particolari di uno schema (marcatura scritta sul campo o sulla sua definizione), come
// percorsi con "[]" per gli elementi degli elenchi. Si seguono properties, items, oneOf, anyOf e allOf.
void campi_riservati(const JsonDati& radice, const JsonDati& nodo, const std::string& percorso, std::set<std::string>& out, int prof) {
    if (prof > 40 || !nodo.is_object()) return;
    const JsonDati& def = SchemiDati::risolvi(radice, nodo);
    for (const JsonDati* x : {&nodo, &def}) {
        const auto c = x->find("x-kzero-classe");
        if (c != x->end() && c->is_string() && c->get<std::string>() != "comune" && !percorso.empty()) out.insert(percorso);
    }
    if (!def.is_object()) return;
    if (const auto props = def.find("properties"); props != def.end() && props->is_object())
        for (const auto& [k, v] : props->items()) campi_riservati(radice, v, percorso + "/" + k, out, prof + 1);
    if (const auto items = def.find("items"); items != def.end() && items->is_object())
        campi_riservati(radice, *items, percorso + "/[]", out, prof + 1);
    for (const char* k : {"oneOf", "anyOf", "allOf"})
        if (const auto alt = def.find(k); alt != def.end() && alt->is_array())
            for (const JsonDati& a : *alt) campi_riservati(radice, a, percorso, out, prof + 1);
}

// I percorsi che un documento usa davvero (gli stessi "[]").
void percorsi_usati(const JsonDati& v, const std::string& percorso, std::set<std::string>& out) {
    if (v.is_object()) {
        for (const auto& [k, x] : v.items()) {
            out.insert(percorso + "/" + k);
            percorsi_usati(x, percorso + "/" + k, out);
        }
    } else if (v.is_array() && !v.empty()) {
        out.insert(percorso + "/[]");
        for (const JsonDati& x : v) percorsi_usati(x, percorso + "/[]", out);
    }
}

std::string nome_dell_errore(const ErroreKZero& e) { return e.definizione().codice + ": " + e.messaggio(); }

// Lancia un comando su un pacchetto gia' letto: true se finisce senza un errore interno (un errore K-ERR dei dati o dei
// parametri va bene: e' una risposta del programma); altrimenti scrive cosa e' successo in "dove".
bool senza_errore_interno(const std::function<Risultato()>& comando, const std::string& dove, std::string& errori) {
    try {
        const Risultato r = comando();
        if (r.codice == static_cast<int>(CodiceUscita::ErroreInterno)) {
            errori += dove + ": uscita 3\n";
            return false;
        }
    } catch (const ErroreKZero& e) {
        if (e.definizione().errore == Errore::ErroreInterno) {
            errori += dove + ": " + nome_dell_errore(e) + "\n";
            return false;
        }
    } catch (const std::exception& e) {
        errori += dove + ": eccezione " + e.what() + "\n";
        return false;
    }
    return true;
}

}  // namespace

// Un elemento marcato intero (es. un'assenza) va nella cassaforte tutto nella sua classe: non puo' contenere campi marcati con
// l'altra classe riservata, che finirebbero a chi non ha il loro permesso (K-011 aggiunta 2). Un campo personale dentro un
// campo particolare invece si separa e va tra i legati (classi.hpp).
namespace {
void elementi_interi(const JsonDati& radice, const JsonDati& nodo, const std::string& percorso, const std::string& intero,
                     const std::string& classe_intero, std::string& errori, int prof) {
    if (prof > 40 || !nodo.is_object()) return;
    const JsonDati& def = SchemiDati::risolvi(radice, nodo);
    std::string classe;
    for (const JsonDati* x : {&def, &nodo}) {
        const auto c = x->find("x-kzero-classe");
        if (c != x->end() && c->is_string()) classe = c->get<std::string>();
    }
    if (!intero.empty() && !classe.empty() && classe != "comune" && classe != classe_intero)
        errori += percorso + " (" + classe + ") dentro l'elemento intero " + intero + " (" + classe_intero + ")\n";
    if (!def.is_object()) return;
    if (const auto props = def.find("properties"); props != def.end() && props->is_object())
        for (const auto& [k, v] : props->items()) elementi_interi(radice, v, percorso + "/" + k, intero, classe_intero, errori, prof + 1);
    if (const auto items = def.find("items"); items != def.end() && items->is_object()) {
        std::string ci;
        const JsonDati& di = SchemiDati::risolvi(radice, *items);
        for (const JsonDati* x : {&di, &*items}) {
            const auto c = x->find("x-kzero-classe");
            if (c != x->end() && c->is_string()) ci = c->get<std::string>();
        }
        const bool marcato_intero = intero.empty() && !ci.empty() && ci != "comune";
        elementi_interi(radice, *items, percorso + "/[]", marcato_intero ? percorso + "/[]" : intero, marcato_intero ? ci : classe_intero,
                        errori, prof + 1);
    }
    for (const char* k : {"oneOf", "anyOf", "allOf"})
        if (const auto alt = def.find(k); alt != def.end() && alt->is_array())
            for (const JsonDati& a : *alt) elementi_interi(radice, a, percorso, intero, classe_intero, errori, prof + 1);
}
}  // namespace

TEST(Unione, ElementiInteriSenzaCampiDiUnAltraClasse) {
    std::string errori;
    for (const FileDati& f : file_dati())
        for (const int v : f.versioni) {
            const JsonDati* radice = SchemiDati::incorporati().schema(f.schema, v);
            ASSERT_NE(radice, nullptr) << f.schema;
            elementi_interi(*radice, *radice, f.schema + ".v" + std::to_string(v), "", "", errori, 0);
        }
    EXPECT_TRUE(errori.empty()) << errori;
}

// K-012: il nome e la nota di un committente privato vanno nella cassaforte; quelli di chi noleggia per andare in un luogo
// sensibile (nel demo CP-02, verso il centro diurno) tra i legati, gli altri (CP-01, il matrimonio) tra i personali. Riaperti,
// chi ha tutti e due i permessi li ha tutti; chi ha solo i personali solo quelli di CP-01; chi ha solo i particolari nessuno.
// Il codice resta nel file normale.
TEST(Unione, CommittentiPrivatiNellaCassaforte) {
    const std::string prove = KZERO_CARTELLA_PROVE;
    const Chiuso c = chiudi(da_cartella(prove + "/../esempi/demo"));
    std::map<std::string, std::string> trovate;   // "<codice>/<campo>" -> classe e legato
    for (const Voce& v : c.voci) {
        if (v.file != "committenti.json") continue;
        ASSERT_FALSE(v.percorso.empty());
        ASSERT_TRUE(v.percorso.front() == "committenti_privati") << v.percorso.dump();
        const std::string chiave = v.percorso.back().at("codice").get<std::string>() + "/" + v.campo;
        trovate[chiave] = std::string(v.classe == Classe::Particolare ? "particolare" : "personale") + (v.legato ? " legato" : "");
    }
    const std::map<std::string, std::string> attese = {
        {"CP-01/nome", "personale"}, {"CP-01/nota", "personale"}, {"CP-02/nome", "particolare legato"}};
    EXPECT_EQ(trovate, attese);
    const JsonDati normale = JsonDati::parse(c.normali.file.at("committenti.json"));
    EXPECT_EQ(normale.at("committenti_privati").dump(), R"([{"codice":"CP-01"},{"codice":"CP-02"}])");
    for (const auto& [v, nomi] : std::vector<std::pair<Visibilita, std::string>>{
             {Visibilita{true, true}, "CP-01:Galli Marta CP-02:Ferri Bruno "},
             {Visibilita{true, false}, "CP-01:Galli Marta CP-02: "},
             {Visibilita{false, true}, "CP-01: CP-02: "}}) {
        const ValoriCassaforte x = valori(c, v);
        const Lettura l = leggi_pacchetto(c.normali, &x);
        ASSERT_TRUE(l.forma_valida()) << elenco(l.problemi);
        std::string visti;
        for (const kzero::Client& k : l.pacchetto.clients)
            if (k.is_private) visti += k.code + ":" + k.name + " ";
        EXPECT_EQ(visti, nomi);
    }
}

// K-011 aggiunta 2: la dichiarazione del modo delle corse e' un dato particolare; motivo e chi, personali dentro un dato
// particolare, stanno tra i legati (tutti e due i permessi). La forzatura resta comune: motivo e chi solo personali.
TEST(Unione, DichiarazioneParticolareConMotivoLegato) {
    const std::string prove = KZERO_CARTELLA_PROVE;
    const Chiuso c = chiudi(da_cartella(prove + "/esempi/k011-orari"));
    std::map<std::string, std::string> trovate;   // "<cosa>/<campo>" -> classe e legato
    for (const Voce& v : c.voci) {
        if (v.file != "aziende.json") continue;
        std::string cosa;
        for (const JsonDati& p : v.percorso)
            if (p.is_string() && (p == "dichiarazione" || p == "forzatura")) cosa = p.get<std::string>();
        if (cosa.empty()) continue;
        const std::string chiave = cosa + "/" + v.campo;
        const std::string valore = std::string(v.classe == Classe::Particolare ? "particolare" : "personale") + (v.legato ? " legato" : "");
        EXPECT_TRUE(!trovate.count(chiave) || trovate[chiave] == valore) << chiave;
        trovate[chiave] = valore;
    }
    EXPECT_EQ(trovate["dichiarazione/motivo"], "particolare legato");
    EXPECT_EQ(trovate["dichiarazione/chi"], "particolare legato");
    EXPECT_EQ(trovate["dichiarazione/quando"], "particolare");
    EXPECT_EQ(trovate["forzatura/motivo"], "personale");
    EXPECT_EQ(trovate["forzatura/chi"], "personale");
    EXPECT_FALSE(trovate.count("forzatura/quando"));   // comune: resta nel file normale
    // Chi ha solo i dati particolari riceve la dichiarazione senza motivo e chi; chi ha solo i personali non la riceve.
    for (const Visibilita v : {Visibilita{false, true}, Visibilita{true, false}}) {
        const ValoriCassaforte x = valori(c, v);
        const Lettura l = leggi_pacchetto(c.normali, &x);
        ASSERT_TRUE(l.forma_valida()) << elenco(l.problemi);
        const kzero::CompanySettings* alfa = nullptr;
        for (const kzero::CompanySettings& a : l.pacchetto.company_settings)
            if (a.company.code == "ALFA") alfa = &a;
        ASSERT_TRUE(alfa && alfa->trip_modes);
        std::size_t dichiarazioni = 0;
        for (const auto* elenco_voci : {&alfa->trip_modes->by_service, &alfa->trip_modes->by_trip})
            for (const kzero::ModeEntry& e : *elenco_voci)
                if (e.declaration) {
                    ++dichiarazioni;
                    EXPECT_TRUE(e.declaration->reason.empty() && e.declaration->who.empty());
                }
        EXPECT_EQ(dichiarazioni, v.particolari ? 2u : 0u);
    }
}

// K-012: nel pacchetto di prova dei servizi la gita scolastica (studenti delle medie, servizio NCC) ha la categoria NCC, non
// scolastico; l'auto e il pullman del noleggio NCC; la navetta degli operai (servizio "altro") altro.
TEST(Unione, CategorieDelPacchettoDeiServizi) {
    const std::string prove = KZERO_CARTELLA_PROVE;
    const Lettura l = leggi_pacchetto(da_cartella(prove + "/esempi/k012-servizi"));
    ASSERT_TRUE(l.forma_valida());
    const kzero::Index idx(l.pacchetto);
    const auto categoria = [&](const char* corsa) { return idx.category(*idx.trip(corsa)).value(); };
    for (const char* c : {"C-GITA-SCUOLA-A", "C-GITA-SCUOLA-R", "C-NCC-AUTO-A", "C-NCC-BUS-1", "C-GITA-A"})
        EXPECT_EQ(categoria(c), kzero::ServiceType::Ncc) << c;
    EXPECT_EQ(categoria("C-NAVETTA-1"), kzero::ServiceType::Other);
    EXPECT_EQ(categoria("C-RM-1"), kzero::ServiceType::Mixed);
}

// K-012: i servizi occasionali del Reg. 561 nelle norme. Versione 3 (il demo): l'NCC certo, "altro" da confermare. Versione 2
// (il pacchetto di prova del K-011, che resta alla versione 2): un solo "da confermare" per tutto l'elenco, letto tipo per tipo
// come allora: "altro" da confermare, nessun NCC (mai trasformato da solo).
TEST(Unione, ServiziOccasionaliNelleVersioni) {
    const std::string prove = KZERO_CARTELLA_PROVE;
    const auto occasionali = [&](const std::string& cartella) {
        const Lettura l = leggi_pacchetto(da_cartella(prove + cartella));
        EXPECT_TRUE(l.forma_valida()) << cartella;
        for (const kzero::Norms& n : l.pacchetto.norms)
            for (const kzero::Regime& r : n.regimes)
                if (r.code == "R-561") return std::make_pair(l.versioni.at("norme.json"), r.occasional);
        return std::make_pair(0, kzero::Applies<kzero::ServiceKind>{});
    };
    const auto [v3, demo] = occasionali("/../esempi/demo");
    EXPECT_EQ(v3, 3);
    EXPECT_TRUE(demo.present);
    EXPECT_EQ(demo.certain, std::vector<kzero::ServiceKind>{kzero::ServiceKind::Ncc});
    EXPECT_EQ(demo.to_confirm, std::vector<kzero::ServiceKind>{kzero::ServiceKind::Other});
    const auto [v2, k011] = occasionali("/esempi/k011-orari");
    EXPECT_EQ(v2, 2);
    EXPECT_TRUE(k011.present);
    EXPECT_TRUE(k011.certain.empty());
    EXPECT_EQ(k011.to_confirm, std::vector<kzero::ServiceKind>{kzero::ServiceKind::Other});
}

// K-013 (controllo del K-012): nelle norme versione 2 "da_confermare" dei servizi occasionali e' facoltativo. Senza, oppure
// false, i tipi dell'elenco sono servizi occasionali certi (mai da confermare per un valore che manca).
TEST(Unione, ServiziOccasionaliVersione2Certi) {
    const std::string prove = KZERO_CARTELLA_PROVE;
    const std::string scritto = "\"servizi_occasionali\": { \"tipi_servizio\": [\"altro\"], \"da_confermare\": true }";
    for (const std::string nuovo : {"\"servizi_occasionali\": { \"tipi_servizio\": [\"altro\"] }",
                                    "\"servizi_occasionali\": { \"tipi_servizio\": [\"altro\"], \"da_confermare\": false }"}) {
        InMemoria m = da_cartella(prove + "/esempi/k011-orari");
        std::string& norme = m.file.at("norme.json");
        const std::size_t pos = norme.find(scritto);
        ASSERT_NE(pos, std::string::npos);
        norme.replace(pos, scritto.size(), nuovo);
        const Lettura l = leggi_pacchetto(m);
        ASSERT_TRUE(l.forma_valida()) << nuovo;
        EXPECT_EQ(l.versioni.at("norme.json"), 2);
        bool trovato = false;
        for (const kzero::Norms& n : l.pacchetto.norms)
            for (const kzero::Regime& r : n.regimes)
                if (r.code == "R-561") {
                    trovato = true;
                    EXPECT_TRUE(r.occasional.present) << nuovo;
                    EXPECT_EQ(r.occasional.certain, std::vector<kzero::ServiceKind>{kzero::ServiceKind::Other}) << nuovo;
                    EXPECT_TRUE(r.occasional.to_confirm.empty()) << nuovo;
                }
        EXPECT_TRUE(trovato);
    }
}

TEST(Unione, OgniCampoRiservatoDegliSchemiCompareNeiPacchettiDelleProve) {
    std::map<std::string, std::set<std::string>> usati;   // schema -> percorsi usati nei file dell'ultima versione
    for (const PacchettoDiProva& p : pacchetti_delle_prove())
        for (const auto& [file, testo] : p.chiaro.file) {
            const FileDati* f = file_dati_per_nome(file);
            if (!f || file == "pacchetto.json") continue;
            const JsonDati d = JsonDati::parse(testo);
            if (d.value("schema_versione", 0) != f->versioni.back()) continue;
            percorsi_usati(d, "", usati[f->schema]);
        }
    std::string mancanti;
    for (const FileDati& f : file_dati()) {
        if (f.file == "pacchetto.json") continue;
        const JsonDati* radice = SchemiDati::incorporati().schema(f.schema, f.versioni.back());
        ASSERT_NE(radice, nullptr) << f.schema;
        std::set<std::string> riservati;
        campi_riservati(*radice, *radice, "", riservati, 0);
        for (const std::string& c : riservati)
            if (!usati[f.schema].count(c)) mancanti += f.file + " " + c + "\n";
    }
    EXPECT_TRUE(mancanti.empty()) << "Campi personali o particolari degli schemi che nessun pacchetto delle prove usa (vanno "
                                     "aggiunti al pacchetto di prova dei servizi, tests/strumenti/genera_k012.py):\n"
                                  << mancanti;
}

TEST(Unione, ComandiConPermessiParziali) {
    const DefinizioneComando* controlla = trova_comando("controlla");
    const DefinizioneComando* giornata = trova_comando("giornata");
    const DefinizioneComando* ore = trova_comando("ore");
    ASSERT_TRUE(controlla && giornata && ore);
    std::string errori;
    for (const PacchettoDiProva& p : pacchetti_delle_prove()) {
        // I giorni della giornata: pochi, scelti perche' ogni corsa fatta da una persona compaia almeno una volta.
        const Lettura completa = leggi_pacchetto(p.chiaro);
        ASSERT_TRUE(completa.forma_valida()) << p.nome << "\n" << elenco(completa.problemi);
        const kzero::DateRange periodo = completa.pacchetto.manifest.period;
        kzero::HoursRequest richiesta;
        richiesta.report = true;
        richiesta.from = periodo.from;
        richiesta.to = periodo.to;
        kzero::HoursReport orari;
        const Resoconto r = controlla_lettura(leggi_pacchetto(p.chiaro), Visibilita{true, true}, &richiesta, &orari);
        std::map<kzero::Day, std::set<std::string>> corse_del_giorno;
        for (const kzero::PersonHours& ph : orari.persons)
            for (const kzero::DayHours& dh : ph.days)
                for (const kzero::TripOnDay& t : dh.trips) corse_del_giorno[dh.pd->day].insert(t.trip->code);
        std::set<std::string> da_coprire;
        for (const auto& [d, c] : corse_del_giorno) da_coprire.insert(c.begin(), c.end());
        std::vector<kzero::Day> giorni = {periodo.from};
        while (!da_coprire.empty()) {
            kzero::Day migliore = 0;
            std::size_t quante = 0;
            for (const auto& [d, c] : corse_del_giorno) {
                const std::size_t n = static_cast<std::size_t>(std::count_if(c.begin(), c.end(), [&](const std::string& x) { return da_coprire.count(x) > 0; }));
                if (n > quante) {
                    quante = n;
                    migliore = d;
                }
            }
            if (quante == 0) break;
            giorni.push_back(migliore);
            for (const std::string& x : corse_del_giorno.at(migliore)) da_coprire.erase(x);
        }
        const Chiuso c = chiudi(p.chiaro);
        for (const Visibilita v : {Visibilita{true, false}, Visibilita{false, true}, Visibilita{true, true}}) {
            const ValoriCassaforte valori_utente = valori(c, v);
            const auto lettura = [&] {
                accesso::LetturaPerComando lc;
                lc.lettura = leggi_pacchetto(c.normali, &valori_utente);
                lc.visibilita = v;
                lc.utente = "PROVA";
                lc.permessi = cassaforte::Permessi{v.personali, v.particolari};
                return lc;
            };
            const std::string chi = p.nome + (v.personali && v.particolari ? " (tutti i permessi)" : v.personali ? " (solo personali)" : " (solo particolari)");
            for (const Formato formato : {Formato::Testo, Formato::Json}) {
                Parametri par;
                par.formato = formato;
                par.includi_dati_personali = v.personali;
                par.includi_dati_particolari = v.particolari;
                par.utente = "PROVA";
                par.argomenti = {"pacchetto in memoria"};
                const std::string f = formato == Formato::Json ? " json" : "";
                senza_errore_interno([&] { return controlla_su(*controlla, par, lettura()); }, chi + ": controlla" + f, errori);
                Parametri po = par;
                po.dal = periodo.from;
                po.al = periodo.to;
                senza_errore_interno([&] { return ore_su(*ore, po, lettura()); }, chi + ": ore di tutto il periodo" + f, errori);
                for (const kzero::Day d : giorni) {
                    Parametri pg = par;
                    pg.data = d;
                    senza_errore_interno([&] { return giornata_su(*giornata, pg, lettura()); },
                                         chi + ": giornata del " + kzero::text_from_day(d) + f, errori);
                }
            }
        }
    }
    EXPECT_TRUE(errori.empty()) << errori;
}

// --- K-013: orari senza i dati particolari, prova per differenza -----------------------------------------------------------

namespace {

// Il pacchetto di prova dei servizi (k012-servizi) con il tipo di un servizio cambiato. Uguale in tutte le coppie: il servizio
// ha una descrizione del tipo (obbligatoria per "altro", ammessa per gli altri) e i mezzi delle corse provate hanno tutti e due
// gli usi (un'autorizzazione senza fine per l'uso che non hanno), cosi' nessun tipo fa nascere un errore nei dati.
InMemoria con_il_tipo(const std::string& servizio, const std::string& tipo) {
    InMemoria m = da_cartella(std::string(KZERO_CARTELLA_PROVE) + "/esempi/k012-servizi");
    JsonDati s = JsonDati::parse(m.file.at("servizi.json"));
    bool trovato = false;
    for (JsonDati& x : s.at("servizi"))
        if (x.at("codice") == servizio) {
            x["tipo"] = tipo;
            x["tipo_descrizione"] = "Servizio della prova per differenza";
            trovato = true;
        }
    EXPECT_TRUE(trovato) << servizio;
    m.file["servizi.json"] = s.dump(2);
    JsonDati v = JsonDati::parse(m.file.at("mezzi.json"));
    const std::map<std::string, std::string> uso_in_piu = {{"V-NCC-50", "LINEA"}, {"V-17", "NCC"}, {"ALFA-03", "LINEA"}, {"V-NCC-8", "LINEA"}};
    for (JsonDati& x : v.at("mezzi")) {
        const auto it = uso_in_piu.find(x.at("codice").get<std::string>());
        if (it == uso_in_piu.end()) continue;
        x["autorizzazioni_uso"].push_back({{"uso", it->second},
                                           {"ente", "Ente di prova (inventato)"},
                                           {"riferimento", "Distrazione di prova (inventata)"},
                                           {"dal", "2026-01-01"},
                                           {"al", "2099-12-31"}});
    }
    m.file["mezzi.json"] = v.dump(2);
    return m;
}

// Toglie dall'uscita i totali dell'intero pacchetto (errori e avvisi, anche quelli che si contano soltanto): l'unica
// differenza ammessa tra le due uscite.
std::string senza_totali(const std::string& uscita, Formato formato) {
    if (formato == Formato::Json) {
        JsonDati j = JsonDati::parse(uscita);
        for (const char* k : {"riepilogo", "problemi_riservati", "controllo"}) j.erase(k);
        return j.dump(1);
    }
    std::istringstream in(uscita);
    std::string out;
    for (std::string r; std::getline(in, r);) {
        if (r.rfind("Riepilogo:", 0) == 0 || r.rfind("Problemi sui dati particolari (salute), non elencati", 0) == 0 ||
            r.rfind("Controllo del pacchetto:", 0) == 0)
            continue;
        out += r + "\n";
    }
    return out;
}

// L'esito delle leggi in un'uscita JSON di kzero ore (con tutti i dati): per ogni persona e giornata i regimi con le loro
// famiglie, e i problemi della persona.
std::string esito_delle_leggi(const std::string& uscita) {
    const JsonDati j = JsonDati::parse(uscita);
    std::string out;
    for (const JsonDati& p : j.at("persone")) {
        for (const JsonDati& g : p.at("giornate")) {
            if (!g.at("regimi").is_array()) continue;
            for (const JsonDati& r : g.at("regimi"))
                out += p.at("codice").get<std::string>() + " " + g.at("data").get<std::string>() + " " + r.at("codice").dump() +
                       (r.at("per_scelta").get<bool>() ? " per scelta " : " ") + r.at("famiglie").dump() + "\n";
        }
        if (p.at("problemi").is_array())
            for (const JsonDati& q : p.at("problemi")) out += p.at("codice").get<std::string>() + " " + q.dump() + "\n";
    }
    return out;
}

// La prima riga diversa, per il messaggio della prova.
std::string prima_differenza(const std::string& a, const std::string& b) {
    std::istringstream x(a), y(b);
    std::string ra, rb;
    for (int n = 1;; ++n) {
        const bool ha = static_cast<bool>(std::getline(x, ra)), hb = static_cast<bool>(std::getline(y, rb));
        if (!ha && !hb) return "";
        if (!ha || !hb || ra != rb) return "riga " + std::to_string(n) + ":\n    " + (ha ? ra : "(fine)") + "\n    " + (hb ? rb : "(fine)");
    }
}

}  // namespace

// Criterio di accettazione del K-013 (arbitro): per ogni coppia di tipi di servizio, due pacchetti uguali tranne il tipo
// dichiarato di un servizio, tutti e due senza errori nei dati. Senza i dati particolari (di base, e con i soli dati personali)
// kzero controlla, kzero ore (tutto il periodo) e kzero giornata (i giorni delle corse del servizio), in testo e JSON, danno
// la stessa uscita, tranne i totali dell'intero pacchetto: ogni altra differenza e' una fuga del tipo di servizio. I servizi
// sono scelti dove il tipo cambia l'esito delle leggi (lo controlla la prova con i dati particolari): l'escursione NCC con il
// pullman (corse di 140, 25 e 100 km: linea, scolastico e disabili oltre i 50 km, NCC occasionale certo, altro da confermare,
// L. 138 certa, da confermare o no), il servizio per disabili del centro diurno (corse fino a 50 km con il minibus da 13 persone
// e con il mezzo da 18: escluso dal 561 con certezza come scolastico o linea, da confermare come disabili, mai come NCC) e il
// noleggio con l'auto da 9 persone (fuori dal 561: come NCC nessuna legge sugli orari vale, come linea vale la L. 138).
TEST(Unione, OrariSenzaDatiParticolariProvaPerDifferenza) {
    const DefinizioneComando* controlla = trova_comando("controlla");
    const DefinizioneComando* giornata = trova_comando("giornata");
    const DefinizioneComando* ore = trova_comando("ore");
    ASSERT_TRUE(controlla && giornata && ore);
    const std::vector<std::string> tipi = {"linea", "scolastico", "disabili", "ncc", "altro"};
    std::string fughe;
    std::map<std::string, int> con_effetto;   // coppia di tipi -> in quanti servizi il tipo cambia l'esito delle leggi
    int coppie = 0;
    for (const std::string servizio : {"S-NCC-BUS", "S-DIS", "S-NCC-AUTO"}) {
        // I giorni delle corse del servizio fatte da una persona (con tutti i dati, dal pacchetto del primo tipo).
        std::vector<kzero::Day> giorni;
        {
            const Lettura l = leggi_pacchetto(con_il_tipo(servizio, tipi.front()));
            ASSERT_TRUE(l.forma_valida()) << servizio;
            std::set<std::string> corse;
            for (const kzero::Trip& t : l.pacchetto.trips)
                for (const kzero::Ref& s : t.services)
                    if (s.code == servizio) corse.insert(t.code);
            kzero::HoursRequest richiesta;
            richiesta.report = true;
            richiesta.from = l.pacchetto.manifest.period.from;
            richiesta.to = l.pacchetto.manifest.period.to;
            kzero::HoursReport orari;
            // Il resoconto resta vivo: le corse del resoconto degli orari puntano al suo pacchetto.
            const Resoconto r = controlla_lettura(leggi_pacchetto(con_il_tipo(servizio, tipi.front())), Visibilita{true, true}, &richiesta, &orari);
            std::set<kzero::Day> visti;
            for (const kzero::PersonHours& ph : orari.persons)
                for (const kzero::DayHours& dh : ph.days)
                    for (const kzero::TripOnDay& t : dh.trips)
                        if (corse.count(t.trip->code)) visti.insert(dh.pd->day);
            ASSERT_FALSE(visti.empty()) << servizio;
            giorni.assign(visti.begin(), visti.end());
            if (giorni.size() > 2) giorni.erase(giorni.begin() + 1, giorni.end() - 1);   // il primo e l'ultimo
        }
        for (std::size_t i = 0; i < tipi.size(); ++i)
            for (std::size_t k = i + 1; k < tipi.size(); ++k) {
                ++coppie;
                const std::string tipi_coppia = tipi[i] + " <-> " + tipi[k];
                const std::string coppia = servizio + " " + tipi_coppia;
                con_effetto[tipi_coppia] += 0;
                const InMemoria a = con_il_tipo(servizio, tipi[i]), b = con_il_tipo(servizio, tipi[k]);
                for (const InMemoria* m : {&a, &b}) {
                    const Resoconto r = controlla_lettura(leggi_pacchetto(*m), Visibilita{true, true});
                    ASSERT_TRUE(r.lettura.forma_valida()) << coppia;
                    ASSERT_EQ(r.errori_nei_dati(), 0) << coppia << ": la coppia si deve poter fare senza errori nei dati";
                }
                const auto esegui = [&](const InMemoria& m, const DefinizioneComando& c, Parametri par) {
                    accesso::LetturaPerComando lc;
                    lc.lettura = leggi_pacchetto(m);
                    lc.visibilita = Visibilita{par.includi_dati_personali, par.includi_dati_particolari};
                    par.argomenti = {"pacchetto in memoria"};
                    if (&c == controlla) return controlla_su(c, par, std::move(lc)).uscita;
                    if (&c == ore) return ore_su(c, par, std::move(lc)).uscita;
                    return giornata_su(c, par, std::move(lc)).uscita;
                };
                // Con i dati particolari il tipo cambia l'esito delle leggi (altrimenti la coppia non proverebbe niente).
                {
                    Parametri par;
                    par.formato = Formato::Json;
                    par.includi_dati_personali = par.includi_dati_particolari = true;
                    par.dal = giorni.front() - 6;
                    par.al = giorni.back() + 6;
                    if (esito_delle_leggi(esegui(a, *ore, par)) != esito_delle_leggi(esegui(b, *ore, par))) ++con_effetto[tipi_coppia];
                }
                for (const bool personali : {false, true})
                    for (const Formato formato : {Formato::Testo, Formato::Json}) {
                        Parametri par;
                        par.formato = formato;
                        par.includi_dati_personali = personali;
                        std::vector<std::pair<std::string, Parametri>> comandi;
                        comandi.push_back({"controlla", par});
                        Parametri po = par;
                        po.dal = kzero::day_from_ymd(2026, 9, 1);
                        po.al = kzero::day_from_ymd(2027, 8, 31);
                        comandi.push_back({"ore di tutto il periodo", po});
                        for (const kzero::Day d : giorni) {
                            Parametri pg = par;
                            pg.data = d;
                            comandi.push_back({"giornata del " + kzero::text_from_day(d), pg});
                        }
                        for (const auto& [nome, p] : comandi) {
                            const DefinizioneComando& c = nome == "controlla" ? *controlla : nome.rfind("ore", 0) == 0 ? *ore : *giornata;
                            const std::string x = senza_totali(esegui(a, c, p), formato), y = senza_totali(esegui(b, c, p), formato);
                            if (x != y)
                                fughe += "\n  " + coppia + ", " + nome + (personali ? ", --includi-dati-personali" : "") +
                                         (formato == Formato::Json ? ", json" : ", testo") + ": " + prima_differenza(x, y);
                        }
                    }
            }
    }
    EXPECT_EQ(coppie, 30);
    // Ogni coppia di tipi cambia l'esito delle leggi in almeno uno dei servizi (nel servizio per disabili NCC e "altro" danno
    // lo stesso esito: la differenza, il 15 + 15, si vede nell'escursione con il pullman).
    std::string senza_effetto;
    for (const auto& [c, n] : con_effetto)
        if (n == 0) senza_effetto += "\n  " + c;
    EXPECT_EQ(con_effetto.size(), 10u);
    EXPECT_TRUE(senza_effetto.empty()) << "Coppie di tipi che non cambiano l'esito delle leggi in nessun servizio:" << senza_effetto;
    EXPECT_TRUE(fughe.empty()) << "Uscite diverse senza i dati particolari (fughe del tipo di servizio):" << fughe;
}

// K-013: il nucleo dice "dipende dal tipo di servizio" (Problem::by_service_kind) solo per i controlli che il catalogo dichiara
// (dal_tipo): cosi' la tabella di docs/controlli.md, provata contro il catalogo, dice il vero. Su tutti i pacchetti delle prove.
TEST(Unione, DipendeDalTipoSoloNeiControlliCheLoDichiarano) {
    std::string fuori;
    int dal_tipo = 0;
    for (const PacchettoDiProva& p : pacchetti_delle_prove()) {
        const Lettura l = leggi_pacchetto(p.chiaro);
        if (!l.forma_valida()) continue;
        for (const kzero::Problem& x : kzero::check_package(l.pacchetto)) {
            if (!x.by_service_kind) continue;
            ++dal_tipo;
            const DefinizioneControllo& d = definizione_controllo(x.check);
            if (!d.dal_tipo) fuori += "\n  " + p.nome + ": " + d.codice;
        }
    }
    EXPECT_GT(dal_tipo, 0);
    EXPECT_TRUE(fuori.empty()) << "Problemi che dipendono dal tipo di servizio in controlli che non lo dichiarano:" << fuori;
}

// K-013, con la cassaforte (decisione chiesta prima di scrivere il codice: "uguali sul tipo"): il titolare senza
// --includi-dati-particolari e l'utente con i soli dati personali vedono le stesse cose di quello che dipende dal tipo di
// servizio, con parole diverse: "riservato" per il titolare (i dati li ha, li vede con il parametro), "non verificato" per
// l'altro (non sono calcolati). Il titolare vede come il pacchetto in chiaro senza il parametro (la prova per differenza
// vale anche per lui); per l'utente con i soli dati personali leggi, lavoro e rientri restano "non verificati", come prima.
TEST(Unione, OrariSenzaDatiParticolariConLaCassaforte) {
    const DefinizioneComando* ore = trova_comando("ore");
    ASSERT_TRUE(ore);
    const InMemoria chiaro = da_cartella(std::string(KZERO_CARTELLA_PROVE) + "/../esempi/demo");
    const Chiuso c = chiudi(chiaro);
    const auto uscita = [&](const InMemoria* in_chiaro, Visibilita permessi, Visibilita mostra, Formato formato) {
        accesso::LetturaPerComando lc;
        const ValoriCassaforte v = valori(c, permessi);
        lc.lettura = in_chiaro ? leggi_pacchetto(*in_chiaro) : leggi_pacchetto(c.normali, &v);
        lc.visibilita = mostra;
        Parametri par;
        par.formato = formato;
        par.includi_dati_personali = mostra.personali;
        par.includi_dati_particolari = mostra.particolari;
        par.argomenti = {"pacchetto in memoria"};
        par.data = kzero::day_from_ymd(2026, 11, 11);
        par.persona = "P-A";
        return ore_su(*ore, par, std::move(lc)).uscita;
    };
    for (const bool personali : {false, true}) {
        const Visibilita mostra{personali, false};
        for (const Formato f : {Formato::Testo, Formato::Json}) {
            const std::string titolare = uscita(nullptr, Visibilita{true, true}, mostra, f);
            EXPECT_EQ(titolare, uscita(&chiaro, Visibilita{true, true}, mostra, f)) << personali;
            const std::string ufficio = uscita(nullptr, Visibilita{true, false}, mostra, f);
            for (const std::string* u : {&titolare, &ufficio})
                for (const char* regime : {"R-L138", "R-561", "R-234", "servizio disabili", "\"disabili\""})
                    EXPECT_EQ(u->find(regime), std::string::npos) << regime << "\n" << *u;
            if (f == Formato::Testo) {
                EXPECT_NE(titolare.find("  Regimi che dipendono dal tipo di servizio delle corse: riservati, anche quando l'azienda li segue per "
                                        "scelta, con il lavoro che contano, il lavoro per il contratto, i riposi e i problemi (dato particolare; "
                                        "--includi-dati-particolari)."),
                          std::string::npos)
                    << titolare;
                EXPECT_NE(ufficio.find("  Regimi che dipendono dal tipo di servizio delle corse: non verificati, anche quando l'azienda li segue "
                                       "per scelta, con il lavoro che contano, il lavoro per il contratto, i riposi e i problemi (servono i dati "
                                       "particolari)."),
                          std::string::npos)
                    << ufficio;
                EXPECT_NE(ufficio.find("Leggi, lavoro e rientri: non verificati"), std::string::npos) << ufficio;
            } else {
                const JsonDati t = JsonDati::parse(titolare), u = JsonDati::parse(ufficio);
                EXPECT_EQ(t.at("persone").at(0).at("regimi_dal_tipo_di_servizio").at("esito"), "riservato");
                EXPECT_EQ(u.at("persone").at(0).at("regimi_dal_tipo_di_servizio").at("esito"), "non_verificato");
                EXPECT_EQ(t.at("persone").at(0).at("giornate").at(0).at("lavoro_contratto_minuti"), "riservato");
                EXPECT_TRUE(u.at("persone").at(0).at("giornate").at(0).at("lavoro_contratto_minuti").is_null());
            }
        }
    }
}

// --- K-014: turni scoperti o senza mezzo senza il motivo (scelta A del supervisore, 5/11) ------------------------------------------

namespace {

// Il demo senza nessun requisito che dipende dal tipo di servizio (usi dei mezzi, autorizzazione del committente, vincolo di
// destinazione).
InMemoria senza_requisiti_del_tipo(InMemoria m) {
    JsonDati r = JsonDati::parse(m.file.at("regolamenti.json"));
    for (JsonDati& x : r.at("regolamenti")) {
        x.at("parametri").erase("usi_dei_mezzi");
        x.at("parametri").erase("autorizzazione_del_committente");
    }
    m.file["regolamenti.json"] = r.dump(2);
    JsonDati n = JsonDati::parse(m.file.at("norme.json"));
    for (JsonDati& x : n.at("norme")) x.erase("usi_per_servizio");
    m.file["norme.json"] = n.dump(2);
    JsonDati v = JsonDati::parse(m.file.at("mezzi.json"));
    for (JsonDati& x : v.at("mezzi")) x.erase("vincolo_destinazione");
    m.file["mezzi.json"] = v.dump(2);
    return m;
}

}  // namespace

// Senza i dati particolari, se il pacchetto ha almeno un requisito che dipende dal tipo di servizio, il motivo di un turno senza
// mezzo non si dice nemmeno per un fermo (una causa che non dipende dal tipo); senza nessun requisito del tipo si dice come
// prima. Con la cassaforte il titolare senza il parametro vede come il pacchetto in chiaro senza il parametro; chi non ha il
// permesso dei dati particolari legge "non verificato" (il suo piano non e' verificato).
TEST(Unione, TurniSenzaMezzoSenzaIlMotivo) {
    const DefinizioneComando* giornata = trova_comando("giornata");
    const DefinizioneComando* controlla = trova_comando("controlla");
    ASSERT_TRUE(giornata && controlla);
    // Il 2026-10-19 ALFA-01 e' in revisione: senza l'assegnazione del mezzo di scorta T-SCUOLA-1 resta senza mezzo.
    InMemoria demo = da_cartella(std::string(KZERO_CARTELLA_PROVE) + "/../esempi/demo");
    JsonDati a = JsonDati::parse(demo.file.at("assegnazioni.json"));
    a.at("assegnazioni").erase(0);
    demo.file["assegnazioni.json"] = a.dump(2);
    const InMemoria senza = senza_requisiti_del_tipo(demo);
    ASSERT_TRUE(leggi_pacchetto(senza).forma_valida());
    const auto esegui = [&](const DefinizioneComando& c, accesso::LetturaPerComando lc, Visibilita mostra, Formato f) {
        lc.visibilita = mostra;
        Parametri par;
        par.formato = f;
        par.includi_dati_personali = mostra.personali;
        par.includi_dati_particolari = mostra.particolari;
        par.argomenti = {"pacchetto in memoria"};
        if (&c == controlla) return controlla_su(c, par, std::move(lc)).uscita;
        par.data = kzero::day_from_ymd(2026, 10, 19);
        par.mezzo = "ALFA-01";
        return giornata_su(c, par, std::move(lc)).uscita;
    };
    const auto in_chiaro = [&](const InMemoria& m, const DefinizioneComando& c, Visibilita mostra, Formato f = Formato::Testo) {
        accesso::LetturaPerComando lc;
        lc.lettura = leggi_pacchetto(m);
        return esegui(c, std::move(lc), mostra, f);
    };
    const std::string senza_motivo = "il mezzo previsto ALFA-01 (AA 101 AA) non puo' farlo; il motivo e' riservato, perche' puo' "
                                     "dipendere dal tipo di servizio (dato particolare; --includi-dati-particolari).";
    for (const bool personali : {false, true}) {
        const std::string g = in_chiaro(demo, *giornata, Visibilita{personali, false});
        EXPECT_NE(g.find("TURNO SENZA MEZZO: " + senza_motivo), std::string::npos) << g;
        EXPECT_EQ(g.find("REVISIONE) "), std::string::npos) << g;   // il fermo dell'elenco del mezzo si vede, il motivo no
        const std::string c = in_chiaro(demo, *controlla, Visibilita{personali, false});
        EXPECT_NE(c.find("Avviso K-CTR-079 (mezzi.json, /mezzi/0; il punto preciso con --includi-dati-particolari): Il turno T-SCUOLA-1 "
                         "(Scuola 1) e' senza mezzo il 2026-10-19: " +
                         senza_motivo),
                  std::string::npos)
            << c;
        // Senza requisiti che dipendono dal tipo, il motivo si dice come prima.
        EXPECT_NE(in_chiaro(senza, *giornata, Visibilita{personali, false}).find("TURNO SENZA MEZZO: il mezzo ALFA-01 (AA 101 AA) e' in fermo (REVISIONE)."),
                  std::string::npos);
        EXPECT_NE(in_chiaro(senza, *controlla, Visibilita{personali, false}).find("Avviso K-CTR-079 (fermi.json, /fermi/0): Il turno T-SCUOLA-1"),
                  std::string::npos);
    }
    EXPECT_NE(in_chiaro(demo, *giornata, Visibilita{false, true}).find("e' in fermo (REVISIONE)."), std::string::npos);

    // Cassaforte.
    const Chiuso c = chiudi(demo);
    const auto chiuso = [&](Visibilita permessi, const DefinizioneComando& cmd, Visibilita mostra, Formato f) {
        accesso::LetturaPerComando lc;
        const ValoriCassaforte v = valori(c, permessi);
        lc.lettura = leggi_pacchetto(c.normali, &v);
        return esegui(cmd, std::move(lc), mostra, f);
    };
    for (const bool personali : {false, true})
        for (const Formato f : {Formato::Testo, Formato::Json})
            for (const DefinizioneComando* cmd : {giornata, controlla})
                EXPECT_EQ(chiuso(Visibilita{true, true}, *cmd, Visibilita{personali, false}, f), in_chiaro(demo, *cmd, Visibilita{personali, false}, f))
                    << cmd->nome << " " << personali;
    const std::string ufficio = chiuso(Visibilita{true, false}, *giornata, Visibilita{true, false}, Formato::Testo);
    EXPECT_NE(ufficio.find("TURNO SENZA MEZZO: il mezzo previsto ALFA-01 (AA 101 AA) non puo' farlo; il motivo non e' verificato (servono "
                           "i dati particolari, che l'utente non puo' aprire)."),
              std::string::npos)
        << ufficio;
}

// --- K-014: requisiti che dipendono dal tipo di servizio, prova per differenza -----------------------------------------------
// Criterio di accettazione del K-014 (arbitro): due pacchetti uguali tranne il tipo dichiarato di un servizio, con i requisiti
// veri del demo (mezzi e autisti come sono, senza usi, autorizzazioni o vincoli aggiunti apposta). Senza i dati particolari (di
// base e con i dati personali), in testo e JSON, kzero controlla, kzero giornata e kzero ore danno la stessa uscita tranne:
// - i totali dell'intero pacchetto (anche i problemi che si contano soltanto, e se giornata e ore si fermano per gli errori nei
//   dati: K-ERR-018);
// - per la scelta A del supervisore, il piano e quello che ne segue: in kzero giornata, per i turni il cui piano cambia quel giorno,
//   chi li fa, chi era previsto, il mezzo (anche "a bordo di"), se sono scoperti o senza mezzo, e i totali del giorno; in kzero
//   ore le persone il cui piano cambia nel periodo e la media tra le persone; in kzero controlla i problemi dei controlli fatti
//   sul piano del giorno (quelli che il catalogo dichiara bisognosi delle due classi) che nominano un turno, una persona o un
//   mezzo il cui piano cambia.
// In piu' nelle uscite senza i dati particolari non ci devono essere i testi e le posizioni delle cause dei requisiti del tipo
// (autorizzazione del committente, uso o distrazione del mezzo, vincolo di destinazione) ne' quelli delle altre cause dei turni
// scoperti o senza mezzo (assenza, disponibilita', scadenza, fermo, prestito).

namespace {

const std::vector<std::string> kTipi = {"linea", "scolastico", "disabili", "ncc", "altro"};

// Il demo con il tipo di un servizio cambiato. La descrizione del tipo (obbligatoria per "altro", ammessa per gli altri) c'e'
// in tutte le varianti, cosi' resta uguale nelle due uscite.
InMemoria demo_con_il_tipo(const std::string& servizio, const std::string& tipo) {
    InMemoria m = da_cartella(std::string(KZERO_CARTELLA_PROVE) + "/../esempi/demo");
    JsonDati s = JsonDati::parse(m.file.at("servizi.json"));
    bool trovato = false;
    for (JsonDati& x : s.at("servizi"))
        if (x.at("codice") == servizio) {
            x["tipo"] = tipo;
            x["tipo_descrizione"] = "Servizio della prova per differenza";
            trovato = true;
        }
    EXPECT_TRUE(trovato) << servizio;
    m.file["servizi.json"] = s.dump(2);
    return m;
}

// Il piano di un turno in un giorno: chi lo fa, chi era previsto e se un fatto glielo impedisce, il mezzo, quello previsto e
// se un fatto glielo impedisce, il mezzo su cui e' a bordo; con i codici di persone e mezzi.
struct FirmaDelTurno {
    std::string testo;
    std::set<std::string> codici;
};

std::map<std::pair<kzero::Day, std::string>, FirmaDelTurno> piano_di(const kzero::Package& p) {
    std::map<std::pair<kzero::Day, std::string>, FirmaDelTurno> out;
    const kzero::Planner piano(p);
    for (kzero::Day d = p.manifest.period.from; d <= p.manifest.period.to; ++d)
        for (const kzero::ShiftDay& sd : piano.day(d)) {
            FirmaDelTurno f;
            const auto codice = [&f](const auto* x) {
                if (!x) return std::string("-");
                f.codici.insert(x->code);
                return x->code;
            };
            f.testo = codice(sd.person) + "|" + codice(sd.planned) + "|" + std::to_string(static_cast<int>(sd.person_impediment)) + "|" +
                      codice(sd.vehicle) + "|" + codice(sd.planned_vehicle) + "|" + std::to_string(static_cast<int>(sd.vehicle_impediment)) +
                      "|" + codice(sd.aboard);
            out[{d, sd.shift->code}] = std::move(f);
        }
    return out;
}

// Dove il piano dei due pacchetti cambia: i giorni, i turni di quei giorni e i codici dei turni, delle persone e dei mezzi
// coinvolti.
struct PianoCambiato {
    std::set<kzero::Day> giorni;
    std::set<std::pair<kzero::Day, std::string>> turni;
    std::set<std::string> codici;
    bool vuoto() const { return turni.empty(); }
};

PianoCambiato piano_cambiato(const kzero::Package& a, const kzero::Package& b) {
    const auto pa = piano_di(a), pb = piano_di(b);
    PianoCambiato c;
    const auto segna = [&c](const std::pair<kzero::Day, std::string>& k, const FirmaDelTurno* x) {
        c.giorni.insert(k.first);
        c.turni.insert(k);
        c.codici.insert(k.second);
        if (x) c.codici.insert(x->codici.begin(), x->codici.end());
    };
    for (const auto& [k, f] : pa) {
        const auto it = pb.find(k);
        if (it == pb.end() || it->second.testo != f.testo) {
            segna(k, &f);
            if (it != pb.end()) segna(k, &it->second);
        }
    }
    for (const auto& [k, f] : pb)
        if (!pa.count(k)) segna(k, &f);
    return c;
}

// Il testo nomina uno dei codici (come parola intera: "P-C" non e' in "P-CONTRATTO")?
bool nomina(const std::string& testo, const std::set<std::string>& codici) {
    const auto parte = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'; };
    for (const std::string& c : codici)
        for (std::size_t i = testo.find(c); i != std::string::npos; i = testo.find(c, i + 1)) {
            const bool prima = i == 0 || !parte(testo[i - 1]);
            const bool dopo = i + c.size() == testo.size() || !parte(testo[i + c.size()]);
            if (prima && dopo) return true;
        }
    return false;
}

// I controlli fatti sul piano del giorno: quelli che il catalogo dichiara bisognosi di tutte e due le classi (chi fa i turni
// dipende dai dati personali, i mezzi anche dagli usi richiesti per tipo di servizio) e, dal K-017, i controlli dei turni
// (DefinizioneControllo::turni: richiedono i dati personali; senza i particolari mostrano solo i problemi certi).
const std::set<std::string>& controlli_del_piano() {
    static const std::set<std::string> c = [] {
        std::set<std::string> out;
        for (const DefinizioneControllo& d : catalogo_controlli())
            if (d.richiede_personali && (d.richiede_particolari || d.turni)) out.insert(d.codice);
        return out;
    }();
    return c;
}

std::vector<std::string> righe(const std::string& testo) {
    std::vector<std::string> v;
    std::istringstream in(testo);
    for (std::string r; std::getline(in, r);) v.push_back(r);
    return v;
}

// kzero controlla senza i totali e senza i problemi del piano che nominano un turno, una persona o un mezzo il cui piano cambia.
std::string controlla_da_confrontare(const std::string& uscita, Formato f, const PianoCambiato& c) {
    const auto del_piano = [&c](const std::string& codice, const std::string& testo) {
        return controlli_del_piano().count(codice) && nomina(testo, c.codici);
    };
    if (f == Formato::Json) {
        JsonDati j = JsonDati::parse(uscita);
        for (const char* k : {"riepilogo", "problemi_riservati"}) j.erase(k);
        JsonDati problemi = JsonDati::array();
        for (const JsonDati& x : j.at("problemi"))
            if (!del_piano(x.at("codice").get<std::string>(), x.at("messaggio").get<std::string>())) problemi.push_back(x);
        j["problemi"] = problemi;
        return j.dump(1);
    }
    static const std::regex problema(R"(^(Errore|Avviso) (K-CTR-[0-9]{3}) .*)");
    std::string out;
    for (const std::string& r : righe(uscita)) {
        if (r.rfind("Riepilogo:", 0) == 0 || r.rfind("Problemi sui dati particolari (salute), non elencati", 0) == 0) continue;
        std::smatch m;
        if (std::regex_match(r, m, problema) && del_piano(m[2].str(), r)) continue;
        out += r + "\n";
    }
    return out;
}

// kzero giornata senza i totali e senza il piano dei turni che cambiano quel giorno.
std::string giornata_da_confrontare(const std::string& uscita, Formato f, kzero::Day d, const PianoCambiato& c) {
    const bool cambia = c.giorni.count(d) > 0;
    if (f == Formato::Json) {
        JsonDati j = JsonDati::parse(uscita);
        j.erase("controllo");
        if (cambia) j.erase("totali");
        for (JsonDati& t : j.at("turni"))
            if (c.turni.count({d, t.at("codice").get<std::string>()}))
                for (const char* k : {"persona", "assegnazione", "persona_prevista", "scoperto", "motivo_scoperto", "mezzo", "mezzo_da",
                                      "mezzo_del_turno", "posizione_mezzo", "mezzo_previsto", "senza_mezzo", "motivo_senza_mezzo",
                                      "a_bordo_di"})
                    t.erase(k);
        return j.dump(1);
    }
    std::string out, turno;
    for (const std::string& r : righe(uscita)) {
        if (r.rfind("Controllo del pacchetto:", 0) == 0) continue;
        if (cambia && r.rfind("Totali:", 0) == 0) continue;
        if (!r.empty() && r[0] != ' ') turno = r.substr(0, r.find(' '));
        if (c.turni.count({d, turno}))
            if (r.rfind("   Persona:", 0) == 0 || r.rfind("   Prevista:", 0) == 0 || r.rfind("   Mezzo:", 0) == 0 ||
                r.rfind("   A bordo del mezzo:", 0) == 0)
                continue;
        out += r + "\n";
    }
    return out;
}

// kzero ore senza i totali e senza le persone il cui piano cambia (con la media tra le persone).
std::string ore_da_confrontare(const std::string& uscita, Formato f, const PianoCambiato& c) {
    if (f == Formato::Json) {
        JsonDati j = JsonDati::parse(uscita);
        j.erase("controllo");
        if (!c.vuoto()) j.erase("media_tra_le_persone");
        JsonDati persone = JsonDati::array();
        for (const JsonDati& p : j.at("persone"))
            if (!c.codici.count(p.at("codice").get<std::string>())) persone.push_back(p);
        j["persone"] = persone;
        return j.dump(1);
    }
    std::string out;
    bool salta = false;
    for (const std::string& r : righe(uscita)) {
        if (r.rfind("Controllo del pacchetto:", 0) == 0) continue;
        if (!c.vuoto() && r.rfind("Media tra le ", 0) == 0) continue;
        if (r.rfind("Persona ", 0) == 0) {
            const std::string codice = r.substr(8, r.find_first_of(" ,", 8) - 8);   // "Persona P-C (Verdi Carlo), ...": P-C
            salta = c.codici.count(codice) > 0;
        } else if (!r.empty() && r[0] != ' ') {
            salta = false;
        }
        if (!salta) out += r + "\n";
    }
    return out;
}

// I testi e le posizioni delle cause dei turni scoperti o senza mezzo e dei requisiti del tipo che compaiono nell'uscita.
const std::vector<std::string> kCause = {
    // Requisiti che dipendono dal tipo di servizio.
    "autorizzazione del committente", "che chiede l'uso", "non ha un'autorizzazione a quell'uso", "(distrazione) non vale",
    "vincolo di destinazione", "K-CTR-069", "K-CTR-072",
    // Le altre cause dei turni scoperti o senza mezzo.
    "e' in fermo", "e' assente", "non e' disponibile in quegli orari", "e' scaduta. Rimedio", "non vale. Rimedio",
    "non vale quel giorno", "il prestito della persona prevista", "il prestito del mezzo", "fermi.json"};

std::string cause_visibili(const std::string& uscita, Formato f) {
    std::string out;
    for (const std::string& c : kCause)
        if (uscita.find(c) != std::string::npos) out += " [" + c + "]";
    // Le posizioni di K-CTR-078 e 079: la persona o il mezzo previsto, mai il fatto.
    if (f == Formato::Json && !uscita.empty() && uscita.front() == '{') {
        const JsonDati j = JsonDati::parse(uscita);
        std::vector<const JsonDati*> problemi;
        if (j.contains("problemi") && j.at("problemi").is_array())
            for (const JsonDati& x : j.at("problemi")) problemi.push_back(&x);
        if (j.contains("persone"))
            for (const JsonDati& p : j.at("persone"))
                if (p.at("problemi").is_array())
                    for (const JsonDati& x : p.at("problemi")) problemi.push_back(&x);
        static const std::regex posizione(R"(^/(persone|mezzi)/[0-9]+$)");
        for (const JsonDati* x : problemi) {
            const std::string codice = x->at("codice").get<std::string>();
            if (codice != "K-CTR-078" && codice != "K-CTR-079") continue;
            if (!x->contains("posizione")) continue;   // kzero ore: i problemi della persona non hanno la posizione
            const std::string p = x->at("posizione").at("puntatore").get<std::string>();
            if (!std::regex_match(p, posizione) || x->value("posizione_completa_con", "") != "--includi-dati-particolari")
                out += " [posizione " + codice + " " + p + "]";
        }
    } else {
        static const std::regex riga(R"((K-CTR-07[89]) \(([^)]*)\))");
        static const std::regex buona(R"(^(persone\.json, /persone|mezzi\.json, /mezzi)/[0-9]+; il punto preciso con --includi-dati-particolari$)");
        for (auto it = std::sregex_iterator(uscita.begin(), uscita.end(), riga); it != std::sregex_iterator(); ++it)
            if (!std::regex_match((*it)[2].str(), buona)) out += " [posizione " + (*it)[1].str() + " " + (*it)[2].str() + "]";
    }
    return out;
}

// L'esito dei requisiti con tutti i dati: i problemi dei requisiti e dei turni scoperti o senza mezzo.
std::string esito_dei_requisiti(const Lettura& l) {
    const Resoconto r = controlla_lettura(l, Visibilita{true, true});
    std::string out;
    for (const ProblemaPresentato& p : r.problemi)
        if (p.codice == "K-CTR-067" || p.codice == "K-CTR-068" || p.codice == "K-CTR-069" || p.codice == "K-CTR-072" ||
            p.codice == "K-CTR-078" || p.codice == "K-CTR-079")
            out += p.codice + " " + p.messaggio + "\n";
    return out;
}

struct EsitoDelServizio {
    int coppie = 0;
    std::map<std::string, bool> con_effetto;   // coppia di tipi -> i requisiti o il piano cambiano
    std::string fughe;
};

// Le 10 coppie di tipi su un servizio.
EsitoDelServizio prova_per_differenza_dei_requisiti(const std::string& servizio) {
    const DefinizioneComando* controlla = trova_comando("controlla");
    const DefinizioneComando* giornata = trova_comando("giornata");
    const DefinizioneComando* ore = trova_comando("ore");
    EsitoDelServizio esito;
    // Ogni variante si legge una volta sola.
    std::map<std::string, Lettura> letture;
    for (const std::string& t : kTipi) letture.emplace(t, leggi_pacchetto(demo_con_il_tipo(servizio, t)));
    const auto esegui = [&](const Lettura& l, const DefinizioneComando& c, Parametri par) {
        accesso::LetturaPerComando lc;
        lc.lettura = l;
        lc.visibilita = Visibilita{par.includi_dati_personali, par.includi_dati_particolari};
        par.argomenti = {"pacchetto in memoria"};
        try {
            if (&c == controlla) return controlla_su(c, par, std::move(lc));
            if (&c == ore) return ore_su(c, par, std::move(lc));
            return giornata_su(c, par, std::move(lc));
        } catch (const ErroreKZero& e) {   // es. K-ERR-018: il pacchetto ha errori nei dati
            return Risultato{e.definizione().codice + " " + e.messaggio(), 1};
        }
    };
    // I giorni delle corse del servizio (il primo e l'ultimo, nel piano del primo tipo).
    std::vector<kzero::Day> giorni_del_servizio;
    {
        const kzero::Package& p = letture.at(kTipi.front()).pacchetto;
        const kzero::Planner piano(p);
        for (kzero::Day d = p.manifest.period.from; d <= p.manifest.period.to; ++d)
            for (const kzero::ShiftDay& sd : piano.day(d))
                for (const kzero::PlannedItem& it : sd.items)
                    if (it.trip)
                        for (const kzero::Ref& s : it.trip->services)
                            if (s.code == servizio && (giorni_del_servizio.empty() || giorni_del_servizio.back() != d))
                                giorni_del_servizio.push_back(d);
        EXPECT_FALSE(giorni_del_servizio.empty()) << servizio;
        if (giorni_del_servizio.size() > 2) giorni_del_servizio.erase(giorni_del_servizio.begin() + 1, giorni_del_servizio.end() - 1);
    }
    for (std::size_t i = 0; i < kTipi.size(); ++i)
        for (std::size_t k = i + 1; k < kTipi.size(); ++k) {
            ++esito.coppie;
            const std::string tipi_coppia = kTipi[i] + " <-> " + kTipi[k];
            const std::string coppia = servizio + " " + tipi_coppia;
            const Lettura& a = letture.at(kTipi[i]);
            const Lettura& b = letture.at(kTipi[k]);
            EXPECT_TRUE(a.forma_valida() && b.forma_valida()) << coppia;
            const PianoCambiato cambiato = piano_cambiato(a.pacchetto, b.pacchetto);
            // Con i dati particolari il tipo cambia i requisiti o il piano (altrimenti la coppia non proverebbe niente).
            esito.con_effetto[tipi_coppia] = esito_dei_requisiti(a) != esito_dei_requisiti(b) || !cambiato.vuoto();
            std::vector<kzero::Day> giorni = giorni_del_servizio;
            if (!cambiato.vuoto()) {
                giorni.push_back(*cambiato.giorni.begin());
                giorni.push_back(*cambiato.giorni.rbegin());
            }
            std::sort(giorni.begin(), giorni.end());
            giorni.erase(std::unique(giorni.begin(), giorni.end()), giorni.end());
            for (const bool personali : {false, true})
                for (const Formato formato : {Formato::Testo, Formato::Json}) {
                    Parametri par;
                    par.formato = formato;
                    par.includi_dati_personali = personali;
                    const std::string come = std::string(personali ? ", --includi-dati-personali" : "") + (formato == Formato::Json ? ", json" : ", testo");
                    const auto confronta = [&](const std::string& nome, const std::string& x, const std::string& y) {
                        if (x != y) esito.fughe += "\n  " + coppia + ", " + nome + come + ": " + prima_differenza(x, y);
                    };
                    const auto cause = [&](const std::string& nome, const Risultato& r, const std::string& tipo) {
                        const std::string c = cause_visibili(r.uscita, formato);
                        if (!c.empty()) esito.fughe += "\n  " + servizio + " " + tipo + ", " + nome + come + ": cause visibili" + c;
                    };
                    const Risultato ca = esegui(a, *controlla, par), cb = esegui(b, *controlla, par);
                    cause("controlla", ca, kTipi[i]);
                    cause("controlla", cb, kTipi[k]);
                    confronta("controlla", controlla_da_confrontare(ca.uscita, formato, cambiato), controlla_da_confrontare(cb.uscita, formato, cambiato));
                    Parametri po = par;
                    po.dal = a.pacchetto.manifest.period.from;
                    po.al = a.pacchetto.manifest.period.to;
                    const Risultato oa = esegui(a, *ore, po), ob = esegui(b, *ore, po);
                    cause("ore", oa, kTipi[i]);
                    cause("ore", ob, kTipi[k]);
                    // Con errori nei dati giornata e ore si fermano (K-ERR-018): se si fermano tutti e due o no e' un totale.
                    const bool fermi = oa.codice != 0 || ob.codice != 0;
                    if (!fermi) confronta("ore", ore_da_confrontare(oa.uscita, formato, cambiato), ore_da_confrontare(ob.uscita, formato, cambiato));
                    for (const kzero::Day d : giorni) {
                        Parametri pg = par;
                        pg.data = d;
                        const Risultato ga = esegui(a, *giornata, pg), gb = esegui(b, *giornata, pg);
                        const std::string nome = "giornata del " + kzero::text_from_day(d);
                        cause(nome, ga, kTipi[i]);
                        cause(nome, gb, kTipi[k]);
                        if (!fermi)
                            confronta(nome, giornata_da_confrontare(ga.uscita, formato, d, cambiato),
                                      giornata_da_confrontare(gb.uscita, formato, d, cambiato));
                    }
                }
        }
    return esito;
}

// Le coppie della prova sono quelle in cui, con i dati particolari, il tipo cambia i requisiti o il piano (altrimenti non
// proverebbero niente): devono essere esattamente quelle attese, cosi' un requisito non riconosciuto si vede. Le altre coppie
// (nel demo NCC e "altro" chiedono lo stesso uso, e scolastico e disabili gli stessi usi) girano lo stesso, come controllo in
// piu' delle fughe, ma non contano.
void controlla_esito(const std::string& servizio, const EsitoDelServizio& e, const std::set<std::string>& attese) {
    EXPECT_EQ(e.coppie, 10) << servizio;
    std::set<std::string> con_effetto;
    std::string senza_effetto;
    for (const auto& [c, si] : e.con_effetto) {
        if (si) con_effetto.insert(c);
        else senza_effetto += " [" + c + "]";
    }
    std::cout << "[ " << servizio << " ] coppie della prova (il tipo cambia i requisiti o il piano): " << con_effetto.size()
              << "; altre coppie provate:" << (senza_effetto.empty() ? " nessuna" : senza_effetto)
              << "; fughe: " << (e.fughe.empty() ? "nessuna" : "SI") << "\n";
    EXPECT_EQ(con_effetto, attese) << servizio;
    EXPECT_TRUE(e.fughe.empty()) << "Uscite diverse senza i dati particolari (fughe del tipo di servizio):" << e.fughe;
}

}  // namespace

// S-SCU-AM (scolastico): come servizio per disabili R-TN chiede l'autorizzazione del committente CVA, che P-F e P-G non hanno
// (K-CTR-068); come linea le norme chiedono l'uso LINEA, che i minibus non hanno (K-CTR-069).
TEST(Unione, RequisitiSenzaDatiParticolariScuolaAndataMattina) {
    const EsitoDelServizio e = prova_per_differenza_dei_requisiti("S-SCU-AM");
    controlla_esito("S-SCU-AM", e,
                    {"linea <-> scolastico", "linea <-> disabili", "linea <-> ncc", "linea <-> altro", "scolastico <-> disabili",
                     "disabili <-> ncc", "disabili <-> altro"});
    const Lettura disabili = leggi_pacchetto(demo_con_il_tipo("S-SCU-AM", "disabili"));
    const std::string r = esito_dei_requisiti(disabili);
    EXPECT_NE(r.find("K-CTR-068 Nel turno T-SCUOLA-2 (Scuola 2) la persona P-F (Moretti Luca) guida per il servizio S-SCU-AM"), std::string::npos) << r;
    EXPECT_NE(r.find("che chiede un'autorizzazione del committente CVA"), std::string::npos) << r;
}

// S-DIS (disabili) sul minibus ALFA-03, il cui vincolo di destinazione ammette solo i servizi per disabili: con ogni altro tipo
// K-CTR-072; come linea anche l'uso LINEA, che ALFA-03 non ha (K-CTR-069).
TEST(Unione, RequisitiSenzaDatiParticolariCentroDiurno) {
    const EsitoDelServizio e = prova_per_differenza_dei_requisiti("S-DIS");
    controlla_esito("S-DIS", e,
                    {"linea <-> scolastico", "linea <-> disabili", "linea <-> ncc", "linea <-> altro", "scolastico <-> disabili",
                     "disabili <-> ncc", "disabili <-> altro"});
    const std::string scolastico = esito_dei_requisiti(leggi_pacchetto(demo_con_il_tipo("S-DIS", "scolastico")));
    EXPECT_NE(scolastico.find("K-CTR-072 Nel turno T-DIS (Centro diurno) il mezzo ALFA-03 (AA 103 AA) fa il servizio S-DIS"), std::string::npos)
        << scolastico;
    const std::string linea = esito_dei_requisiti(leggi_pacchetto(demo_con_il_tipo("S-DIS", "linea")));
    EXPECT_NE(linea.find("che chiede l'uso LINEA"), std::string::npos) << linea;
}

// S-LIN-B610 (linea) sull'autobus ALFA-05, che ha l'uso NCC solo fino al 2027-06-30 (distrazione): come NCC dal 2027-07-01 i
// turni con ALFA-05 restano senza mezzo (K-CTR-079). Il piano cambia: il 2027-07-07 T-PRIMO, T-SECONDO e T-NOTTE sono senza
// mezzo e T-SECONDO non e' piu' scoperto (senza mezzo non serve la CQC scaduta di P-C).
TEST(Unione, RequisitiSenzaDatiParticolariLineaB610) {
    const EsitoDelServizio e = prova_per_differenza_dei_requisiti("S-LIN-B610");
    controlla_esito("S-LIN-B610", e,
                    {"linea <-> ncc", "linea <-> altro", "scolastico <-> ncc", "scolastico <-> altro", "disabili <-> ncc",
                     "disabili <-> altro"});
    const Lettura linea = leggi_pacchetto(demo_con_il_tipo("S-LIN-B610", "linea"));
    const Lettura ncc = leggi_pacchetto(demo_con_il_tipo("S-LIN-B610", "ncc"));
    const kzero::Day d = kzero::day_from_ymd(2027, 7, 7);
    const auto turno = [d](const kzero::Package& p, const std::string& codice) {
        for (const kzero::ShiftDay& sd : kzero::Planner(p).day(d))
            if (sd.shift->code == codice) return std::make_pair(sd.person != nullptr, sd.vehicle != nullptr);
        ADD_FAILURE() << codice;
        return std::make_pair(false, false);
    };
    for (const char* t : {"T-PRIMO", "T-SECONDO", "T-NOTTE"}) {
        EXPECT_TRUE(turno(linea.pacchetto, t).second) << t;
        EXPECT_FALSE(turno(ncc.pacchetto, t).second) << t;
    }
    EXPECT_FALSE(turno(linea.pacchetto, "T-SECONDO").first);   // scoperto: la CQC di P-C e' scaduta
    EXPECT_TRUE(turno(ncc.pacchetto, "T-SECONDO").first);      // senza mezzo nessuno controlla la CQC
    EXPECT_NE(esito_dei_requisiti(ncc).find("all'uso NCC (distrazione) non vale"), std::string::npos);
}

// La ricerca delle cause non e' vuota: con i dati particolari (e personali) le cause si vedono.
TEST(Unione, RequisitiConDatiParticolariLeCauseSiVedono) {
    const DefinizioneComando* controlla = trova_comando("controlla");
    std::string tutte;
    for (const auto& [s, t] : std::vector<std::pair<std::string, std::string>>{
             {"S-SCU-AM", "disabili"}, {"S-DIS", "scolastico"}, {"S-DIS", "linea"}, {"S-LIN-B610", "ncc"}}) {
        accesso::LetturaPerComando lc;
        lc.lettura = leggi_pacchetto(demo_con_il_tipo(s, t));
        lc.visibilita = Visibilita{true, true};
        Parametri par;
        par.includi_dati_personali = par.includi_dati_particolari = true;
        par.argomenti = {"pacchetto in memoria"};
        tutte += controlla_su(*controlla, par, std::move(lc)).uscita;
    }
    for (const char* c : {"autorizzazione del committente", "che chiede l'uso", "(distrazione) non vale", "vincolo di destinazione",
                          "e' assente", "e' scaduta. Rimedio", "K-CTR-069", "K-CTR-072"})
        EXPECT_NE(tutte.find(c), std::string::npos) << c;
}

// --- K-014, punto 4: la prova mancante del K-013 (sabotaggio B2) ----------------------------------------------------------
// "Nessuna legge vale" (K-CTR-107) dipende dal tipo di servizio quando una legge sugli orari ne dipende (KindDependence::law).
// Una legge sugli orari ha regole sul lavoro, sulla guida o sui riposi: nel demo le leggi che dipendono dal tipo hanno sempre
// anche regole sul lavoro (L. 138, D.Lgs. 234), quindi una "law" calcolata guardando solo il lavoro non si vedeva. Qui le norme
// del demo hanno una sola legge che dipende dal tipo, il Reg. 561, con regole solo su guida e riposi: senza i dati particolari
// "nessuna legge vale" e' riservato (K-CTR-107 si conta soltanto, in kzero ore "riservato").
TEST(Unione, NessunaLeggeRiservataConLaSolaLeggeDelTipoSuGuidaERiposi) {
    InMemoria m = da_cartella(std::string(KZERO_CARTELLA_PROVE) + "/../esempi/demo");
    JsonDati n = JsonDati::parse(m.file.at("norme.json"));
    JsonDati regimi = JsonDati::array();
    for (JsonDati& r : n.at("norme").at(0).at("regimi")) {
        const std::string c = r.at("codice").get<std::string>();
        if (c == "R-L138" || c == "R-234") continue;   // le leggi che dipendono dal tipo con regole sul lavoro
        r.erase("esclude");                            // il Reg. 561 escludeva la L. 138
        regimi.push_back(r);
    }
    n.at("norme").at(0)["regimi"] = regimi;
    m.file["norme.json"] = n.dump(2);
    const Lettura l = leggi_pacchetto(m);
    ASSERT_TRUE(l.forma_valida());

    // Nel nucleo: il solo regime che dipende dal tipo e' il Reg. 561, una legge sugli orari senza regole sul lavoro.
    const kzero::KindDependence k = kzero::kind_dependence(l.pacchetto.norms.at(0));
    ASSERT_EQ(k.regimes.size(), 1u);
    EXPECT_EQ(k.regimes.front()->code, "R-561");
    for (const kzero::WorkRule& w : k.regimes.front()->rules) EXPECT_NE(w.family, kzero::RuleFamily::Work) << w.code;
    EXPECT_FALSE(k.work);
    EXPECT_TRUE(k.law);
    int nessuna_legge = 0;
    for (const kzero::Problem& x : kzero::check_package(l.pacchetto))
        if (x.check == Check::EmployeeWithoutLaw) {
            ++nessuna_legge;
            EXPECT_TRUE(x.by_service_kind);
        }
    EXPECT_GT(nessuna_legge, 0);

    // Nelle uscite.
    const Resoconto r = controlla_lettura(l, Visibilita{true, true});
    ASSERT_EQ(r.errori_nei_dati(), 0);
    for (const bool personali : {false, true}) {
        const Resoconto base = controlla_lettura(l, Visibilita{personali, false});
        for (const ProblemaPresentato& p : base.problemi) EXPECT_NE(p.codice, "K-CTR-107") << p.messaggio;
        EXPECT_GE(base.avvisi_riservati, nessuna_legge);
        const DefinizioneComando* ore = trova_comando("ore");
        accesso::LetturaPerComando lc;
        lc.lettura = l;
        lc.visibilita = Visibilita{personali, false};
        Parametri par;
        par.formato = Formato::Json;
        par.includi_dati_personali = personali;
        par.argomenti = {"pacchetto in memoria"};
        par.dal = kzero::day_from_ymd(2026, 11, 9);
        par.al = kzero::day_from_ymd(2026, 11, 15);
        const JsonDati j = JsonDati::parse(ore_su(*ore, par, std::move(lc)).uscita);
        int giornate = 0;
        for (const JsonDati& p : j.at("persone"))
            for (const JsonDati& g : p.at("giornate")) {
                ++giornate;
                EXPECT_EQ(g.at("nessuna_legge"), "riservato") << p.at("codice") << " " << g.at("data");
            }
        EXPECT_GT(giornate, 0);
    }
    bool visibile = false;
    for (const ProblemaPresentato& p : r.problemi) visibile = visibile || p.codice == "K-CTR-107";
    EXPECT_TRUE(visibile);   // con i dati particolari si vede
}

// K-014: lo stesso turno scoperto per cause diverse (qui T-SECONDO: un'assenza di P-C il 2027-06-30 e la sua CQC scaduta dal
// 2027-07-07), senza i dati particolari, e' un problema solo con tutti i giorni, e il riepilogo lo conta una volta: contarlo due
// volte direbbe quante cause diverse ci sono. Con i dati particolari due problemi, ognuno con la sua causa.
TEST(Unione, TurnoScopertoPerCauseDiverseUnProblemaSolo) {
    InMemoria m = da_cartella(std::string(KZERO_CARTELLA_PROVE) + "/../esempi/demo");
    JsonDati a = JsonDati::parse(m.file.at("assenze.json"));
    a.at("assenze").push_back({{"persona", "P-C"}, {"tipo", "FERIE"}, {"dal", "2027-06-30"}, {"al", "2027-06-30"}});
    m.file["assenze.json"] = a.dump(2);
    const Lettura l = leggi_pacchetto(m);
    ASSERT_TRUE(l.forma_valida());
    const auto del_secondo = [](const Resoconto& r) {
        std::vector<const ProblemaPresentato*> v;
        for (const ProblemaPresentato& p : r.problemi)
            if (p.codice == "K-CTR-078" && p.messaggio.find("T-SECONDO") != std::string::npos) v.push_back(&p);
        return v;
    };
    const Resoconto tutti = controlla_lettura(l, Visibilita{true, true});
    const auto due = del_secondo(tutti);
    ASSERT_EQ(due.size(), 2u);
    for (const bool personali : {false, true}) {
        const Resoconto r = controlla_lettura(l, Visibilita{personali, false});
        const auto uno = del_secondo(r);
        ASSERT_EQ(uno.size(), 1u) << personali;
        EXPECT_EQ(uno.front()->tutti_i_giorni.size(), due[0]->tutti_i_giorni.size() + due[1]->tutti_i_giorni.size());
        EXPECT_NE(uno.front()->messaggio.find("il motivo e' riservato"), std::string::npos);
        EXPECT_EQ(r.avvisi, tutti.avvisi - 1);
    }
}

// --- K-017: il piano di chi non apre i dati particolari (scelta A del supervisore, 11/11) --------------------------------------
// Il tipo dei servizi e dei gruppi e' un dato particolare: per chi non puo' leggerlo non e' noto, mai "altro". Un fatto del piano
// che dipende dal tipo si valuta per ogni tipo possibile: uguale per tutti = certo, diverso = non verificato. Criteri (punto 5 del
// mandato), su varianti del demo che cambiano solo il tipo di un servizio (verso ogni altro tipo), lette dal titolare (tutti i
// permessi) e da chi ha i soli dati personali, chiuse in memoria:
// (a) per chi ha i soli dati personali kzero giornata e kzero controlla (testo e JSON, di base e con i dati personali) sono
//     identiche tra le varianti;
// (b) ogni risultato del piano dato per certo (chi fa il turno, il mezzo, il mezzo su cui e' a bordo) coincide con quello del
//     titolare in ogni variante. Le varianti in cui il titolare ha errori nei dati (la giornata si ferma, K-ERR-018) per (b) si
//     saltano e si elencano.

namespace {

// I risultati del piano di un pacchetto, turno per turno e giorno per giorno, con quello che e' verificato.
struct EsitoDelTurno {
    std::string persona, mezzo, a_bordo;   // "-" = nessuno
    bool persona_certa = true, mezzo_certo = true, a_bordo_certo = true;
};
using PianoVerificato = std::map<std::pair<kzero::Day, std::string>, EsitoDelTurno>;

PianoVerificato piano_verificato(const kzero::Package& p) {
    PianoVerificato out;
    const kzero::Planner piano(p);
    const auto codice = [](const auto* x) { return x ? x->code : std::string("-"); };
    for (kzero::Day d = p.manifest.period.from; d <= p.manifest.period.to; ++d)
        for (const kzero::ShiftDay& sd : piano.day(d))
            out[{d, sd.shift->code}] = {codice(sd.person), codice(sd.vehicle), codice(sd.aboard), sd.person_verified, sd.vehicle_verified,
                                        sd.aboard_verified};
    return out;
}

// Un pacchetto chiuso in memoria, letto da chi ha tutti i permessi e da chi ha i soli dati personali.
struct DueLetture {
    Chiuso chiuso;
    Lettura titolare, personali;
};

DueLetture due_letture(const InMemoria& chiaro) {
    DueLetture d{chiudi(chiaro), {}, {}};
    const ValoriCassaforte tutti = valori(d.chiuso, {true, true}), personali = valori(d.chiuso, {true, false});
    d.titolare = leggi_pacchetto(d.chiuso.normali, &tutti);
    d.personali = leggi_pacchetto(d.chiuso.normali, &personali);
    return d;
}

Risultato esegui_su(const Lettura& l, const std::string& comando, Parametri par) {
    accesso::LetturaPerComando lc;
    lc.lettura = l;
    lc.visibilita = Visibilita{par.includi_dati_personali, par.includi_dati_particolari};
    par.argomenti = {"pacchetto in memoria"};
    const DefinizioneComando& c = *trova_comando(comando);
    try {
        if (comando == "controlla") return controlla_su(c, par, std::move(lc));
        return giornata_su(c, par, std::move(lc));
    } catch (const ErroreKZero& e) {
        return Risultato{e.definizione().codice + " " + e.messaggio(), 1};
    }
}

// I totali di una giornata JSON contano solo cio' che e' certo: devono venire dai turni mostrati (un risultato non verificato e'
// null). Restituisce cosa non torna.
std::string totali_della_giornata(const std::string& uscita) {
    const JsonDati j = JsonDati::parse(uscita);
    long long scoperti = 0, senza_mezzo = 0, non_verificati = 0;
    std::set<std::string> persone, mezzi;
    for (const JsonDati& t : j.at("turni")) {
        scoperti += t.at("scoperto") == true ? 1 : 0;
        senza_mezzo += t.at("senza_mezzo") == true ? 1 : 0;
        non_verificati += t.at("non_verificato").is_null() ? 0 : 1;
        if (!t.at("persona").is_null()) persone.insert(t.at("persona").at("codice").get<std::string>());
        if (!t.at("mezzo").is_null()) mezzi.insert(t.at("mezzo").at("codice").get<std::string>());
        // Un risultato non verificato non si mostra come certo.
        const JsonDati& nv = t.at("non_verificato");
        if (!nv.is_null() && nv.at("persona") == true && (!t.at("persona").is_null() || !t.at("scoperto").is_null()))
            return "turno " + t.at("codice").get<std::string>() + ": persona non verificata mostrata come certa";
        if (!nv.is_null() && nv.at("mezzo") == true && (!t.at("mezzo").is_null() || !t.at("senza_mezzo").is_null()))
            return "turno " + t.at("codice").get<std::string>() + ": mezzo non verificato mostrato come certo";
    }
    const JsonDati& tot = j.at("totali");
    std::string male;
    if (tot.at("scoperti") != scoperti) male += " scoperti";
    if (tot.at("senza_mezzo") != senza_mezzo) male += " senza_mezzo";
    if (tot.at("non_verificati") != non_verificati) male += " non_verificati";
    if (tot.at("persone") != persone.size()) male += " persone";
    if (tot.at("mezzi") != mezzi.size()) male += " mezzi";
    return male;
}

struct EsitoDelPiano {
    std::string fughe;                       // (a): uscite diverse tra le varianti per chi ha i soli dati personali
    std::string sbagliati;                   // (b): un risultato certo diverso da quello del titolare
    std::vector<std::string> con_errori;     // varianti saltate per (b): il titolare ha errori nei dati
    int certi = 0, non_verificati = 0;       // turni nei giorni del periodo (varianti senza errori)
    int varianti = 0;
};

// Quanti problemi dei controlli dei turni il resoconto di kzero controlla mostra e quanti conta tra i non verificati, per chi ha
// i soli dati personali, e quanti ne dice la stessa regola applicata ai problemi del nucleo (problemi_per_giorno, che la prova
// dei 375 pacchetti confronta con chi ha tutti i dati): devono coincidere. Restituisce cosa non torna.
std::string resoconto_dei_turni(const Lettura& personali) {
    const Visibilita v{true, false};
    const Resoconto r = controlla_lettura(personali, v);
    std::set<std::string> dei_turni;
    for (const DefinizioneControllo& d : catalogo_controlli())
        if (d.turni) dei_turni.insert(d.codice);
    int mostrati = 0;
    for (const ProblemaPresentato& p : r.problemi) mostrati += dei_turni.count(p.codice) ? 1 : 0;
    std::vector<kzero::Problem> problemi = tutti_i_problemi(personali);
    if (motivo_dei_turni_riservato(personali.pacchetto, v)) problemi = turni_senza_motivo(std::move(problemi), personali.pacchetto, true);
    int attesi = 0, non_verificati = 0;
    for (const kzero::Problem& p : problemi) {
        const DefinizioneControllo& d = definizione_controllo(p.check);
        if (!d.turni) continue;
        if (p.unverified || p.by_service_kind) ++non_verificati;
        else if (verificabile(d, p, classe_della_posizione(p.where, personali), in_trasportato(p.where, personali), v)) ++attesi;
    }
    if (!r.turni_non_verificati) return "manca il conteggio dei non verificati";
    std::string male;
    if (mostrati != attesi) male += " mostrati " + std::to_string(mostrati) + " invece di " + std::to_string(attesi);
    if (r.turni_non_verificati->first + r.turni_non_verificati->second != non_verificati)
        male += " non verificati " + std::to_string(r.turni_non_verificati->first + r.turni_non_verificati->second) + " invece di " +
                std::to_string(non_verificati);
    return male;
}

EsitoDelPiano prova_del_piano(const std::string& servizio, const std::function<void(InMemoria&)>& modifica = {}) {
    EsitoDelPiano e;
    std::map<std::string, std::string> prima;   // (a): l'uscita della prima variante, per uscita
    const std::vector<kzero::Day> giorni = {*kzero::day_from_text("2026-11-11"), *kzero::day_from_text("2027-07-07")};
    for (const std::string& tipo : kTipi) {
        ++e.varianti;
        const std::string variante = servizio + " " + tipo;
        InMemoria chiaro = demo_con_il_tipo(servizio, tipo);
        if (modifica) modifica(chiaro);
        const DueLetture l = due_letture(chiaro);
        if (!l.titolare.forma_valida() || !l.personali.forma_valida()) {
            e.sbagliati += "\n  " + variante + ": errori di forma";
            continue;
        }
        // (b) anche per i controlli dei turni (punto 4: una regola sola con la giornata): ogni giorno di un problema mostrato a
        // chi ha i soli dati personali e' un problema del titolare; ogni problema del titolare e' mostrato o non verificato. E il
        // resoconto di kzero controlla segue la stessa regola.
        std::map<std::string, std::string> diversi;
        confronta_problemi(problemi_per_giorno(l.titolare, {true, false}), problemi_per_giorno(l.personali, {true, false}), diversi);
        for (const auto& [k, esempio] : diversi) e.sbagliati += "\n  " + variante + ", controlla: " + k + " es. " + esempio.substr(0, 200);
        const std::string resoconto = resoconto_dei_turni(l.personali);
        if (!resoconto.empty()) e.sbagliati += "\n  " + variante + ", resoconto di controlla:" + resoconto;
        // (a) Le uscite di chi ha i soli dati personali.
        for (const bool personali : {false, true})
            for (const Formato f : {Formato::Testo, Formato::Json}) {
                Parametri par;
                par.formato = f;
                par.includi_dati_personali = personali;
                const std::string come = std::string(personali ? " --includi-dati-personali" : "") + (f == Formato::Json ? " json" : " testo");
                std::vector<std::pair<std::string, Risultato>> uscite = {{"controlla" + come, esegui_su(l.personali, "controlla", par)}};
                for (const kzero::Day d : giorni) {
                    Parametri pg = par;
                    pg.data = d;
                    uscite.push_back({"giornata del " + kzero::text_from_day(d) + come, esegui_su(l.personali, "giornata", pg)});
                }
                for (const auto& [nome, r] : uscite) {
                    const auto [it, nuova] = prima.emplace(nome, r.uscita);
                    if (!nuova && it->second != r.uscita) e.fughe += "\n  " + variante + ", " + nome + ": " + prima_differenza(it->second, r.uscita);
                    if (f == Formato::Json && nome.rfind("giornata", 0) == 0 && r.codice == 0) {
                        const std::string m = totali_della_giornata(r.uscita);
                        if (!m.empty()) e.sbagliati += "\n  " + variante + ", " + nome + ": totali o turni sbagliati:" + m;
                    }
                }
            }
        // (b) I risultati certi, giorno per giorno, contro quelli del titolare.
        const Resoconto r = controlla_lettura(l.titolare, Visibilita{true, true});
        if (r.errori_nei_dati() > 0) {
            e.con_errori.push_back(tipo);
            continue;
        }
        const PianoVerificato t = piano_verificato(l.titolare.pacchetto), p = piano_verificato(l.personali.pacchetto);
        if (t.size() != p.size()) e.sbagliati += "\n  " + variante + ": turni diversi";
        for (const auto& [chiave, x] : p) {
            const auto it = t.find(chiave);
            if (it == t.end()) continue;
            const EsitoDelTurno& y = it->second;
            if (!y.persona_certa || !y.mezzo_certo || !y.a_bordo_certo) e.sbagliati += "\n  " + variante + ": il titolare ha un risultato non verificato";
            const bool certo = x.persona_certa && x.mezzo_certo && x.a_bordo_certo;
            (certo ? e.certi : e.non_verificati) += 1;
            const std::string dove = "\n  " + variante + ", " + kzero::text_from_day(chiave.first) + " " + chiave.second + ": ";
            if (x.persona_certa && x.persona != y.persona) e.sbagliati += dove + "persona " + x.persona + " invece di " + y.persona;
            if (x.mezzo_certo && x.mezzo != y.mezzo) e.sbagliati += dove + "mezzo " + x.mezzo + " invece di " + y.mezzo;
            if (x.a_bordo_certo && x.a_bordo != y.a_bordo) e.sbagliati += dove + "a bordo di " + x.a_bordo + " invece di " + y.a_bordo;
            if (e.sbagliati.size() > 4000) return e;
        }
    }
    return e;
}

void controlla_il_piano(const std::string& servizio, const std::function<void(InMemoria&)>& modifica = {}) {
    const EsitoDelPiano e = prova_del_piano(servizio, modifica);
    std::string saltate;
    for (const std::string& t : e.con_errori) saltate += " " + t;
    std::cout << "[ " << servizio << " ] varianti: " << e.varianti << "; turni nei giorni del periodo: " << e.certi << " certi, "
              << e.non_verificati << " non verificati; saltate per (b), il titolare ha errori nei dati:"
              << (saltate.empty() ? std::string(" nessuna") : saltate) << std::endl;
    EXPECT_EQ(e.varianti, 5);
    EXPECT_TRUE(e.fughe.empty()) << "Uscite diverse tra le varianti per chi ha i soli dati personali:" << e.fughe;
    EXPECT_TRUE(e.sbagliati.empty()) << "Risultati certi diversi da quelli del titolare:" << e.sbagliati;
    EXPECT_LT(e.con_errori.size(), 5u) << servizio << ": nessuna variante provata per (b)";
}

}  // namespace

TEST(Unione, PianoSenzaParticolariScuolaAndataMattina) { controlla_il_piano("S-SCU-AM"); }
TEST(Unione, PianoSenzaParticolariScuolaRitornoMezzogiorno) { controlla_il_piano("S-SCU-RM"); }
TEST(Unione, PianoSenzaParticolariSuperiori) { controlla_il_piano("S-SUP-RM"); }
TEST(Unione, PianoSenzaParticolariCentroDiurno) { controlla_il_piano("S-DIS"); }
TEST(Unione, PianoSenzaParticolariAsilo) { controlla_il_piano("S-ASILO"); }
TEST(Unione, PianoSenzaParticolariLineaB610) { controlla_il_piano("S-LIN-B610"); }
TEST(Unione, PianoSenzaParticolariCentroDiurnoGiovedi) { controlla_il_piano("S-DIS-GIO"); }
TEST(Unione, PianoSenzaParticolariLineaB620) { controlla_il_piano("S-LIN-B620"); }
TEST(Unione, PianoSenzaParticolariGita) { controlla_il_piano("S-GITA"); }
TEST(Unione, PianoSenzaParticolariMatrimonio) { controlla_il_piano("S-MATRIMONIO"); }
TEST(Unione, PianoSenzaParticolariNoleggioPrivato) { controlla_il_piano("S-PRIVATO-CENTRO"); }

// Due casi che nel demo non ci sono, con la stessa prova (sabotaggi del K-017).
// Il mezzo abituale segue chi fa il turno: T-SCUOLA-1 con il mezzo abituale di P-E e la sua autorizzazione del committente CVA
// scaduta (R-TN la chiede per i servizi per disabili; P-F e P-G la hanno valida, cosi' la variante "disabili" non ha errori):
// senza i dati particolari chi fa il turno non e' verificato, e quindi nemmeno il mezzo.
void mezzo_abituale_e_autorizzazione(InMemoria& m) {
    JsonDati t = JsonDati::parse(m.file.at("turni.json"));
    for (JsonDati& x : t.at("turni"))
        if (x.at("codice") == "T-SCUOLA-1") x["mezzo"] = {{"modo", "abituale"}};
    m.file["turni.json"] = t.dump(2);
    JsonDati p = JsonDati::parse(m.file.at("persone.json"));
    for (JsonDati& x : p.at("persone")) {
        if (x.at("codice") == "P-E") x.at("autorizzazioni")[0]["scadenza"] = "2026-10-01";
        if (x.at("codice") == "P-F" || x.at("codice") == "P-G")
            x["autorizzazioni"] = JsonDati::array({{{"committente", "CVA"}, {"riferimento", "Cartellino di prova (inventato)"}, {"scadenza", "2027-08-31"}}});
    }
    m.file["persone.json"] = p.dump(2);
}

TEST(Unione, PianoSenzaParticolariMezzoAbituale) {
    controlla_il_piano("S-SCU-AM", mezzo_abituale_e_autorizzazione);
    InMemoria m = demo_in_chiaro();
    mezzo_abituale_e_autorizzazione(m);
    const DueLetture l = due_letture(m);
    int tutti_e_due = 0;
    for (const auto& [k, x] : piano_verificato(l.personali.pacchetto))
        tutti_e_due += k.second == "T-SCUOLA-1" && !x.persona_certa && !x.mezzo_certo && x.mezzo == "ALFA-01" ? 1 : 0;
    EXPECT_GT(tutti_e_due, 100);   // i giorni di scuola dal 2026-10-01
}

// Una sovrapposizione con un turno non verificato: il 2027-07-07 ALFA-05 anche su T-DIS (assegnazione del singolo giorno, certa);
// su T-PRIMO non e' verificato (come linea si', come NCC o "altro" no): la sovrapposizione dei mezzi (K-CTR-064) non e' verificata.
// Senza il blocco della corsa del centro diurno su ALFA-03 (sarebbe un errore in ogni variante: nessuna da provare per (b)).
void alfa05_su_tdis(InMemoria& m) {
    JsonDati a = JsonDati::parse(m.file.at("assegnazioni.json"));
    a.at("assegnazioni").push_back({{"data", "2027-07-07"}, {"turno", "T-DIS"}, {"mezzo", "ALFA-05"}});
    m.file["assegnazioni.json"] = a.dump(2);
    JsonDati b = JsonDati::parse(m.file.at("blocchi.json"));
    JsonDati tenuti = JsonDati::array();
    for (const JsonDati& x : b.at("blocchi"))
        if (x.at("codice") != "B-DIS") tenuti.push_back(x);
    b["blocchi"] = tenuti;
    m.file["blocchi.json"] = b.dump(2);
}

TEST(Unione, PianoSenzaParticolariSovrapposizione) {
    controlla_il_piano("S-LIN-B610", alfa05_su_tdis);
    InMemoria m = demo_in_chiaro();
    alfa05_su_tdis(m);
    const DueLetture l = due_letture(m);
    const ProblemiPerGiorno p = problemi_per_giorno(l.personali, {true, false});
    std::string trovati;
    for (const std::string& k : p.non_verificati)
        if (k.rfind("K-CTR-064", 0) == 0) trovati += " [" + k + "]";
    for (const std::string& k : p.mostrati)
        if (k.rfind("K-CTR-064", 0) == 0) trovati += " mostrato [" + k + "]";
    EXPECT_TRUE(p.non_verificati.count("K-CTR-064 turno=T-DIS mezzo=ALFA-05 2027-07-07"))
        << "la sovrapposizione non e' tra i non verificati:" << trovati;
}

// Ogni servizio del demo ha la sua prova qui sopra (un servizio nuovo nel demo va aggiunto).
TEST(Unione, PianoSenzaParticolariTuttiIServizi) {
    const Lettura l = leggi_pacchetto(demo_in_chiaro());
    std::set<std::string> servizi;
    for (const kzero::Service& s : l.pacchetto.services) servizi.insert(s.code);
    EXPECT_EQ(servizi, (std::set<std::string>{"S-SCU-AM", "S-SCU-RM", "S-SUP-RM", "S-DIS", "S-ASILO", "S-LIN-B610", "S-DIS-GIO", "S-LIN-B620",
                                              "S-GITA", "S-MATRIMONIO", "S-PRIVATO-CENTRO"}));
}

// Per chi non puo' leggerlo il tipo dei servizi e dei gruppi non e' noto (mai "altro"); per chi puo', sempre noto. I tipi possibili
// sono tutti quelli dello schema dei servizi.
TEST(Unione, TipoNonNotoMaiAltro) {
    const DueLetture l = due_letture(demo_in_chiaro());
    ASSERT_TRUE(l.titolare.forma_valida() && l.personali.forma_valida());
    for (const kzero::Service& s : l.personali.pacchetto.services) {
        EXPECT_FALSE(s.kind.known()) << s.code;
        EXPECT_THROW((void)s.kind.value(), std::logic_error) << s.code;
        EXPECT_EQ(kzero::possible_kinds(s.kind).size(), 5u);
        for (const kzero::PassengerGroup& g : s.groups) EXPECT_FALSE(g.type.known()) << g.code;
    }
    for (const kzero::Service& s : l.titolare.pacchetto.services) {
        EXPECT_TRUE(s.kind.known()) << s.code;
        for (const kzero::PassengerGroup& g : s.groups) EXPECT_TRUE(g.type.known()) << g.code;
    }
    // La categoria della corsa nasce dai tipi: per chi non li legge non e' nota (tranne il promiscuo di piu' servizi).
    const kzero::Index idx(l.personali.pacchetto);
    for (const kzero::Trip& t : l.personali.pacchetto.trips)
        EXPECT_EQ(idx.category(t).known(), t.services.size() > 1) << t.code;
    // I tipi possibili sono quelli dello schema dei servizi (versione di oggi).
    const JsonDati& schema = *SchemiDati::incorporati().schema("servizi", 3);
    std::set<std::string> dello_schema, del_programma;
    const std::function<void(const JsonDati&)> cerca = [&](const JsonDati& n) {
        if (n.is_object()) {
            if (n.contains("properties") && n.at("properties").contains("tipo") && n.at("properties").contains("liberta_oraria"))
                for (const auto& v : n.at("properties").at("tipo").at("enum")) dello_schema.insert(v.get<std::string>());
            for (const auto& [k, v] : n.items()) cerca(v);
        } else if (n.is_array()) {
            for (const auto& v : n) cerca(v);
        }
    };
    cerca(schema);
    for (const kzero::ServiceKind k : kzero::kServiceKinds) del_programma.insert(nome_tipo_servizio(k));
    EXPECT_FALSE(dello_schema.empty());
    EXPECT_EQ(dello_schema, del_programma);
}

// Il caso del mandato: il demo chiuso, il 2027-07-07, per chi ha i soli dati personali. Prima: "9 turni (nessuno scoperto, 3 senza
// mezzo), 4 mezzi", con tre TURNO SENZA MEZZO su ALFA-05 che non esistono (il tipo che mancava era diventato "altro", che chiede
// l'uso NCC, scaduto il 2027-06-30). Ora i tre turni con ALFA-05 non sono verificati (il mezzo, e per T-SECONDO anche la persona:
// la CQC-P di P-C e' scaduta e conta solo se il mezzo c'e'); nessun turno senza mezzo o scoperto inventato. Il titolare resta
// com'era: 1 scoperto (T-SECONDO), nessuno senza mezzo, 5 mezzi.
TEST(Unione, DemoChiusoLuglio2027SoloPersonali) {
    const DueLetture l = due_letture(demo_in_chiaro());
    Parametri par;
    par.formato = Formato::Json;
    par.data = *kzero::day_from_text("2027-07-07");
    const Risultato r = esegui_su(l.personali, "giornata", par);
    ASSERT_EQ(r.codice, 0) << r.uscita;
    const JsonDati j = JsonDati::parse(r.uscita);
    EXPECT_EQ(j.at("totali").at("turni"), 9);
    EXPECT_EQ(j.at("totali").at("scoperti"), 0);
    EXPECT_EQ(j.at("totali").at("senza_mezzo"), 0);
    EXPECT_EQ(j.at("totali").at("non_verificati"), 3);
    EXPECT_EQ(j.at("totali").at("persone"), 8);
    EXPECT_EQ(j.at("totali").at("mezzi"), 4);
    EXPECT_EQ(totali_della_giornata(r.uscita), "");
    std::map<std::string, JsonDati> turni;
    for (const JsonDati& t : j.at("turni")) turni[t.at("codice").get<std::string>()] = t;
    for (const char* t : {"T-PRIMO", "T-SECONDO", "T-NOTTE"}) {
        const JsonDati& x = turni.at(t);
        EXPECT_EQ(x.at("non_verificato").at("mezzo"), true) << t;
        EXPECT_TRUE(x.at("mezzo").is_null()) << t;
        EXPECT_TRUE(x.at("senza_mezzo").is_null()) << t;
        EXPECT_EQ(x.at("mezzo_previsto").at("codice"), "ALFA-05") << t;
        EXPECT_EQ(x.at("non_verificato").at("motivo"), "dipende dal tipo di servizio (dato particolare)") << t;
    }
    EXPECT_EQ(turni.at("T-PRIMO").at("persona").at("codice"), "P-B");
    EXPECT_EQ(turni.at("T-NOTTE").at("persona").at("codice"), "P-NUOVO");
    EXPECT_EQ(turni.at("T-SECONDO").at("non_verificato").at("persona"), true);
    EXPECT_TRUE(turni.at("T-SECONDO").at("scoperto").is_null());
    EXPECT_EQ(turni.at("T-SECONDO").at("persona_prevista").at("codice"), "P-C");
    for (const auto& [codice, x] : turni) {
        if (codice != "T-PRIMO" && codice != "T-SECONDO" && codice != "T-NOTTE") {
            EXPECT_TRUE(x.at("non_verificato").is_null()) << codice;
        }
    }
    EXPECT_EQ(j.at("non_verificato").size(), 1u);
    EXPECT_EQ(j.at("non_verificato")[0].at("turni"), 3);
    // Il testo dice lo stesso.
    par.formato = Formato::Testo;
    const std::string testo = esegui_su(l.personali, "giornata", par).uscita;
    EXPECT_NE(testo.find("Totali: 9 turni (nessuno scoperto, nessuno senza mezzo, 3 non verificati), 8 persone, 4 mezzi;"), std::string::npos);
    EXPECT_NE(testo.find("Mezzo: non verificato - previsto ALFA-05"), std::string::npos);
    EXPECT_NE(testo.find("Persona: non verificata - prevista P-C"), std::string::npos);
    EXPECT_EQ(testo.find("SENZA MEZZO"), std::string::npos);
    EXPECT_EQ(testo.find("SCOPERTO"), std::string::npos);
    // Il titolare, senza e con i dati particolari, come prima.
    for (const bool particolari : {false, true}) {
        Parametri pt;
        pt.formato = Formato::Json;
        pt.data = par.data;
        pt.includi_dati_particolari = particolari;
        const JsonDati t = JsonDati::parse(esegui_su(l.titolare, "giornata", pt).uscita);
        EXPECT_EQ(t.at("totali").at("scoperti"), 1);
        EXPECT_EQ(t.at("totali").at("senza_mezzo"), 0);
        EXPECT_EQ(t.at("totali").at("non_verificati"), 0);
        EXPECT_EQ(t.at("totali").at("mezzi"), 5);
        for (const JsonDati& x : t.at("turni")) EXPECT_TRUE(x.at("non_verificato").is_null());
    }
}

// Il calcolo non legge il tipo in nessun modo: ne' dai codici esterni dei servizi ne' dai loro nomi. Cambiarli non cambia cosa e'
// certo e cosa non e' verificato per chi ha i soli dati personali (nel demo i codici esterni dicono "linea", "servizio", "scheda").
TEST(Unione, PianoSenzaParticolariNonLeggeCodiciEsterniNeNomi) {
    const InMemoria base = demo_in_chiaro();
    InMemoria cambiato = base;
    JsonDati s = JsonDati::parse(cambiato.file.at("servizi.json"));
    int n = 0;
    for (JsonDati& x : s.at("servizi")) {
        x.erase("codici_esterni");
        x["nome"] = "Servizio " + std::to_string(++n);
    }
    cambiato.file["servizi.json"] = s.dump(2);
    const DueLetture a = due_letture(base), b = due_letture(cambiato);
    ASSERT_TRUE(a.personali.forma_valida() && b.personali.forma_valida());
    const PianoVerificato pa = piano_verificato(a.personali.pacchetto), pb = piano_verificato(b.personali.pacchetto);
    int non_verificati = 0;
    for (const auto& [k, x] : pa) non_verificati += x.persona_certa && x.mezzo_certo && x.a_bordo_certo ? 0 : 1;
    EXPECT_GT(non_verificati, 0);
    ASSERT_EQ(pa.size(), pb.size());
    for (const auto& [k, x] : pa) {
        const EsitoDelTurno& y = pb.at(k);
        EXPECT_TRUE(x.persona == y.persona && x.mezzo == y.mezzo && x.a_bordo == y.a_bordo && x.persona_certa == y.persona_certa &&
                    x.mezzo_certo == y.mezzo_certo && x.a_bordo_certo == y.a_bordo_certo)
            << kzero::text_from_day(k.first) << " " << k.second;
    }
    const Resoconto ra = controlla_lettura(a.personali, Visibilita{}), rb = controlla_lettura(b.personali, Visibilita{});
    ASSERT_TRUE(ra.turni_non_verificati && rb.turni_non_verificati);
    EXPECT_EQ(*ra.turni_non_verificati, *rb.turni_non_verificati);
    EXPECT_EQ(ra.avvisi, rb.avvisi);
    EXPECT_EQ(ra.errori, rb.errori);
}
