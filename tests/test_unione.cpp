// K-Zero - prove interne della separazione e dell'unione dei dati con la cassaforte (K-010), senza cifratura: il pacchetto
// demo separato in memoria in file normali e valori, poi riletto da utenti con permessi diversi.
// - la separazione segue la stessa regola delle classi con cui kzero controlla conta i valori;
// - l'unione e' per codice (o per la chiave dichiarata nello schema), mai per posizione: un riordino non cambia nulla, un
//   elemento tolto o cambiato e' un errore K-CTR-094, mai un valore sull'elemento sbagliato;
// - per chi non ha un permesso i campi di quella classe valgono come presenti ma non leggibili.
#include <algorithm>
#include <functional>
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
#include "lettore.hpp"

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
    return c;
}

ValoriCassaforte valori(const Chiuso& c, Visibilita leggibili) {
    ValoriCassaforte v;
    v.leggibili = leggibili;
    v.id = kId;
    v.generazione = 1;
    for (const Voce& x : c.voci)
        if (x.classe == Classe::Particolare ? leggibili.particolari : leggibili.personali) v.voci.push_back(&x);
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
