// K-Zero - schede dei servizi del committente (vedi schede.hpp).
#include "schede.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

#include "ricerca.hpp"
#include "turni.hpp"

namespace kzero {

namespace {

const char* kGiorni[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};

bool stessa_parola(const std::string& a, const std::string& b) { return normalize_name(a) == normalize_name(b); }
bool una_di(const std::string& t, const std::vector<std::string>& parole) {
    for (const std::string& p : parole)
        if (stessa_parola(t, p)) return true;
    return false;
}
// La parola (una o piu' parole intere) dentro il testo, confrontati come i nomi dei luoghi.
bool contiene_parola(const std::string& testo, const std::string& parola) {
    const std::string t = " " + normalize_name(testo) + " ", w = normalize_name(parola);
    return !w.empty() && t.find(" " + w + " ") != std::string::npos;
}

std::string pulito(const std::string& s) { return clean_cell(s); }

// Un testo UTF-8 valido (forme piu' corte, niente meta' di coppie, niente oltre U+10FFFF). Il lettore Excel da' sempre UTF-8
// valido; la regola deve reggere da sola (fuzzing del K-021): un testo che non lo e' rende la cella illeggibile.
bool utf8_valido(const std::string& t) {
    for (std::size_t i = 0; i < t.size();) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        std::size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (!n || i + n > t.size()) return false;
        char32_t cp = n == 1 ? c : n == 2 ? (c & 0x1F) : n == 3 ? (c & 0x0F) : (c & 0x07);
        for (std::size_t k = 1; k < n; ++k) {
            const unsigned char d = static_cast<unsigned char>(t[i + k]);
            if ((d & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (d & 0x3F);
        }
        if ((n == 2 && cp < 0x80) || (n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return false;
        i += n;
    }
    return true;
}
// Al massimo 300 byte: la misura dei testi del formato (Nr e ID si copiano nel registro).
constexpr std::size_t kTestoMassimo = 300;

bool solo_cifre(const std::string& t) {
    return !t.empty() && std::all_of(t.begin(), t.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// Un intero da 0 a 10.000 (scritto come testo o come numero di Excel, anche "12.0").
std::optional<int> intero(const std::string& testo) {
    std::string t = pulito(testo);
    if (t.size() > 2 && t.compare(t.size() - 2, 2, ".0") == 0) t.resize(t.size() - 2);
    if (!solo_cifre(t) || t.size() > 5) return std::nullopt;
    const int n = std::stoi(t);
    if (n > 10000) return std::nullopt;
    return n;
}

// Un numero con i decimali (virgola o punto), da 0 a 10.000.
std::optional<double> decimale(const std::string& testo) {
    const std::string t = pulito(testo);
    if (t.empty() || t.size() > 40) return std::nullopt;
    std::string intera, frazione;
    bool sep = false;
    for (char c : t) {
        if (c == ',' || c == '.') {
            if (sep) return std::nullopt;
            sep = true;
            continue;
        }
        if (c < '0' || c > '9') return std::nullopt;
        (sep ? frazione : intera) += c;
    }
    if (intera.empty() || intera.size() > 6 || (sep && frazione.empty())) return std::nullopt;
    double v = std::stod(intera);
    double peso = 0.1;
    for (char c : frazione) {
        v += (c - '0') * peso;
        peso /= 10;
    }
    if (v > 10000) return std::nullopt;
    return v;
}

// Una data: un numero di data di Excel (sistema 1900 o 1904) oppure "GG<sep>MM<sep>AAAA".
std::optional<Day> data(const std::string& testo, const std::string& sep, bool date1904) {
    std::string t = pulito(testo);
    if (t.size() > 2 && t.compare(t.size() - 2, 2, ".0") == 0) t.resize(t.size() - 2);
    std::optional<Day> d;
    if (solo_cifre(t)) {
        if (t.size() > 6) return std::nullopt;
        const int n = std::stoi(t);
        // Sistema 1900: il numero 60 e' il 29/02/1900, che non esiste (un errore storico di Excel): da 61 in poi il giorno 0 e'
        // il 30/12/1899. Sistema 1904: il giorno 0 e' l'1/1/1904.
        if (date1904) d = day_from_ymd(1904, 1, 1) + n;
        else if (n >= 61) d = day_from_ymd(1899, 12, 30) + n;
        else return std::nullopt;
    } else {
        const std::string s = pulito(sep);
        std::vector<std::string> parti;
        std::size_t a = 0;
        for (;;) {
            const std::size_t b = t.find(s, a);
            parti.push_back(pulito(t.substr(a, b == std::string::npos ? std::string::npos : b - a)));
            if (b == std::string::npos) break;
            a = b + s.size();
        }
        if (parti.size() != 3 || !solo_cifre(parti[0]) || !solo_cifre(parti[1]) || !solo_cifre(parti[2]) || parti[0].size() > 2 ||
            parti[1].size() > 2 || parti[2].size() != 4)
            return std::nullopt;
        const std::string iso = parti[2] + "-" + (parti[1].size() == 1 ? "0" : "") + parti[1] + "-" + (parti[0].size() == 1 ? "0" : "") + parti[0];
        d = day_from_text(iso);
    }
    if (!d) return std::nullopt;
    int y = 0, m = 0, g = 0;
    ymd_from_day(*d, y, m, g);
    if (y < 1900 || y > 2100) return std::nullopt;
    return d;
}

// Un orario: un numero di orario di Excel (frazione del giorno, fino a 47:59) oppure "H<sep>MM".
std::optional<Minutes> orario(const std::string& testo, const std::string& sep) {
    const std::string t = pulito(testo);
    if (t.empty() || t.size() > 40) return std::nullopt;
    const std::string s = pulito(sep);
    const std::size_t i = s.empty() ? std::string::npos : t.find(s);
    if (i != std::string::npos && s != ".") {
        const std::string h = pulito(t.substr(0, i)), m = pulito(t.substr(i + s.size()));
        if (!solo_cifre(h) || !solo_cifre(m) || h.size() > 2 || m.size() != 2) return std::nullopt;
        const int hh = std::stoi(h), mm = std::stoi(m);
        if (hh > 47 || mm > 59) return std::nullopt;
        return hh * 60 + mm;
    }
    // Numero di Excel: "0.3159722222", "1", "0".
    std::string intera, frazione;
    bool punto = false;
    for (char c : t) {
        if (c == '.') {
            if (punto) return std::nullopt;
            punto = true;
            continue;
        }
        if (c < '0' || c > '9') return std::nullopt;
        (punto ? frazione : intera) += c;
    }
    if (intera.empty() || intera.size() > 1 || (punto && frazione.empty())) return std::nullopt;
    double v = intera[0] - '0';
    double peso = 0.1;
    for (char c : frazione) {
        v += (c - '0') * peso;
        peso /= 10;
    }
    if (v >= 2.0) return std::nullopt;
    const long long minuti = std::llround(v * 1440.0);
    if (minuti > 47 * 60 + 59) return std::nullopt;
    return static_cast<Minutes>(minuti);
}

bool stessi_km(double a, double b) { return std::llround(a * 100) == std::llround(b * 100); }

// --- Le colonne di un foglio ------------------------------------------------------------------------------------------------

struct Colonne {
    std::map<std::string, int> per_titolo;   // titolo normalizzato -> colonna
    TableFault fault = TableFault::None;
    std::string titolo;
    int colonna(const std::string& t) const {
        const auto it = per_titolo.find(normalize_name(t));
        return it == per_titolo.end() ? 0 : it->second;
    }
};
Colonne colonne_di(const Table& t, const std::vector<std::string>& titoli) {
    Colonne c;
    if (t.rows.empty()) {
        c.fault = TableFault::NoRows;
        return c;
    }
    std::map<std::string, int> visti;
    for (const TableCell& x : t.rows.front().cells) {
        const std::string k = normalize_name(x.text);
        if (k.empty()) continue;
        ++visti[k];
        c.per_titolo.emplace(k, x.column);
    }
    for (const std::string& ti : titoli) {
        const std::string k = normalize_name(ti);
        if (!visti.count(k)) {
            c.fault = TableFault::MissingTitle;
            c.titolo = ti;
            return c;
        }
        if (visti.at(k) > 1) {
            c.fault = TableFault::DuplicateTitle;
            c.titolo = ti;
            return c;
        }
    }
    return c;
}

const TableCell* cella(const TableRow& r, int colonna) {
    for (const TableCell& c : r.cells)
        if (c.column == colonna) return &c;
    return nullptr;
}

// Legge le celle di una riga, fermandosi al primo motivo.
struct Riga {
    const TableRow& r;
    const Colonne& c;
    const std::string& foglio;
    CardRead* scheda;   // dove scrivere il motivo (nullptr: nessuna)
    bool fermo = false;

    bool ferma(CardFault f, const std::string& titolo) {
        if (fermo) return false;
        fermo = true;
        if (scheda && !scheda->fault) {
            scheda->fault = f;
            scheda->fault_sheet = foglio;
            scheda->fault_row = r.number;
            scheda->fault_column = titolo;
        }
        return false;
    }
    // Il testo della cella (vuoto se non c'e'); nullopt (e la riga si ferma) se la cella non si legge.
    std::optional<std::string> testo(const std::string& titolo) {
        if (fermo) return std::nullopt;
        const TableCell* x = cella(r, c.colonna(titolo));
        if (!x) return std::string();
        if (x->unreadable || !utf8_valido(x->text) || pulito(x->text).size() > kTestoMassimo) {
            ferma(CardFault::UnreadableCell, titolo);
            return std::nullopt;
        }
        return pulito(x->text);
    }
    std::optional<std::string> serve(const std::string& titolo) {
        const auto t = testo(titolo);
        if (t && t->empty()) {
            ferma(CardFault::MissingValue, titolo);
            return std::nullopt;
        }
        return t;
    }
};

// --- Ricerca di scuole, luoghi e calendari per codice esterno ----------------------------------------------------------------

struct Ricerche {
    std::map<std::string, std::vector<Code>> scuole, calendari;
    PlaceSearch luoghi;
    Ricerche(const Package& p, const CardProfile& pr) : luoghi(p.places) {
        for (const School& s : p.schools)
            for (const ExternalCode& e : s.external)
                if (PlaceSearch::external_key(e.system, e.type, e.code) ==
                    PlaceSearch::external_key(pr.ext_system, pr.ext_school, e.code)) {
                    auto& v = scuole[normalize_name(e.code)];
                    if (std::find(v.begin(), v.end(), s.code) == v.end()) v.push_back(s.code);
                }
        for (const Calendar& c : p.calendars)
            for (const ExternalCode& e : c.external)
                if (PlaceSearch::external_key(e.system, e.type, e.code) ==
                    PlaceSearch::external_key(pr.ext_system, pr.ext_calendar, e.code)) {
                    auto& v = calendari[normalize_name(e.code)];
                    if (std::find(v.begin(), v.end(), c.code) == v.end()) v.push_back(c.code);
                }
    }
    const std::vector<Code>* scuola(const std::string& t) const {
        const auto it = scuole.find(normalize_name(t));
        return it == scuole.end() ? nullptr : &it->second;
    }
    const std::vector<Code>* calendario(const std::string& t) const {
        const auto it = calendari.find(normalize_name(t));
        return it == calendari.end() ? nullptr : &it->second;
    }
};

bool codice_valido(const std::string& c) {
    if (c.empty() || c.size() > 64) return false;
    const auto ok = [](char x) { return (x >= 'A' && x <= 'Z') || (x >= 'a' && x <= 'z') || (x >= '0' && x <= '9'); };
    if (!ok(c[0])) return false;
    return std::all_of(c.begin(), c.end(), [&](char x) { return ok(x) || x == '.' || x == '_' || x == '-'; });
}

std::string chiave_id(const std::string& id) {
    std::string k = pulito(id);
    for (char& c : k)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return k;
}

}  // namespace

std::string card_nr_key(const std::string& nr, const CardProfile& p) {
    const std::string t = pulito(nr), s = pulito(p.nr_sep);
    if (s.empty()) return t;
    std::string out;
    std::size_t a = 0;
    for (;;) {
        const std::size_t b = t.find(s, a);
        out += pulito(t.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        out += s;
        a = b + s.size();
    }
    return out;
}

std::string card_service_number(const std::string& nr, const CardProfile& p) {
    const std::string k = card_nr_key(nr, p), s = pulito(p.nr_sep);
    const std::size_t i = s.empty() ? std::string::npos : k.find(s);
    return i == std::string::npos ? k : k.substr(0, i);
}

// --- Lettura ------------------------------------------------------------------------------------------------------------

CardsRead read_cards(const CardTables& tab, const CardProfile& pr, const Index& idx, bool date1904) {
    CardsRead out;
    const Package& pk = idx.package();
    const auto& cs = pr.card_cols;
    const auto& cf = pr.stop_cols;
    std::vector<std::string> titoli_schede = {cs.nr, cs.id, cs.service, cs.schools, cs.from, cs.to, cs.pupils, cs.individual,
                                              cs.equipped, cs.total_days, cs.total_loaded_km, cs.total_empty_km, cs.calendar,
                                              cs.cadence, cs.new_or_change};
    for (const CardProfileDay& g : pr.days) titoli_schede.push_back(g.column);
    const std::vector<std::string> titoli_fermate = {cf.nr, cf.time, cf.point, cf.home, cf.boarding, cf.alighting, cf.loaded_km,
                                                     cf.empty_km, cf.escort};
    const std::vector<std::string> titoli_date = {pr.date_cols.nr, pr.date_cols.date};
    const Colonne ks = colonne_di(tab.cards, titoli_schede);
    const Colonne kf = colonne_di(tab.stops, titoli_fermate);
    const Colonne kd = tab.dates.rows.empty() ? Colonne{} : colonne_di(tab.dates, titoli_date);
    for (const auto& [k, foglio] : {std::make_pair(&ks, pr.sheet_cards), std::make_pair(&kf, pr.sheet_stops),
                                    std::make_pair(&kd, pr.sheet_dates)})
        if (k->fault != TableFault::None) {
            out.fault = k->fault;
            out.fault_sheet = foglio;
            out.fault_title = k->titolo;
            return out;
        }
    out.card_rows = static_cast<int>(tab.cards.rows.size()) - 1;
    out.stop_rows = static_cast<int>(tab.stops.rows.size()) - 1;
    out.date_rows = tab.dates.rows.empty() ? 0 : static_cast<int>(tab.dates.rows.size()) - 1;
    const Ricerche cerca(pk, pr);

    // Le schede: prima i Nr e gli ID (doppi: ferme tutte).
    std::map<std::string, int> nr_visti, id_visti;
    for (std::size_t i = 1; i < tab.cards.rows.size(); ++i) {
        const TableRow& r = tab.cards.rows[i];
        CardRead x;
        x.row = r.number;
        const TableCell* n = cella(r, ks.colonna(cs.nr));
        const TableCell* d = cella(r, ks.colonna(cs.id));
        if (n && !n->unreadable && utf8_valido(n->text) && pulito(n->text).size() <= kTestoMassimo) x.nr = card_nr_key(n->text, pr);
        if (d && !d->unreadable && utf8_valido(d->text) && pulito(d->text).size() <= kTestoMassimo) x.id = pulito(d->text);
        if (!x.nr.empty()) ++nr_visti[x.nr];
        if (!x.id.empty()) ++id_visti[chiave_id(x.id)];
        out.cards.push_back(std::move(x));
    }
    std::map<std::string, CardRead*> per_nr;
    for (CardRead& x : out.cards)
        if (!x.nr.empty() && nr_visti[x.nr] == 1) per_nr[x.nr] = &x;

    // Le righe delle fermate e delle date, per scheda (le righe senza la loro scheda: K-CTR-161).
    std::map<std::string, std::vector<const TableRow*>> fermate, date;
    const auto per_scheda = [&](const Table& t, const Colonne& k, const std::string& titolo_nr, const std::string& foglio,
                                std::map<std::string, std::vector<const TableRow*>>& dove) {
        for (std::size_t i = 1; i < t.rows.size(); ++i) {
            const TableRow& r = t.rows[i];
            const TableCell* n = cella(r, k.colonna(titolo_nr));
            const std::string nr = n && !n->unreadable && utf8_valido(n->text) ? card_nr_key(n->text, pr) : std::string();
            if (!nr.empty() && nr_visti.count(nr)) {
                dove[nr].push_back(&r);
                continue;
            }
            out.problems.push_back(Problem{Check::CardRowWithoutCard, 0, Source{},
                                           {text_param("foglio", foglio), number_param("riga", r.number)}});
            out.problems.back().where.line = r.number;
        }
    };
    per_scheda(tab.stops, kf, cf.nr, pr.sheet_stops, fermate);
    if (!tab.dates.rows.empty()) per_scheda(tab.dates, kd, pr.date_cols.nr, pr.sheet_dates, date);

    for (std::size_t i = 0; i < out.cards.size(); ++i) {
        CardRead& x = out.cards[i];
        const TableRow& r = tab.cards.rows[i + 1];
        Riga q{r, ks, pr.sheet_cards, &x};
        Card& k = x.card;
        // Intestazione e piede, nell'ordine dei campi.
        if (!q.serve(cs.nr)) continue;
        k.nr = x.nr;
        if (nr_visti[x.nr] > 1) {
            q.ferma(CardFault::DuplicateNr, cs.nr);
            continue;
        }
        if (!q.serve(cs.id)) continue;
        if (id_visti[chiave_id(x.id)] > 1) {
            q.ferma(CardFault::DuplicateId, cs.id);
            continue;
        }
        const auto servizio = q.serve(cs.service);
        if (!servizio) continue;
        const CardProfileService* sv = nullptr;
        for (const CardProfileService& s : pr.services)
            if (stessa_parola(s.text, *servizio)) sv = &s;
        if (!sv) {
            q.ferma(CardFault::ServiceText, cs.service);
            continue;
        }
        k.service_text = sv->text;   // la parola del profilo (il testo del file e' uguale a meno di maiuscole e punteggiatura)
        k.direction = sv->direction;
        const auto scuole = q.serve(cs.schools);
        if (!scuole) continue;
        {
            const std::string s = pulito(pr.schools_sep);
            std::size_t a = 0;
            for (;;) {
                const std::size_t b = s.empty() ? std::string::npos : scuole->find(s, a);
                const std::string t = pulito(scuole->substr(a, b == std::string::npos ? std::string::npos : b - a));
                if (!t.empty()) {
                    const std::vector<Code>* v = cerca.scuola(t);
                    if (!v) {
                        q.ferma(CardFault::SchoolUnknown, cs.schools);
                        break;
                    }
                    if (v->size() > 1) {
                        q.ferma(CardFault::SchoolAmbiguous, cs.schools);
                        break;
                    }
                    if (std::find(k.schools.begin(), k.schools.end(), v->front()) == k.schools.end()) k.schools.push_back(v->front());
                }
                if (b == std::string::npos) break;
                a = b + s.size();
            }
            if (q.fermo) continue;
            if (k.schools.empty()) {
                q.ferma(CardFault::MissingValue, cs.schools);
                continue;
            }
        }
        const auto dal = q.serve(cs.from);
        if (!dal) continue;
        const auto d1 = data(*dal, pr.date_sep, date1904);
        if (!d1) {
            q.ferma(CardFault::BadDate, cs.from);
            continue;
        }
        const auto al = q.serve(cs.to);
        if (!al) continue;
        const auto d2 = data(*al, pr.date_sep, date1904);
        if (!d2) {
            q.ferma(CardFault::BadDate, cs.to);
            continue;
        }
        if (*d2 < *d1) {
            q.ferma(CardFault::DatesReversed, cs.to);
            continue;
        }
        k.from = *d1;
        k.to = *d2;
        // Le caselle: vuote o con un segno del profilo.
        const auto casella = [&](const std::string& titolo) -> std::optional<bool> {
            const auto t = q.testo(titolo);
            if (!t) return std::nullopt;
            if (t->empty()) return false;
            if (una_di(*t, pr.mark_words)) return true;
            q.ferma(CardFault::BadMark, titolo);
            return std::nullopt;
        };
        for (const CardProfileDay& g : pr.days) {
            const auto v = casella(g.column);
            if (!v) break;
            if (*v) k.days.add(g.day);
        }
        if (q.fermo) continue;
        const auto totali = q.serve(cs.pupils);
        if (!totali) continue;
        const auto n_tot = intero(*totali);
        if (!n_tot) {
            q.ferma(CardFault::BadNumber, cs.pupils);
            continue;
        }
        k.pupils = *n_tot;
        k.individual = casella(cs.individual);
        if (q.fermo) continue;
        k.equipped = casella(cs.equipped);
        if (q.fermo) continue;
        const auto tg = q.serve(cs.total_days);
        if (!tg) continue;
        const auto n_tg = intero(*tg);
        if (!n_tg) {
            q.ferma(CardFault::BadNumber, cs.total_days);
            continue;
        }
        k.total_days = *n_tg;
        const auto km_tot = [&](const std::string& titolo, double& dove) {
            const auto t = q.serve(titolo);
            if (!t) return false;
            const auto v = decimale(*t);
            if (!v) return q.ferma(CardFault::BadNumber, titolo);
            dove = *v;
            return true;
        };
        if (!km_tot(cs.total_loaded_km, k.total_loaded_km) || !km_tot(cs.total_empty_km, k.total_empty_km)) continue;
        const auto cal = q.serve(cs.calendar);
        if (!cal) continue;
        const std::vector<Code>* cv = cerca.calendario(*cal);
        if (!cv) {
            q.ferma(CardFault::CalendarUnknown, cs.calendar);
            continue;
        }
        if (cv->size() > 1) {
            q.ferma(CardFault::CalendarAmbiguous, cs.calendar);
            continue;
        }
        k.calendar_word = normalize_name(*cal);   // come il codice esterno del calendario, confrontato sempre normalizzato
        k.base_calendar = cv->front();
        const auto cad = q.serve(cs.cadence);
        if (!cad) continue;
        const CardProfileCadence* cd = nullptr;
        for (const CardProfileCadence& c : pr.cadences)
            if (stessa_parola(c.word, *cad)) cd = &c;
        if (!cd) {
            q.ferma(CardFault::CadenceUnknown, cs.cadence);   // mai trattata come un'altra (es. "Sempre")
            continue;
        }
        k.cadence_word = cd->word;
        k.cadence_form = cd->form;
        const auto nm = q.serve(cs.new_or_change);
        if (!nm) continue;
        if (una_di(*nm, pr.change_words)) k.is_change = true;
        else if (!una_di(*nm, pr.new_words)) {
            q.ferma(CardFault::NewOrChangeUnknown, cs.new_or_change);
            continue;
        }
        if (k.cadence_form == CadenceForm::MarkedDays && k.days.empty()) {
            q.ferma(CardFault::NoDays, pr.days.empty() ? cs.cadence : pr.days.front().column);
            continue;
        }

        // Le date (cadenza a date elencate).
        std::set<Day> giorni_elencati;
        for (const TableRow* rd : date[x.nr]) {
            Riga z{*rd, kd, pr.sheet_dates, &x};
            const auto t = z.serve(pr.date_cols.date);
            if (!t) break;
            const auto d = data(*t, pr.date_sep, date1904);
            if (!d) {
                z.ferma(CardFault::BadDate, pr.date_cols.date);
                break;
            }
            if (*d < k.from || *d > k.to) {
                z.ferma(CardFault::DateOutsidePeriod, pr.date_cols.date);
                break;
            }
            giorni_elencati.insert(*d);
        }
        if (x.fault) continue;
        if (k.cadence_form == CadenceForm::ListedDates) {
            if (giorni_elencati.empty()) {
                q.ferma(CardFault::NoListedDates, cs.cadence);
                continue;
            }
            k.dates.assign(giorni_elencati.begin(), giorni_elencati.end());
        }

        // Le fermate, nell'ordine.
        for (const TableRow* rf : fermate[x.nr]) {
            Riga z{*rf, kf, pr.sheet_stops, &x};
            CardStop s;
            s.row = rf->number;
            const auto ora = z.serve(cf.time);
            if (!ora) break;
            const auto m = orario(*ora, pr.time_sep);
            if (!m) {
                z.ferma(CardFault::BadTime, cf.time);
                break;
            }
            s.time = *m;
            const auto punto = z.testo(cf.point);
            if (!punto) break;
            const auto casa = z.testo(cf.home);
            if (!casa) break;
            if (punto->empty() == casa->empty()) {
                z.ferma(CardFault::PointAndHome, punto->empty() ? cf.point : cf.home);
                break;
            }
            if (!punto->empty()) {
                if (contiene_parola(*punto, pr.escort_word)) {
                    z.ferma(CardFault::EscortWordInPoint, cf.point);
                    break;
                }
                // Una scuola (codici esterni delle scuole), poi i codici esterni dei luoghi pubblici, poi il nome.
                if (const std::vector<Code>* v = cerca.scuola(*punto)) {
                    if (v->size() > 1) {
                        z.ferma(CardFault::PointAmbiguous, cf.point);
                        break;
                    }
                    if (std::find(k.schools.begin(), k.schools.end(), v->front()) == k.schools.end()) {
                        z.ferma(CardFault::SchoolRowNotInHeader, cf.point);
                        break;
                    }
                    const School* sc = idx.school(v->front());
                    s.place = sc->place.code;
                    s.match = CardStopMatch::School;
                } else {
                    PlaceSearch::Result t = cerca.luoghi.by_external_code(pr.ext_system, pr.ext_stop, *punto);
                    s.match = CardStopMatch::ExternalCode;
                    if (t.outcome == PlaceSearch::Outcome::Unknown) {
                        t = cerca.luoghi.by_name(*punto);
                        s.match = CardStopMatch::Name;
                    }
                    if (t.outcome == PlaceSearch::Outcome::Unknown) {
                        z.ferma(CardFault::PointUnknown, cf.point);
                        break;
                    }
                    if (t.outcome == PlaceSearch::Outcome::Ambiguous) {
                        z.ferma(CardFault::PointAmbiguous, cf.point);
                        break;
                    }
                    s.place = t.codes.front();
                }
            } else {
                // La casa: un codice di luogo privato porta a porta, mai un indirizzo; se non c'e' si crea senza dati personali.
                if (pk.manifest.level == Level::NumbersOnly) {
                    z.ferma(CardFault::HomeInNumbersOnly, cf.home);
                    break;
                }
                if (!codice_valido(*casa)) {
                    z.ferma(CardFault::HomeNotDoorToDoor, cf.home);
                    break;
                }
                const Place* l = idx.place(*casa);
                if (l && (!l->is_private || l->type != PlaceType::DoorToDoor)) {
                    z.ferma(CardFault::HomeNotDoorToDoor, cf.home);
                    break;
                }
                if (!l && std::find(x.new_homes.begin(), x.new_homes.end(), *casa) == x.new_homes.end()) x.new_homes.push_back(*casa);
                s.place = *casa;
                s.match = CardStopMatch::Home;
            }
            const auto conto = [&](const std::string& titolo, int& dove) {
                const auto t = z.testo(titolo);
                if (!t) return false;
                if (t->empty()) return true;   // cella vuota = 0 (righe con la sola salita)
                const auto n = intero(*t);
                if (!n) return z.ferma(CardFault::BadNumber, titolo);
                dove = *n;
                return true;
            };
            if (!conto(cf.boarding, s.boarding) || !conto(cf.alighting, s.alighting)) break;
            const auto km = [&](const std::string& titolo, std::optional<double>& dove) {
                const auto t = z.testo(titolo);
                if (!t) return false;
                if (t->empty()) return true;   // non scritto, mai 0
                const auto v = decimale(*t);
                if (!v) return z.ferma(CardFault::BadNumber, titolo);
                dove = *v;
                return true;
            };
            if (!km(cf.loaded_km, s.loaded_km) || !km(cf.empty_km, s.empty_km)) break;
            const auto acc = z.testo(cf.escort);
            if (!acc) break;
            if (!acc->empty()) {
                if (una_di(*acc, pr.escort_boards)) s.escort = AssistantMove::Boards;
                else if (una_di(*acc, pr.escort_alights)) s.escort = AssistantMove::Alights;
                else {
                    z.ferma(CardFault::EscortWordUnknown, cf.escort);
                    break;
                }
            }
            k.stops.push_back(std::move(s));
        }
        if (x.fault) continue;

        // I controlli sulla scheda intera.
        const auto ferma_su = [&](CardFault f, const std::string& foglio, int riga, const std::string& colonna) {
            x.fault = f;
            x.fault_sheet = foglio;
            x.fault_row = riga;
            x.fault_column = colonna;
        };
        if (k.stops.size() < 2) {
            ferma_su(CardFault::TooFewStops, pr.sheet_cards, x.row, cs.nr);
            continue;
        }
        for (const Code& sc : k.schools) {
            const School* s = idx.school(sc);
            const bool c_e = std::any_of(k.stops.begin(), k.stops.end(), [&](const CardStop& f) {
                return f.match == CardStopMatch::School && s && f.place == s->place.code;
            });
            if (!c_e && !x.fault) ferma_su(CardFault::SchoolRowMissing, pr.sheet_cards, x.row, cs.schools);
        }
        if (x.fault) continue;
        int a_bordo = 0;
        bool accompagnatore = false;
        int salgono = 0, scendono = 0;
        double km_pieni = 0, km_vuoti = 0;
        for (std::size_t j = 0; j < k.stops.size() && !x.fault; ++j) {
            const CardStop& s = k.stops[j];
            if (j > 0 && s.time < k.stops[j - 1].time) {
                ferma_su(CardFault::TimeBackwards, pr.sheet_stops, s.row, cf.time);
                break;
            }
            // Due righe di fila nello stesso luogo alla stessa ora sono un passaggio solo (C5): la seconda non ha km.
            const bool stesso = j > 0 && s.place == k.stops[j - 1].place && s.time == k.stops[j - 1].time;
            if (stesso && (s.loaded_km || s.empty_km)) {
                ferma_su(CardFault::SameStopKm, pr.sheet_stops, s.row, s.loaded_km ? cf.loaded_km : cf.empty_km);
                break;
            }
            if (stesso && s.escort != AssistantMove::None && k.stops[j - 1].escort != AssistantMove::None) {
                ferma_su(CardFault::EscortSamePassage, pr.sheet_stops, s.row, cf.escort);
                break;
            }
            if (s.escort == AssistantMove::Boards && s.boarding == 0) {
                ferma_su(CardFault::EscortZero, pr.sheet_stops, s.row, cf.boarding);
                break;
            }
            if (s.escort == AssistantMove::Alights && s.alighting == 0) {
                ferma_su(CardFault::EscortZero, pr.sheet_stops, s.row, cf.alighting);
                break;
            }
            if ((s.escort == AssistantMove::Boards && accompagnatore) || (s.escort == AssistantMove::Alights && !accompagnatore)) {
                ferma_su(CardFault::EscortOrder, pr.sheet_stops, s.row, cf.escort);
                break;
            }
            if (s.escort == AssistantMove::Boards) accompagnatore = true;
            if (s.escort == AssistantMove::Alights) accompagnatore = false;
            a_bordo += s.boarding;
            if (s.alighting > a_bordo) {
                ferma_su(CardFault::MoreAlightingThanOnBoard, pr.sheet_stops, s.row, cf.alighting);
                break;
            }
            a_bordo -= s.alighting;
            salgono += s.boarding;
            scendono += s.alighting;
            km_pieni += s.loaded_km.value_or(0.0);
            km_vuoti += s.empty_km.value_or(0.0);
        }
        if (x.fault) continue;
        if (accompagnatore) ferma_su(CardFault::EscortStillOnBoard, pr.sheet_stops, k.stops.back().row, cf.escort);
        else if (salgono != k.pupils) ferma_su(CardFault::TotalsBoarding, pr.sheet_cards, x.row, cs.pupils);
        else if (scendono != k.pupils) ferma_su(CardFault::TotalsAlighting, pr.sheet_cards, x.row, cs.pupils);
        else if (!stessi_km(km_pieni, k.total_loaded_km)) ferma_su(CardFault::TotalLoadedKm, pr.sheet_cards, x.row, cs.total_loaded_km);
        else if (!stessi_km(km_vuoti, k.total_empty_km)) ferma_su(CardFault::TotalEmptyKm, pr.sheet_cards, x.row, cs.total_empty_km);
    }
    return out;
}

// --- Calendario e giorni ---------------------------------------------------------------------------------------------------

Calendar card_calendar(const Card& c, const Code& code, DateRange valid) {
    Calendar k;
    k.code = code;
    k.base = Ref{c.base_calendar, {}};
    k.range = valid;
    if (c.cadence_form == CadenceForm::MarkedDays) k.days = c.days;
    else
        for (Day d : c.dates)
            if (d >= valid.from && d <= valid.to) k.added.push_back({d, {}});
    return k;
}

int card_days(const Card& c, const Index& idx) {
    const Calendar k = card_calendar(c, "", {c.from, c.to});
    int n = 0;
    for (Day d = c.from; d <= c.to; ++d) n += calendar_active(k, d, idx) ? 1 : 0;
    return n;
}

// --- Differenze ------------------------------------------------------------------------------------------------------------

namespace {

std::string km_testo(const std::optional<double>& v) {
    if (!v) return "-";
    const long long c = std::llround(*v * 100);
    std::string t = std::to_string(c / 100);
    const long long f = c % 100;
    if (f) t += "," + std::string(f < 10 ? "0" : "") + std::to_string(f);
    if (t.size() > 3 && t.compare(t.size() - 1, 1, "0") == 0 && t.find(',') != std::string::npos) t.pop_back();
    return t;
}
std::string giorni_testo(const Weekdays& w) {
    std::string t;
    for (int i = 0; i < 7; ++i)
        if (w.has(static_cast<Weekday>(i))) t += (t.empty() ? "" : ", ") + std::string(kGiorni[i]);
    return t.empty() ? "nessuno" : t;
}
std::string lista(const std::vector<std::string>& v) {
    std::string t;
    for (const std::string& x : v) t += (t.empty() ? "" : ", ") + x;
    return t.empty() ? "-" : t;
}

}  // namespace

std::vector<CardChange> card_changes(const Card& a, const Card& b) {
    std::vector<CardChange> out;
    const auto cambia = [&](const std::string& cosa, const std::string& x, const std::string& y, bool part = false) {
        if (x != y) out.push_back({cosa, x, y, part});
    };
    cambia("nr", a.nr, b.nr);
    // Le parole del file si confrontano come le parole del profilo (senza maiuscole, accenti, punteggiatura).
    const auto parola = [&](const std::string& cosa, const std::string& x, const std::string& y) {
        if (normalize_name(x) != normalize_name(y)) out.push_back({cosa, x, y, false});
    };
    parola("servizio", a.service_text, b.service_text);
    cambia("scuole", lista(a.schools), lista(b.schools));
    std::vector<std::string> la, lb;
    for (const CardStop& s : a.stops) la.push_back(s.place);
    for (const CardStop& s : b.stops) lb.push_back(s.place);
    if (la != lb) {
        cambia("fermate", lista(la), lista(lb));
    } else {
        std::vector<std::string> oa, ob, na, nb, ka, kb;
        for (std::size_t i = 0; i < a.stops.size(); ++i) {
            const CardStop &x = a.stops[i], &y = b.stops[i];
            if (x.time != y.time) {
                oa.push_back(x.place + " " + text_from_minutes(x.time));
                ob.push_back(y.place + " " + text_from_minutes(y.time));
            }
            if (x.boarding != y.boarding || x.alighting != y.alighting || x.escort != y.escort) {
                const auto n = [](const CardStop& s) {
                    return s.place + " +" + std::to_string(s.boarding) + " -" + std::to_string(s.alighting) +
                           (s.escort == AssistantMove::Boards ? " (sale l'accompagnatore)" : s.escort == AssistantMove::Alights ? " (scende l'accompagnatore)" : "");
                };
                na.push_back(n(x));
                nb.push_back(n(y));
            }
            if (km_testo(x.loaded_km) != km_testo(y.loaded_km) || km_testo(x.empty_km) != km_testo(y.empty_km)) {
                ka.push_back(x.place + " " + km_testo(x.loaded_km) + "/" + km_testo(x.empty_km));
                kb.push_back(y.place + " " + km_testo(y.loaded_km) + "/" + km_testo(y.empty_km));
            }
        }
        cambia("orari", lista(oa), lista(ob));
        cambia("numeri", lista(na), lista(nb));
        cambia("km", lista(ka), lista(kb));
    }
    cambia("totali", std::to_string(a.pupils) + " alunni, " + km_testo(a.total_loaded_km) + " km carico, " + km_testo(a.total_empty_km) +
                         " km vuoto, " + std::to_string(a.total_days) + " giorni",
           std::to_string(b.pupils) + " alunni, " + km_testo(b.total_loaded_km) + " km carico, " + km_testo(b.total_empty_km) +
               " km vuoto, " + std::to_string(b.total_days) + " giorni");
    cambia("date", text_from_day(a.from) + " - " + text_from_day(a.to), text_from_day(b.from) + " - " + text_from_day(b.to));
    cambia("giorni", giorni_testo(a.days), giorni_testo(b.days));
    std::vector<std::string> da, db;
    for (Day d : a.dates) da.push_back(text_from_day(d));
    for (Day d : b.dates) db.push_back(text_from_day(d));
    cambia("date_elencate", lista(da), lista(db));
    if (normalize_name(a.calendar_word) != normalize_name(b.calendar_word) || a.base_calendar != b.base_calendar)
        out.push_back({"calendario", a.calendar_word + " (" + a.base_calendar + ")", b.calendar_word + " (" + b.base_calendar + ")", false});
    parola("cadenza", a.cadence_word, b.cadence_word);
    const auto sn = [](const std::optional<bool>& v) { return !v ? std::string("non noto") : *v ? std::string("si'") : std::string("no"); };
    cambia("individualizzato", sn(a.individual), sn(b.individual), true);
    cambia("attrezzato", sn(a.equipped), sn(b.equipped), true);
    return out;
}

bool same_card(const Card& a, const Card& b) { return card_changes(a, b).empty(); }

// --- Versioni ----------------------------------------------------------------------------------------------------------------

namespace {

// Il Nr come pezzo di un codice: i caratteri non ammessi diventano "-", senza "-" doppi o ai bordi.
std::string nr_in_codice(const std::string& nr) {
    std::string out;
    for (char c : nr) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
        if (ok) out += c;
        else if (!out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.size() > 40) out.resize(40);
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "X" : out;
}

// Un codice libero: la base, oppure la base con -2, -3... (mai un codice gia' usato).
Code libero(const std::string& base, std::set<Code>& usati) {
    Code c = base;
    for (int k = 2; usati.count(c); ++k) c = base + "-" + std::to_string(k);
    usati.insert(c);
    return c;
}

}  // namespace

CardsMerge merge_cards(const Package& p, const CardProfile& pr, const Code& contract, Day taken, const std::string& reference,
                       const DateChoices& choices, const CardsRead& read) {
    CardsMerge out;
    out.cards = p.service_cards;
    const Index idx(p);
    std::set<Code> usati = all_codes(p, true);
    for (const ServiceCard& s : p.service_cards)
        for (const CardVersion& v : s.versions)
            for (const Code& c : {v.service, v.group, v.trip, v.calendar}) usati.insert(c);
    const Contract* ct = idx.contract(contract);
    const std::string committente = ct ? ct->client.code : pr.client.code;
    std::set<Code> case_messe;
    const auto problema = [&](Check c, int variante, std::vector<Param> par, int riga) {
        Problem x{c, variante, Source{}, std::move(par)};
        x.where.line = riga;
        out.problems.push_back(std::move(x));
    };
    for (const CardRead& r : read.cards) {
        CardOutcome o;
        o.row = r.row;
        o.nr = r.nr;
        o.id = r.id;
        const auto ferma = [&](CardFault f, const std::string& foglio, int riga, const std::string& colonna) {
            o.kind = CardOutcome::Kind::Stopped;
            o.fault = f;
            problema(Check::CardUnreadable, static_cast<int>(f),
                     {text_param("scheda", r.nr.empty() ? std::string("senza Nr") : r.nr), text_param("foglio", foglio),
                      number_param("riga", riga), text_param("colonna", colonna)},
                     riga);
        };
        if (r.fault) {
            ferma(*r.fault, r.fault_sheet, r.fault_row, r.fault_column);
            out.outcomes.push_back(o);
            continue;
        }
        const Card& k = r.card;
        // La scheda nel registro, dall'ID (mai dal Nr).
        ServiceCard* esistente = nullptr;
        for (ServiceCard& s : out.cards)
            if (chiave_id(s.id) == chiave_id(r.id)) esistente = &s;
        if (esistente && (esistente->contract.code != contract || esistente->client.code != committente)) {
            ferma(CardFault::OtherContract, pr.sheet_cards, r.row, pr.card_cols.id);
            out.outcomes.push_back(o);
            continue;
        }
        // La data da cui vale.
        const bool serve_scelta = k.from < taken;
        const auto it = choices.by_nr.find(r.nr);
        DateChoice scelta;
        ChoiceFrom da = ChoiceFrom::None;
        if (it != choices.by_nr.end() && (serve_scelta || it->second.kind == DateChoice::Kind::Date)) {
            scelta = it->second;
            da = ChoiceFrom::Card;
        } else if (serve_scelta && choices.file.kind != DateChoice::Kind::None) {
            scelta = choices.file;
            da = ChoiceFrom::File;
        }
        Day dal = k.from;
        ChoiceMode modo = ChoiceMode::Activation;
        if (serve_scelta || da != ChoiceFrom::None) {
            switch (scelta.kind) {
                case DateChoice::Kind::None:
                    o.kind = CardOutcome::Kind::ChoiceMissing;
                    problema(Check::CardChoiceMissing, 0,
                             {text_param("scheda", r.nr), date_param("attivazione", k.from), date_param("presa_in_carico", taken)}, r.row);
                    out.outcomes.push_back(o);
                    continue;
                case DateChoice::Kind::Activation: dal = k.from; modo = ChoiceMode::Activation; break;
                case DateChoice::Kind::TakenInCharge: dal = taken; modo = ChoiceMode::TakenInCharge; break;
                case DateChoice::Kind::Date: dal = scelta.date; modo = ChoiceMode::Date; break;
            }
        }
        if (dal < k.from) {
            ferma(CardFault::ChoiceBeforeActivation, pr.sheet_cards, r.row, pr.card_cols.from);
            out.outcomes.push_back(o);
            continue;
        }
        if (dal > k.to) {
            ferma(CardFault::ChoiceAfterEnd, pr.sheet_cards, r.row, pr.card_cols.to);
            out.outcomes.push_back(o);
            continue;
        }
        o.valid = {dal, k.to};
        o.mode = modo;
        o.from = da;
        // Avvisi della lettura.
        const int giorni = card_days(k, idx);
        if (giorni != k.total_days)
            problema(Check::CardTotalDaysDiffer, 0,
                     {text_param("scheda", r.nr), number_param("tot_giorni", k.total_days), number_param("giorni", giorni),
                      ref_param("calendario", Element::Calendar, k.base_calendar)},
                     r.row);
        if (k.cadence_form == CadenceForm::ListedDates) {
            int fuori = 0;
            Day prima = 0;
            for (Day d : k.dates) {
                Calendar solo_base;
                solo_base.base = Ref{k.base_calendar, {}};
                solo_base.range = {d, d};
                solo_base.days.add(weekday_of(d));
                const bool ok = k.days.has(weekday_of(d)) && calendar_active(solo_base, d, idx);
                if (!ok && !fuori++) prima = d;
            }
            if (fuori)
                problema(Check::CardListedDateOutside, 0,
                         {text_param("scheda", r.nr), number_param("quante", fuori), date_param("prima", prima),
                          ref_param("calendario", Element::Calendar, k.base_calendar)},
                         r.row);
        }
        if (k.to < taken) problema(Check::CardEndedBeforeTaken, 0, {text_param("scheda", r.nr), date_param("disattivazione", k.to)}, r.row);
        if (esistente && !k.is_change) problema(Check::CardNewOrChangeOdd, 0, {text_param("scheda", r.nr), ref_param("codice", Element::ServiceCard, esistente->code)}, r.row);
        if (!esistente && k.is_change) problema(Check::CardNewOrChangeOdd, 1, {text_param("scheda", r.nr)}, r.row);

        if (esistente) {
            const CardVersion& ultima = esistente->versions.back();
            if (same_card(ultima.card, k)) {
                o.kind = CardOutcome::Kind::Same;
                o.card_code = esistente->code;
                o.version = ultima.number;
                o.valid = ultima.valid;
                out.outcomes.push_back(o);
                continue;
            }
            if (ultima.card.nr != k.nr)
                problema(Check::CardNrChanged, 0, {text_param("scheda", r.nr), text_param("prima", ultima.card.nr),
                                                   ref_param("codice", Element::ServiceCard, esistente->code)}, r.row);
        }
        // Codici degli elementi e numero della versione (mai riusati).
        int numero = 1;
        if (esistente) {
            for (const CardVersion& v : esistente->versions) numero = std::max(numero, v.number + 1);
            for (const RemovedCardVersions& t : p.removed_cards)
                if (t.card == esistente->code)
                    for (int n : t.versions) numero = std::max(numero, n + 1);
            for (const CardVersion& v : esistente->versions)
                for (int n : v.removed) numero = std::max(numero, n + 1);
        }
        const std::string base = nr_in_codice(k.nr) + (numero > 1 ? "-V" + std::to_string(numero) : std::string());
        CardVersion nv;
        nv.number = numero;
        nv.valid = o.valid;
        nv.taken = taken;
        nv.reference = reference;
        nv.mode = modo;
        nv.from = da;
        nv.service = libero("S-" + base, usati);
        nv.group = libero("G-" + base, usati);
        nv.trip = libero("C-" + base, usati);
        nv.calendar = libero("CAL-" + base, usati);
        nv.card = k;
        for (CardStop& s : nv.card.stops) s.row = 0;
        o.service = nv.service;
        o.group = nv.group;
        o.trip = nv.trip;
        o.calendar = nv.calendar;
        o.version = numero;
        for (const Code& c : r.new_homes)
            if (case_messe.insert(c).second) {
                out.new_homes.push_back(c);
                o.new_homes.push_back(c);
            }
        if (!esistente) {
            ServiceCard s;
            s.code = libero("SCH-" + nr_in_codice(k.nr), usati);
            s.id = r.id;
            s.client = Ref{committente, {}};
            s.profile = Ref{pr.code, {}};
            s.contract = Ref{contract, {}};
            s.versions.push_back(nv);
            o.kind = CardOutcome::Kind::New;
            o.card_code = s.code;
            out.added.push_back({s.code, numero});
            out.cards.push_back(std::move(s));
            out.outcomes.push_back(o);
            continue;
        }
        // Versione nuova: le versioni che cominciano dalla data scelta in poi si tolgono; quella che vale il giorno prima finisce
        // il giorno prima. I giorni prima della data scelta non cambiano.
        o.kind = CardOutcome::Kind::NewVersion;
        o.card_code = esistente->code;
        o.changes = card_changes(esistente->versions.back().card, k);
        std::vector<CardVersion> restano;
        std::vector<CardVersion> tolte;
        for (const CardVersion& v : esistente->versions) (v.valid.from >= dal ? tolte : restano).push_back(v);
        // Le giornate prima della presa in carico che cambiano: dove valeva una versione di prima o vale quella nuova.
        if (dal < taken) {
            const Calendar nuovo = card_calendar(k, "", o.valid);
            for (Day d = dal; d < taken; ++d) {
                bool cambia = calendar_active(nuovo, d, idx);
                for (const CardVersion& v : esistente->versions)
                    if (d >= v.valid.from && d <= v.valid.to) {
                        const Calendar* c = idx.calendar(v.calendar);
                        const Calendar vecchio = c ? *c : card_calendar(v.card, "", v.valid);
                        cambia = cambia || calendar_active(vecchio, d, idx);
                    }
                o.days_before_taken += cambia ? 1 : 0;
            }
        }
        for (const CardVersion& v : tolte) {
            o.removed_versions.push_back(v.number);
            nv.removed.push_back(v.number);
            out.removed.push_back(v);
        }
        if (!restano.empty() && restano.back().valid.to >= dal) {
            CardVersion& c = restano.back();
            nv.previous_end = c.valid.to;
            o.cut_version = c.number;
            o.cut_to = dal - 1;
            c.valid.to = dal - 1;
            out.cuts.push_back({c.calendar, dal - 1});
        }
        if (dal < taken)
            problema(Check::CardChosenBeforeTaken, o.removed_versions.empty() ? 0 : 1,
                     {text_param("scheda", r.nr), date_param("dal", dal), date_param("presa_in_carico", taken),
                      number_param("giornate", o.days_before_taken),
                      number_param("quante_versioni", static_cast<int>(o.removed_versions.size())), text_param("versioni", [&] {
                          std::string t;
                          for (int n : o.removed_versions) t += (t.empty() ? "" : ", ") + std::to_string(n);
                          return t;
                      }())},
                     r.row);
        restano.push_back(nv);
        esistente->versions = restano;
        out.added.push_back({esistente->code, numero});
        out.outcomes.push_back(o);
    }
    std::stable_sort(out.cards.begin(), out.cards.end(), [](const ServiceCard& a, const ServiceCard& b) { return a.code < b.code; });
    return out;
}

// --- Togliere -----------------------------------------------------------------------------------------------------------

CardRemoval remove_card(std::vector<ServiceCard>& cards, std::vector<RemovedCardVersions>& removed, const Code& card, bool all,
                        const std::string& reference, Day date, const Package& p) {
    CardRemoval out;
    out.card = card;
    auto it = std::find_if(cards.begin(), cards.end(), [&](const ServiceCard& s) { return s.code == card; });
    if (it == cards.end()) throw std::logic_error("scheda che non c'e'");
    std::vector<CardVersion> via;
    if (all) via = it->versions;
    else {
        if (!it->versions.back().removed.empty()) {
            out.refusal = CardRemovalRefusal::RemovedOthers;
            return out;
        }
        via.push_back(it->versions.back());
    }
    for (const CardVersion& v : via) {
        for (const Shift& s : p.shifts)
            for (const ShiftItem& i : s.items)
                if (i.is_trip && i.trip.trip.code == v.trip) {
                    out.refusal = CardRemovalRefusal::TripInUse;
                    out.user = s.code;
                    out.trip = v.trip;
                    return out;
                }
        for (const Block& b : p.blocks)
            for (const Ref& t : b.trips)
                if (t.code == v.trip) {
                    out.refusal = CardRemovalRefusal::TripInUse;
                    out.user = b.code;
                    out.trip = v.trip;
                    return out;
                }
    }
    RemovedCardVersions r;
    r.card = it->code;
    r.id = it->id;
    r.reference = reference;
    r.date = date;
    for (const CardVersion& v : via) r.versions.push_back(v.number);
    out.removed = via;
    if (all || it->versions.size() == 1) {
        out.card_gone = true;
        cards.erase(it);
    } else {
        const std::optional<Day> fine = it->versions.back().previous_end;
        it->versions.pop_back();
        if (fine && it->versions.back().valid.to != *fine) {
            it->versions.back().valid.to = *fine;
            out.restored = CardsMerge::Cut{it->versions.back().calendar, *fine};
            out.restored_version = it->versions.back().number;
        }
    }
    removed.push_back(std::move(r));
    return out;
}

// --- Km dei pezzi di corsa ------------------------------------------------------------------------------------------------

PartKm part_km(const Trip& t, std::size_t from, std::size_t to, bool from_card) {
    PartKm k;
    for (std::size_t i = from + 1; i <= to && i < t.passages.size(); ++i) {
        k.loaded += t.passages[i].loaded_km.value_or(0.0);
        k.empty += t.passages[i].empty_km.value_or(0.0);
    }
    if (!from_card) return k;
    const auto con_km = [&](std::size_t i) { return t.passages[i].loaded_km || t.passages[i].empty_km; };
    // I km scritti su una riga coprono il tratto dalla riga prima che ha dei km: il pezzo li sa solo se i suoi capi ne hanno.
    k.known = (from == 0 || con_km(from)) && (to + 1 >= t.passages.size() || con_km(to));
    if (!k.known) k.loaded = k.empty = 0;
    return k;
}

std::set<Code> card_trips(const Package& p) {
    std::set<Code> s;
    for (const ServiceCard& c : p.service_cards)
        for (const CardVersion& v : c.versions) s.insert(v.trip);
    return s;
}

// --- Controlli sul registro --------------------------------------------------------------------------------------------------

namespace {
std::string fonte(const Regulation& r) {
    const RuleSource& f = r.assistant->source;
    const std::string doc = f.document.empty() ? r.document : f.document;
    return f.point.empty() ? doc : doc + ", " + f.point;
}
}  // namespace

void check_cards(const Package& p, const Index& idx, std::vector<Problem>& out) {
    const auto add = [&](Check c, const Source& where, std::vector<Param> params, int variant = 0) {
        out.push_back(Problem{c, variant, where, std::move(params)});
    };
    const auto ref = [&](const Ref& r, Element e, bool esiste) {
        if (!esiste) add(Check::UnknownReference, r.src, {text_param("codice", r.code), elements_param("elementi", e)});
    };
    // Profili.
    std::set<Code> visti;
    for (const CardProfile& pr : p.card_profiles) {
        if (!visti.insert(pr.code).second)
            add(Check::DuplicateCode, pr.src, {text_param("codice", pr.code), elements_param("elementi", Element::CardProfile)});
        ref(pr.client, Element::Client, idx.client(pr.client.code) != nullptr);
        // Una parola che vuol dire due cose diverse (es. la stessa per "sale" e "scende"): il profilo e' ambiguo.
        const auto chi = ref_param("profilo", Element::CardProfile, pr.code);
        const auto doppie = [&](const std::vector<std::vector<std::string>>& gruppi, int variante) {
            std::map<std::string, int> dove;
            for (std::size_t g = 0; g < gruppi.size(); ++g)
                for (const std::string& w : gruppi[g]) {
                    const std::string k = normalize_name(w);
                    const auto it = dove.find(k);
                    if (it != dove.end() && it->second != static_cast<int>(g)) add(Check::CardProfileInvalid, pr.src, {chi, text_param("parola", w)}, variante);
                    else if (it != dove.end()) add(Check::CardProfileInvalid, pr.src, {chi, text_param("parola", w)}, variante);
                    dove.emplace(k, static_cast<int>(g));
                }
        };
        doppie({pr.escort_boards, pr.escort_alights}, 0);
        doppie({pr.new_words, pr.change_words}, 1);
        std::vector<std::string> servizi, cadenze, giorni;
        for (const auto& s : pr.services) servizi.push_back(s.text);
        for (const auto& c : pr.cadences) cadenze.push_back(c.word);
        for (const auto& g : pr.days) giorni.push_back(g.column);
        doppie({servizi}, 2);
        doppie({cadenze}, 3);
        doppie({giorni}, 4);
        std::set<int> gg;
        for (const auto& g : pr.days)
            if (!gg.insert(static_cast<int>(g.day)).second) add(Check::CardProfileInvalid, pr.src, {chi, text_param("parola", g.column)}, 5);
    }
    // Registro: codici, ID, riferimenti.
    std::set<Code> schede;
    std::map<std::string, Code> per_id;
    for (const ServiceCard& s : p.service_cards) {
        if (!schede.insert(s.code).second)
            add(Check::DuplicateCode, s.src, {text_param("codice", s.code), elements_param("elementi", Element::ServiceCard)});
        const auto [it, nuovo] = per_id.emplace(chiave_id(s.id), s.code);
        if (!nuovo) add(Check::CardRegisterInvalid, s.src, {ref_param("scheda", Element::ServiceCard, s.code), ref_param("altra", Element::ServiceCard, it->second)}, 0);
        ref(s.client, Element::Client, idx.client(s.client.code) != nullptr);
        ref(s.profile, Element::CardProfile, idx.card_profile(s.profile.code) != nullptr);
        ref(s.contract, Element::Contract, idx.contract(s.contract.code) != nullptr);
        for (std::size_t i = 0; i < s.versions.size(); ++i) {
            const CardVersion& v = s.versions[i];
            if (v.valid.to < v.valid.from || (i > 0 && (v.number <= s.versions[i - 1].number || v.valid.from <= s.versions[i - 1].valid.to)))
                add(Check::CardRegisterInvalid, v.src, {ref_param("scheda", Element::ServiceCard, s.code), number_param("versione", v.number)}, 1);
            const Card& k = v.card;
            for (const Code& sc : k.schools) ref(Ref{sc, v.src}, Element::School, idx.school(sc) != nullptr);
            ref(Ref{k.base_calendar, v.src}, Element::Calendar, idx.calendar(k.base_calendar) != nullptr);
            for (const CardStop& f : k.stops) ref(Ref{f.place, f.src}, Element::Place, idx.place(f.place) != nullptr);
        }
    }

    // K-CTR-169: schede dello stesso servizio (numero prima della barra) con periodi che si sovrappongono. Mai unite.
    for (std::size_t a = 0; a < p.service_cards.size(); ++a)
        for (std::size_t b = a + 1; b < p.service_cards.size(); ++b) {
            const ServiceCard &x = p.service_cards[a], &y = p.service_cards[b];
            const CardProfile* px = idx.card_profile(x.profile.code);
            const CardProfile* py = idx.card_profile(y.profile.code);
            if (!px || !py || x.client.code != y.client.code || x.versions.empty() || y.versions.empty()) continue;
            if (normalize_name(card_service_number(x.versions.back().card.nr, *px)) != normalize_name(card_service_number(y.versions.back().card.nr, *py)))
                continue;
            bool fatto = false;
            for (const CardVersion& u : x.versions)
                for (const CardVersion& w : y.versions) {
                    if (fatto || u.valid.to < w.valid.from || w.valid.to < u.valid.from) continue;
                    fatto = true;
                    add(Check::CardServiceOverlap, y.src,
                        {ref_param("scheda", Element::ServiceCard, x.code), ref_param("altra", Element::ServiceCard, y.code),
                         text_param("servizio", card_service_number(x.versions.back().card.nr, *px)),
                         date_param("dal", std::max(u.valid.from, w.valid.from)), date_param("al", std::min(u.valid.to, w.valid.to))});
                }
        }

    // K-CTR-171: l'accompagnatore che manca su una corsa di una scheda, con la regola "assistente a bordo" del regolamento del
    // contratto (nessuna regola nel programma: senza il parametro nel regolamento, nessun avviso).
    for (const ServiceCard& s : p.service_cards)
        for (const CardVersion& v : s.versions) {
            const Trip* t = idx.trip(v.trip);
            const Service* sv = idx.service(v.service);
            const PassengerGroup* g = idx.group(v.group);
            if (!t || !sv || !g) continue;
            const Regulation* reg = idx.regulation_of(*sv);
            if (!reg || !reg->assistant) continue;
            OnBoard b;
            for (const Code& sc : v.card.schools)
                if (const School* x = idx.school(sc)) {
                    if (x->grade) b.grades.push_back(*x->grade);
                    if (x->center_type) b.center_types.push_back(*x->center_type);
                }
            const bool tipi_noti = sv->kind.known() && g->type.known();
            if (sv->kind.known()) b.service_kinds.push_back(sv->kind.value());
            if (g->type.known()) b.group_types.push_back(g->type.value());
            // La condizione che vale (con i posti, se la dice: allora dipende dal mezzo, che la scheda non ha).
            const PassengerCondition* certa = nullptr;
            const PassengerCondition* con_posti = nullptr;
            for (const PassengerCondition& c : reg->assistant->conditions) {
                const bool usa_tipi = !c.group_types.empty() || !c.service_kinds.empty();
                if (usa_tipi && !tipi_noti) continue;   // non verificabile: il controllo vuole i dati particolari
                if (!condition_holds(c, b, 1000000)) continue;
                if (c.seats_over) {
                    if (!con_posti) con_posti = &c;
                } else if (!certa) {
                    certa = &c;
                }
            }
            const PassengerCondition* c = certa ? certa : con_posti;
            if (!c) continue;
            // Il primo tratto con qualcuno a bordo e senza l'assistente.
            int a_bordo = 0;
            bool assistente = false;
            for (std::size_t i = 0; i + 1 < t->passages.size(); ++i) {
                const Passage& ps = t->passages[i];
                for (const PassageCount& pc : ps.persons) a_bordo += pc.boarding - pc.alighting;
                if (ps.assistant == AssistantMove::Boards) assistente = true;
                if (ps.assistant == AssistantMove::Alights) assistente = false;
                if (a_bordo > 0 && !assistente) {
                    const bool particolare = !c->group_types.empty() || !c->service_kinds.empty() || !c->needs.empty() || !c->center_types.empty();
                    const int variante = (c->seats_over ? 1 : 0) + (particolare ? 2 : 0);
                    std::vector<Param> par = {ref_param("corsa", Element::Trip, t->code), ref_param("scheda", Element::ServiceCard, s.code),
                                              ref_param("da", Element::Place, ps.place.code),
                                              ref_param("a", Element::Place, t->passages[i + 1].place.code),
                                              time_param("orario", ps.leaves()), ref_param("regolamento", Element::Regulation, reg->code),
                                              text_param("fonte", fonte(*reg))};
                    if (c->seats_over) par.push_back(number_param("posti", *c->seats_over));
                    add(Check::CardEscortMissing, t->src, par, variante);
                    break;
                }
            }
        }

    // K-CTR-172: un turno con un pezzo di una corsa di una scheda i cui km non si sanno.
    const std::set<Code> da_schede = card_trips(p);
    for (const Shift& sh : p.shifts)
        for (const ShiftItem& it : sh.items) {
            if (!it.is_trip || !da_schede.count(it.trip.trip.code)) continue;
            const Trip* t = idx.trip(it.trip.trip.code);
            if (!t) continue;
            const PartResolution r = resolve_part(it.trip, *t);
            if (r.problem != PartProblem::None || (r.from == 0 && r.to + 1 == t->passages.size())) continue;
            if (!part_km(*t, r.from, r.to, true).known)
                add(Check::CardTripPartKmUnknown, it.src, {ref_param("turno", Element::Shift, sh.code), ref_param("corsa", Element::Trip, t->code)});
        }
}

}  // namespace kzero
