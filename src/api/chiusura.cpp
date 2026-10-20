// K-Zero API - chiudere un pacchetto nella cassaforte e riaprirlo (vedi chiusura.hpp).
#include "chiusura.hpp"

#include <stdexcept>

#include "formato_dati.hpp"
#include "pacchetto.hpp"
#include "riservatezza.hpp"

namespace kzero::api::chiusura {

Separazione separa(const Lettura& lettura) {
    if (!lettura.forma_valida()) throw std::logic_error("chiusura: pacchetto con errori di forma");
    const kzero::Index indice(lettura.pacchetto);
    const LegameSensibile legame(&indice);
    Separazione out;
    for (const auto& [file, documento] : lettura.documenti) {
        const FileDati* f = file_dati_per_nome(file);
        if (!f) throw std::logic_error("chiusura: file sconosciuto " + file);
        JsonDati normale = documento;
        const int versione = lettura.versioni.at(file);
        if (file != "pacchetto.json") {
            for (Voce& v : kzero::api::separa(file, normale, f->schema, versione, &legame))
                (v.classe == Classe::Particolare ? out.contenuto.particolari : out.contenuto.personali).push_back(std::move(v));
            const ValoriPerClasse n = conta_valori(documento, f->schema, versione);
            out.contenuto.valori.personali += n.personali;
            out.contenuto.valori.particolari += n.particolari;
        }
        out.normali.emplace(file, std::move(normale));
    }
    return out;
}

JsonDati manifesto_chiuso(const JsonDati& manifesto, const std::string& id, std::uint32_t generazione) {
    JsonDati m = manifesto;
    m["cassaforte"] = JsonDati{{"id", id}, {"generazione", generazione}};
    return m;
}

JsonDati manifesto_in_chiaro(const JsonDati& manifesto) {
    JsonDati m = manifesto;
    m["cassaforte"] = false;
    return m;
}

std::string testo_file(const JsonDati& documento) {
    return documento.dump(2, ' ', false, JsonDati::error_handler_t::strict) + "\n";
}

bool stessi_dati(const Lettura& a, const Lettura& b, std::string& differenza) {
    for (const FileDati& f : file_dati()) {
        const auto x = a.documenti.find(f.file);
        const auto y = b.documenti.find(f.file);
        if ((x == a.documenti.end()) != (y == b.documenti.end())) {
            differenza = f.file + " (presente in una sola delle due letture)";
            return false;
        }
        if (x == a.documenti.end()) continue;
        JsonDati u = x->second, v = y->second;
        if (f.file == "pacchetto.json") {
            u.erase("cassaforte");
            v.erase("cassaforte");
        }
        if (u != v) {
            differenza = f.file;
            return false;
        }
    }
    return true;
}

ValoriCassaforte valori_di(const cassaforte::Cassaforte& c) {
    ValoriCassaforte v;
    const cassaforte::Permessi p = c.permessi();
    v.leggibili = Visibilita{p.personali, p.particolari};
    const cassaforte::Riepilogo r = c.riepilogo();
    v.id = r.id;
    v.generazione = r.generazione;
    for (const Classe classe : {Classe::Personale, Classe::Particolare})
        for (const Voce& x : c.voci(classe)) v.voci.push_back(&x);
    return v;
}

}  // namespace kzero::api::chiusura
