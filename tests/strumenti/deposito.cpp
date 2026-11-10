// K-Zero - controllo del deposito pubblico (K-015). Vedi deposito.hpp.
#include "deposito.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>

#include <sodium.h>
#include <utf8proc.h>

namespace kzero::deposito {

namespace {

namespace fs = std::filesystem;

Impronta da_esadecimale(std::string_view esa) {
    Impronta i{};
    std::size_t lunghezza = 0;
    if (esa.size() != 2 * i.size() ||
        sodium_hex2bin(i.data(), i.size(), esa.data(), esa.size(), nullptr, &lunghezza, nullptr) != 0 ||
        lunghezza != i.size()) {
        std::fprintf(stderr, "impronta scritta male nella tabella dei nomi\n");
        std::abort();
    }
    return i;
}

// Un carattere letto dal testo: il codice Unicode (o -1 per un byte che non e' UTF-8) e i byte che occupa.
struct Carattere {
    std::int32_t codice = -1;
    std::size_t byte = 1;
};

Carattere leggi(std::string_view t, std::size_t i) {
    utf8proc_int32_t c = -1;
    const utf8proc_ssize_t n = utf8proc_iterate(reinterpret_cast<const utf8proc_uint8_t*>(t.data() + i),
                                                static_cast<utf8proc_ssize_t>(t.size() - i), &c);
    if (n <= 0) return {};
    return {c, static_cast<std::size_t>(n)};
}

// Lettere, segni (accenti) e cifre: i caratteri di una parola.
bool di_parola(std::int32_t c) {
    if (c < 0) return false;
    switch (utf8proc_category(c)) {
        case UTF8PROC_CATEGORY_LU: case UTF8PROC_CATEGORY_LL: case UTF8PROC_CATEGORY_LT: case UTF8PROC_CATEGORY_LM:
        case UTF8PROC_CATEGORY_LO: case UTF8PROC_CATEGORY_MN: case UTF8PROC_CATEGORY_MC: case UTF8PROC_CATEGORY_ME:
        case UTF8PROC_CATEGORY_ND: case UTF8PROC_CATEGORY_NL: case UTF8PROC_CATEGORY_NO:
            return true;
        default:
            return false;
    }
}

// Tra le due parole di un nome: spazi (anche a capo) e segni di commento o di citazione.
bool di_separazione(std::int32_t c) {
    if (c < 0) return false;
    if (c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r') return true;
    if (c == '/' || c == '*' || c == '#' || c == '-' || c == '>' || c == '"' || c == '\'') return true;
    const utf8proc_category_t k = utf8proc_category(c);
    return k == UTF8PROC_CATEGORY_ZS || k == UTF8PROC_CATEGORY_ZL || k == UTF8PROC_CATEGORY_ZP;
}

struct Parola {
    std::string minuscola;
    std::size_t inizio = 0;
    std::size_t fine = 0;
    std::size_t riga = 1;
};

std::vector<Parola> parole_di(std::string_view t) {
    std::vector<Parola> parole;
    std::size_t riga = 1;
    std::size_t i = 0;
    while (i < t.size()) {
        const Carattere c = leggi(t, i);
        if (!di_parola(c.codice)) {
            if (t[i] == '\n') ++riga;
            i += c.byte;
            continue;
        }
        Parola p;
        p.inizio = i;
        p.riga = riga;
        while (i < t.size()) {
            const Carattere d = leggi(t, i);
            if (!di_parola(d.codice)) break;
            utf8proc_uint8_t byte[4];
            const utf8proc_ssize_t n = utf8proc_encode_char(utf8proc_tolower(d.codice), byte);
            p.minuscola.append(reinterpret_cast<const char*>(byte), static_cast<std::size_t>(n));
            i += d.byte;
        }
        p.fine = i;
        parole.push_back(std::move(p));
    }
    return parole;
}

bool solo_separazione(std::string_view t) {
    if (t.empty()) return false;
    for (std::size_t i = 0; i < t.size();) {
        const Carattere c = leggi(t, i);
        if (!di_separazione(c.codice)) return false;
        i += c.byte;
    }
    return true;
}

bool utf8_valido(std::string_view t) {
    for (std::size_t i = 0; i < t.size();) {
        const Carattere c = leggi(t, i);
        if (c.codice < 0) return false;
        i += c.byte;
    }
    return true;
}

std::string minuscolo_ascii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::string messaggio_nome(const Nome& n) {
    return "c'e' il nome " + std::string(1, n.etichetta) + " (" + n.chi + "): al suo posto si scrive " + n.sostituto;
}

// Formati dei dati che non stanno fuori da tests/ ed esempi/ (stessi di .gitignore).
const std::vector<std::string>& formati_dei_dati() {
    static const std::vector<std::string> f = {".xlsx", ".xls", ".xlsm", ".ods", ".csv", ".pdf", ".docx", ".doc",
                                               ".kzc", ".kzr"};
    return f;
}

const char* const k_regola_file =
    "fuori da tests/ ed esempi/: i dati veri restano fuori dalla cartella del deposito (CLAUDE.md, Confini)";

void controlla_file(const std::string& relativo, Resoconto& r) {
    const std::size_t barra = relativo.find('/');
    const std::string cima = barra == std::string::npos ? std::string() : relativo.substr(0, barra);
    if (cima == "tests" || cima == "esempi") return;
    const std::string nome = minuscolo_ascii(relativo.substr(relativo.rfind('/') == std::string::npos
                                                                 ? 0
                                                                 : relativo.rfind('/') + 1));
    if (nome == "pacchetto.json") {
        r.problemi.push_back({relativo, 0, std::string("manifesto di un pacchetto (pacchetto.json) ") + k_regola_file});
        return;
    }
    const std::size_t punto = nome.rfind('.');
    if (punto == std::string::npos) return;
    const std::string estensione = nome.substr(punto);
    const auto& f = formati_dei_dati();
    if (std::find(f.begin(), f.end(), estensione) != f.end())
        r.problemi.push_back({relativo, 0, "file di un formato di dati (" + estensione + ") " + k_regola_file});
}

// Un nome corrisponde a un modello in cui "*" vale per qualsiasi fila di caratteri, anche vuota (come in .gitignore).
bool corrisponde(std::string_view modello, std::string_view nome) {
    if (modello.empty()) return nome.empty();
    if (modello.front() == '*') {
        for (std::size_t i = 0; i <= nome.size(); ++i)
            if (corrisponde(modello.substr(1), nome.substr(i))) return true;
        return false;
    }
    return !nome.empty() && modello.front() == nome.front() && corrisponde(modello.substr(1), nome.substr(1));
}

std::string_view senza_spazi_in_fondo(std::string_view riga) {
    while (!riga.empty() && (riga.back() == ' ' || riga.back() == '\t' || riga.back() == '\r')) riga.remove_suffix(1);
    return riga;
}

bool escluso(const fs::path& p, const std::vector<fs::path>& escluse) {
    std::error_code e;
    for (const fs::path& x : escluse)
        if (fs::equivalent(p, x, e)) return true;
    return false;
}

}  // namespace

Impronta impronta(std::string_view testo) {
    static const bool pronto = sodium_init() >= 0;
    if (!pronto) {
        std::fprintf(stderr, "libsodium non si inizializza\n");
        std::abort();
    }
    Impronta i{};
    crypto_hash_sha256(i.data(), reinterpret_cast<const unsigned char*>(testo.data()), testo.size());
    return i;
}

const std::vector<std::string>& cartelle_saltate() {
    static const std::vector<std::string> c = {"build", "build-*"};
    return c;
}

bool cartella_saltata(std::string_view nome) {
    for (const std::string& modello : cartelle_saltate())
        if (corrisponde(modello, nome)) return true;
    return false;
}

std::vector<std::string> righe_delle_cartelle(std::string_view gitignore) {
    std::vector<std::string> righe;
    while (!gitignore.empty()) {
        const std::size_t a_capo = gitignore.find('\n');
        const std::string_view riga = senza_spazi_in_fondo(gitignore.substr(0, a_capo));
        gitignore.remove_prefix(a_capo == std::string_view::npos ? gitignore.size() : a_capo + 1);
        if (riga.empty() || riga.front() == '#' || riga.front() == '!') continue;
        if (riga.size() > 2 && riga.substr(0, 2) == "*.") continue;
        righe.emplace_back(riga);
    }
    return righe;
}

const std::vector<Nome>& nomi_del_deposito() {
    // Impronte SHA-256 dei nomi in minuscolo; per d le due parole con uno spazio, e anche attaccate (es. in un indirizzo).
    static const std::vector<Nome> nomi = {
        {'a', "l'azienda", "ALP", da_esadecimale("d9d7dde486f70fd99211f3660e4530c6f56b03f12fbae08a16ea845698522020"), 1},
        {'b', "il supervisore", "\"il supervisore\"",
         da_esadecimale("19e37c4dfe9a2e1570be1f4cbafca76f3ed757231c0865e02465c909d916b1d1"), 1},
        {'c', "la sigla del consorzio", "\"il consorzio\" (nei codici esterni il sistema \"Consorzio\")",
         da_esadecimale("061ab7e7d3b73063bc35ef643b565e0f3ec4e97a012bac0d9379e0af007106e8"), 1},
        {'d', "l'azienda del trasporto pubblico provinciale",
         "\"l'azienda del trasporto pubblico provinciale\" (nei codici esterni il sistema \"Trasporti Provinciali\")",
         da_esadecimale("282f73f6ebf38bb3ba34e4fc5aaf5cf63353ada7a7c8fe20772a67f5e618b6b2"), 2},
        {'d', "l'azienda del trasporto pubblico provinciale",
         "\"l'azienda del trasporto pubblico provinciale\" (nei codici esterni il sistema \"Trasporti Provinciali\")",
         da_esadecimale("dffd6cbe419c63cb0ef9a317837e0009b4e543846f9faad099aec6d59d49b20e"), 1},
    };
    return nomi;
}

std::vector<Problema> cerca_nomi(std::string_view testo, std::string_view dove, const std::vector<Nome>& nomi) {
    std::vector<Problema> trovati;
    const std::vector<Parola> parole = parole_di(testo);
    bool due = false;
    for (const Nome& n : nomi) due = due || n.parole == 2;
    for (std::size_t i = 0; i < parole.size(); ++i) {
        const Impronta una = impronta(parole[i].minuscola);
        for (const Nome& n : nomi)
            if (n.parole == 1 && n.impronta == una)
                trovati.push_back({std::string(dove), parole[i].riga, messaggio_nome(n)});
        if (!due || i + 1 >= parole.size()) continue;
        if (!solo_separazione(testo.substr(parole[i].fine, parole[i + 1].inizio - parole[i].fine))) continue;
        const Impronta coppia = impronta(parole[i].minuscola + " " + parole[i + 1].minuscola);
        for (const Nome& n : nomi)
            if (n.parole == 2 && n.impronta == coppia)
                trovati.push_back({std::string(dove), parole[i].riga, messaggio_nome(n)});
    }
    return trovati;
}

Resoconto controlla_cartella(const fs::path& radice, const std::vector<fs::path>& escluse,
                             const std::vector<Nome>& nomi) {
    Resoconto r;
    std::vector<std::pair<std::string, fs::path>> file;
    std::vector<std::string> collegamenti;
    fs::recursive_directory_iterator it(radice, fs::directory_options::none);
    for (const fs::recursive_directory_iterator fine; it != fine; ++it) {
        const fs::path& p = it->path();
        const std::string relativo = p.lexically_relative(radice).generic_string();
        const std::string nome = p.filename().string();
        if (it->is_symlink()) {
            collegamenti.push_back(relativo);
            continue;
        }
        if (it->is_directory()) {
            if (nome == ".git" || cartella_saltata(nome) || escluso(p, escluse))
                it.disable_recursion_pending();
            continue;
        }
        if (nome == ".git") continue;   // un deposito collegato (worktree, sottomodulo)
        if (it->is_regular_file()) file.emplace_back(relativo, p);
    }
    std::sort(file.begin(), file.end());
    std::sort(collegamenti.begin(), collegamenti.end());
    for (const std::string& c : collegamenti) {
        for (Problema& q : cerca_nomi(c, c, nomi)) r.problemi.push_back({q.dove, 0, "nel percorso " + q.cosa});
        controlla_file(c, r);
    }
    r.collegamenti = collegamenti;
    for (const auto& [relativo, percorso] : file) {
        for (Problema& q : cerca_nomi(relativo, relativo, nomi))
            r.problemi.push_back({q.dove, 0, "nel percorso " + q.cosa});
        controlla_file(relativo, r);
        std::ifstream in(percorso, std::ios::binary);
        if (!in) {
            r.problemi.push_back({relativo, 0, "file che non si riesce a leggere"});
            continue;
        }
        const std::string contenuto((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (contenuto.find('\0') != std::string::npos && !utf8_valido(contenuto)) {
            r.binari.push_back(relativo);
            continue;
        }
        ++r.letti;
        for (Problema& q : cerca_nomi(contenuto, relativo, nomi)) r.problemi.push_back(std::move(q));
    }
    std::stable_sort(r.problemi.begin(), r.problemi.end(), [](const Problema& x, const Problema& y) {
        return x.dove != y.dove ? x.dove < y.dove : x.riga < y.riga;
    });
    return r;
}

int esegui(const std::vector<std::string>& argomenti, std::istream& ingresso, const std::vector<Nome>& nomi,
           std::ostream& uscita) {
    const char* const uso =
        "Uso: controlla_deposito --cartella <cartella> [--escludi <cartella>]...\n"
        "     controlla_deposito --testo <dove>   (il testo dall'ingresso standard)\n";
    std::string cartella, testo;
    std::vector<fs::path> escluse;
    for (std::size_t i = 0; i < argomenti.size(); ++i) {
        const std::string& a = argomenti[i];
        if (i + 1 >= argomenti.size() || (a != "--cartella" && a != "--escludi" && a != "--testo")) {
            uscita << uso;
            return 2;
        }
        const std::string& v = argomenti[++i];
        if (a == "--cartella") cartella = v;
        if (a == "--escludi") escluse.emplace_back(v);
        if (a == "--testo") testo = v;
    }
    if (cartella.empty() == testo.empty()) {
        uscita << uso;
        return 2;
    }
    auto scrivi = [&uscita](const Problema& q) {
        uscita << q.dove;
        if (q.riga > 0) uscita << ":" << q.riga;
        uscita << ": " << q.cosa << "\n";
    };
    if (!testo.empty()) {
        const std::string t((std::istreambuf_iterator<char>(ingresso)), std::istreambuf_iterator<char>());
        std::vector<Problema> problemi = cerca_nomi(t, testo, nomi);
        for (const Problema& q : problemi) scrivi(q);
        uscita << testo << ": ";
        if (problemi.empty()) uscita << "nessun nome\n";
        else uscita << problemi.size() << (problemi.size() == 1 ? " nome da togliere\n" : " nomi da togliere\n");
        return problemi.empty() ? 0 : 1;
    }
    std::error_code e;
    if (!fs::is_directory(cartella, e)) {
        uscita << "La cartella " << cartella << " non c'e'.\n";
        return 3;
    }
    Resoconto r;
    try {
        r = controlla_cartella(cartella, escluse, nomi);
    } catch (const fs::filesystem_error& x) {
        uscita << "Errore nel leggere la cartella: " << x.what() << "\n";
        return 3;
    }
    for (const Problema& q : r.problemi) scrivi(q);
    uscita << "Deposito controllato: " << r.letti << " file letti come testo";
    if (!r.binari.empty()) {
        uscita << "; " << r.binari.size() << " binari, di cui si controlla solo il percorso:";
        for (const std::string& b : r.binari) uscita << " " << b;
    }
    if (!r.collegamenti.empty()) {
        uscita << "; " << r.collegamenti.size() << " collegamenti non seguiti:";
        for (const std::string& c : r.collegamenti) uscita << " " << c;
    }
    uscita << ".\n";
    if (r.problemi.empty()) {
        uscita << "Nessun nome e nessun file di dati.\n";
        return 0;
    }
    uscita << r.problemi.size() << (r.problemi.size() == 1 ? " problema: va tolto" : " problemi: vanno tolti")
           << " prima di salvare (CLAUDE.md, Confini).\n";
    return 1;
}

}  // namespace kzero::deposito
