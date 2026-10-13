// K-Zero - calcoli della dimostrazione sui dati di un pacchetto (vedi dimostrazione.hpp).
#include "dimostrazione.hpp"

#include <algorithm>
#include <set>

#include "pacchetto.hpp"

namespace kzero {

namespace {

// Mezzi di un'azienda, nell'ordine del pacchetto (anche quelli ipotetici).
std::vector<const Vehicle*> mezzi_di(const Package& p, const Code& azienda) {
    std::vector<const Vehicle*> v;
    for (const Vehicle& m : p.vehicles)
        if (m.company.code == azienda) v.push_back(&m);
    return v;
}

// Gruppi trasportati da una corsa: quelli nominati nei passaggi, altrimenti quelli dei suoi servizi.
std::vector<const PassengerGroup*> gruppi_della_corsa(const Trip& t, const Index& idx) {
    std::set<Code> codici;
    for (const Passage& p : t.passages)
        for (const PassageCount& c : p.persons) codici.insert(c.group.code);
    std::vector<const PassengerGroup*> gruppi;
    for (const Code& c : codici)
        if (const PassengerGroup* g = idx.group(c)) gruppi.push_back(g);
    if (gruppi.empty())
        for (const Ref& r : t.services)
            if (const Service* s = idx.service(r.code))
                for (const PassengerGroup& g : s->groups) gruppi.push_back(&g);
    return gruppi;
}

}  // namespace

Dimostrazione calcola_dimostrazione(const Package& p) {
    const Index idx(p);
    Dimostrazione d;

    // Giri: servizi con la domanda in numeri esatti che non sta sul mezzo piu' grande dell'azienda in flotta.
    for (const Service& s : p.services) {
        const Contract* c = idx.contract(s.contract.code);
        if (!c || s.groups.empty()) continue;
        int persone = 0;
        bool esatti = true;
        for (const PassengerGroup& g : s.groups) {
            esatti = esatti && g.counts == CountKind::Exact;
            persone += g.total_boarding();
        }
        if (!esatti || persone == 0) continue;
        GiriServizio giri{s.code, persone, {}};
        int posti_massimi = 0;
        for (const Vehicle* v : mezzi_di(p, c->company.code)) {
            giri.mezzi.push_back({v->code, v->seats, trips_needed(*v, persone)});
            if (!v->hypothetical) posti_massimi = std::max(posti_massimi, v->seats);
        }
        if (persone > posti_massimi) d.giri.push_back(giri);
    }

    // Carrozzine: ogni mezzo con pedana, da nessuna carrozzina a una in piu' del massimo.
    for (const Vehicle& v : p.vehicles) {
        if (!v.has_ramp) continue;
        CarrozzineMezzo cm{v.code, {}};
        for (int w = 0; w <= v.wheelchair_places + 1; ++w) {
            CarrozzineMezzo::Caso caso;
            caso.carrozzine = w;
            caso.ammesso = w <= v.wheelchair_places;
            caso.posti_a_sedere = caso.ammesso ? v.seats - w * v.seats_lost_per_wheelchair : 0;
            cm.casi.push_back(caso);
        }
        d.carrozzine.push_back(cm);
    }

    // Ausili piegati: ogni mezzo con il bagagliaio o che li ammette tra i sedili, a mezzo vuoto da un ausilio
    // fino al primo caso non ammesso (al massimo due oltre il bagagliaio), poi un ausilio a mezzo pieno.
    for (const Vehicle& v : p.vehicles) {
        if (v.aids_in_trunk <= 0 && !v.seats_per_aid_between) continue;
        AusiliMezzo am{v.code, v.seats, v.aids_in_trunk, v.seats_per_aid_between, {}};
        const auto caso = [&v](int ausili, int seduti) {
            const Fit f = fits(v, {seduti, 0, ausili, false});
            AusiliMezzo::Caso c;
            c.ausili = ausili;
            c.seduti = seduti;
            c.ammesso = f.admitted;
            if (f.admitted) {
                c.nel_bagagliaio = f.aids_in_trunk;
                c.tra_i_sedili = f.aids_between_seats;
                c.posti_liberi = f.free_seats;
            }
            return c;
        };
        for (int a = 1; a <= v.aids_in_trunk + 2; ++a) {
            am.casi.push_back(caso(a, 0));
            if (!am.casi.back().ammesso) break;
        }
        am.casi.push_back(caso(1, v.seats));
        d.ausili_piegati.push_back(am);
    }

    // Assistente: corse in cui il regolamento chiede l'assistente per il grado delle scuole servite,
    // almeno con uno dei mezzi dell'azienda.
    for (const Trip& t : p.trips) {
        if (t.services.empty()) continue;
        const Service* s = idx.service(t.services.front().code);
        const Contract* c = s ? idx.contract(s->contract.code) : nullptr;
        const Regulation* r = c ? idx.regulation(c->regulation.code) : nullptr;
        if (!r || !r->assistant) continue;
        OnBoard a_bordo;
        for (const PassengerGroup* g : gruppi_della_corsa(t, idx)) {
            const School* sc = g->school ? idx.school(g->school->code) : nullptr;
            if (sc && sc->grade &&
                std::find(a_bordo.grades.begin(), a_bordo.grades.end(), *sc->grade) == a_bordo.grades.end())
                a_bordo.grades.push_back(*sc->grade);
        }
        if (a_bordo.grades.empty()) continue;
        // Solo le condizioni sul grado e sui posti: senza tipi di gruppo ed esigenze (dati particolari).
        ConditionsRule per_grado = *r->assistant;
        per_grado.conditions.clear();
        for (const PassengerCondition& cond : r->assistant->conditions)
            if (!cond.grades.empty() && cond.group_types.empty() && cond.needs.empty() && cond.service_kinds.empty() &&
                cond.center_types.empty())
                per_grado.conditions.push_back(cond);
        AssistenteCorsa ac{t.code, r->code, a_bordo.grades, {}};
        bool serve = false;
        for (const Vehicle* v : mezzi_di(p, c->company.code)) {
            ac.mezzi.push_back({v->code, v->seats, needs_assistant(&per_grado, a_bordo, v->seats)});
            serve = serve || ac.mezzi.back().serve;
        }
        if (serve) d.assistente.push_back(ac);
    }

    // Costi: servizi con un mezzo minimo chiesto dal committente (nel servizio o nel contratto).
    for (const Service& s : p.services) {
        const Contract* c = idx.contract(s.contract.code);
        if (!c) continue;
        const std::optional<int> minimo = s.rules.min_seats ? s.rules.min_seats : c->rules.min_seats;
        const std::optional<int> massimo = s.rules.max_seats ? s.rules.max_seats : c->rules.max_seats;
        if (!minimo) continue;
        CostiServizio cs;
        cs.servizio = s.code;
        cs.posti_minimi = *minimo;
        cs.posti_massimi = massimo;
        for (const Trip& t : p.trips)
            for (const Ref& r : t.services)
                if (r.code == s.code) cs.km_al_giorno += t.loaded_km() + t.empty_km();
        for (const Vehicle* v : mezzi_di(p, c->company.code)) {
            CostiServizio::PerMezzo m;
            m.mezzo = v->code;
            m.posti = v->seats;
            m.ipotetico = v->hypothetical;
            m.ammesso = v->seats >= *minimo && (!massimo || v->seats <= *massimo);
            m.costo = day_cost(*v, cs.km_al_giorno);
            cs.mezzi.push_back(m);
        }
        d.costi.push_back(cs);
    }
    return d;
}

}  // namespace kzero
