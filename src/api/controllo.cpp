// K-Zero API - controllo completo di un pacchetto dati (vedi controllo.hpp).
#include "controllo.hpp"

#include <algorithm>
#include <tuple>

#include "formato_dati.hpp"
#include "riservatezza.hpp"
#include "validazione.hpp"

namespace kzero::api {

namespace {

// Posizione del file nell'ordine del formato; i file sconosciuti dopo, i problemi senza file in fondo.
std::size_t ordine_file(const std::string& file) {
    const auto& elenco = file_dati();
    if (file.empty()) return elenco.size() + 1;
    for (std::size_t i = 0; i < elenco.size(); ++i)
        if (elenco[i].file == file) return i;
    return elenco.size();
}

bool prima(const ProblemaPresentato& a, const ProblemaPresentato& b) {
    const auto chiave = [](const ProblemaPresentato& p) {
        return std::make_tuple(p.livello == Livello::Errore ? 0 : 1, ordine_file(p.posizione.file));
    };
    if (chiave(a) != chiave(b)) return chiave(a) < chiave(b);
    if (a.posizione.file != b.posizione.file) return a.posizione.file < b.posizione.file;
    if (a.posizione.pointer != b.posizione.pointer) return prima_nel_documento(a.posizione.pointer, b.posizione.pointer);
    if (std::tie(a.posizione.line, a.posizione.column, a.codice, a.messaggio) !=
        std::tie(b.posizione.line, b.posizione.column, b.codice, b.messaggio))
        return std::tie(a.posizione.line, a.posizione.column, a.codice, a.messaggio) <
               std::tie(b.posizione.line, b.posizione.column, b.codice, b.messaggio);
    // Due problemi che si mostrano uguali (posizione ridotta): l'ordine della posizione vera, per un resoconto stabile.
    if (a.completa.file != b.completa.file) return ordine_file(a.completa.file) < ordine_file(b.completa.file);
    return prima_nel_documento(a.completa.pointer, b.completa.pointer);
}

}  // namespace

std::optional<std::size_t> Resoconto::quanti(const std::string& file, std::size_t n) const {
    if (lettura.usato(file)) return n;
    // Il file c'e' ma non e' stato usato (errori di forma), oppure manca il manifesto: numero non noto.
    if (lettura.presenti.count(file) || file == "pacchetto.json") return std::nullopt;
    return 0;
}

Resoconto controlla_pacchetto(const Sorgente& sorgente, const Visibilita& visibilita) {
    return controlla_lettura(leggi_pacchetto(sorgente), visibilita);
}

Resoconto controlla_lettura(Lettura lettura, const Visibilita& visibilita) {
    Resoconto r;
    r.lettura = std::move(lettura);
    std::vector<kzero::Problem> problemi = r.lettura.problemi;
    if (r.lettura.forma_valida()) {
        r.contenuto_controllato = true;
        std::vector<kzero::Problem> contenuto = kzero::check_package(r.lettura.pacchetto);
        problemi.insert(problemi.end(), contenuto.begin(), contenuto.end());
    }
    const Presentatore presentatore(r.contenuto_controllato ? &r.lettura.pacchetto : nullptr, &r.lettura.versioni, visibilita);
    for (const kzero::Problem& p : problemi) {
        const DefinizioneControllo& d = definizione_controllo(p.check);
        (d.livello == Livello::Errore ? r.errori : r.avvisi) += 1;
        // Il problema rivela il piu' riservato tra cio' che dice in se' e il dato in cui sta la sua posizione. Se e' un
        // dato particolare (salute), di base si conta soltanto: niente codice, messaggio, posizione.
        const Classe rivela = piu_riservata(d.rivela_variante(p.variant), classe_della_posizione(p.where, r.lettura));
        if (rivela == Classe::Particolare && !visibile(Classe::Particolare, visibilita)) {
            (d.livello == Livello::Errore ? r.errori_riservati : r.avvisi_riservati) += 1;
            continue;
        }
        std::vector<std::pair<kzero::Day, kzero::Day>> giorni;
        for (const kzero::Param& x : p.params)
            if (x.kind == kzero::Param::Kind::Days) giorni = periodi_di_giorni(x.days);
        const PosizioneMostrata dove = mostra_posizione(p.where, r.lettura, visibilita);
        r.problemi.push_back({d.livello, d.codice, presentatore.messaggio(p), dove.posizione, dove.ridotta, giorni, p.where});
    }
    std::stable_sort(r.problemi.begin(), r.problemi.end(), prima);
    return r;
}

}  // namespace kzero::api
