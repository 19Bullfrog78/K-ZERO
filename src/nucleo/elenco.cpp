// K-Zero - elenco utenti del committente (K-019). Vedi elenco.hpp.
#include "elenco.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include "ricerca.hpp"

namespace kzero {

namespace {


bool spazio(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

// Lettere ASCII uguali senza maiuscole; gli altri byte uguali.
bool uguale_ci(char a, char b) {
    const auto basso = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
    return basso(a) == basso(b);
}
bool inizia_con_ci(const std::string& s, std::size_t i, const std::string& prefisso) {
    if (prefisso.empty() || i + prefisso.size() > s.size()) return false;
    for (std::size_t k = 0; k < prefisso.size(); ++k)
        if (!uguale_ci(s[i + k], prefisso[k])) return false;
    return true;
}
bool lettera(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || static_cast<unsigned char>(c) >= 0x80; }
bool cifra(char c) { return c >= '0' && c <= '9'; }

std::string pulito(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && s[a] == ' ') ++a;
    while (b > a && s[b - 1] == ' ') --b;
    return s.substr(a, b - a);
}

void salta_spazi(const std::string& s, std::size_t& i) {
    while (i < s.size() && s[i] == ' ') ++i;
}

// La sigla piu' lunga tra "sigle" che comincia in s[i] (senza maiuscole). Con dopo_non_lettera, il carattere dopo non deve
// essere una lettera (cosi' "o" non si prende dentro "ora"). Restituisce l'indice della sigla o -1.
int sigla_in(const std::string& s, std::size_t i, const std::vector<std::string>& sigle, bool dopo_non_lettera) {
    int scelta = -1;
    std::size_t lunghezza = 0;
    for (std::size_t k = 0; k < sigle.size(); ++k) {
        const std::string sg = pulito(sigle[k]);
        if (sg.size() <= lunghezza || !inizia_con_ci(s, i, sg)) continue;
        if (dopo_non_lettera && i + sg.size() < s.size() && lettera(s[i + sg.size()]) && lettera(sg.back())) continue;
        scelta = static_cast<int>(k);
        lunghezza = sg.size();
    }
    return scelta;
}

// "hh<sep>mm" in s[i]: ore 0-23 (una o due cifre), minuti 00-59.
std::optional<Minutes> leggi_ora(const std::string& s, std::size_t& i, const std::string& sep) {
    std::size_t k = i;
    int ore = 0, cifre = 0;
    while (k < s.size() && cifra(s[k]) && cifre < 2) {
        ore = ore * 10 + (s[k] - '0');
        ++k;
        ++cifre;
    }
    if (cifre == 0 || cifra(s.size() > k ? s[k] : 'x')) return std::nullopt;
    const std::string sp = pulito(sep);
    if (!inizia_con_ci(s, k, sp)) return std::nullopt;
    k += sp.size();
    if (k + 2 > s.size() || !cifra(s[k]) || !cifra(s[k + 1]) || (k + 2 < s.size() && cifra(s[k + 2]))) return std::nullopt;
    const int minuti = (s[k] - '0') * 10 + (s[k + 1] - '0');
    if (ore > 23 || minuti > 59) return std::nullopt;
    i = k + 2;
    return ore * 60 + minuti;
}

// Un intero di 1-2 cifre in s[i].
std::optional<int> leggi_numero(const std::string& s, std::size_t& i) {
    int n = 0, cifre = 0;
    while (i < s.size() && cifra(s[i]) && cifre < 2) {
        n = n * 10 + (s[i] - '0');
        ++i;
        ++cifre;
    }
    if (cifre == 0 || (i < s.size() && cifra(s[i]))) return std::nullopt;
    return n;
}

std::vector<std::string> sigle_tratte(const ReadingProfile& p) {
    std::vector<std::string> v;
    for (const ProfileLeg& l : p.legs) v.push_back(l.code);
    return v;
}
std::vector<std::string> sigle_giorni(const ReadingProfile& p) {
    std::vector<std::string> v;
    for (const ProfileDay& d : p.days) v.push_back(d.code);
    return v;
}
std::vector<std::string> sigle_periodi(const ReadingProfile& p) {
    std::vector<std::string> v;
    for (const ProfilePeriod& x : p.periods) v.push_back(x.code);
    return v;
}

}  // namespace

std::string clean_cell(const std::string& text) {
    std::string out;
    bool in_spazio = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        bool sp = spazio(c);
        std::size_t salta = 0;
        if (c == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xA0) {   // spazio indivisibile
            sp = true;
            salta = 1;
        }
        if (sp) {
            in_spazio = true;
            i += salta;
            continue;
        }
        if (in_spazio && !out.empty()) out += ' ';
        in_spazio = false;
        out += static_cast<char>(c);
    }
    return out;
}

DateRange school_year(int first_year, int start_month) {
    const Day dal = day_from_ymd(first_year, start_month, 1);
    const Day al = day_from_ymd(first_year + 1, start_month, 1) - 1;
    return DateRange{dal, al};
}

std::optional<Day> school_date(int first_year, int start_month, int day, int month) {
    if (month < 1 || month > 12 || day < 1 || day > 31) return std::nullopt;
    const int anno = month >= start_month ? first_year : first_year + 1;
    const auto due = [](int n) { return std::string(1, static_cast<char>('0' + n / 10)) + static_cast<char>('0' + n % 10); };
    if (anno < 1000 || anno > 9999) return std::nullopt;
    return day_from_text(std::to_string(anno) + "-" + due(month) + "-" + due(day));
}

RoutesParse parse_routes(const std::string& text, const ReadingProfile& p) {
    RoutesParse out;
    const std::string s = clean_cell(text);
    if (s.empty()) return out;
    const std::string sep = pulito(p.company_sep);
    const std::vector<std::string> sigle = sigle_tratte(p);
    // Dove comincia ogni tratta: una sigla all'inizio o dopo uno spazio, seguita (con spazi o no) dal separatore.
    struct Inizio {
        std::size_t inizio, ditta;
        int tratta;
    };
    std::vector<Inizio> inizi;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i > 0 && s[i - 1] != ' ') continue;
        const int k = sigla_in(s, i, sigle, false);
        if (k < 0) continue;
        std::size_t j = i + pulito(sigle[static_cast<std::size_t>(k)]).size();
        salta_spazi(s, j);
        if (!inizia_con_ci(s, j, sep)) continue;
        j += sep.size();
        inizi.push_back({i, j, k});
    }
    if (inizi.empty() || inizi.front().inizio != 0) {
        out.fault = RowFault::RoutesForm;
        return out;
    }
    std::set<int> viste;
    for (std::size_t n = 0; n < inizi.size(); ++n) {
        const std::size_t fine = n + 1 < inizi.size() ? inizi[n + 1].inizio : s.size();
        const std::string ditta = pulito(s.substr(inizi[n].ditta, fine - inizi[n].ditta));
        if (ditta.empty()) {
            out.fault = RowFault::RoutesForm;
            return out;
        }
        if (!viste.insert(inizi[n].tratta).second) {
            out.fault = RowFault::RoutesLegTwice;
            return out;
        }
        ListUserLeg l;
        l.leg = p.legs[static_cast<std::size_t>(inizi[n].tratta)].code;
        for (const ProfileCompany& c : p.companies)
            if (normalize_name(c.text) == normalize_name(ditta)) l.company = Ref{c.company.code, {}};
        if (!l.company) l.other_company = ditta;
        out.legs.push_back(std::move(l));
    }
    return out;
}

namespace {

void voce_per_tratta(const std::string& voce, const ProfileTimes& c, const ReadingProfile& p, const DateRange& anno, TimesParse& out) {
    std::size_t i = 0;
    const int k = sigla_in(voce, i, sigle_tratte(p), false);
    if (k < 0) {
        out.fault = RowFault::TimesLegUnknown;
        return;
    }
    i += pulito(p.legs[static_cast<std::size_t>(k)].code).size();
    salta_spazi(voce, i);
    const std::string sep = pulito(c.leg_sep);
    if (!inizia_con_ci(voce, i, sep)) {
        out.fault = RowFault::TimesForm;
        return;
    }
    i += sep.size();
    salta_spazi(voce, i);
    const std::optional<Minutes> ora = leggi_ora(voce, i, c.time_sep);
    if (!ora) {
        out.fault = RowFault::TimesHour;
        return;
    }
    salta_spazi(voce, i);
    const std::string giorni = voce.substr(i);
    if (giorni.empty()) {
        out.fault = RowFault::TimesForm;
        return;
    }
    const std::string sg = pulito(c.days_sep);
    std::size_t a = 0;
    std::set<int> visti;
    while (a <= giorni.size()) {
        std::size_t b = giorni.find(sg, a);
        if (sg.empty()) b = std::string::npos;
        if (b == std::string::npos) b = giorni.size();
        const std::string g = pulito(giorni.substr(a, b - a));
        int trovato = -1;
        for (std::size_t d = 0; d < p.days.size(); ++d)
            if (normalize_name(p.days[d].code) == normalize_name(g)) trovato = static_cast<int>(d);
        if (g.empty() || trovato < 0) {
            out.fault = RowFault::TimesDayUnknown;
            return;
        }
        const Weekday w = p.days[static_cast<std::size_t>(trovato)].day;
        if (visti.insert(static_cast<int>(w)).second)
            out.times.push_back(ListUserTime{p.legs[static_cast<std::size_t>(k)].code, w, *ora, std::string(), anno, {}});
        a = b + sg.size();
        if (b == giorni.size()) break;
    }
}

void blocco_per_giorno(const std::string& blocco, const ProfileTimes& c, const ReadingProfile& p, int first_year,
                       const DateRange& anno, TimesParse& out) {
    std::size_t i = 0;
    const int g = sigla_in(blocco, i, sigle_giorni(p), true);
    if (g < 0) {
        out.fault = RowFault::TimesDayUnknown;
        return;
    }
    const Weekday giorno = p.days[static_cast<std::size_t>(g)].day;
    i += pulito(p.days[static_cast<std::size_t>(g)].code).size();
    const std::string sep_voci = pulito(c.item_sep);
    const std::string apre = pulito(c.date_open), chiude = pulito(c.date_close), sep_date = pulito(c.date_sep),
                      sep_gm = pulito(c.day_month_sep);
    const std::vector<std::string> periodi = sigle_periodi(p);
    bool almeno_una = false;
    for (;;) {
        salta_spazi(blocco, i);
        if (inizia_con_ci(blocco, i, sep_voci)) {
            i += sep_voci.size();
            salta_spazi(blocco, i);
        }
        if (i >= blocco.size()) break;
        const int k = sigla_in(blocco, i, periodi, true);
        if (k < 0) {
            out.fault = RowFault::TimesPeriodUnknown;
            return;
        }
        const ProfilePeriod& periodo = p.periods[static_cast<std::size_t>(k)];
        i += pulito(periodo.code).size();
        salta_spazi(blocco, i);
        const std::optional<Minutes> ora = leggi_ora(blocco, i, c.time_sep);
        if (!ora) {
            out.fault = RowFault::TimesHour;
            return;
        }
        DateRange date = anno;
        std::size_t j = i;
        salta_spazi(blocco, j);
        if (inizia_con_ci(blocco, j, apre)) {
            j += apre.size();
            const auto data = [&](std::optional<Day>& d) -> bool {
                salta_spazi(blocco, j);
                const std::optional<int> gg = leggi_numero(blocco, j);
                salta_spazi(blocco, j);
                if (!gg || !inizia_con_ci(blocco, j, sep_gm)) return false;
                j += sep_gm.size();
                salta_spazi(blocco, j);
                const std::optional<int> mm = leggi_numero(blocco, j);
                if (!mm) return false;
                d = school_date(first_year, p.start_month, *gg, *mm);
                return true;
            };
            std::optional<Day> dal, al;
            if (!data(dal)) {
                out.fault = RowFault::TimesForm;
                return;
            }
            salta_spazi(blocco, j);
            if (!inizia_con_ci(blocco, j, sep_date)) {
                out.fault = RowFault::TimesForm;
                return;
            }
            j += sep_date.size();
            if (!data(al)) {
                out.fault = RowFault::TimesForm;
                return;
            }
            salta_spazi(blocco, j);
            if (!inizia_con_ci(blocco, j, chiude)) {
                out.fault = RowFault::TimesForm;
                return;
            }
            j += chiude.size();
            if (!dal || !al) {
                out.fault = RowFault::TimesDate;
                return;
            }
            if (*al < *dal) {
                out.fault = RowFault::TimesDatesReversed;
                return;
            }
            date = DateRange{*dal, *al};
            i = j;
        }
        out.times.push_back(ListUserTime{periodo.leg, giorno, *ora, periodo.code, date, {}});
        almeno_una = true;
    }
    if (!almeno_una) out.fault = RowFault::TimesForm;
}

}  // namespace

TimesParse parse_times(const std::string& text, const ProfileTimes& c, const ReadingProfile& p, int first_year) {
    TimesParse out;
    const std::string s = clean_cell(text);
    if (s.empty()) return out;
    const DateRange anno = school_year(first_year, p.start_month);
    if (c.form == TimesForm::ByLeg) {
        const std::string sep = pulito(c.item_sep);
        std::size_t a = 0;
        while (a <= s.size()) {
            std::size_t b = sep.empty() ? std::string::npos : s.find(sep, a);
            if (b == std::string::npos) b = s.size();
            const std::string voce = pulito(s.substr(a, b - a));
            if (!voce.empty()) voce_per_tratta(voce, c, p, anno, out);
            if (out.fault) {
                out.times.clear();
                return out;
            }
            if (b == s.size()) break;
            a = b + sep.size();
        }
        return out;
    }
    const std::string inizio = pulito(c.day_start);
    if (inizio.empty() || !inizia_con_ci(s, 0, inizio)) {
        out.fault = RowFault::TimesForm;
        return out;
    }
    std::size_t a = inizio.size();
    while (a <= s.size()) {
        std::size_t b = s.find(inizio, a);
        if (b == std::string::npos) b = s.size();
        const std::string blocco = pulito(s.substr(a, b - a));
        blocco_per_giorno(blocco, c, p, first_year, anno, out);
        if (out.fault) {
            out.times.clear();
            return out;
        }
        if (b == s.size()) break;
        a = b + inizio.size();
    }
    return out;
}

StatusParse parse_status(const std::string& text, const ReadingProfile& p) {
    StatusParse out;
    const std::string s = clean_cell(text);
    const std::string sep = pulito(p.note_sep);
    std::string parola = s, nota;
    const std::size_t k = sep.empty() ? std::string::npos : s.find(sep);
    if (k != std::string::npos) {
        parola = pulito(s.substr(0, k));
        nota = pulito(s.substr(k + sep.size()));
    }
    const std::string n = normalize_name(parola);
    if (n.empty()) return out;
    for (const std::string& w : p.active_words)
        if (normalize_name(w) == n) {
            out.active = true;
            out.ok = true;
        }
    for (const std::string& w : p.suspended_words)
        if (normalize_name(w) == n) {
            if (out.ok) {   // parola sia attiva sia sospesa: un profilo ambiguo (K-CTR-139), mai una scelta
                out.ok = false;
                return out;
            }
            out.active = false;
            out.ok = true;
        }
    out.note = nota;
    return out;
}

std::string list_key(const std::string& name, const Code& school) { return normalize_name(name) + '\x1f' + school; }

std::string list_user_code(int number) {
    std::string n = std::to_string(number);
    if (n.size() < 6) n.insert(0, 6 - n.size(), '0');
    return "U-" + n;
}

namespace {

// Le colonne del file: dal titolo del profilo all'indice.
struct Colonne {
    std::map<std::string, int> per_campo;   // campo del profilo ("nome", ...) -> colonna
    std::map<std::string, std::string> titolo;
    std::vector<std::pair<const ProfileTimes*, int>> orari;
};

Problem problema_riga(Check c, int variante, int riga, int colonna, std::vector<Param> param) {
    Problem p{c, variante, Source{"", "", riga, colonna}, {number_param("riga", riga)}};
    for (Param& x : param) p.params.push_back(std::move(x));
    return p;
}

// Le scuole del pacchetto per codice esterno del profilo.
class Scuole {
public:
    Scuole(const Package& pk, const ReadingProfile& p) {
        for (const School& s : pk.schools)
            for (const ExternalCode& e : s.external)
                if (normalize_name(e.system) == normalize_name(p.ext_system) && normalize_name(e.type) == normalize_name(p.ext_school_type))
                    indice_[normalize_name(e.code)].insert(s.code);
    }
    const std::set<Code>* trova(const std::string& nome) const {
        const auto it = indice_.find(normalize_name(nome));
        return it == indice_.end() ? nullptr : &it->second;
    }

private:
    std::map<std::string, std::set<Code>> indice_;
};

// Le fermate: dal codice esterno del profilo fra tutti i luoghi pubblici, poi dal nome fra i luoghi del comune della casa.
class Fermate {
public:
    Fermate(const Package& pk, const ReadingProfile& p) : pk_(pk), p_(p) {
        std::vector<Place> pubblici;
        for (const Place& l : pk.places)
            if (!l.is_private) pubblici.push_back(l);
        tutti_ = std::make_unique<PlaceSearch>(pubblici);
    }
    // Restituisce il luogo e come e' stato riconosciuto, oppure nullopt.
    std::optional<std::pair<Code, StopMatch>> trova(const std::string& testo, const std::string& comune) {
        const PlaceSearch::Result e = tutti_->by_external_code(p_.ext_system, p_.ext_stop_type, testo);
        if (e.outcome == PlaceSearch::Outcome::Found && e.codes.size() == 1) return std::make_pair(e.codes.front(), StopMatch::ExternalCode);
        if (comune.empty()) return std::nullopt;
        const std::string chiave = normalize_name(comune);
        auto it = per_comune_.find(chiave);
        if (it == per_comune_.end()) {
            std::vector<Place> del_comune;
            for (const Place& l : pk_.places)
                if (!l.is_private && normalize_name(l.municipality) == chiave) del_comune.push_back(l);
            it = per_comune_.emplace(chiave, std::make_unique<PlaceSearch>(del_comune)).first;
        }
        const PlaceSearch::Result n = it->second->by_name(testo);
        if (n.outcome == PlaceSearch::Outcome::Found && n.codes.size() == 1) return std::make_pair(n.codes.front(), StopMatch::Name);
        return std::nullopt;
    }

private:
    const Package& pk_;
    const ReadingProfile& p_;
    std::unique_ptr<PlaceSearch> tutti_;
    std::map<std::string, std::unique_ptr<PlaceSearch>> per_comune_;
};

}  // namespace

ListRead read_list(const Table& table, const ReadingProfile& profile, const Index& idx, int first_year, const ClientList* existing) {
    ListRead out;
    const Package& pk = idx.package();
    if (table.rows.empty()) {
        out.fault = TableFault::NoRows;
        return out;
    }
    // La riga dei titoli: la prima con un valore.
    const TableRow& titoli = table.rows.front();
    std::map<std::string, std::vector<int>> per_titolo;   // titolo normalizzato -> colonne
    for (const TableCell& c : titoli.cells)
        if (!c.unreadable) per_titolo[normalize_name(c.text)].push_back(c.column);
    Colonne col;
    const auto colonna = [&](const std::string& campo, const std::string& titolo, bool obbligatoria) -> bool {
        if (titolo.empty()) {
            if (obbligatoria) {
                out.fault = TableFault::MissingTitle;
                out.fault_title = campo;
                return false;
            }
            return true;
        }
        const auto it = per_titolo.find(normalize_name(titolo));
        if (it == per_titolo.end()) {
            out.fault = TableFault::MissingTitle;
            out.fault_title = titolo;
            return false;
        }
        if (it->second.size() > 1) {
            out.fault = TableFault::DuplicateTitle;
            out.fault_title = titolo;
            return false;
        }
        col.per_campo[campo] = it->second.front();
        col.titolo[campo] = titolo;
        return true;
    };
    const ReadingProfile::Columns& c = profile.columns;
    const std::vector<std::tuple<const char*, const std::string*, bool>> campi = {
        {"nome", &c.name, true},          {"scuola", &c.school, true},   {"stato", &c.status, true},
        {"percorsi", &c.routes, true},    {"fermata", &c.stop, false},   {"comune", &c.town, false},
        {"frazione", &c.hamlet, false},   {"via", &c.street, false},     {"civico", &c.number, false},
        {"classe", &c.class_number, false}, {"sezione", &c.section, false}};
    for (const auto& [campo, titolo, obbligatoria] : campi)
        if (!colonna(campo, *titolo, obbligatoria)) return out;
    for (const ProfileTimes& t : profile.times) {
        const auto it = per_titolo.find(normalize_name(t.column));
        if (it == per_titolo.end() || it->second.size() > 1) {
            out.fault = it == per_titolo.end() ? TableFault::MissingTitle : TableFault::DuplicateTitle;
            out.fault_title = t.column;
            return out;
        }
        col.orari.push_back({&t, it->second.front()});
    }

    Scuole scuole(pk, profile);
    Fermate fermate(pk, profile);
    std::map<std::string, int> esistenti;   // chiave -> quanti utenti dell'elenco del pacchetto
    if (existing)
        for (const ListUser& u : existing->users) ++esistenti[list_key(u.name, u.school.code)];

    struct Letta {
        int riga;
        std::string chiave;
        std::optional<ParsedUser> utente;
        std::vector<Problem> avvisi;   // fermata non riconosciuta
    };
    std::vector<Letta> lette;
    for (std::size_t r = 1; r < table.rows.size(); ++r) {
        const TableRow& row = table.rows[r];
        ++out.data_rows;
        std::map<int, const TableCell*> celle;
        for (const TableCell& x : row.cells) celle[x.column] = &x;
        Letta l{row.number, std::string(), std::nullopt, {}};
        std::optional<Problem> guasto;
        const auto ferma = [&](RowFault f, const std::string& campo, std::vector<Param> extra = {}) {
            if (guasto) return;
            const int colonna_n = col.per_campo.count(campo) ? col.per_campo.at(campo) : 0;
            std::vector<Param> param{text_param("colonna", col.titolo.count(campo) ? col.titolo.at(campo) : campo)};
            for (Param& x : extra) param.push_back(std::move(x));
            guasto = problema_riga(Check::ListRowUnreadable, static_cast<int>(f), row.number, colonna_n, std::move(param));
        };
        // Il testo di una colonna del profilo ("" se vuota o se la colonna non c'e'); nullopt se la cella e' illeggibile.
        const auto testo = [&](const std::string& campo) -> std::optional<std::string> {
            const auto it = col.per_campo.find(campo);
            if (it == col.per_campo.end()) return std::string();
            const auto x = celle.find(it->second);
            if (x == celle.end()) return std::string();
            if (x->second->unreadable) return std::nullopt;
            return clean_cell(x->second->text);
        };
        ParsedUser u;
        u.row = row.number;
        // Prima nome e scuola: sono la chiave, che serve anche se il resto della riga e' illeggibile.
        const std::optional<std::string> nome = testo("nome");
        const std::optional<std::string> scuola = testo("scuola");
        if (!nome) ferma(RowFault::UnreadableCell, "nome");
        else if (nome->empty()) ferma(RowFault::EmptyRequired, "nome");
        if (!scuola) {
            ferma(RowFault::UnreadableCell, "scuola");
        } else if (scuola->empty()) {
            ferma(RowFault::EmptyRequired, "scuola");
        } else {
            const std::set<Code>* trovate = scuole.trova(*scuola);
            if (!trovate) ferma(RowFault::SchoolUnknown, "scuola", {text_param("scuola", *scuola)});
            else if (trovate->size() > 1) ferma(RowFault::SchoolAmbiguous, "scuola", {text_param("scuola", *scuola)});
            else u.user.school = Ref{*trovate->begin(), {}};
        }
        if (nome && !nome->empty() && !u.user.school.code.empty()) {
            u.user.name = *nome;
            l.chiave = list_key(*nome, u.user.school.code);
            if (esistenti.count(l.chiave) && esistenti.at(l.chiave) > 1) ferma(RowFault::HomonymExisting, "nome");
        }
        // Lo stato e la nota.
        const std::optional<std::string> stato = testo("stato");
        if (!stato) ferma(RowFault::UnreadableCell, "stato");
        else if (stato->empty()) ferma(RowFault::EmptyRequired, "stato");
        else {
            const StatusParse s = parse_status(*stato, profile);
            if (!s.ok) ferma(RowFault::StatusUnknown, "stato");
            u.user.active = s.active;
            u.user.status_note = s.note;
        }
        // Casa, classe, sezione.
        ListUserHome casa;
        for (const auto& [campo, dove] : std::vector<std::pair<const char*, std::string*>>{
                 {"comune", &casa.town}, {"frazione", &casa.hamlet}, {"via", &casa.street}, {"civico", &casa.number}}) {
            const std::optional<std::string> t = testo(campo);
            if (!t) ferma(RowFault::UnreadableCell, campo);
            else *dove = *t;
        }
        if (!casa.town.empty() || !casa.hamlet.empty() || !casa.street.empty() || !casa.number.empty()) u.user.home = casa;
        if (const std::optional<std::string> cl = testo("classe"); !cl) {
            ferma(RowFault::UnreadableCell, "classe");
        } else if (!cl->empty()) {
            std::string n = *cl;
            if (n.size() > 2 && n.compare(n.size() - 2, 2, ".0") == 0) n.resize(n.size() - 2);   // un numero salvato come 3.0
            int v = 0;
            const bool buono = !n.empty() && n.size() <= 2 && std::all_of(n.begin(), n.end(), cifra);   // prima la lunghezza: niente trabocchi
            if (buono)
                for (char ch : n) v = v * 10 + (ch - '0');
            if (!buono || v > 20) ferma(RowFault::ClassNotNumber, "classe");
            else u.user.class_number = v;
        }
        if (const std::optional<std::string> sz = testo("sezione"); !sz) ferma(RowFault::UnreadableCell, "sezione");
        else u.user.section = *sz;
        // Percorsi.
        if (const std::optional<std::string> pc = testo("percorsi"); !pc) {
            ferma(RowFault::UnreadableCell, "percorsi");
        } else {
            RoutesParse rp = parse_routes(*pc, profile);
            if (rp.fault) ferma(*rp.fault, "percorsi");
            u.user.legs = std::move(rp.legs);
        }
        // Orari, colonna per colonna.
        for (const auto& [t, n] : col.orari) {
            const auto x = celle.find(n);
            if (x == celle.end()) continue;
            if (x->second->unreadable) {
                if (!guasto) {
                    guasto = problema_riga(Check::ListRowUnreadable, static_cast<int>(RowFault::UnreadableCell), row.number, n,
                                           {text_param("colonna", t->column)});
                }
                continue;
            }
            TimesParse tp = parse_times(x->second->text, *t, profile, first_year);
            if (tp.fault && !guasto)
                guasto = problema_riga(Check::ListRowUnreadable, static_cast<int>(*tp.fault), row.number, n, {text_param("colonna", t->column)});
            for (ListUserTime& v : tp.times) u.user.times.push_back(std::move(v));
        }
        // Fermata indicata.
        if (const std::optional<std::string> f = testo("fermata"); !f) {
            ferma(RowFault::UnreadableCell, "fermata");
        } else if (!f->empty()) {
            const auto trovata = fermate.trova(*f, u.user.home ? u.user.home->town : std::string());
            if (trovata) {
                u.user.stop = Ref{trovata->first, {}};
                u.user.stop_match = trovata->second;
            } else {
                u.user.stop_text = *f;
                l.avvisi.push_back(problema_riga(Check::ListStopNotRecognized, 0, row.number, col.per_campo.at("fermata"),
                                                 {text_param("colonna", col.titolo.at("fermata"))}));
            }
        }
        if (guasto) {
            out.problems.push_back(std::move(*guasto));
            out.unreadable_rows.insert(row.number);
            if (!l.chiave.empty()) out.unreadable_keys.insert(l.chiave);
        } else {
            l.utente = std::move(u);
        }
        lette.push_back(std::move(l));
    }
    // Righe con la stessa chiave (lette o no): si fermano tutte. Mai indovinare quale sia l'utente gia' nell'elenco.
    std::map<std::string, std::vector<int>> per_chiave;
    for (const Letta& l : lette)
        if (!l.chiave.empty()) per_chiave[l.chiave].push_back(l.riga);
    std::set<std::string> omonime;
    for (const auto& [chiave, righe] : per_chiave)
        if (righe.size() > 1) {
            omonime.insert(chiave);
            std::string elenco;
            for (int r : righe) elenco += (elenco.empty() ? "" : ", ") + std::to_string(r);
            const Code scuola = chiave.substr(chiave.find('\x1f') + 1);
            Problem p{Check::ListRowHomonyms, 0, Source{"", "", righe.front(), 0},
                      {text_param("righe", elenco), ref_param("scuola", Element::School, scuola), number_param("quante", static_cast<long long>(righe.size()))}};
            out.problems.push_back(std::move(p));
            out.unreadable_keys.insert(chiave);
            for (int r : righe) out.unreadable_rows.insert(r);
        }
    for (Letta& l : lette) {
        if (omonime.count(l.chiave)) continue;
        for (Problem& a : l.avvisi) out.problems.push_back(std::move(a));
        if (l.utente) out.users.push_back(std::move(*l.utente));
    }
    std::stable_sort(out.problems.begin(), out.problems.end(),
                     [](const Problem& a, const Problem& b) { return a.where.line < b.where.line; });
    return out;
}

std::set<Code> all_codes(const Package& p, bool with_list_users) {
    std::set<Code> s;
    const auto tutti = [&](const auto& v) {
        for (const auto& x : v) s.insert(x.code);
    };
    tutti(p.manifest.companies);
    tutti(p.norms);
    for (const Norms& n : p.norms) {
        tutti(n.licences);
        tutti(n.uses);
    }
    tutti(p.places);
    tutti(p.segments);
    tutti(p.regulations);
    tutti(p.holidays);
    tutti(p.calendars);
    tutti(p.schools);
    tutti(p.clients);
    tutti(p.contracts);
    tutti(p.lots);
    tutti(p.services);
    for (const Service& sv : p.services)
        for (const PassengerGroup& g : sv.groups) {
            s.insert(g.code);
            tutti(g.passengers);
        }
    tutti(p.trips);
    tutti(p.vehicles);
    tutti(p.depots);
    tutti(p.persons);
    tutti(p.shifts);
    tutti(p.rotations);
    tutti(p.fixed);
    tutti(p.blocks);
    tutti(p.reading_profiles);
    if (with_list_users)
        for (const ClientList& l : p.client_lists) tutti(l.users);
    return s;
}

namespace {

bool stessi_orari(const std::vector<ListUserTime>& a, const std::vector<ListUserTime>& b) {
    const auto chiave = [](const ListUserTime& t) {
        return std::make_tuple(t.leg, static_cast<int>(t.day), t.time, t.period, t.dates.from, t.dates.to);
    };
    std::vector<decltype(chiave(a.front()))> x, y;
    for (const auto& t : a) x.push_back(chiave(t));
    for (const auto& t : b) y.push_back(chiave(t));
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    return x == y;
}

bool stesse_tratte(const std::vector<ListUserLeg>& a, const std::vector<ListUserLeg>& b) {
    const auto chiave = [](const ListUserLeg& l) { return std::make_tuple(l.leg, l.company ? l.company->code : std::string(), l.other_company); };
    std::vector<std::tuple<std::string, std::string, std::string>> x, y;
    for (const auto& l : a) x.push_back(chiave(l));
    for (const auto& l : b) y.push_back(chiave(l));
    std::sort(x.begin(), x.end());
    std::sort(y.begin(), y.end());
    return x == y;
}

std::vector<std::string> campi_cambiati(const ListUser& vecchio, const ListUser& nuovo) {
    std::vector<std::string> c;
    if (vecchio.name != nuovo.name) c.push_back("nome");
    if (vecchio.home != nuovo.home) c.push_back("casa");
    if (vecchio.class_number != nuovo.class_number) c.push_back("classe");
    if (vecchio.section != nuovo.section) c.push_back("sezione");
    if (vecchio.active != nuovo.active) c.push_back("stato");
    if (vecchio.status_note != nuovo.status_note) c.push_back("nota");
    const auto fermata = [](const ListUser& u) {
        return std::make_tuple(u.stop ? u.stop->code : std::string(), u.stop ? static_cast<int>(u.stop_match) : -1, u.stop_text);
    };
    if (fermata(vecchio) != fermata(nuovo)) c.push_back("fermata");
    if (!stesse_tratte(vecchio.legs, nuovo.legs)) c.push_back("tratte");
    if (!stessi_orari(vecchio.times, nuovo.times)) c.push_back("orari");
    if (vecchio.out_of_list != nuovo.out_of_list) c.push_back("fuori_elenco");
    return c;
}

bool stessa_casa(const ListUser& a, const ListUser& b) {
    if (!a.home || !b.home) return false;
    const auto n = [](const std::string& s) { return normalize_name(s); };
    return n(a.home->town) == n(b.home->town) && n(a.home->hamlet) == n(b.home->hamlet) && n(a.home->street) == n(b.home->street) &&
           n(a.home->number) == n(b.home->number);
}

}  // namespace

ListMerge merge_list(const Package& p, const ReadingProfile& profile, int first_year, const ListRead& read, bool complete,
                     const std::string& fingerprint) {
    ListMerge out;
    out.complete = complete;
    out.exits_applied = complete && read.unreadable_rows.empty();
    const ClientList* prima = nullptr;
    for (const ClientList& l : p.client_lists)
        if (l.client.code == profile.client.code) prima = &l;
    out.list.client = Ref{profile.client.code, {}};
    out.list.profile = Ref{profile.code, {}};
    out.list.first_year = first_year;
    out.list.has_last_reading = true;
    out.list.last_fingerprint = fingerprint;
    out.list.last_complete = complete;

    std::map<std::string, const ListUser*> esistenti;   // chiave -> utente (solo le chiavi di un utente solo)
    std::map<std::string, int> quanti;
    if (prima)
        for (const ListUser& u : prima->users) ++quanti[list_key(u.name, u.school.code)];
    if (prima)
        for (const ListUser& u : prima->users)
            if (quanti[list_key(u.name, u.school.code)] == 1) esistenti[list_key(u.name, u.school.code)] = &u;

    std::set<Code> usati = all_codes(p, true);
    int numero = p.list_last_number;
    std::set<Code> trovati;   // utenti dell'elenco del pacchetto ritrovati nel file
    std::vector<ListUser> risultato;
    std::vector<std::size_t> indici_nuovi;
    for (const ParsedUser& r : read.users) {
        ListUser u = r.user;
        const auto it = esistenti.find(list_key(u.name, u.school.code));
        if (it != esistenti.end()) {
            const ListUser& v = *it->second;
            u.code = v.code;
            u.unlocks = v.unlocks;   // una lettura non cancella mai uno sblocco
            u.out_of_list = false;
            trovati.insert(v.code);
            std::vector<std::string> campi = campi_cambiati(v, u);
            out.changes.push_back({campi.empty() ? ListChange::Kind::Unchanged : ListChange::Kind::Changed, u.code, r.row, std::move(campi)});
        } else {
            // Un codice nuovo: il numero dopo l'ultimo dato, saltando quelli usati da qualunque elemento del pacchetto.
            do {
                ++numero;
            } while (usati.count(list_user_code(numero)));
            u.code = list_user_code(numero);
            usati.insert(u.code);
            out.changes.push_back({ListChange::Kind::New, u.code, r.row, {}});
            indici_nuovi.push_back(risultato.size());
        }
        risultato.push_back(std::move(u));
    }
    // Gli utenti dell'elenco del pacchetto che il file non ha.
    std::vector<const ListUser*> non_trovati;
    if (prima)
        for (const ListUser& v : prima->users) {
            if (trovati.count(v.code)) continue;
            non_trovati.push_back(&v);
            if (read.unreadable_keys.count(list_key(v.name, v.school.code))) {
                risultato.push_back(v);   // la sua riga e' illeggibile: resta com'e'
                out.changes.push_back({ListChange::Kind::Unread, v.code, 0, {}});
                continue;
            }
            if (!out.exits_applied) {
                risultato.push_back(v);
                out.changes.push_back({ListChange::Kind::NotInFile, v.code, 0, {}});
                continue;
            }
            if (!v.unlocks.empty()) {
                ListUser w = v;
                w.out_of_list = true;
                out.changes.push_back({ListChange::Kind::KeptForUnlock, v.code, 0, v.out_of_list ? std::vector<std::string>{} : std::vector<std::string>{"fuori_elenco"}});
                risultato.push_back(std::move(w));
                continue;
            }
            out.changes.push_back({ListChange::Kind::Exited, v.code, 0, {}});
        }
    // Forse lo stesso, scritto diverso: un utente che il file non ha e un nuovo con la stessa casa, nella stessa scuola (0) o con
    // lo stesso nome in un'altra scuola (1). Mai un'unione: lo dice e basta.
    for (const ListUser* v : non_trovati)
        for (std::size_t i : indici_nuovi) {
            const ListUser& n = risultato[i];
            if (!stessa_casa(*v, n)) continue;
            const bool stessa_scuola = v->school.code == n.school.code;
            const bool stesso_nome = normalize_name(v->name) == normalize_name(n.name);
            if (!stessa_scuola && !stesso_nome) continue;
            int riga = 0;
            for (const ListChange& c : out.changes)
                if (c.code == n.code) riga = c.row;
            out.warnings.push_back(Problem{Check::ListMaybeSameUser, stessa_scuola ? 0 : 1, Source{"", "", riga, 0},
                                           {number_param("riga", riga), ref_param("uscito", Element::ListUser, v->code),
                                            ref_param("nuovo", Element::ListUser, n.code), ref_param("scuola", Element::School, n.school.code),
                                            ref_param("scuola_prima", Element::School, v->school.code)}});
        }
    if (complete && !read.unreadable_rows.empty())
        out.warnings.push_back(Problem{Check::ListExitsNotApplied, 0, Source{},
                                       {number_param("righe", static_cast<long long>(read.unreadable_rows.size()))}});
    std::sort(risultato.begin(), risultato.end(), [](const ListUser& a, const ListUser& b) { return a.code < b.code; });
    std::sort(out.changes.begin(), out.changes.end(), [](const ListChange& a, const ListChange& b) { return a.code < b.code; });
    out.list.users = std::move(risultato);
    out.last_number = numero;
    return out;
}

// --- Controlli ----------------------------------------------------------------------------------------------------------

namespace {

class ControlliElenco {
public:
    ControlliElenco(const Package& p, const Index& idx, std::vector<Problem>& out) : p_(p), idx_(idx), out_(out) {}

    void run() {
        for (const ReadingProfile& pr : p_.reading_profiles) profilo(pr);
        std::map<Code, const Source*> committenti;
        std::map<Code, const Source*> codici;
        const std::set<Code> altri = all_codes(p_, false);
        for (const ClientList& l : p_.client_lists) {
            const auto [it, nuovo] = committenti.emplace(l.client.code, &l.src);
            if (!nuovo)
                add(Check::DuplicateCode, l.src, {text_param("codice", l.client.code), elements_param("elementi", Element::ClientList),
                                                  text_param("prima", it->second->file + " " + it->second->pointer)});
            elenco(l, codici, altri);
        }
        if (p_.manifest.level == Level::NumbersOnly)
            for (const ClientList& l : p_.client_lists)
                if (!l.users.empty())
                    add(Check::NumbersOnlyPersonalData, l.src,
                        {ref_param("committente", Element::Client, l.client.code), number_param("utenti", static_cast<long long>(l.users.size()))}, 10);
    }

private:
    void add(Check c, const Source& where, std::vector<Param> params, int variant = 0) {
        out_.push_back(Problem{c, variant, where, std::move(params)});
    }
    void ref(const Ref& r, Element e, bool esiste) {
        if (!esiste) add(Check::UnknownReference, r.src, {text_param("codice", r.code), elements_param("elementi", e)});
    }

    void profilo(const ReadingProfile& pr) {
        ref(pr.client, Element::Client, idx_.client(pr.client.code) != nullptr);
        for (const ProfileCompany& c : pr.companies) ref(c.company, Element::Company, idx_.company(c.company.code) != nullptr);
        const Param chi = ref_param("profilo", Element::ReadingProfile, pr.code);
        const auto doppie = [&](const std::vector<std::string>& v, int variante) {
            std::set<std::string> viste;
            for (const std::string& s : v)
                if (!viste.insert(normalize_name(s)).second) add(Check::ListProfileInvalid, pr.src, {chi, text_param("parola", s)}, variante);
        };
        std::vector<std::string> tratte, giorni, periodi, titoli, ditte;
        for (const ProfileLeg& l : pr.legs) tratte.push_back(l.code);
        for (const ProfileDay& d : pr.days) giorni.push_back(d.code);
        for (const ProfilePeriod& x : pr.periods) periodi.push_back(x.code);
        for (const ProfileCompany& c : pr.companies) ditte.push_back(c.text);
        const ReadingProfile::Columns& c = pr.columns;
        for (const std::string* t : {&c.name, &c.school, &c.status, &c.routes, &c.stop, &c.town, &c.hamlet, &c.street, &c.number,
                                     &c.class_number, &c.section})
            if (!t->empty()) titoli.push_back(*t);
        for (const ProfileTimes& t : pr.times) titoli.push_back(t.column);
        doppie(tratte, 0);
        doppie(giorni, 1);
        doppie(periodi, 2);
        doppie(titoli, 4);
        doppie(ditte, 6);
        std::set<int> giorni_visti;
        for (const ProfileDay& d : pr.days)
            if (!giorni_visti.insert(static_cast<int>(d.day)).second)
                add(Check::ListProfileInvalid, pr.src, {chi, text_param("parola", d.code)}, 1);
        for (const ProfilePeriod& x : pr.periods)
            if (!pr.leg(x.leg)) add(Check::ListProfileInvalid, pr.src, {chi, text_param("parola", x.code), text_param("tratta", x.leg)}, 3);
        for (const std::string& a : pr.active_words)
            for (const std::string& s : pr.suspended_words)
                if (normalize_name(a) == normalize_name(s)) add(Check::ListProfileInvalid, pr.src, {chi, text_param("parola", a)}, 5);
        // Una sigla dei periodi uguale a una sigla dei giorni non si potrebbe leggere (per_giorno).
        for (const std::string& x : periodi)
            for (const std::string& g : giorni)
                if (normalize_name(x) == normalize_name(g)) add(Check::ListProfileInvalid, pr.src, {chi, text_param("parola", x)}, 7);
    }

    void elenco(const ClientList& l, std::map<Code, const Source*>& codici, const std::set<Code>& altri) {
        ref(l.client, Element::Client, idx_.client(l.client.code) != nullptr);
        const ReadingProfile* pr = idx_.reading_profile(l.profile.code);
        ref(l.profile, Element::ReadingProfile, pr != nullptr);
        if (pr && pr->client.code != l.client.code)
            add(Check::ListInvalid, l.profile.src, {ref_param("profilo", Element::ReadingProfile, pr->code), ref_param("committente", Element::Client, l.client.code)}, 0);
        const DateRange anno = school_year(l.first_year, pr ? pr->start_month : 9);
        std::map<std::string, std::vector<const ListUser*>> per_chiave;
        for (const ListUser& u : l.users) {
            const auto [it, nuovo] = codici.emplace(u.code, &u.src);
            if (!nuovo)
                add(Check::DuplicateCode, u.src, {text_param("codice", u.code), elements_param("elementi", Element::ListUser),
                                                  text_param("prima", it->second->file + " " + it->second->pointer)});
            if (altri.count(u.code)) add(Check::ListUserCodeClash, u.src, {ref_param("utente", Element::ListUser, u.code)});
            if (!u.name.empty()) per_chiave[list_key(u.name, u.school.code)].push_back(&u);
            utente(u, pr, anno);
        }
        for (const auto& [chiave, utenti] : per_chiave)
            if (utenti.size() > 1)
                for (std::size_t i = 1; i < utenti.size(); ++i)
                    add(Check::ListHomonymsInList, utenti[i]->src,
                        {ref_param("utente", Element::ListUser, utenti[i]->code), ref_param("altro", Element::ListUser, utenti[0]->code),
                         ref_param("scuola", Element::School, utenti[i]->school.code)});
    }

    void utente(const ListUser& u, const ReadingProfile* pr, const DateRange& anno) {
        const Param chi = ref_param("utente", Element::ListUser, u.code);
        ref(u.school, Element::School, idx_.school(u.school.code) != nullptr);
        if (u.stop) {
            const Place* l = idx_.place(u.stop->code);
            ref(*u.stop, Element::Place, l != nullptr && !l->is_private);
        }
        std::set<std::string> tratte_utente;
        for (const ListUserLeg& t : u.legs) {
            if (t.company) ref(*t.company, Element::Company, idx_.company(t.company->code) != nullptr);
            if (pr && !pr->leg(t.leg)) add(Check::ListInvalid, t.src, {chi, text_param("tratta", t.leg)}, 1);
            if (!tratte_utente.insert(t.leg).second) add(Check::ListInvalid, t.src, {chi, text_param("tratta", t.leg)}, 3);
        }
        for (const ListUserTime& t : u.times) {
            if (pr && !pr->leg(t.leg)) add(Check::ListInvalid, t.src, {chi, text_param("tratta", t.leg)}, 1);
            if (pr && !t.period.empty()) {
                bool c = false;
                for (const ProfilePeriod& x : pr->periods) c = c || x.code == t.period;
                if (!c) add(Check::ListInvalid, t.src, {chi, text_param("periodo", t.period)}, 2);
            }
            if (t.dates.to < t.dates.from)
                add(Check::InvertedPeriod, t.src, {date_param("dal", t.dates.from), date_param("al", t.dates.to)});
        }
        for (const ListUserUnlock& s : u.unlocks) {
            if (pr && !pr->leg(s.leg)) add(Check::ListInvalid, s.src, {chi, text_param("tratta", s.leg)}, 1);
            if (u.active) add(Check::ListUnlockNotNeeded, s.src, {chi, text_param("tratta", s.leg)}, 0);
            else if (!tratte_utente.count(s.leg)) add(Check::ListUnlockNotNeeded, s.src, {chi, text_param("tratta", s.leg)}, 1);
        }
        if (u.out_of_list)
            add(Check::ListUserOutOfList, u.src, {chi, number_param("sblocchi", static_cast<long long>(u.unlocks.size()))}, u.unlocks.empty() ? 1 : 0);
        if (!pr) return;
        andata_senza_ritorno(u, *pr, chi);
        sovrapposizioni(u, chi);
        fuori_anno(u, anno, chi);
    }

    // Per ogni giorno della settimana: i giorni con l'andata e senza nessun ritorno (in tutto o in parte dell'anno).
    void andata_senza_ritorno(const ListUser& u, const ReadingProfile& pr, const Param& chi) {
        for (int g = 0; g < 7; ++g) {
            std::vector<DateRange> andate, ritorni;
            for (const ListUserTime& t : u.times) {
                if (static_cast<int>(t.day) != g || t.dates.to < t.dates.from) continue;
                const ProfileLeg* l = pr.leg(t.leg);
                if (!l) continue;
                (l->direction == Direction::Outbound ? andate : ritorni).push_back(t.dates);
            }
            if (andate.empty()) continue;
            // I giorni g coperti dall'andata e non dai ritorni: il primo pezzo (dal primo all'ultimo giorno scoperto di fila).
            std::vector<Day> scoperti;
            for (const DateRange& a : andate)
                for (Day d = a.from; d <= a.to; ++d) {
                    if (static_cast<int>(weekday_of(d)) != g) continue;
                    bool coperto = false;
                    for (const DateRange& r : ritorni) coperto = coperto || (d >= r.from && d <= r.to);
                    if (!coperto) scoperti.push_back(d);
                }
            if (scoperti.empty()) continue;
            std::sort(scoperti.begin(), scoperti.end());
            scoperti.erase(std::unique(scoperti.begin(), scoperti.end()), scoperti.end());
            add(Check::ListOutboundWithoutReturn, u.src,
                {chi, text_param("giorno", nome_giorno(g)), date_param("dal", scoperti.front()), date_param("al", scoperti.back()),
                 number_param("giorni", static_cast<long long>(scoperti.size()))});
        }
    }

    void sovrapposizioni(const ListUser& u, const Param& chi) {
        for (std::size_t i = 0; i < u.times.size(); ++i)
            for (std::size_t j = i + 1; j < u.times.size(); ++j) {
                const ListUserTime& a = u.times[i];
                const ListUserTime& b = u.times[j];
                if (a.leg != b.leg || a.day != b.day || a.time == b.time) continue;
                if (a.dates.to < b.dates.from || b.dates.to < a.dates.from) continue;
                add(Check::ListTimesOverlap, b.src,
                    {chi, text_param("tratta", a.leg), text_param("giorno", nome_giorno(static_cast<int>(a.day))), time_param("ora", a.time),
                     time_param("altra_ora", b.time), date_param("dal", std::max(a.dates.from, b.dates.from)),
                     date_param("al", std::min(a.dates.to, b.dates.to))});
            }
    }

    // Date scritte nel file (non "tutto l'anno") fuori dai periodi degli orari della scuola.
    void fuori_anno(const ListUser& u, const DateRange& anno, const Param& chi) {
        const School* s = idx_.school(u.school.code);
        if (!s || s->timetables.empty()) return;
        Day dal = s->timetables.front().period.from, al = s->timetables.front().period.to;
        for (const Timetable& t : s->timetables) {
            dal = std::min(dal, t.period.from);
            al = std::max(al, t.period.to);
        }
        for (const ListUserTime& t : u.times) {
            if (t.dates.from == anno.from && t.dates.to == anno.to) continue;
            if (t.dates.from < dal || t.dates.to > al)
                add(Check::ListDateOutsideSchoolYear, t.src,
                    {chi, date_param("dal", t.dates.from), date_param("al", t.dates.to), ref_param("scuola", Element::School, s->code),
                     date_param("scuola_dal", dal), date_param("scuola_al", al)});
        }
    }

    static std::string nome_giorno(int g) {
        static const char* nomi[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
        return nomi[g];
    }

    const Package& p_;
    const Index& idx_;
    std::vector<Problem>& out_;
};

}  // namespace

void check_lists(const Package& p, const Index& idx, std::vector<Problem>& out) { ControlliElenco(p, idx, out).run(); }

}  // namespace kzero
