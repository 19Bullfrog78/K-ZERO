// K-Zero API - i punti degli utenti dell'elenco (K-027): kzero proponi-punti, assegna-punto, conferma-punti, non-frequenta e
// confronta-schede. Uscite: schemi "proposte_punti", "punti" e "confronto_schede".
//
// K-Zero propone, una persona conferma: proponi-punti non scrive niente; assegna-punto (un punto scritto a mano) e conferma-punti (le
// righe pronte della proposta, una per punto) scrivono solo con --scrivi, con la scrittura e i permessi dell'elenco (elenco_scrittura.hpp).
// I valori (scheda, passaggio, giorni, consenso, motivo, nota) arrivano dall'ingresso standard, mai dagli argomenti; --riferimento e --data
// sono obbligatori. Le regole stanno nel nucleo (punti.hpp): il punto punta al passaggio, i giorni prima della voce nuova non cambiano mai,
// la scheda e l'elenco non si correggono mai da soli.
#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#define KZERO_ISATTY 1
#endif

#include "accesso.hpp"
#include "campi_ingresso.hpp"
#include "chiusura.hpp"
#include "controlli_catalogo.hpp"
#include "elenco.hpp"
#include "elenco_scrittura.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "punti.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

namespace fs = std::filesystem;
using elenco::kFile;

const char* nome_giorno(kzero::Weekday g) {
    static const char* nomi[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
    return nomi[static_cast<int>(g)];
}
std::string testo_giorni(bool sempre, const kzero::Weekdays& g) {
    if (sempre) return "sempre";
    std::string t;
    for (int i = 0; i < 7; ++i)
        if (g.has(static_cast<kzero::Weekday>(i))) t += (t.empty() ? "" : ", ") + std::string(nome_giorno(static_cast<kzero::Weekday>(i)));
    return t;
}
Json giorni_json(bool sempre, const kzero::Weekdays& g) {
    if (sempre) return Json(nullptr);
    Json v = Json::array();
    for (int i = 0; i < 7; ++i)
        if (g.has(static_cast<kzero::Weekday>(i))) v.push_back(nome_giorno(static_cast<kzero::Weekday>(i)));
    return v;
}
const char* nome_fonte(kzero::PointSource f) {
    switch (f) {
        case kzero::PointSource::IndicatedStop: return "fermata_indicata";
        case kzero::PointSource::StreetTable: return "stradario";
        case kzero::PointSource::None: break;
    }
    return "nessuna";
}
const char* testo_fonte(kzero::PointSource f) {
    switch (f) {
        case kzero::PointSource::IndicatedStop: return "dalla fermata indicata nell'elenco";
        case kzero::PointSource::StreetTable: return "dallo stradario dell'azienda";
        case kzero::PointSource::None: break;
    }
    return "senza fonte";
}
const char* nome_motivo(kzero::NotAttendingReason m) {
    static const char* nomi[] = {"famiglia", "altro_mezzo", "assente", "altro"};
    return nomi[static_cast<int>(m)];
}

// --- I valori dall'ingresso -----------------------------------------------------------------------------------------------

std::vector<std::string> righe_dall_ingresso(const char* aiuto) {
    std::function<int()> byte = accesso::ambiente().valori;
    if (!byte) {
#if defined(KZERO_ISATTY)
        if (::isatty(0)) std::fputs(aiuto, stderr);
#else
        (void)aiuto;
#endif
        byte = campi::ingresso_standard();
    }
    const campi::Righe r = campi::leggi(byte);
    if (r.motivo != campi::Motivo::Nessuno)
        throw ErroreKZero(Errore::CampiScrittiMale, "Valori dall'ingresso, riga " + std::to_string(r.riga) + ": " + campi::spiegazione(r.motivo) +
                                                        ". Niente e' cambiato.");
    return r.righe;
}

[[noreturn]] void errore_righe(const kzero::PointLines& x) {
    const std::string riga = x.line > 0 ? "riga " + std::to_string(x.line) + ": " : "";
    std::string m;
    switch (x.fault) {
        case kzero::PointLinesFault::UnknownField: m = "la riga non e' tra quelle che il comando legge"; break;
        case kzero::PointLinesFault::RepeatedField: m = "lo stesso valore e' scritto due volte"; break;
        case kzero::PointLinesFault::Syntax:
            m = "il punto non si legge: si scrive \"<scheda> / <luogo> [/ <volta>] [/ <giorni>]\" (la scheda con il codice del registro o "
                "l'ID, il luogo con il suo codice)";
            break;
        case kzero::PointLinesFault::Day: m = "un giorno della settimana non esiste (lunedi, martedi, mercoledi, giovedi, venerdi, sabato, domenica)"; break;
        case kzero::PointLinesFault::Number: m = "la volta non e' un numero da 1 a 99"; break;
        case kzero::PointLinesFault::Reason: m = "il motivo non e' uno di quelli fissi (famiglia, altro_mezzo, assente, altro; chi non c'e' piu' si toglie dall'elenco: kzero togli-da-elenco)"; break;
        case kzero::PointLinesFault::Missing: m = "manca il valore che serve (un punto, o \"nessuno\"; il motivo; chi l'ha detto)"; break;
        case kzero::PointLinesFault::NobodyWithPoints: m = "\"nessuno\" si scrive da solo, senza punti ne' consenso"; break;
        case kzero::PointLinesFault::None: break;
    }
    throw ErroreKZero(Errore::CampiScrittiMale, "Valori dall'ingresso, " + riga + m + ". Niente e' cambiato.");
}

[[noreturn]] void non_ammesso(const std::string& m) {
    throw ErroreKZero(Errore::PuntoNonAmmesso, "Punto non scritto: " + m + ". Niente e' cambiato.");
}

// I nomi, come si mostrano: con --includi-dati-personali (entro i permessi); di chi e' legato anche con --includi-dati-particolari.
class Nomi {
public:
    Nomi(const Lettura& l, const Visibilita& v) : idx_(l.pacchetto), legame_(&idx_), v_(v) {
        for (const kzero::ClientList& x : l.pacchetto.client_lists)
            for (const kzero::ListUser& u : x.users) nomi_[u.code] = u.name;
    }
    std::optional<std::string> di(const kzero::Code& c) const {
        if (!v_.personali) return std::nullopt;
        if (legame_.utente(c) && !v_.particolari) return std::nullopt;
        const auto it = nomi_.find(c);
        if (it == nomi_.end() || it->second.empty()) return std::nullopt;
        return it->second;
    }

private:
    kzero::Index idx_;
    LegameSensibile legame_;
    Visibilita v_;
    std::map<kzero::Code, std::string> nomi_;
};

// --- L'utente e la tratta ---------------------------------------------------------------------------------------------------

struct Chi {
    const kzero::ClientList* lista = nullptr;
    const kzero::ListUser* utente = nullptr;
    const kzero::ListUserLeg* tratta = nullptr;   // con la tratta: una tratta della nostra azienda nei percorsi dell'utente
    const kzero::ProfileLeg* profilo = nullptr;
};
Chi trova(const kzero::Package& p, const kzero::Index& idx, const kzero::Code& codice, const std::string& tratta, Errore e) {
    Chi c;
    for (const kzero::ClientList& l : p.client_lists)
        for (const kzero::ListUser& u : l.users)
            if (u.code == codice) {
                c.lista = &l;
                c.utente = &u;
            }
    if (!c.utente) throw ErroreKZero(Errore::UtenteElencoNonTrovato, "L'utente '" + codice + "' non c'e' nell'elenco utenti del pacchetto.");
    const auto no = [&](const std::string& m) { throw ErroreKZero(e, (e == Errore::PuntoNonAmmesso ? "Punto non scritto: " : "Periodo non scritto: ") + m + ". Niente e' cambiato."); };
    if (!c.utente->in_plan())
        no("l'utente " + codice + " non e' nel piano (fuori elenco o tolto a mano)");
    if (tratta.empty()) return c;
    const kzero::ReadingProfile* pr = idx.reading_profile(c.lista->profile.code);
    c.profilo = pr ? pr->leg(tratta) : nullptr;
    if (!c.profilo) no("la tratta '" + tratta + "' non e' una tratta del profilo dell'elenco");
    for (const kzero::ListUserLeg& l : c.utente->legs)
        if (l.leg == tratta) c.tratta = &l;
    if (!c.tratta) no("il committente non da' all'utente " + codice + " la tratta '" + tratta + "' (percorsi del file)");
    if (!c.tratta->company) no("la tratta '" + tratta + "' dell'utente " + codice + " e' di un'altra ditta, non di un'azienda del pacchetto");
    return c;
}

const kzero::ServiceCard* scheda_da(const kzero::Package& p, const std::string& testo) {
    for (const kzero::ServiceCard& s : p.service_cards)
        if (s.code == testo || s.id == testo) return &s;
    return nullptr;
}

// Una regola scritta: la scheda, il passaggio (luogo e volta) nella versione in vigore il giorno dal, la scuola dell'utente raggiunta.
kzero::PointRule regola_da(const kzero::PointLine& x, const kzero::Package& p, const kzero::Index& idx, const Chi& chi, kzero::Day dal) {
    const kzero::ServiceCard* s = scheda_da(p, x.card);
    if (!s) non_ammesso("la scheda '" + x.card + "' non c'e' nel registro delle schede (codice o ID)");
    if (s->client.code != chi.lista->client.code)
        non_ammesso("la scheda " + s->code + " e' del committente " + s->client.code + ", l'elenco del committente " + chi.lista->client.code);
    const kzero::Contract* ct = idx.contract(s->contract.code);
    if (!ct || ct->company.code != chi.tratta->company->code)
        non_ammesso("la scheda " + s->code + " e' di un contratto di un'altra azienda: la tratta " + chi.tratta->leg + " e' dell'azienda " +
                    chi.tratta->company->code);
    const kzero::CardVersion* v = kzero::card_version_on(*s, dal);
    if (!v) non_ammesso("la scheda " + s->code + " non ha una versione in vigore il " + kzero::text_from_day(dal));
    if (v->card.direction != chi.profilo->direction)
        non_ammesso("la scheda " + s->code + " va nel verso opposto a quello della tratta " + chi.tratta->leg);
    const int volte = kzero::card_place_count(v->card, x.place);
    if (volte == 0) non_ammesso("la versione " + std::to_string(v->number) + " della scheda " + s->code + " non passa dal luogo " + x.place);
    int volta = 1;
    if (x.occurrence) volta = *x.occurrence;
    else if (volte > 1)
        non_ammesso("la scheda " + s->code + " passa " + std::to_string(volte) + " volte dal luogo " + x.place +
                    ": scrivere quale volta (es. \"" + s->code + " / " + x.place + " / 2\"); il punto e' il passaggio, non il luogo");
    if (volta > volte)
        non_ammesso("la scheda " + s->code + " passa " + std::to_string(volte) + (volte == 1 ? " volta" : " volte") + " dal luogo " + x.place +
                    ": la volta " + std::to_string(volta) + " non c'e'");
    const std::size_t riga = *kzero::card_passage(v->card, x.place, volta);
    if (!kzero::point_school_row(v->card, riga, chi.utente->school.code, idx).row)
        non_ammesso("la scheda " + s->code + " non porta alla scuola " + chi.utente->school.code + " dell'utente dopo quel passaggio (al ritorno: "
                    "non parte dalla sua scuola prima)");
    kzero::PointRule r;
    r.all_days = x.all_days;
    r.days = x.days;
    r.card_id = s->id;
    r.place = kzero::Ref{x.place, {}};
    r.occurrence = volta;
    r.version = v->number;
    return r;
}

bool giorni_in_comune(const kzero::PointRule& a, const kzero::PointRule& b) {
    for (int i = 0; i < 7; ++i)
        if (a.covers(static_cast<kzero::Weekday>(i)) && b.covers(static_cast<kzero::Weekday>(i))) return true;
    return false;
}

// --- Il calcolo, sulla lettura in chiaro della cassaforte aperta ------------------------------------------------------------

struct VoceScritta {
    kzero::Code utente;
    std::string tratta;
    kzero::PointEntry voce;
    std::vector<kzero::Day> sostituite;
};
struct Calcolo {
    Lettura vecchia, nuova;
    std::string committente;
    std::vector<VoceScritta> voci;
    std::optional<kzero::NotAttending> non_frequenta;   // il periodo scritto, chiuso o tolto
    kzero::Code utente_non_frequenta;
    std::string nota;
};

enum class Azione { Assegna, Conferma, NonFrequenta, NonFrequentaChiudi, NonFrequentaTogli };
const char* nome_azione(Azione a) {
    switch (a) {
        case Azione::Assegna: return "assegnato";
        case Azione::Conferma: return "confermato";
        case Azione::NonFrequenta: return "non_frequenta";
        case Azione::NonFrequentaChiudi: return "non_frequenta_chiuso";
        case Azione::NonFrequentaTogli: return "non_frequenta_tolto";
    }
    return "?";
}

// Scrive le voci nella copia dell'elenco del committente (una sola lista: le righe di un comando sono di un committente solo).
void scrivi_voci(kzero::ClientList& lista, std::vector<VoceScritta>& voci) {
    for (VoceScritta& v : voci)
        for (kzero::ListUser& u : lista.users) {
            if (u.code != v.utente) continue;
            kzero::UserPoints* up = nullptr;
            for (kzero::UserPoints& x : u.points)
                if (x.leg == v.tratta) up = &x;
            if (!up) {
                u.points.push_back(kzero::UserPoints{v.tratta, {}, {}});
                up = &u.points.back();
            }
            v.sostituite = kzero::add_point_entry(*up, v.voce);
        }
}

Calcolo calcola_assegna(const Lettura& vecchia, const Parametri& par, const std::vector<std::string>& righe, const std::string& cartella) {
    elenco::pacchetto_completo(vecchia, cartella, par.utente);
    const kzero::PointLines in = kzero::parse_point_lines(righe, kzero::PointLinesMode::Assign);
    if (in.fault != kzero::PointLinesFault::None) errore_righe(in);
    const kzero::Index idx(vecchia.pacchetto);
    const Chi chi = trova(vecchia.pacchetto, idx, par.utente_elenco, par.tratta, Errore::PuntoNonAmmesso);
    const kzero::Day dal = *par.dal;
    std::vector<kzero::PointRule> regole;
    for (const kzero::PointLine& x : in.points) regole.push_back(regola_da(x, vecchia.pacchetto, idx, chi, dal));
    std::set<kzero::Code> luoghi;
    for (std::size_t i = 0; i < regole.size(); ++i) {
        luoghi.insert(regole[i].place.code);
        for (std::size_t j = 0; j < i; ++j)
            if (regole[i].place.code == regole[j].place.code && giorni_in_comune(regole[i], regole[j]))
                non_ammesso("due punti allo stesso luogo hanno giorni in comune: si scrive una regola sola con i giorni");
    }
    if (luoghi.size() > 1 && in.consent.empty())
        non_ammesso("i punti hanno luoghi diversi (doppia tratta): serve il consenso del committente (una riga \"consenso: ...\")");
    if (luoghi.size() <= 1 && !in.consent.empty()) non_ammesso("il consenso serve solo per una doppia tratta (punti con luoghi diversi)");
    Calcolo c;
    c.committente = chi.lista->client.code;
    VoceScritta v;
    v.utente = chi.utente->code;
    v.tratta = par.tratta;
    v.voce.from = dal;
    v.voce.mark = kzero::ManualMark{par.riferimento, *par.data_sblocco};
    v.voce.origin = kzero::PointOrigin::Manual;
    v.voce.consent = in.consent;
    v.voce.rules = regole;
    c.voci.push_back(v);
    kzero::ClientList lista = *chi.lista;
    scrivi_voci(lista, c.voci);
    c.nuova = elenco::con_documento(vecchia, kFile, elenco::documento_con(vecchia, lista, vecchia.pacchetto.list_last_number));
    c.vecchia = vecchia;
    c.nota = "punto scritto a mano: utente " + v.utente + ", tratta " + v.tratta + ", dal " + kzero::text_from_day(dal) +
             (in.nobody ? ", nessun punto" : ", " + std::to_string(regole.size()) + (regole.size() == 1 ? " regola" : " regole"));
    return c;
}

Calcolo calcola_conferma(const Lettura& vecchia, const Parametri& par, const std::vector<std::string>& righe, const std::string& cartella) {
    elenco::pacchetto_completo(vecchia, cartella, par.utente);
    const kzero::PointLines in = kzero::parse_point_lines(righe, kzero::PointLinesMode::Confirm);
    if (in.fault != kzero::PointLinesFault::None) errore_righe(in);
    const kzero::Index idx(vecchia.pacchetto);
    const kzero::Day dal = *par.dal;
    // La proposta di oggi (anche per chi ha gia' un punto): ogni riga deve esserne un candidato.
    const std::vector<kzero::PointProposal> proposte = kzero::propose_points(vecchia.pacchetto, idx, dal, "", "", true);
    std::vector<std::pair<kzero::Code, std::string>> ordine;
    std::map<std::pair<kzero::Code, std::string>, std::vector<const kzero::PointLine*>> gruppi;
    for (const kzero::PointLine& x : in.points) {
        const auto k = std::make_pair(x.user, x.leg);
        if (!gruppi.count(k)) ordine.push_back(k);
        gruppi[k].push_back(&x);
    }
    Calcolo c;
    const kzero::ClientList* lista = nullptr;
    for (const auto& k : ordine) {
        const Chi chi = trova(vecchia.pacchetto, idx, k.first, k.second, Errore::PuntoNonAmmesso);
        if (lista && lista != chi.lista) non_ammesso("le righe sono di elenchi di committenti diversi: una conferma per committente");
        lista = chi.lista;
        const kzero::PointProposal* pr = nullptr;
        for (const kzero::PointProposal& x : proposte)
            if (x.user->code == k.first && x.leg == k.second) pr = &x;
        if (!pr || pr->source == kzero::PointSource::None)
            non_ammesso("per l'utente " + k.first + ", tratta " + k.second + ", la proposta del " + kzero::text_from_day(dal) +
                        " non ha candidati: un punto scelto a mano si scrive con kzero assegna-punto");
        std::vector<kzero::PointRule> regole;
        for (const kzero::PointLine* x : gruppi.at(k)) {
            const kzero::PointRule r = regola_da(*x, vecchia.pacchetto, idx, chi, dal);
            // Un candidato della proposta in ogni giorno della regola (sempre: in ogni giorno della proposta).
            bool ok = !pr->days.empty();
            for (const kzero::PointDayProposal& g : pr->days) {
                if (!r.covers(g.weekday)) continue;
                const bool c1 = std::any_of(g.candidates.begin(), g.candidates.end(), [&](const kzero::PointCandidate& cd) {
                    return cd.card->id == r.card_id && cd.place == r.place.code && cd.occurrence == r.occurrence;
                });
                ok = ok && c1;
            }
            if (!ok)
                non_ammesso("riga " + std::to_string(x->line) + ": il punto non e' un candidato della proposta del " + kzero::text_from_day(dal) +
                            " per l'utente " + k.first + ", tratta " + k.second + " (o non in tutti i suoi giorni): un punto scelto a mano si scrive "
                            "con kzero assegna-punto");
            regole.push_back(r);
        }
        std::set<kzero::Code> luoghi;
        for (std::size_t i = 0; i < regole.size(); ++i) {
            luoghi.insert(regole[i].place.code);
            for (std::size_t j = 0; j < i; ++j)
                if (giorni_in_comune(regole[i], regole[j]))
                    non_ammesso("per l'utente " + k.first + ", tratta " + k.second + ", due righe hanno giorni in comune");
        }
        if (luoghi.size() > 1)
            non_ammesso("per l'utente " + k.first + ", tratta " + k.second + ", le righe hanno luoghi diversi (doppia tratta): si scrive con "
                        "kzero assegna-punto, con il consenso del committente");
        VoceScritta v;
        v.utente = k.first;
        v.tratta = k.second;
        v.voce.from = dal;
        v.voce.mark = kzero::ManualMark{par.riferimento, *par.data_sblocco};
        v.voce.origin = kzero::PointOrigin::Proposal;
        v.voce.source = pr->source;
        v.voce.rules = regole;
        c.voci.push_back(v);
    }
    c.committente = lista->client.code;
    kzero::ClientList nuova = *lista;
    scrivi_voci(nuova, c.voci);
    c.nuova = elenco::con_documento(vecchia, kFile, elenco::documento_con(vecchia, nuova, vecchia.pacchetto.list_last_number));
    c.vecchia = vecchia;
    c.nota = "punti confermati dalla proposta: " + std::to_string(c.voci.size()) + ", committente " + c.committente + ", dal " + kzero::text_from_day(dal);
    return c;
}

Calcolo calcola_non_frequenta(Azione azione, const Lettura& vecchia, const Parametri& par, const std::vector<std::string>& righe,
                            const std::string& cartella) {
    elenco::pacchetto_completo(vecchia, cartella, par.utente);
    const kzero::Index idx(vecchia.pacchetto);
    const Chi chi = trova(vecchia.pacchetto, idx, par.utente_elenco, par.tratta, Errore::NonFrequentaNonAmmesso);
    const auto no = [](const std::string& m) { throw ErroreKZero(Errore::NonFrequentaNonAmmesso, "Periodo non scritto: " + m + ". Niente e' cambiato."); };
    kzero::ClientList lista = *chi.lista;
    kzero::ListUser* u = nullptr;
    for (kzero::ListUser& x : lista.users)
        if (x.code == chi.utente->code) u = &x;
    const std::vector<std::string> tratte = par.tratta.empty() ? std::vector<std::string>{} : std::vector<std::string>{par.tratta};
    Calcolo c;
    c.committente = lista.client.code;
    c.utente_non_frequenta = u->code;
    if (azione == Azione::NonFrequentaTogli) {
        const auto it = std::find_if(u->not_attending.begin(), u->not_attending.end(),
                                     [&](const kzero::NotAttending& n) { return n.from == *par.dal && n.legs == tratte; });
        if (it == u->not_attending.end())
            no("l'utente " + u->code + " non ha un periodo \"non frequenta\" dal " + kzero::text_from_day(*par.dal) +
               (par.tratta.empty() ? " per tutte le tratte" : " per la tratta " + par.tratta));
        c.non_frequenta = *it;
        u->not_attending.erase(it);
    } else if (azione == Azione::NonFrequentaChiudi) {
        kzero::NotAttending* aperto = nullptr;
        for (kzero::NotAttending& n : u->not_attending)
            if (!n.to && n.from <= *par.fino_al && n.legs == tratte) aperto = &n;
        if (!aperto)
            no("l'utente " + u->code + " non ha un periodo \"non frequenta\" aperto (senza fine) cominciato entro il " + kzero::text_from_day(*par.fino_al) +
               (par.tratta.empty() ? " per tutte le tratte" : " per la tratta " + par.tratta));
        aperto->to = *par.fino_al;
        c.non_frequenta = *aperto;
    } else {
        const kzero::PointLines in = kzero::parse_point_lines(righe, kzero::PointLinesMode::NotAttending);
        if (in.fault != kzero::PointLinesFault::None) errore_righe(in);
        kzero::NotAttending n;
        n.from = *par.dal;
        n.to = par.al;
        if (n.to && *n.to < n.from) no("il periodo finisce (--al) prima di cominciare (--dal)");
        n.legs = tratte;
        n.reason = *in.reason;
        n.note = in.note;
        n.mark = kzero::ManualMark{par.riferimento, *par.data_sblocco};
        for (const kzero::NotAttending& x : u->not_attending) {
            const bool tratte_in_comune =
                x.legs.empty() || n.legs.empty() || std::find(x.legs.begin(), x.legs.end(), par.tratta) != x.legs.end();
            const kzero::Day f1 = x.to ? *x.to : 1000000000, f2 = n.to ? *n.to : 1000000000;
            if (tratte_in_comune && x.from <= f2 && n.from <= f1)
                no("l'utente " + u->code + " ha gia' un periodo \"non frequenta\" dal " + kzero::text_from_day(x.from) +
                   " che si sovrappone (per chiuderlo: --fino-al; per toglierlo: --togli)");
        }
        u->not_attending.push_back(n);
        std::stable_sort(u->not_attending.begin(), u->not_attending.end(),
                         [](const kzero::NotAttending& a, const kzero::NotAttending& b) { return a.from < b.from; });
        c.non_frequenta = n;
    }
    c.nuova = elenco::con_documento(vecchia, kFile, elenco::documento_con(vecchia, lista, vecchia.pacchetto.list_last_number));
    c.vecchia = vecchia;
    c.nota = std::string("non frequenta (") + nome_azione(azione) + "): utente " + u->code + (par.tratta.empty() ? std::string() : ", tratta " + par.tratta);
    return c;
}

// --- L'uscita dei comandi che scrivono ---------------------------------------------------------------------------------------

struct Problema {
    std::string codice, livello, messaggio;
};

// Gli avvisi sui punti per gli utenti toccati, sul pacchetto nuovo (giorni raggruppati).
std::vector<Problema> problemi_di(const Lettura& l, const std::set<kzero::Code>& utenti, const Visibilita& v) {
    std::vector<kzero::Problem> tutti;
    const kzero::Index idx(l.pacchetto);
    kzero::check_points(l.pacchetto, idx, tutti);
    tutti = kzero::group_by_days(std::move(tutti));
    const Presentatore pr(&l.pacchetto, &l.versioni, v);
    std::vector<Problema> out;
    for (const kzero::Problem& p : tutti) {
        bool suo = false;
        for (const kzero::Param& x : p.params) suo = suo || (x.name == "utente" && utenti.count(x.text));
        if (!suo) continue;
        const DefinizioneControllo& d = definizione_controllo(p.check);
        out.push_back({d.codice, nome_livello(d.livello), pr.messaggio(p)});
    }
    return out;
}

Json regole_json(const std::vector<kzero::PointRule>& regole, const kzero::Package& p) {
    Json v = Json::array();
    for (const kzero::PointRule& r : regole) {
        const kzero::ServiceCard* s = kzero::card_by_id(p, r.card_id);
        v.push_back({{"scheda", s ? Json(s->code) : Json(nullptr)}, {"id", r.card_id}, {"luogo", r.place.code}, {"volta", r.occurrence},
                     {"versione", r.version}, {"giorni", giorni_json(r.all_days, r.days)}});
    }
    return v;
}
std::string regole_testo(const std::vector<kzero::PointRule>& regole, const kzero::Package& p) {
    if (regole.empty()) return "nessun punto";
    std::string t;
    for (const kzero::PointRule& r : regole) {
        const kzero::ServiceCard* s = kzero::card_by_id(p, r.card_id);
        t += (t.empty() ? "" : "; ") + (s ? s->code : r.card_id) + " / " + r.place.code + " / " + std::to_string(r.occurrence) + " (" +
             testo_giorni(r.all_days, r.days) + ", versione " + std::to_string(r.version) + ")";
    }
    return t;
}

Risultato uscita(Azione azione, const DefinizioneComando& comando, const Parametri& par, const Calcolo& c, bool scritto,
                 const cassaforte::Riepilogo& r, std::size_t voci, const std::vector<accesso::AvvisoMostrato>& avvisi) {
    const Visibilita v{par.includi_dati_personali, par.includi_dati_particolari};
    const Nomi nomi(c.nuova, v);
    std::set<kzero::Code> utenti;
    for (const VoceScritta& x : c.voci) utenti.insert(x.utente);
    if (c.non_frequenta) utenti.insert(c.utente_non_frequenta);
    const std::vector<Problema> problemi = problemi_di(c.nuova, utenti, v);
    const std::string data = kzero::text_from_day(par.data_sblocco.value_or(0));
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["utente"] = par.utente;
        j["azione"] = nome_azione(azione);
        j["scritto"] = scritto;
        j["committente"] = c.committente;
        j["riferimento"] = par.riferimento;
        j["data"] = data;
        Json vj = Json::array();
        for (const VoceScritta& x : c.voci) {
            const auto n = nomi.di(x.utente);
            Json s = Json::array();
            for (kzero::Day d : x.sostituite) s.push_back(kzero::text_from_day(d));
            vj.push_back({{"utente_elenco", x.utente}, {"nome", n ? Json(*n) : Json(nullptr)}, {"tratta", x.tratta},
                          {"dal", kzero::text_from_day(x.voce.from)},
                          {"origine", x.voce.origin == kzero::PointOrigin::Proposal ? "proposta" : "a_mano"},
                          {"fonte", x.voce.source == kzero::PointSource::None ? Json(nullptr) : Json(nome_fonte(x.voce.source))},
                          {"consenso", x.voce.consent.empty() ? Json(nullptr) : Json(x.voce.consent)},
                          {"regole", regole_json(x.voce.rules, c.nuova.pacchetto)}, {"sostituite", s}});
        }
        j["voci"] = vj;
        if (c.non_frequenta) {
            const kzero::NotAttending& n = *c.non_frequenta;
            const auto nome = nomi.di(c.utente_non_frequenta);
            const bool nota_visibile = nome.has_value();   // la nota e' personale come il nome (e legata come il nome)
            j["non_frequenta"] = {{"utente_elenco", c.utente_non_frequenta}, {"nome", nome ? Json(*nome) : Json(nullptr)},
                                {"tratte", n.legs}, {"dal", kzero::text_from_day(n.from)},
                                {"al", n.to ? Json(kzero::text_from_day(*n.to)) : Json(nullptr)}, {"motivo", nome_motivo(n.reason)},
                                {"nota", n.note.empty() || !nota_visibile ? Json(nullptr) : Json(n.note)},
                                {"nota_riservata", !n.note.empty() && !nota_visibile}};
        } else {
            j["non_frequenta"] = nullptr;
        }
        Json pj = Json::array();
        for (const Problema& p : problemi) pj.push_back({{"codice", p.codice}, {"livello", p.livello}, {"messaggio", p.messaggio}});
        j["problemi"] = pj;
        Json av = Json::array();
        for (const accesso::AvvisoMostrato& a : avvisi)
            av.push_back({{"codice", a.codice.empty() ? Json(nullptr) : Json(a.codice)}, {"messaggio", a.messaggio}});
        j["avvisi"] = av;
        j["cassaforte"] = {{"id", r.id}, {"generazione", r.generazione}, {"voci_registro", voci}};
        return {testo_json(j), 0};
    }
    std::string t = scritto ? "" : "Anteprima (niente e' stato scritto: per scrivere, --scrivi).\n";
    for (const VoceScritta& x : c.voci) {
        const auto n = nomi.di(x.utente);
        t += std::string(azione == Azione::Conferma ? "Punto confermato dalla proposta" : "Punto scritto a mano") + ": utente " + x.utente +
             (n ? " \"" + *n + "\"" : "") + ", tratta " + x.tratta + ", dal " + kzero::text_from_day(x.voce.from) + ": " +
             regole_testo(x.voce.rules, c.nuova.pacchetto) + ".";
        if (x.voce.source != kzero::PointSource::None) t += std::string(" Proposto ") + testo_fonte(x.voce.source) + ".";
        if (!x.voce.consent.empty()) t += " Doppia tratta, consenso: \"" + x.voce.consent + "\".";
        t += "\n";
        if (!x.sostituite.empty()) {
            std::string d;
            for (kzero::Day y : x.sostituite) d += (d.empty() ? "" : ", ") + kzero::text_from_day(y);
            t += "  Sostituisce le voci dal " + d + " (restano scritte, segnate \"sostituita\"); i giorni prima del " +
                 kzero::text_from_day(x.voce.from) + " non cambiano.\n";
        }
    }
    if (c.non_frequenta) {
        const kzero::NotAttending& n = *c.non_frequenta;
        const auto nome = nomi.di(c.utente_non_frequenta);
        std::string cosa = azione == Azione::NonFrequentaTogli ? "Periodo \"non frequenta\" tolto" : azione == Azione::NonFrequentaChiudi ? "Periodo \"non frequenta\" chiuso"
                                                                                                                                    : "Non frequenta";
        t += cosa + ": utente " + c.utente_non_frequenta + (nome ? " \"" + *nome + "\"" : "") + ", " +
             (n.legs.empty() ? std::string("tutte le tratte della nostra azienda") : "tratta " + n.legs.front()) + ", dal " +
             kzero::text_from_day(n.from) + (n.to ? " al " + kzero::text_from_day(*n.to) : std::string(" senza fine")) + ", motivo " +
             nome_motivo(n.reason) + (n.note.empty() ? "" : nome ? ", nota \"" + n.note + "\"" : ", nota (riservata: --includi-dati-personali)") +
             ". Resta nell'elenco; nel confronto con le schede non conta.\n";
    }
    for (const Problema& p : problemi) t += (p.livello == "errore" ? "Errore " : "Avviso ") + p.codice + ": " + p.messaggio + "\n";
    t += "Cassaforte " + r.id + ", chiusura numero " + std::to_string(r.generazione) + "; registro degli accessi: " + std::to_string(voci) +
         (voci == 1 ? " voce.\n" : " voci.\n");
    for (const accesso::AvvisoMostrato& a : avvisi) t += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return {t, 0};
}

Risultato esegui_scrittura(Azione azione, const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    const bool scrivere = !par.scrivi.empty();
    if (scrivere) elenco::stessa_cartella(cartella.string(), par.scrivi);
    if (par.riferimento.empty() || !par.data_sblocco)
        throw ErroreKZero(Errore::ParametroObbligatorio, "Ogni punto o periodo vuole --riferimento (es. la mail del committente, chi l'ha detto) "
                                                         "e --data (scritta da chi lo scrive).");
    if ((azione == Azione::Assegna || azione == Azione::Conferma || azione == Azione::NonFrequenta || azione == Azione::NonFrequentaTogli) && !par.dal)
        throw ErroreKZero(Errore::ParametroObbligatorio, "Serve --dal: da quando vale (i giorni prima non cambiano mai).");
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    elenco::permessi_prima_della_password(cartella, par.utente, comando.nome);
    const auto ingresso = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*ingresso, par.utente);
    cripto::niente_core_dump();
    // I valori dopo la password, prima di aprire la cassaforte.
    std::vector<std::string> righe;
    if (azione == Azione::Assegna)
        righe = righe_dall_ingresso("Scrivi i punti, una riga per regola (punto: <scheda> / <luogo> [/ <volta>] [/ <giorni>]), oppure \"nessuno\"; "
                                    "\"consenso: ...\" per una doppia tratta; una riga vuota per finire.\n");
    else if (azione == Azione::Conferma)
        righe = righe_dall_ingresso("Scrivi le righe pronte della proposta (kzero proponi-punti), una per punto; una riga vuota per finire.\n");
    else if (azione == Azione::NonFrequenta)
        righe = righe_dall_ingresso("Scrivi il motivo (motivo: famiglia, altro_mezzo, assente o altro) e, se serve, una nota (nota: ...); "
                                    "una riga vuota per finire.\n");
    const auto calcola = [&](const Lettura& vecchia) {
        switch (azione) {
            case Azione::Assegna: return calcola_assegna(vecchia, par, righe, cartella.string());
            case Azione::Conferma: return calcola_conferma(vecchia, par, righe, cartella.string());
            default: return calcola_non_frequenta(azione, vecchia, par, righe, cartella.string());
        }
    };
    if (!scrivere) {
        accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome,
                                            Visibilita{par.includi_dati_personali, par.includi_dati_particolari},
                                            std::string("anteprima dei punti (") + nome_azione(azione) + ")" +
                                                (par.utente_elenco.empty() ? std::string() : ": utente " + par.utente_elenco),
                                            accesso::ambiente());
        elenco::permessi(a.cassaforte.permessi(), par.utente, comando.nome);
        const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
        const Calcolo c = calcola(leggi_pacchetto(CartellaSuDisco(cartella), &v));
        const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
        return uscita(azione, comando, par, c, false, r, a.registro.voci.size(), elenco::avvisi_di(r, par.utente));
    }
    std::optional<Calcolo> calcolo;
    const cassaforte::Riepilogo r = elenco::scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        calcolo = calcola(vecchia);
        return std::make_pair(calcolo->nuova, calcolo->nota);
    });
    return uscita(azione, comando, par, *calcolo, true, r, r.registro_voci, elenco::avvisi_di(r, par.utente));
}

// --- I comandi che leggono soltanto -----------------------------------------------------------------------------------------

accesso::LetturaPerComando leggi_senza_errori(const Parametri& par, const std::string& comando, const std::string& nota) {
    accesso::LetturaPerComando lc = accesso::leggi_per_comando(par.argomenti.at(0), par, comando, nota);
    if (!lc.lettura.forma_valida())
        throw ErroreKZero(Errore::PacchettoConErrori, "Il pacchetto ha errori nella forma dei file: prima vanno corretti (kzero controlla " +
                                                          par.argomenti.at(0) + ").");
    return lc;
}

std::string riga_pronta(const kzero::PointProposal& x, const kzero::PointRule& r, const kzero::Package& p) {
    const kzero::ServiceCard* s = kzero::card_by_id(p, r.card_id);
    return x.user->code + " " + x.leg + ": " + (s ? s->code : r.card_id) + " / " + r.place.code + " / " + std::to_string(r.occurrence) +
           (r.all_days ? std::string() : " / " + testo_giorni(false, r.days));
}

}  // namespace

Risultato esegui_proponi_punti(const DefinizioneComando& comando, const Parametri& par) {
    accesso::LetturaPerComando lc = leggi_senza_errori(par, comando.nome, "proposta dei punti degli utenti");
    if (lc.cassaforte) elenco::permessi(lc.permessi, par.utente, comando.nome);   // lo stradario guarda le case (dati personali)
    const kzero::Package& p = lc.lettura.pacchetto;
    const kzero::Index idx(p);
    if (!par.utente_elenco.empty() && !idx.list_user(par.utente_elenco))
        throw ErroreKZero(Errore::UtenteElencoNonTrovato, "L'utente '" + par.utente_elenco + "' non c'e' nell'elenco utenti del pacchetto.");
    const std::vector<kzero::PointProposal> proposte = kzero::propose_points(p, idx, *par.dal, par.utente_elenco, par.tratta, par.tutti);
    const Nomi nomi(lc.lettura, lc.visibilita);
    std::vector<std::string> pronte;
    for (const kzero::PointProposal& x : proposte)
        for (const kzero::PointRule& r : x.ready) pronte.push_back(riga_pronta(x, r, p));
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["dal"] = kzero::text_from_day(*par.dal);
        Json pj = Json::array();
        for (const kzero::PointProposal& x : proposte) {
            const auto n = nomi.di(x.user->code);
            Json giorni = Json::array();
            for (const kzero::PointDayProposal& g : x.days) {
                Json cand = Json::array();
                for (const kzero::PointCandidate& c : g.candidates)
                    cand.push_back({{"scheda", c.card->code}, {"id", c.card->id}, {"versione", c.version->number}, {"luogo", c.place},
                                    {"volta", c.occurrence}, {"orario", kzero::text_from_minutes(c.version->card.stops[c.row].time)},
                                    {"orario_scuola", kzero::text_from_minutes(c.version->card.stops[c.school_row].time)}});
                giorni.push_back({{"giorno", nome_giorno(g.weekday)},
                                  {"ora", g.user_time ? Json(kzero::text_from_minutes(*g.user_time)) : Json(nullptr)}, {"candidati", cand}});
            }
            Json righe = Json::array();
            for (const kzero::PointRule& r : x.ready) righe.push_back(riga_pronta(x, r, p));
            pj.push_back({{"utente_elenco", x.user->code}, {"nome", n ? Json(*n) : Json(nullptr)}, {"tratta", x.leg},
                          {"verso", x.direction == kzero::Direction::Outbound ? "andata" : "ritorno"}, {"fonte", nome_fonte(x.source)},
                          {"luoghi", x.places}, {"giorni_noti", x.days_known}, {"ha_punto", x.has_point}, {"giorni", giorni},
                          {"righe_pronte", righe}});
        }
        j["proposte"] = pj;
        j["righe_pronte"] = pronte;
        return {testo_json(j), 0};
    }
    std::string t = "Proposta dei punti dal " + kzero::text_from_day(*par.dal) + " (niente e' scritto: si conferma con kzero conferma-punti, "
                    "o si scrive a mano con kzero assegna-punto).\n";
    if (proposte.empty()) t += "Nessun utente con una tratta della nostra azienda senza punto.\n";
    for (const kzero::PointProposal& x : proposte) {
        const auto n = nomi.di(x.user->code);
        t += "Utente " + x.user->code + (n ? " \"" + *n + "\"" : "") + ", tratta " + x.leg + (x.has_point ? " (ha gia' un punto)" : "") + ": ";
        if (x.source == kzero::PointSource::None) {
            t += "nessun candidato (ne' dalla fermata indicata ne' dallo stradario): il punto si sceglie a mano.\n";
            continue;
        }
        std::string luoghi;
        for (const kzero::Code& l : x.places) luoghi += (luoghi.empty() ? "" : ", ") + l;
        t += std::string(testo_fonte(x.source)) + " (" + luoghi + ")" + (x.days_known ? "" : "; senza orari, i giorni sono quelli delle schede") + ".\n";
        // I giorni con gli stessi candidati e la stessa ora in una riga sola.
        const auto candidati = [](const kzero::PointDayProposal& g) {
            std::string c;
            if (g.candidates.empty()) return std::string("nessun candidato");
            for (std::size_t i = 0; i < g.candidates.size(); ++i) {
                const kzero::PointCandidate& k = g.candidates[i];
                c += (i ? "; " : "") + k.card->code + " versione " + std::to_string(k.version->number) + ", " + k.place + " " +
                     std::to_string(k.occurrence) + "a volta alle " + kzero::text_from_minutes(k.version->card.stops[k.row].time) + " (scuola alle " +
                     kzero::text_from_minutes(k.version->card.stops[k.school_row].time) + ")";
            }
            if (g.candidates.size() > 1) c += ": piu' candidati, nessuna scelta";
            return c;
        };
        std::vector<std::pair<std::string, std::string>> righe;   // giorni, resto
        for (const kzero::PointDayProposal& g : x.days) {
            const std::string resto = (g.user_time ? " (ora " + kzero::text_from_minutes(*g.user_time) + ")" : std::string()) + ": " + candidati(g);
            if (!righe.empty() && righe.back().second == resto) righe.back().first += std::string(", ") + nome_giorno(g.weekday);
            else righe.push_back({nome_giorno(g.weekday), resto});
        }
        for (const auto& [giorni, resto] : righe) t += "  " + giorni + resto + ".\n";
    }
    if (!pronte.empty()) {
        t += "Righe pronte per kzero conferma-punti (una per punto; i giorni senza un candidato solo restano da scegliere a mano):\n";
        for (const std::string& r : pronte) t += "  " + r + "\n";
    }
    return {t, 0};
}

Risultato esegui_assegna_punto(const DefinizioneComando& c, const Parametri& p) { return esegui_scrittura(Azione::Assegna, c, p); }
Risultato esegui_conferma_punti(const DefinizioneComando& c, const Parametri& p) { return esegui_scrittura(Azione::Conferma, c, p); }
Risultato esegui_non_frequenta(const DefinizioneComando& c, const Parametri& p) {
    if (p.togli && p.fino_al)
        throw ErroreKZero(Errore::ParametriIncompatibili, "--togli e --fino-al non vanno insieme: togliere un periodo scritto per errore, o chiuderne uno aperto.");
    return esegui_scrittura(p.togli ? Azione::NonFrequentaTogli : p.fino_al ? Azione::NonFrequentaChiudi : Azione::NonFrequenta, c, p);
}

Risultato esegui_confronta_schede(const DefinizioneComando& comando, const Parametri& par) {
    accesso::LetturaPerComando lc = leggi_senza_errori(par, comando.nome, "confronto dei punti con le schede");
    const kzero::Package& p = lc.lettura.pacchetto;
    const kzero::Index idx(p);
    if (!par.scheda.empty() && !std::any_of(p.service_cards.begin(), p.service_cards.end(),
                                            [&](const kzero::ServiceCard& s) { return s.code == par.scheda || s.id == par.scheda; }))
        throw ErroreKZero(Errore::SchedaNonTrovata, "La scheda '" + par.scheda + "' non c'e' nel registro delle schede (codice o ID).");
    if ((par.dal && !par.al) || (!par.dal && par.al))
        throw ErroreKZero(Errore::ParametroObbligatorio, "--dal e --al vanno insieme.");
    if (par.dal && par.al && *par.al < *par.dal) throw ErroreKZero(Errore::ValoreScrittoMale, "--al e' prima di --dal.");
    const kzero::DateRange periodo = par.dal ? kzero::DateRange{*par.dal, *par.al} : p.manifest.period;
    kzero::PointsComparison c = kzero::compare_points(p, idx, periodo, par.scheda);
    std::vector<kzero::Problem> problemi = kzero::group_by_days(std::move(c.problems));
    // "Senza punto" in testa (D5: grave, ha diritto), poi gli altri nell'ordine del controllo.
    std::stable_sort(problemi.begin(), problemi.end(), [](const kzero::Problem& a, const kzero::Problem& b) {
        return (a.check == kzero::Check::PointsMissing) > (b.check == kzero::Check::PointsMissing);
    });
    const Nomi nomi(lc.lettura, lc.visibilita);
    const Presentatore pr(&lc.lettura.pacchetto, &lc.lettura.versioni, lc.visibilita);
    const auto codici_con_nomi = [&](const std::vector<kzero::Code>& v) {
        std::string t;
        for (const kzero::Code& x : v) {
            const auto n = nomi.di(x);
            t += (t.empty() ? "" : ", ") + x + (n ? " \"" + *n + "\"" : "");
        }
        return t;
    };
    // Gli utenti una volta sola (alla scuola un doppio e' contato due volte nel numero, ma e' una persona).
    const auto lista_json = [&](const std::vector<kzero::Code>& v) {
        Json a = Json::array();
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (i > 0 && v[i] == v[i - 1]) continue;
            const kzero::Code& x = v[i];
            const auto n = nomi.di(x);
            a.push_back({{"utente_elenco", x}, {"nome", n ? Json(*n) : Json(nullptr)}});
        }
        return a;
    };
    int diversi = 0;
    for (const kzero::ComparedVersion& v : c.versions)
        for (const kzero::ComparedPeriod& pe : v.periods) diversi += pe.differs ? 1 : 0;
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["dal"] = kzero::text_from_day(periodo.from);
        j["al"] = kzero::text_from_day(periodo.to);
        j["scheda"] = par.scheda.empty() ? Json(nullptr) : Json(par.scheda);
        Json vj = Json::array();
        for (const kzero::ComparedVersion& v : c.versions) {
            const kzero::Card& k = v.version->card;
            Json per = Json::array();
            for (const kzero::ComparedPeriod& pe : v.periods) {
                Json pas = Json::array();
                for (std::size_t i = 0; i < pe.passages.size(); ++i) {
                    const kzero::ComparedPassage& q = pe.passages[i];
                    pas.push_back({{"riga", i + 1}, {"luogo", k.stops[i].place}, {"volta", kzero::card_occurrence(k, i)},
                                   {"orario", kzero::text_from_minutes(k.stops[i].time)}, {"scuola", q.school},
                                   {"scheda_salgono", q.card_boarding}, {"salgono", q.boarding.size()}, {"scheda_scendono", q.card_alighting},
                                   {"scendono", q.alighting.size()}, {"utenti_salgono", lista_json(q.boarding)},
                                   {"utenti_scendono", lista_json(q.alighting)}, {"doppi", lista_json(q.doubles)},
                                   {"doppi_stessa_scheda", lista_json(q.doubles_same_card)}});
                }
                Json giorni = Json::array();
                for (kzero::Day d : pe.days) giorni.push_back(kzero::text_from_day(d));
                per.push_back({{"giorno", nome_giorno(pe.weekday)}, {"dal", kzero::text_from_day(pe.days.front())},
                               {"al", kzero::text_from_day(pe.days.back())}, {"giorni", giorni}, {"diverso", pe.differs}, {"passaggi", pas},
                               {"non_frequentano", lista_json(pe.not_attending)}, {"non_contati", lista_json(pe.not_counted)}});
            }
            vj.push_back({{"scheda", v.card->code}, {"id", v.card->id}, {"nr", k.nr}, {"versione", v.version->number},
                          {"verso", k.direction == kzero::Direction::Outbound ? "andata" : "ritorno"}, {"servizio", k.service_text},
                          {"dal", kzero::text_from_day(v.version->valid.from)}, {"al", kzero::text_from_day(v.version->valid.to)},
                          {"periodi", per}});
        }
        j["versioni"] = vj;
        j["periodi_diversi"] = diversi;
        Json pj = Json::array();
        for (const kzero::Problem& x : problemi) {
            const DefinizioneControllo& d = definizione_controllo(x.check);
            pj.push_back({{"codice", d.codice}, {"livello", nome_livello(d.livello)}, {"messaggio", pr.messaggio(x)}});
        }
        j["problemi"] = pj;
        return {testo_json(j), 0};
    }
    std::string t = "Confronto dei punti degli utenti con le schede, dal " + kzero::text_from_day(periodo.from) + " al " +
                    kzero::text_from_day(periodo.to) + (par.scheda.empty() ? "" : ", scheda " + par.scheda) +
                    ". Il programma non corregge ne' la scheda ne' l'elenco.\n";
    for (const kzero::Problem& x : problemi)
        if (x.check == kzero::Check::PointsMissing) t += "Avviso K-CTR-183: " + pr.messaggio(x) + "\n";
    for (const kzero::ComparedVersion& v : c.versions) {
        const kzero::Card& k = v.version->card;
        t += "Scheda " + v.card->code + " (" + k.nr + ", " + k.service_text + "), versione " + std::to_string(v.version->number) + ", dal " +
             kzero::text_from_day(v.version->valid.from) + " al " + kzero::text_from_day(v.version->valid.to) + ":\n";
        for (const kzero::ComparedPeriod& pe : v.periods) {
            t += "  " + std::string(nome_giorno(pe.weekday)) +
                 (pe.days.size() == 1 ? " " + kzero::text_from_day(pe.days.front())
                                      : " dal " + kzero::text_from_day(pe.days.front()) + " al " + kzero::text_from_day(pe.days.back()) + " (" +
                                            std::to_string(pe.days.size()) + " giorni)") +
                 ": " + (pe.differs ? "numeri diversi" : "numeri uguali") + ".\n";
            for (std::size_t i = 0; i < pe.passages.size(); ++i) {
                const kzero::ComparedPassage& q = pe.passages[i];
                const bool diverso = static_cast<int>(q.boarding.size()) != q.card_boarding || static_cast<int>(q.alighting.size()) != q.card_alighting;
                t += "    " + kzero::text_from_minutes(k.stops[i].time) + " " + k.stops[i].place + " (" + std::to_string(kzero::card_occurrence(k, i)) +
                     "a volta" + (q.school ? ", scuola" : "") + "): salgono " + std::to_string(q.card_boarding) + " sulla scheda, " +
                     std::to_string(q.boarding.size()) + " con il punto; scendono " + std::to_string(q.card_alighting) + " sulla scheda, " +
                     std::to_string(q.alighting.size()) + " con il punto" + (diverso ? " (diverso)" : "");
                if (!q.boarding.empty() && !q.school) t += ". Salgono: " + codici_con_nomi(q.boarding);
                if (!q.alighting.empty() && !q.school) t += ". Scendono: " + codici_con_nomi(q.alighting);
                if (!q.doubles.empty()) t += ". Doppi (contati anche su un altro passaggio): " + codici_con_nomi(q.doubles);
                if (!q.doubles_same_card.empty())
                    t += ". Doppi sulla stessa scheda (a bordo un posto solo: i posti li conta il K-028): " + codici_con_nomi(q.doubles_same_card);
                t += ".\n";
            }
            if (!pe.not_attending.empty()) t += "    Non frequentano (non contati): " + codici_con_nomi(pe.not_attending) + ".\n";
            if (!pe.not_counted.empty()) t += "    Con il punto qui ma non contati (vedi gli avvisi): " + codici_con_nomi(pe.not_counted) + ".\n";
        }
    }
    for (const kzero::Problem& x : problemi) {
        if (x.check == kzero::Check::PointsMissing) continue;
        const DefinizioneControllo& d = definizione_controllo(x.check);
        t += std::string(d.livello == Livello::Errore ? "Errore " : "Avviso ") + d.codice + ": " + pr.messaggio(x) + "\n";
    }
    if (!lc.visibilita.personali) t += "Nomi non mostrati: --includi-dati-personali.\n";
    return {t, 0};
}

}  // namespace kzero::api
