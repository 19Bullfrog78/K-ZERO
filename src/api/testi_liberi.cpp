// K-Zero API - avviso sui nomi liberi (vedi testi_liberi.hpp e docs/controlli.md).
#include "testi_liberi.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <utility>

#include "classi.hpp"
#include "controlli_catalogo.hpp"
#include "formato_dati.hpp"
#include "pacchetto.hpp"
#include "ricerca.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

using kzero::Code;

bool cifra(char c) { return c >= '0' && c <= '9'; }

// Quante lettere ha una parola normalizzata: i caratteri UTF-8 che non sono cifre (le lettere di altri alfabeti restano com'erano).
std::size_t lettere(const std::string& parola) {
    std::size_t n = 0;
    for (const char c : parola)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80 && !cifra(c)) ++n;
    return n;
}

constexpr std::size_t kLettereMinime = 3;        // una parola sola del nome: almeno 3 lettere (mandato K-018)
constexpr std::size_t kParoleAttaccateMax = 8;   // il nome attaccato in una parola sola: fino a 8 parole (i nomi veri ne hanno meno)

// --- Elenchi delle parole sulla salute ----------------------------------------------------------------------------------------

struct VoceElenco {
    std::string scritta;               // come nell'elenco
    std::vector<std::string> parole;   // normalizzata
    std::string elenco;                // "delle norme N-IT", "dell'azienda ALFA"
};

struct Elenchi {
    std::vector<VoceElenco> voci;   // tutte, nell'ordine: norme (nell'ordine del file), poi aziende
    // Per azienda: le voci delle sue norme e le sue (indici in voci). Un testo che non e' di un'azienda: tutte.
    std::map<Code, std::vector<std::size_t>> per_azienda;
    std::vector<std::size_t> tutte;
};

Elenchi elenchi_di(const kzero::Package& p) {
    Elenchi e;
    std::map<Code, std::vector<std::size_t>> per_norme;
    const auto aggiungi = [&](const std::string& scritta, const std::string& elenco) -> std::optional<std::size_t> {
        std::vector<std::string> parole = parole_del_testo(scritta);
        if (parole.empty()) return std::nullopt;   // una voce senza lettere ne' cifre non trova niente
        e.voci.push_back({scritta, std::move(parole), elenco});
        e.tutte.push_back(e.voci.size() - 1);
        return e.voci.size() - 1;
    };
    for (const kzero::Norms& n : p.norms)
        for (const std::string& w : n.health_words)
            if (const auto i = aggiungi(w, "delle norme " + n.code)) per_norme[n.code].push_back(*i);
    for (const kzero::CompanySettings& s : p.company_settings) {
        auto& mie = e.per_azienda[s.company.code];
        if (s.norms) {
            const auto it = per_norme.find(s.norms->code);
            if (it != per_norme.end()) mie.insert(mie.end(), it->second.begin(), it->second.end());
        }
        for (const std::string& w : s.health_words)
            if (const auto i = aggiungi(w, "dell'azienda " + s.company.code)) mie.push_back(*i);
    }
    return e;
}

// Le voci dell'elenco che il testo contiene (parole intere, una dopo l'altra), senza ripetere la stessa voce normalizzata.
std::vector<std::size_t> voci_nel_testo(const std::vector<std::string>& testo, const Elenchi& e, const std::vector<std::size_t>& quali) {
    std::vector<std::size_t> trovate;
    std::set<std::vector<std::string>> viste;
    for (const std::size_t i : quali) {
        const VoceElenco& v = e.voci[i];
        if (v.parole.size() > testo.size() || viste.count(v.parole)) continue;
        for (std::size_t k = 0; k + v.parole.size() <= testo.size(); ++k)
            if (std::equal(v.parole.begin(), v.parole.end(), testo.begin() + static_cast<std::ptrdiff_t>(k))) {
                trovate.push_back(i);
                viste.insert(v.parole);
                break;
            }
    }
    return trovate;
}

std::string testo_parole(const std::vector<std::size_t>& trovate, const Elenchi& e) {
    std::string t;
    for (const std::size_t i : trovate)
        t += (t.empty() ? "" : "; ") + std::string("\"") + e.voci[i].scritta + "\" dell'elenco " + e.voci[i].elenco;
    return t;
}

// --- Di quale azienda e' un testo --------------------------------------------------------------------------------------------
// Le aziende dell'elemento dell'elenco principale del file, dai riferimenti dei dati (docs/controlli.md, tabella dei testi
// comuni); vuoto = non e' di un'azienda (es. un luogo pubblico), oppure il riferimento non si trova: allora tutti gli elenchi.
std::set<Code> aziende_del_testo(const std::string& file, const TestoComune& t, const kzero::Index& indice) {
    const kzero::Package& p = indice.package();
    std::set<Code> out;
    const auto di_contratto = [&](const Code& c) {
        if (const kzero::Contract* k = indice.contract(c)) out.insert(k->company.code);
    };
    const auto di_servizio = [&](const Code& c) {
        if (const kzero::Service* s = indice.service(c)) di_contratto(s->contract.code);
    };
    const auto di_mezzo = [&](const Code& c) {
        if (const kzero::Vehicle* v = indice.vehicle(c)) out.insert(v->company.code);
    };
    const auto di_turno = [&](const Code& c) {
        if (const kzero::Shift* s = indice.shift(c)) out.insert(s->company.code);
    };
    const auto i = t.indice;
    if (file == "pacchetto.json") {
        if (t.elenco == "aziende" && i >= 0 && static_cast<std::size_t>(i) < p.manifest.companies.size())
            out.insert(p.manifest.companies[static_cast<std::size_t>(i)].code);
    } else if (file == "aziende.json") {
        if (t.elenco == "aziende" && i >= 0 && static_cast<std::size_t>(i) < p.company_settings.size())
            out.insert(p.company_settings[static_cast<std::size_t>(i)].company.code);
    } else if (file == "contratti.json") {
        di_contratto(t.radice);
    } else if (file == "gruppi_servizi.json") {
        if (const kzero::ServiceLot* l = indice.lot(t.radice)) di_contratto(l->contract.code);
    } else if (file == "servizi.json") {
        di_servizio(t.radice);
    } else if (file == "corse.json") {
        if (const kzero::Trip* c = indice.trip(t.radice))
            for (const kzero::Ref& s : c->services) di_servizio(s.code);
    } else if (file == "mezzi.json") {
        di_mezzo(t.radice);
    } else if (file == "rimesse.json") {
        if (const kzero::Depot* d = indice.depot(t.radice)) out.insert(d->company.code);
    } else if (file == "persone.json") {
        if (const kzero::Person* x = indice.person(t.radice)) out.insert(x->company.code);
    } else if (file == "turni.json") {
        di_turno(t.radice);
    } else if (file == "rotazioni.json") {
        if (const kzero::Rotation* r = indice.rotation(t.radice))
            for (const kzero::RotationRow& riga : r->rows)
                if (const kzero::Person* x = indice.person(riga.person.code)) out.insert(x->company.code);
    } else if (file == "servizi_fissi.json") {
        for (const kzero::FixedAssignment& f : p.fixed)
            if (f.code == t.radice) di_turno(f.shift.code);
    } else if (file == "assegnazioni.json") {
        if (t.elenco == "assegnazioni" && i >= 0 && static_cast<std::size_t>(i) < p.day_assignments.size())
            di_turno(p.day_assignments[static_cast<std::size_t>(i)].shift.code);
    } else if (file == "fermi.json") {
        if (t.elenco == "fermi" && i >= 0 && static_cast<std::size_t>(i) < p.stops.size())
            di_mezzo(p.stops[static_cast<std::size_t>(i)].vehicle.code);
    } else if (file == "blocchi.json") {
        for (const kzero::Block& b : p.blocks)
            if (b.code == t.radice) di_mezzo(b.vehicle.code);
    }
    return out;
}

// --- Nomi delle persone e delle case -----------------------------------------------------------------------------------------

enum class Chi { Personale, Passeggero, Committente, Casa };

struct Nome {
    Chi chi = Chi::Personale;
    Code codice;
    bool legato = false;
    std::vector<std::string> parole;      // tutte le parole del nome
    std::set<std::string> parole_lunghe;   // quelle con almeno 3 lettere (la regola della parola sola)
};

std::string testo_persona(const Nome& n) {
    switch (n.chi) {
        case Chi::Personale: return "della persona del personale " + n.codice;
        case Chi::Passeggero: return "del passeggero " + n.codice;
        case Chi::Committente: return "del committente privato " + n.codice;
        case Chi::Casa: return "della casa " + n.codice;
    }
    return n.codice;
}

std::string chiave_cosa(const std::string& file, const std::string& campo_elemento, const std::string& codice) {
    return file + '\x1F' + campo_elemento + '\x1F' + codice;
}

struct Nomi {
    std::vector<Nome> nomi;
    // Nome intero, parole una dopo l'altra: le parole in ordine alfabetico -> nomi; e le lunghezze (quante parole).
    std::map<std::vector<std::string>, std::vector<std::size_t>> per_parole;
    std::set<std::size_t> lunghezze;
    // Nome intero attaccato in una parola sola: le lettere della parola in ordine -> nomi (poi si prova l'ordine delle parole).
    std::map<std::string, std::vector<std::size_t>> attaccati;
    // Le cose di ogni persona (file, elenco dell'elemento, codice) -> nomi: i testi dove basta una parola del nome.
    std::map<std::string, std::vector<std::size_t>> cose;
};

std::string lettere_in_ordine(std::string s) {
    std::sort(s.begin(), s.end());
    return s;
}

// La parola e' fatta dalle parole del nome attaccate, in un ordine qualsiasi?
bool attaccato(const std::string& parola, std::size_t da, const std::vector<std::string>& nome, std::vector<bool>& usate) {
    if (da == parola.size()) return std::all_of(usate.begin(), usate.end(), [](bool u) { return u; });
    for (std::size_t k = 0; k < nome.size(); ++k) {
        if (usate[k] || parola.compare(da, nome[k].size(), nome[k]) != 0) continue;
        usate[k] = true;
        const bool ok = attaccato(parola, da + nome[k].size(), nome, usate);
        usate[k] = false;
        if (ok) return true;
    }
    return false;
}

Nomi nomi_di(const kzero::Package& p, const LegameSensibile& legame) {
    Nomi out;
    const auto aggiungi = [&](Chi chi, const Code& codice, const std::string& nome, bool legato) -> std::optional<std::size_t> {
        Nome n;
        n.chi = chi;
        n.codice = codice;
        n.legato = legato;
        n.parole = parole_del_testo(nome);
        std::size_t tutte = 0;
        for (const std::string& w : n.parole) {
            tutte += lettere(w);
            if (lettere(w) >= kLettereMinime) n.parole_lunghe.insert(w);
        }
        if (n.parole.empty() || tutte < kLettereMinime) return std::nullopt;   // nome troppo corto per un confronto utile
        out.nomi.push_back(std::move(n));
        const std::size_t i = out.nomi.size() - 1;
        const Nome& x = out.nomi[i];
        std::vector<std::string> ordinate = x.parole;
        std::sort(ordinate.begin(), ordinate.end());
        out.per_parole[ordinate].push_back(i);
        out.lunghezze.insert(x.parole.size());
        if (x.parole.size() >= 2 && x.parole.size() <= kParoleAttaccateMax) {
            std::string tutto;
            for (const std::string& w : x.parole) tutto += w;
            out.attaccati[lettere_in_ordine(tutto)].push_back(i);
        }
        return i;
    };
    const auto cosa = [&](std::size_t nome, const std::string& file, const std::string& elenco, const Code& codice) {
        auto& v = out.cose[chiave_cosa(file, elenco, codice)];
        if (std::find(v.begin(), v.end(), nome) == v.end()) v.push_back(nome);
    };
    // Persone del personale: il suo codice, i suoi servizi fissi, i turni dei suoi servizi fissi e delle rotazioni in cui sta.
    for (const kzero::Person& x : p.persons) {
        if (x.name.empty()) continue;
        const auto i = aggiungi(Chi::Personale, x.code, x.name, false);
        if (!i) continue;
        cosa(*i, "persone.json", "persone[]", x.code);
        for (const kzero::FixedAssignment& f : p.fixed)
            if (f.person.code == x.code) {
                cosa(*i, "servizi_fissi.json", "servizi_fissi[]", f.code);
                cosa(*i, "turni.json", "turni[]", f.shift.code);
            }
        for (const kzero::Rotation& r : p.rotations) {
            const bool sta = std::any_of(r.rows.begin(), r.rows.end(), [&](const kzero::RotationRow& riga) { return riga.person.code == x.code; });
            if (!sta) continue;
            for (const kzero::RotationRow& riga : r.rows)
                for (const kzero::RotationStep& passo : riga.steps)
                    for (const kzero::Ref& t : passo.shifts) cosa(*i, "turni.json", "turni[]", t.code);
        }
    }
    // Passeggeri: il suo codice, il suo gruppo e il servizio del gruppo.
    for (const kzero::Service& s : p.services)
        for (const kzero::PassengerGroup& g : s.groups)
            for (const kzero::Passenger& q : g.passengers) {
                if (q.name.empty()) continue;
                const auto i = aggiungi(Chi::Passeggero, q.code, q.name, legame.passeggero(q.code));
                if (!i) continue;
                cosa(*i, "servizi.json", "servizi[].domanda.gruppi[].passeggeri[]", q.code);
                cosa(*i, "servizi.json", "servizi[].domanda.gruppi[]", g.code);
                cosa(*i, "servizi.json", "servizi[]", s.code);
            }
    // Committenti privati: il suo codice e i suoi codici esterni, i suoi contratti e i loro servizi (con i gruppi) e gruppi di servizi.
    for (const kzero::Client& c : p.clients) {
        if (!c.is_private || c.name.empty()) continue;
        const auto i = aggiungi(Chi::Committente, c.code, c.name, legame.committente(c.code));
        if (!i) continue;
        cosa(*i, "committenti.json", "committenti_privati[]", c.code);
        std::set<Code> contratti;
        for (const kzero::Contract& k : p.contracts)
            if (k.client.code == c.code) {
                contratti.insert(k.code);
                cosa(*i, "contratti.json", "contratti[]", k.code);
            }
        for (const kzero::Service& s : p.services)
            if (contratti.count(s.contract.code)) {
                cosa(*i, "servizi.json", "servizi[]", s.code);
                for (const kzero::PassengerGroup& g : s.groups) cosa(*i, "servizi.json", "servizi[].domanda.gruppi[]", g.code);
            }
        for (const kzero::ServiceLot& l : p.lots)
            if (contratti.count(l.contract.code)) cosa(*i, "gruppi_servizi.json", "gruppi_servizi[]", l.code);
    }
    // Case (luoghi privati): solo il nome intero.
    for (const kzero::Place& l : p.places)
        if (l.is_private && !l.name.empty()) aggiungi(Chi::Casa, l.code, l.name, legame.casa(l.code));
    return out;
}

// I nomi interi che il testo contiene: parole una dopo l'altra in qualsiasi ordine, oppure attaccate in una parola sola.
std::set<std::size_t> nomi_interi(const std::vector<std::string>& testo, const Nomi& n) {
    std::set<std::size_t> out;
    for (const std::size_t lung : n.lunghezze) {
        if (lung > testo.size()) break;
        for (std::size_t k = 0; k + lung <= testo.size(); ++k) {
            std::vector<std::string> finestra(testo.begin() + static_cast<std::ptrdiff_t>(k),
                                              testo.begin() + static_cast<std::ptrdiff_t>(k + lung));
            std::sort(finestra.begin(), finestra.end());
            const auto it = n.per_parole.find(finestra);
            if (it != n.per_parole.end()) out.insert(it->second.begin(), it->second.end());
        }
    }
    if (!n.attaccati.empty())
        for (const std::string& w : testo) {
            const auto it = n.attaccati.find(lettere_in_ordine(w));
            if (it == n.attaccati.end()) continue;
            for (const std::size_t i : it->second) {
                std::vector<bool> usate(n.nomi[i].parole.size(), false);
                if (attaccato(w, 0, n.nomi[i].parole, usate)) out.insert(i);
            }
        }
    return out;
}

// --- Falsi allarmi -----------------------------------------------------------------------------------------------------------

using ChiaveFalso = std::tuple<std::string, std::string, std::string, std::string, std::string>;   // controllo, file, campo, elemento, testo

std::string dove(const std::string& campo, const std::string& elemento) { return elemento.empty() ? campo : campo + " di " + elemento; }

kzero::Param personale(std::string nome, std::string valore, std::string nascosto) {
    kzero::Param x = kzero::text_param(std::move(nome), std::move(valore));
    x.sensitivity = kzero::Param::Sensitivity::Personal;
    x.hidden = std::move(nascosto);
    return x;
}

}  // namespace

std::vector<std::string> parole_del_testo(const std::string& testo) {
    std::vector<std::string> out;
    const std::string n = kzero::normalize_name(testo);
    std::string w;
    const auto chiudi = [&] {
        if (!w.empty()) out.push_back(w);
        w.clear();
    };
    for (std::size_t i = 0; i < n.size(); ++i) {
        const char c = n[i];
        if (c == ' ') {
            chiudi();
            continue;
        }
        if (!w.empty() && cifra(c) != cifra(w.back())) chiudi();
        w += c;
    }
    chiudi();
    return out;
}

std::vector<kzero::Problem> controlla_testi_liberi(const Lettura& lettura) {
    const kzero::Package& p = lettura.pacchetto;
    const kzero::Index indice(p);
    const LegameSensibile legame(&indice);
    const Elenchi elenchi = elenchi_di(p);
    const Nomi nomi = nomi_di(p, legame);
    const std::string cod_nome = definizione_controllo(kzero::Check::NameInCommonText).codice;
    const std::string cod_salute = definizione_controllo(kzero::Check::HealthWordInCommonText).codice;
    const std::string cod_luogo = definizione_controllo(kzero::Check::PlaceMaybeSensitive).codice;

    // I falsi allarmi scritti, e quali spengono almeno un avviso.
    std::map<ChiaveFalso, std::vector<std::size_t>> falsi;
    for (std::size_t i = 0; i < p.manifest.false_alarms.size(); ++i) {
        const kzero::FalseAlarm& f = p.manifest.false_alarms[i];
        falsi[{f.check, f.file, f.field, f.element, f.text}].push_back(i);
    }
    std::vector<bool> usato(p.manifest.false_alarms.size(), false);
    const auto spento = [&](const std::string& controllo, const std::string& file, const TestoComune& t) {
        const auto it = falsi.find({controllo, file, t.campo, t.elemento, t.testo});
        if (it == falsi.end()) return false;
        for (const std::size_t i : it->second) usato[i] = true;
        return true;
    };

    // Luoghi sensibili e scuole o centri che stanno li': i loro testi non si controllano per le parole sulla salute.
    std::set<Code> luoghi_sensibili, scuole_sensibili;
    for (const kzero::Place& l : p.places)
        if (!l.is_private && l.sensitive) luoghi_sensibili.insert(l.code);
    for (const kzero::School& s : p.schools)
        if (luoghi_sensibili.count(s.place.code)) scuole_sensibili.insert(s.code);
    const auto versione_luoghi = lettura.versioni.find("luoghi.json");
    const bool luoghi_v1 = versione_luoghi != lettura.versioni.end() && versione_luoghi->second < 2;

    std::vector<kzero::Problem> out;
    const auto problema = [&](kzero::Check c, int variante, const std::string& file, const std::string& puntatore,
                              std::vector<kzero::Param> param) {
        out.push_back(kzero::Problem{c, variante, kzero::Source{file, puntatore, 0, 0}, std::move(param)});
    };

    for (const FileDati& f : file_dati()) {
        const auto d = lettura.documenti.find(f.file);
        const auto v = lettura.versioni.find(f.file);
        if (d == lettura.documenti.end() || v == lettura.versioni.end()) continue;
        const bool pubbliche = regole_pubbliche(f.schema, v->second);
        for (const TestoComune& t : testi_comuni(d->second, f.schema, v->second)) {
            const std::vector<std::string> parole = parole_del_testo(t.testo);
            if (parole.empty()) continue;
            const std::string qui = dove(t.campo, t.elemento);

            // Nomi delle persone e delle case.
            const std::set<std::size_t> interi = nomi_interi(parole, nomi);
            std::set<std::size_t> per_parola;
            if (!t.elemento.empty()) {
                const auto it = nomi.cose.find(chiave_cosa(f.file, t.campo_elemento, t.elemento));
                if (it != nomi.cose.end())
                    for (const std::size_t i : it->second) {
                        if (interi.count(i)) continue;
                        for (const std::string& w : parole)
                            if (nomi.nomi[i].parole_lunghe.count(w)) {
                                per_parola.insert(i);
                                break;
                            }
                    }
            }
            if ((!interi.empty() || !per_parola.empty()) && !spento(cod_nome, f.file, t)) {
                for (const std::size_t i : interi) {
                    const Nome& n = nomi.nomi[i];
                    problema(kzero::Check::NameInCommonText, n.legato ? kNomeInteroLegato : kNomeIntero, f.file, t.puntatore,
                             {kzero::text_param("testo", t.testo), kzero::text_param("dove", qui),
                              personale("persona", testo_persona(n), "di una persona o di una casa dei dati (quale: dati personali, riservati)")});
                }
                for (const std::size_t i : per_parola) {
                    const Nome& n = nomi.nomi[i];
                    std::string trovata;
                    for (const std::string& w : parole)
                        if (n.parole_lunghe.count(w)) {
                            trovata = w;
                            break;
                        }
                    problema(kzero::Check::NameInCommonText, n.legato ? kNomeParolaLegato : kNomeParola, f.file, t.puntatore,
                             {kzero::text_param("testo", t.testo), kzero::text_param("dove", qui),
                              personale("persona", testo_persona(n), "di una persona o di una casa dei dati (quale: dati personali, riservati)"),
                              personale("parola", "(" + trovata + ")", "(riservata)")});
                }
            }

            // Parole sulla salute.
            if (pubbliche) continue;
            if (f.file == "luoghi.json" && t.elenco == "luoghi" && luoghi_sensibili.count(t.radice)) continue;
            if (f.file == "scuole.json" && scuole_sensibili.count(t.radice)) continue;
            const std::set<Code> aziende = aziende_del_testo(f.file, t, indice);
            std::vector<std::size_t> quali;
            if (aziende.empty()) {
                quali = elenchi.tutte;
            } else {
                for (const Code& a : aziende) {
                    const auto it = elenchi.per_azienda.find(a);
                    if (it != elenchi.per_azienda.end()) quali.insert(quali.end(), it->second.begin(), it->second.end());
                }
                std::sort(quali.begin(), quali.end());
                quali.erase(std::unique(quali.begin(), quali.end()), quali.end());
            }
            const std::vector<std::size_t> trovate = voci_nel_testo(parole, elenchi, quali);
            if (trovate.empty()) continue;
            const bool nome_del_luogo = f.file == "luoghi.json" && t.elenco == "luoghi" &&
                                        (t.campo == "luoghi[].nome" || t.campo == "luoghi[].nomi_alternativi[]");
            if (nome_del_luogo) {
                if (spento(cod_luogo, f.file, t)) continue;
                problema(kzero::Check::PlaceMaybeSensitive, luoghi_v1 ? kLuogoVersione1 : kLuogoNonSegnato, f.file, t.puntatore,
                         {kzero::ref_param("luogo", kzero::Element::Place, t.radice), kzero::text_param("testo", t.testo),
                          kzero::text_param("dove", qui), kzero::text_param("parole", testo_parole(trovate, elenchi))});
                continue;
            }
            if (spento(cod_salute, f.file, t)) continue;
            problema(kzero::Check::HealthWordInCommonText, kSaluteTesto, f.file, t.puntatore,
                     {kzero::text_param("testo", t.testo), kzero::text_param("dove", qui),
                      kzero::text_param("parole", testo_parole(trovate, elenchi))});
        }
    }

    // Falsi allarmi che non spengono piu' niente: la variante del loro controllo (con la sua riservatezza).
    for (std::size_t i = 0; i < p.manifest.false_alarms.size(); ++i) {
        if (usato[i]) continue;
        const kzero::FalseAlarm& f = p.manifest.false_alarms[i];
        std::vector<kzero::Param> param = {kzero::date_param("quando", f.when), kzero::text_param("testo", f.text),
                                           kzero::text_param("dove", dove(f.field, f.element)), kzero::text_param("file", f.file)};
        const kzero::Source& s = f.src;
        if (f.check == cod_nome)
            problema(kzero::Check::NameInCommonText, kNomeFalsoAllarme, s.file, s.pointer, std::move(param));
        else if (f.check == cod_salute)
            problema(kzero::Check::HealthWordInCommonText, kSaluteFalsoAllarme, s.file, s.pointer, std::move(param));
        else if (f.check == cod_luogo)
            problema(kzero::Check::PlaceMaybeSensitive, kLuogoFalsoAllarme, s.file, s.pointer, std::move(param));
    }
    return out;
}

}  // namespace kzero::api
