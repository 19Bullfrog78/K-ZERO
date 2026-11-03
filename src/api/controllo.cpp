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

Resoconto controlla_lettura(Lettura lettura, const Visibilita& visibilita, const kzero::HoursRequest* richiesta,
                            kzero::HoursReport* orari) {
    Resoconto r;
    r.lettura = std::move(lettura);
    std::vector<kzero::Problem> problemi = r.lettura.problemi;
    if (r.lettura.forma_valida()) {
        r.contenuto_controllato = true;
        std::vector<kzero::Problem> contenuto = kzero::check_package(r.lettura.pacchetto, richiesta, orari);
        problemi.insert(problemi.end(), contenuto.begin(), contenuto.end());
    }
    const Presentatore presentatore(r.contenuto_controllato ? &r.lettura.pacchetto : nullptr, &r.lettura.versioni, visibilita);
    const Visibilita& leggibili = r.lettura.leggibili;
    for (const kzero::Problem& p : problemi) {
        const DefinizioneControllo& d = definizione_controllo(p.check);
        const Classe posizione = classe_della_posizione(p.where, r.lettura);
        // Cassaforte (K-010): un problema che l'utente non puo' calcolare con le classi che apre non si mostra e non si conta
        // (sarebbe fatto su dati mancanti): il resoconto dice quali controlli non sono verificati.
        if (!verificabile(d, p.variant, posizione, in_trasportato(p.where, r.lettura), leggibili)) continue;
        (d.livello == Livello::Errore ? r.errori : r.avvisi) += 1;
        if (d.livello == Livello::Errore && d.piano) r.errori_del_piano += 1;
        // Il problema rivela il piu' riservato tra cio' che dice in se' e il dato in cui sta la sua posizione. Se e' un
        // dato particolare (salute), di base si conta soltanto: niente codice, messaggio, posizione.
        const Classe rivela = piu_riservata(d.rivela_variante(p.variant), posizione);
        if (rivela == Classe::Particolare && !visibile(Classe::Particolare, visibilita)) {
            (d.livello == Livello::Errore ? r.errori_riservati : r.avvisi_riservati) += 1;
            continue;
        }
        std::vector<std::pair<kzero::Day, kzero::Day>> giorni;
        std::vector<kzero::Day> tutti;
        std::string persona, corsa, servizio;
        // K-011 aggiunta 1: i problemi del modo delle corse non parlano di una persona ma di una corsa o di un servizio.
        const bool del_modo = p.check == kzero::Check::ModeForced || p.check == kzero::Check::ModeForcedRisk ||
                              p.check == kzero::Check::ModeDeclared || p.check == kzero::Check::ModeOutNotValid ||
                              p.check == kzero::Check::ModeMixed || p.check == kzero::Check::ModeForcedUseless ||
                              p.check == kzero::Check::ModeDeclaredUseless;
        for (const kzero::Param& x : p.params) {
            if (x.kind == kzero::Param::Kind::Days) {
                giorni = periodi_di_giorni(x.days);
                tutti = x.days;
            }
            if (x.kind == kzero::Param::Kind::Ref && x.element == kzero::Element::Person && x.name == "persona") persona = x.text;
            if (del_modo && x.kind == kzero::Param::Kind::Ref && x.element == kzero::Element::Trip && x.name == "corsa") corsa = x.text;
            if (del_modo && x.kind == kzero::Param::Kind::Ref && x.element == kzero::Element::Service && x.name == "servizio")
                servizio = x.text;
        }
        const PosizioneMostrata dove = mostra_posizione(p.where, r.lettura, visibilita);
        r.problemi.push_back({d.livello, d.piano, d.codice, presentatore.messaggio(p), dove.posizione, dove.ridotta, giorni, p.where, persona,
                              tutti, corsa, servizio});
    }
    std::stable_sort(r.problemi.begin(), r.problemi.end(), prima);
    for (const Classe c : {Classe::Personale, Classe::Particolare}) {
        if (c == Classe::Personale ? leggibili.personali : leggibili.particolari) continue;
        Resoconto::NonVerificati nv;
        nv.classe = c;
        for (const DefinizioneControllo& d : catalogo_controlli())
            if (c == Classe::Personale ? d.richiede_personali : d.richiede_particolari) nv.controlli.push_back(d.codice);
        r.non_verificati.push_back(nv);
    }
    return r;
}

}  // namespace kzero::api
