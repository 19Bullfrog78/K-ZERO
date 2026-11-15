// K-Zero API - le schede dei servizi del committente (K-021): kzero leggi-schede e kzero togli-scheda. Uscite: schemi "schede" e
// "togli_scheda".
//
// Le regole stanno nel nucleo (schede.hpp: lettura, totali, scelta della data, versioni); come una versione diventa servizio,
// gruppo, corsa e calendario in schede_scrittura.hpp. La scrittura nella cassaforte e' quella dei comandi dell'elenco
// (elenco_scrittura.hpp: pacchetto nuovo in memoria, riaperto con la password, poi cassaforte e file normali). Permessi: tutti e due
// (i servizi hanno dati particolari: tipo, pedana, servizio individualizzato; le case porta a porta sono dati personali).
#include <algorithm>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "accesso.hpp"
#include "chiusura.hpp"
#include "controlli_catalogo.hpp"
#include "elenco_scrittura.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "excel.hpp"
#include "riservatezza.hpp"
#include "schede.hpp"
#include "schede_scrittura.hpp"

namespace kzero::api {

namespace {

namespace fs = std::filesystem;

// --- Permessi -------------------------------------------------------------------------------------------------------------

void permessi_schede(const cassaforte::Permessi& p, const std::string& utente, const std::string& comando) {
    if (p.tutti()) return;
    throw ErroreKZero(Errore::PermessoMancante, "kzero " + comando + " scrive nella cassaforte i dati particolari dei servizi (tipo, "
                                                "pedana, servizio individualizzato) e le case porta a porta: lo puo' fare solo chi ha i "
                                                "dati personali e particolari (l'utente " + utente + " ha " +
                                                cassaforte::testo_permessi(p) + ").");
}
void permessi_prima(const fs::path& cartella, const std::string& utente, const std::string& comando) {
    std::vector<cassaforte::UtenteVisibile> utenti;
    try {
        utenti = cassaforte::leggi_intestazione(leggi_dal_disco(cartella, cassaforte::kFile, kMassimoByteFile).testo).utenti;
    } catch (const cassaforte::ErroreCassaforte&) {
        return;   // lo dira' l'apertura
    }
    for (const cassaforte::UtenteVisibile& u : utenti)
        if (u.codice == utente) permessi_schede(u.permessi, utente, comando);
}
void pacchetto_valido(const Lettura& l, const std::string& cartella, const std::string& utente) {
    if (!l.usato("pacchetto.json") || !l.forma_valida())
        throw ErroreKZero(Errore::PacchettoConErrori, "Il pacchetto ha errori nella forma dei file: prima vanno corretti (kzero controlla " +
                                                          cartella + " --utente " + utente + ").");
}

// --- Il file ------------------------------------------------------------------------------------------------------------------

struct FileSchede {
    std::string byte;
    std::string impronta;   // del file si tiene solo questa, mai il nome
};

FileSchede leggi_file(const std::string& percorso) {
    const FileLetto f = leggi_file_esterno(percorso, excel::limiti().file_byte);
    switch (f.esito) {
        case FileLetto::Esito::Letto: break;
        case FileLetto::Esito::TroppoGrande:
            throw ErroreKZero(Errore::FileExcelNonValido, "Il file e' piu' grande del massimo (" +
                                                              std::to_string(excel::limiti().file_byte / (1024 * 1024)) +
                                                              " MB): non e' un normale file di schede.");
        case FileLetto::Esito::NonNormale:
            throw ErroreKZero(Errore::FileDaLeggereIlleggibile, "Quello indicato non e' un file normale (es. una cartella o un "
                                                                "dispositivo): indicare il file .xlsx delle schede.");
        case FileLetto::Esito::CollegamentoRotto:
            throw ErroreKZero(Errore::FileDaLeggereIlleggibile, "Il file indicato e' un collegamento che non porta a nessun file.");
        default:
            throw ErroreKZero(Errore::FileDaLeggereIlleggibile, "Il file indicato non c'e' o non si puo' leggere (permessi, disco).");
    }
    return {f.testo, cripto::esadecimale(cripto::impronta(f.testo))};
}

kzero::Table tabella_da(const excel::Foglio& f) {
    kzero::Table t;
    for (const excel::Riga& r : f.righe) {
        kzero::TableRow x;
        x.number = static_cast<int>(r.numero);
        for (const excel::Cella& c : r.celle) {
            kzero::TableCell y;
            y.column = static_cast<int>(c.colonna);
            y.unreadable = c.tipo == excel::Cella::Tipo::Errore || c.tipo == excel::Cella::Tipo::TroppoLunga;
            y.text = c.testo;
            if (c.tipo == excel::Cella::Tipo::Numero && y.text.size() > 2 && y.text.compare(y.text.size() - 2, 2, ".0") == 0)
                y.text.resize(y.text.size() - 2);
            x.cells.push_back(std::move(y));
        }
        t.rows.push_back(std::move(x));
    }
    return t;
}

struct Tabelle {
    kzero::CardTables t;
    bool date1904 = false;
};

Tabelle tabelle_da(const FileSchede& f, const kzero::CardProfile& p) {
    Tabelle out;
    const auto foglio = [&](const std::string& nome, bool facoltativo) -> std::optional<excel::Foglio> {
        try {
            return excel::leggi_foglio(f.byte, nome);
        } catch (const excel::ErroreExcel& e) {
            if (e.motivo == excel::ErroreExcel::Motivo::VecchioXls) throw ErroreKZero(Errore::FormatoXls, e.what());
            if (facoltativo && e.motivo == excel::ErroreExcel::Motivo::FoglioMancante) return std::nullopt;
            throw ErroreKZero(Errore::FileExcelNonValido, std::string(e.what()) + " (foglio \"" + nome + "\") Niente e' stato letto.");
        }
    };
    const auto schede = foglio(p.sheet_cards, false);
    const auto fermate = foglio(p.sheet_stops, false);
    const auto date = foglio(p.sheet_dates, true);   // senza schede a date elencate puo' mancare
    out.t.cards = tabella_da(*schede);
    out.t.stops = tabella_da(*fermate);
    if (date) out.t.dates = tabella_da(*date);
    out.date1904 = schede->date1904;
    return out;
}

const kzero::CardProfile& profilo_di(const Lettura& l, const std::string& codice) {
    for (const kzero::CardProfile& p : l.pacchetto.card_profiles)
        if (p.code == codice) return p;
    throw ErroreKZero(Errore::ProfiloNonTrovato, "Il profilo delle schede '" + codice + "' non c'e' nel pacchetto (profili_schede.json).");
}

// --- La scelta della data -----------------------------------------------------------------------------------------------------

std::optional<kzero::DateChoice> scelta_di(const std::string& v) {
    kzero::DateChoice c;
    if (v == "attivazione") c.kind = kzero::DateChoice::Kind::Activation;
    else if (v == "presa-in-carico") c.kind = kzero::DateChoice::Kind::TakenInCharge;
    else if (const auto d = kzero::day_from_text(v)) {
        c.kind = kzero::DateChoice::Kind::Date;
        c.date = *d;
    } else
        return std::nullopt;
    return c;
}

kzero::DateChoices scelte_di(const Parametri& par, const kzero::CardProfile& pr) {
    kzero::DateChoices out;
    const std::string forme = " (attivazione, presa-in-carico, oppure una data AAAA-MM-GG). Niente e' stato letto.";
    if (!par.validita.empty()) {
        const auto c = scelta_di(par.validita);
        if (!c) throw ErroreKZero(Errore::ValiditaScrittaMale, "--validita non e' una scelta" + forme);
        out.file = *c;
    }
    if (!par.validita_schede.empty()) {
        std::size_t a = 0;
        for (;;) {
            const std::size_t b = par.validita_schede.find(';', a);
            const std::string voce = kzero::clean_cell(par.validita_schede.substr(a, b == std::string::npos ? std::string::npos : b - a));
            if (!voce.empty()) {
                const std::size_t u = voce.rfind('=');
                if (u == std::string::npos)
                    throw ErroreKZero(Errore::ValiditaScrittaMale, "--validita-scheda: ogni voce si scrive Nr=scelta, separate da \";\"" + forme);
                const std::string nr = kzero::card_nr_key(voce.substr(0, u), pr);
                const auto c = scelta_di(kzero::clean_cell(voce.substr(u + 1)));
                if (nr.empty() || !c)
                    throw ErroreKZero(Errore::ValiditaScrittaMale, "--validita-scheda: una voce non ha il Nr o la scelta" + forme);
                if (!out.by_nr.emplace(nr, *c).second)
                    throw ErroreKZero(Errore::ValiditaScrittaMale, "--validita-scheda: la stessa scheda c'e' due volte" + forme);
            }
            if (b == std::string::npos) break;
            a = b + 1;
        }
    }
    return out;
}

// --- Calcolo -------------------------------------------------------------------------------------------------------------

struct Calcolo {
    kzero::CardsRead letto;
    kzero::CardsMerge confronto;
    Lettura vecchia;
    Lettura nuova;
    std::string committente;
};

Calcolo calcola(Lettura vecchia, const Parametri& par, const Tabelle& t, const std::string& cartella) {
    pacchetto_valido(vecchia, cartella, par.utente);
    const kzero::CardProfile& pr = profilo_di(vecchia, par.profilo);
    const kzero::Index idx(vecchia.pacchetto);
    const kzero::Contract* ct = idx.contract(par.contratto);
    if (!ct)
        throw ErroreKZero(Errore::ContrattoNonValido, "Il contratto '" + par.contratto + "' non c'e' nel pacchetto (contratti.json). Niente e' "
                                                      "stato letto.");
    if (ct->client.code != pr.client.code)
        throw ErroreKZero(Errore::ContrattoNonValido, "Il contratto '" + par.contratto + "' e' del committente " + ct->client.code +
                                                          ", il profilo " + pr.code + " del committente " + pr.client.code +
                                                          ". Niente e' stato letto.");
    Calcolo c;
    c.committente = pr.client.code;
    c.letto = kzero::read_cards(t.t, pr, idx, t.date1904);
    if (c.letto.fault != kzero::TableFault::None)
        throw ErroreKZero(Errore::ColonnaMancante,
                          c.letto.fault == kzero::TableFault::NoRows
                              ? "Il foglio \"" + c.letto.fault_sheet + "\" e' vuoto: manca anche la riga dei titoli. Niente e' stato letto."
                          : c.letto.fault == kzero::TableFault::MissingTitle
                              ? "Nella prima riga del foglio \"" + c.letto.fault_sheet + "\" manca la colonna \"" + c.letto.fault_title +
                                    "\" del profilo " + pr.code + ": forse il modello e' di un'altra versione, o il profilo e' sbagliato. "
                                    "Niente e' stato letto."
                              : "Nella prima riga del foglio \"" + c.letto.fault_sheet + "\" la colonna \"" + c.letto.fault_title +
                                    "\" del profilo " + pr.code + " c'e' piu' di una volta: non si sa quale leggere. Niente e' stato letto.");
    c.confronto = kzero::merge_cards(vecchia.pacchetto, pr, par.contratto, *par.data_sblocco, par.riferimento, scelte_di(par, pr), c.letto);
    c.nuova = schede::con_documenti(vecchia, schede::documenti_dopo(vecchia, c.confronto.cards, vecchia.pacchetto.removed_cards,
                                                                   c.confronto.added, c.confronto.removed, c.confronto.cuts,
                                                                   c.confronto.new_homes));
    c.vecchia = std::move(vecchia);
    return c;
}

// --- Uscita di leggi-schede ---------------------------------------------------------------------------------------------------

const char* nome_esito(kzero::CardOutcome::Kind k) {
    switch (k) {
        case kzero::CardOutcome::Kind::New: return "nuova";
        case kzero::CardOutcome::Kind::NewVersion: return "versione_nuova";
        case kzero::CardOutcome::Kind::Same: return "uguale";
        case kzero::CardOutcome::Kind::Stopped: return "ferma";
        case kzero::CardOutcome::Kind::ChoiceMissing: return "senza_scelta";
    }
    return "?";
}
const char* nome_modo(kzero::ChoiceMode m) {
    return m == kzero::ChoiceMode::Activation ? "attivazione" : m == kzero::ChoiceMode::TakenInCharge ? "presa_in_carico" : "data";
}
const char* nome_da(kzero::ChoiceFrom f) { return f == kzero::ChoiceFrom::File ? "file" : f == kzero::ChoiceFrom::Card ? "scheda" : "nessuna"; }
const char* testo_scelta(kzero::ChoiceMode m, kzero::ChoiceFrom f) {
    if (f == kzero::ChoiceFrom::None) return "dall'attivazione, nessuna scelta serviva";
    if (f == kzero::ChoiceFrom::File)
        return m == kzero::ChoiceMode::Activation ? "dall'attivazione, scelta --validita" : m == kzero::ChoiceMode::TakenInCharge
                                                                                              ? "dalla presa in carico, scelta --validita"
                                                                                              : "dalla data scelta con --validita";
    return m == kzero::ChoiceMode::Activation ? "dall'attivazione, scelta --validita-scheda" : m == kzero::ChoiceMode::TakenInCharge
                                                                                                 ? "dalla presa in carico, scelta --validita-scheda"
                                                                                                 : "dalla data scelta con --validita-scheda";
}

struct Problema {
    std::string codice, livello, messaggio;
    int riga = 0;
};

// I dati particolari di una scheda letta (tipo, pedana, individualizzato, accompagnatore richiesto), dal pacchetto nuovo.
struct Particolari {
    std::string tipo;
    bool pedana = false, individualizzato = false, assistente = false;
};
std::optional<Particolari> particolari_di(const kzero::Index& in, const kzero::Code& servizio) {
    const kzero::Service* s = in.service(servizio);
    if (!s || !s->kind.known()) return std::nullopt;
    Particolari p;
    static const char* tipi[] = {"linea", "scolastico", "disabili", "ncc", "altro"};
    p.tipo = tipi[static_cast<int>(s->kind.value())];
    p.pedana = s->rules.ramp_required.value_or(false);
    p.individualizzato = s->individual.value_or(false);
    for (const kzero::PassengerGroup& g : s->groups) p.assistente = p.assistente || g.assistant_required;
    return p;
}

Risultato uscita_lettura(const DefinizioneComando& comando, const Parametri& par, const Calcolo& c, const std::string& impronta, bool scritto,
                         const cassaforte::Riepilogo& r, std::size_t voci, const std::vector<accesso::AvvisoMostrato>& avvisi) {
    const Visibilita v{par.includi_dati_personali, par.includi_dati_particolari};
    const kzero::Index in(c.nuova.pacchetto);
    const Presentatore pr(&c.nuova.pacchetto, &c.nuova.versioni, v);
    std::vector<Problema> problemi;
    int riservati = 0;
    const auto mostra = [&](const kzero::Problem& p) {
        const DefinizioneControllo& d = definizione_controllo(p.check);
        if (d.rivela_variante(p.variant) == Classe::Particolare && !v.particolari) {
            ++riservati;
            return;
        }
        problemi.push_back({d.codice, nome_livello(d.livello), pr.messaggio(p), p.where.line});
    };
    for (const kzero::Problem& p : c.letto.problems) mostra(p);
    for (const kzero::Problem& p : c.confronto.problems) mostra(p);
    // Gli avvisi sul registro nuovo per le schede di questo file (sovrapposizioni, accompagnatore che manca).
    std::set<std::string> toccate;
    for (const kzero::CardOutcome& o : c.confronto.outcomes)
        if (!o.card_code.empty()) toccate.insert(o.card_code);
    std::vector<kzero::Problem> sul_registro;
    kzero::check_cards(c.nuova.pacchetto, in, sul_registro);
    for (const kzero::Problem& p : sul_registro) {
        if (p.check != kzero::Check::CardServiceOverlap && p.check != kzero::Check::CardEscortMissing) continue;
        bool nostra = false;
        for (const kzero::Param& x : p.params) nostra = nostra || (x.element == kzero::Element::ServiceCard && toccate.count(x.text));
        if (nostra) mostra(p);
    }
    std::map<std::string, int> conti;
    for (const kzero::CardOutcome& o : c.confronto.outcomes) ++conti[nome_esito(o.kind)];
    const auto conto = [&](const char* k) { return conti.count(k) ? conti.at(k) : 0; };
    const std::string presa = kzero::text_from_day(*par.data_sblocco);
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["utente"] = par.utente;
        j["pacchetto"] = {{"codice", c.nuova.pacchetto.manifest.code}, {"nome", c.nuova.pacchetto.manifest.name}};
        j["profilo"] = par.profilo;
        j["committente"] = c.committente;
        j["contratto"] = par.contratto;
        j["presa_in_carico"] = presa;
        j["riferimento"] = par.riferimento;
        j["impronta"] = impronta;
        j["scritto"] = scritto;
        j["righe"] = {{"schede", c.letto.card_rows}, {"fermate", c.letto.stop_rows}, {"date", c.letto.date_rows}};
        j["conteggi"] = {{"nuove", conto("nuova")}, {"versioni_nuove", conto("versione_nuova")}, {"uguali", conto("uguale")},
                         {"ferme", conto("ferma")}, {"senza_scelta", conto("senza_scelta")}};
        Json schede = Json::array();
        for (const kzero::CardOutcome& o : c.confronto.outcomes) {
            Json s = {{"riga", o.row}, {"nr", o.nr.empty() ? Json(nullptr) : Json(o.nr)}, {"id", o.id.empty() ? Json(nullptr) : Json(o.id)},
                      {"esito", nome_esito(o.kind)}};
            const bool scritta = o.kind == kzero::CardOutcome::Kind::New || o.kind == kzero::CardOutcome::Kind::NewVersion;
            s["codice"] = o.card_code.empty() ? Json(nullptr) : Json(o.card_code);
            s["versione"] = o.version ? Json(o.version) : Json(nullptr);
            const bool con_date = scritta || o.kind == kzero::CardOutcome::Kind::Same;
            s["dal"] = con_date ? Json(kzero::text_from_day(o.valid.from)) : Json(nullptr);
            s["al"] = con_date ? Json(kzero::text_from_day(o.valid.to)) : Json(nullptr);
            s["scelta"] = scritta ? Json{{"modo", nome_modo(o.mode)}, {"da", nome_da(o.from)}} : Json(nullptr);
            s["elementi"] = scritta ? Json{{"servizio", o.service}, {"gruppo", o.group}, {"corsa", o.trip}, {"calendario", o.calendar}} : Json(nullptr);
            s["case_nuove"] = o.new_homes;
            s["versione_accorciata"] = o.cut_version ? Json{{"versione", *o.cut_version}, {"al", kzero::text_from_day(o.cut_to)}} : Json(nullptr);
            s["versioni_tolte"] = o.removed_versions;
            s["giornate_prima_della_presa_in_carico"] =
                o.kind == kzero::CardOutcome::Kind::NewVersion && o.valid.from < *par.data_sblocco ? Json(o.days_before_taken) : Json(nullptr);
            Json cambia = Json::array();
            for (const kzero::CardChange& x : o.changes) {
                const bool vede = !x.particular || v.particolari;
                cambia.push_back({{"cosa", x.what}, {"prima", vede ? Json(x.before) : Json(nullptr)}, {"dopo", vede ? Json(x.after) : Json(nullptr)},
                                  {"riservato", !vede}});
            }
            s["cambia"] = cambia;
            const auto pa = scritta && v.particolari ? particolari_di(in, o.service) : std::nullopt;
            s["particolari"] = pa ? Json{{"tipo", pa->tipo}, {"pedana_obbligatoria", pa->pedana}, {"individualizzato", pa->individualizzato},
                                         {"assistente_richiesto", pa->assistente}}
                                  : Json(nullptr);
            schede.push_back(s);
        }
        j["schede"] = schede;
        Json pj = Json::array();
        for (const Problema& p : problemi)
            pj.push_back({{"codice", p.codice}, {"livello", p.livello}, {"messaggio", p.messaggio}, {"riga", p.riga ? Json(p.riga) : Json(nullptr)}});
        j["problemi"] = pj;
        j["problemi_riservati"] = riservati;
        Json av = Json::array();
        for (const accesso::AvvisoMostrato& a : avvisi)
            av.push_back({{"codice", a.codice.empty() ? Json(nullptr) : Json(a.codice)}, {"messaggio", a.messaggio}});
        j["avvisi"] = av;
        j["cassaforte"] = {{"id", r.id}, {"generazione", r.generazione}, {"voci_registro", voci}};
        return {testo_json(j), 0};
    }
    std::string t;
    t += std::string(scritto ? "Schede lette e scritte nel pacchetto" : "Anteprima (niente e' stato scritto: per scrivere, --scrivi)") +
         ": profilo " + par.profilo + ", committente " + c.committente + ", contratto " + par.contratto + ", presa in carico " + presa +
         ", riferimento \"" + par.riferimento + "\".\n";
    t += "File: impronta " + impronta + " (del file si tiene solo l'impronta).\n";
    const auto n = [](int k, const char* uno, const char* tanti) { return std::to_string(k) + " " + (k == 1 ? uno : tanti); };
    t += "Righe: " + n(c.letto.card_rows, "scheda", "schede") + ", " + n(c.letto.stop_rows, "fermata", "fermate") + ", " +
         n(c.letto.date_rows, "data", "date") + ".\n";
    t += "Schede: " + n(conto("nuova"), "nuova", "nuove") + ", " + n(conto("versione_nuova"), "versione nuova", "versioni nuove") + ", " +
         n(conto("uguale"), "uguale", "uguali") + " (niente da fare), " + n(conto("ferma"), "ferma", "ferme") + ", " +
         n(conto("senza_scelta"), "senza", "senza") + " la scelta della data.\n";
    for (const kzero::CardOutcome& o : c.confronto.outcomes) {
        t += "  " + (o.nr.empty() ? std::string("(senza Nr)") : o.nr) + " (riga " + std::to_string(o.row) + "): ";
        const std::string periodo = "vale dal " + kzero::text_from_day(o.valid.from) + " al " + kzero::text_from_day(o.valid.to);
        const std::string elementi = "servizio " + o.service + ", gruppo " + o.group + ", corsa " + o.trip + ", calendario " + o.calendar;
        switch (o.kind) {
            case kzero::CardOutcome::Kind::New:
                t += "scheda nuova " + o.card_code + ", versione 1, " + periodo + " (" + testo_scelta(o.mode, o.from) + "); " + elementi;
                if (!o.new_homes.empty()) {
                    std::string cc;
                    for (const kzero::Code& h : o.new_homes) cc += (cc.empty() ? "" : ", ") + h;
                    t += "; case porta a porta nuove (senza indirizzo): " + cc;
                }
                t += ".\n";
                break;
            case kzero::CardOutcome::Kind::NewVersion: {
                t += "versione nuova " + std::to_string(o.version) + " della scheda " + o.card_code + ", " + periodo + " (" +
                     testo_scelta(o.mode, o.from) + "); " + elementi + ".\n";
                if (o.cut_version)
                    t += "     La versione " + std::to_string(*o.cut_version) + " vale fino al " + kzero::text_from_day(o.cut_to) +
                         " (i giorni prima del " + kzero::text_from_day(o.valid.from) + " non cambiano).\n";
                if (!o.removed_versions.empty()) {
                    std::string vv;
                    for (int n : o.removed_versions) vv += (vv.empty() ? "" : ", ") + std::to_string(n);
                    t += "     Versioni tolte (cominciavano dal " + kzero::text_from_day(o.valid.from) + " in poi): " + vv + ".\n";
                }
                if (o.valid.from < *par.data_sblocco)
                    t += "     Giornate prima della presa in carico che cambiano: " + std::to_string(o.days_before_taken) + ".\n";
                for (const kzero::CardChange& x : o.changes) {
                    if (x.particular && !v.particolari) {
                        t += "     Cambia " + x.what + " (riservato: --includi-dati-particolari).\n";
                        continue;
                    }
                    t += "     Cambia " + x.what + ": prima " + x.before + "; dopo " + x.after + ".\n";
                }
                break;
            }
            case kzero::CardOutcome::Kind::Same:
                t += "uguale alla versione " + std::to_string(o.version) + " della scheda " + o.card_code + ": niente da fare.\n";
                break;
            case kzero::CardOutcome::Kind::Stopped: t += "ferma (K-CTR-160, sotto): non si legge.\n"; break;
            case kzero::CardOutcome::Kind::ChoiceMissing: t += "ferma: manca la scelta della data (K-CTR-162, sotto).\n"; break;
        }
        const bool scritta = o.kind == kzero::CardOutcome::Kind::New || o.kind == kzero::CardOutcome::Kind::NewVersion;
        if (scritta && v.particolari)
            if (const auto pa = particolari_di(in, o.service))
                t += std::string("     Tipo ") + pa->tipo + ", pedana obbligatoria " + (pa->pedana ? "si'" : "no") + ", servizio individualizzato " +
                     (pa->individualizzato ? "si'" : "no") + ", accompagnatore richiesto " + (pa->assistente ? "si'" : "no") + ".\n";
    }
    for (const Problema& p : problemi) t += (p.livello == "errore" ? "Errore " : "Avviso ") + p.codice + ": " + p.messaggio + "\n";
    if (riservati) t += std::to_string(riservati) + (riservati == 1 ? " problema" : " problemi") + " sui dati particolari (salute), non elencati: --includi-dati-particolari.\n";
    if (!v.particolari) t += "Tipo, pedana, servizio individualizzato e accompagnatore richiesto non mostrati: --includi-dati-particolari.\n";
    t += "Cassaforte " + r.id + ", chiusura numero " + std::to_string(r.generazione) + "; registro degli accessi: " + std::to_string(voci) +
         (voci == 1 ? " voce.\n" : " voci.\n");
    for (const accesso::AvvisoMostrato& a : avvisi) t += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return {t, 0};
}

std::string nota_registro(const Calcolo& c) {
    std::map<std::string, int> conti;
    for (const kzero::CardOutcome& o : c.confronto.outcomes) ++conti[nome_esito(o.kind)];
    std::string numeri;
    for (const auto& [k, n] : conti) numeri += (numeri.empty() ? "" : ", ") + k + " " + std::to_string(n);
    return numeri.empty() ? std::string("nessuna scheda") : numeri;
}

}  // namespace

// --- leggi-schede ---------------------------------------------------------------------------------------------------------------

Risultato esegui_leggi_schede(const DefinizioneComando& comando, const Parametri& par) {
    const std::string& percorso_file = par.argomenti.at(0);
    const fs::path cartella(par.argomenti.at(1));
    const bool scrivere = !par.scrivi.empty();
    if (scrivere) elenco::stessa_cartella(cartella.string(), par.scrivi);
    // Prima il file (e i suoi errori), senza password: un file rovinato non apre niente.
    const FileSchede file = leggi_file(percorso_file);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    permessi_prima(cartella, par.utente, comando.nome);
    // Il profilo e' fatto di dati comuni (file normale): serve gia' per i fogli da leggere e per le scelte della data.
    const Lettura senza_valori = leggi_pacchetto(CartellaSuDisco(cartella));
    pacchetto_valido(senza_valori, cartella.string(), par.utente);
    const kzero::CardProfile& profilo = profilo_di(senza_valori, par.profilo);
    const Tabelle tabelle = tabelle_da(file, profilo);
    (void)scelte_di(par, profilo);   // una scelta scritta male si dice prima della password

    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const std::string nota_base = "schede dei servizi, profilo " + par.profilo + ", contratto " + par.contratto + ", file " +
                                  file.impronta.substr(0, 16);
    if (!scrivere) {
        accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome,
                                            Visibilita{par.includi_dati_personali, par.includi_dati_particolari},
                                            "anteprima della lettura delle " + nota_base, accesso::ambiente());
        permessi_schede(a.cassaforte.permessi(), par.utente, comando.nome);
        const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
        const Calcolo c = calcola(leggi_pacchetto(CartellaSuDisco(cartella), &v), par, tabelle, cartella.string());
        const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
        return uscita_lettura(comando, par, c, file.impronta, false, r, a.registro.voci.size(), elenco::avvisi_di(r, par.utente));
    }
    std::optional<Calcolo> calcolo;
    const cassaforte::Riepilogo r = elenco::scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        calcolo = calcola(vecchia, par, tabelle, cartella.string());
        return std::make_pair(calcolo->nuova, "lettura delle " + nota_base + ": " + nota_registro(*calcolo));
    });
    return uscita_lettura(comando, par, *calcolo, file.impronta, true, r, r.registro_voci, elenco::avvisi_di(r, par.utente));
}

// --- togli-scheda -----------------------------------------------------------------------------------------------------------------

namespace {

struct Togli {
    kzero::CardRemoval esito;
    std::string id;
    Lettura nuova;
};

Togli calcola_togli(const Lettura& vecchia, const Parametri& par, const std::string& cartella) {
    pacchetto_valido(vecchia, cartella, par.utente);
    std::vector<kzero::ServiceCard> schede = vecchia.pacchetto.service_cards;
    std::vector<kzero::RemovedCardVersions> tolte = vecchia.pacchetto.removed_cards;
    std::string codice;
    for (const kzero::ServiceCard& s : schede) {
        std::string a = kzero::clean_cell(s.id), b = kzero::clean_cell(par.scheda);
        for (std::string* x : {&a, &b})
            for (char& ch : *x)
                if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
        if (s.code == par.scheda || a == b) codice = s.code;
    }
    if (codice.empty())
        throw ErroreKZero(Errore::SchedaNonTrovata, "La scheda '" + par.scheda + "' non c'e' nel registro (ne' come codice ne' come ID).");
    Togli t;
    for (const kzero::ServiceCard& s : schede)
        if (s.code == codice) t.id = s.id;
    t.esito = kzero::remove_card(schede, tolte, codice, par.tutte, par.riferimento, *par.data_sblocco, vecchia.pacchetto);
    switch (t.esito.refusal) {
        case kzero::CardRemovalRefusal::None: break;
        case kzero::CardRemovalRefusal::RemovedOthers:
            throw ErroreKZero(Errore::ToglimentoNonAmmesso, "L'ultima versione della scheda " + codice + " ha tolto altre versioni (cominciavano "
                                                            "dalla sua data in poi): non si possono rimettere. Si toglie solo tutta la "
                                                            "scheda (--tutte). Niente e' cambiato.");
        case kzero::CardRemovalRefusal::TripInUse:
            throw ErroreKZero(Errore::ToglimentoNonAmmesso, "La corsa " + t.esito.trip + " della scheda " + codice + " e' usata da " +
                                                                t.esito.user + " (un turno o un blocco): prima toglierla da li'. Niente e' "
                                                                "cambiato.");
    }
    std::vector<kzero::CardsMerge::Cut> fini;
    if (t.esito.restored) fini.push_back(*t.esito.restored);
    t.nuova = schede::con_documenti(vecchia, schede::documenti_dopo(vecchia, schede, tolte, {}, t.esito.removed, fini, {}));
    return t;
}

Risultato uscita_togli(const DefinizioneComando& comando, const Parametri& par, const Togli& t, bool scritto, const cassaforte::Riepilogo& r,
                       std::size_t voci, const std::vector<accesso::AvvisoMostrato>& avvisi) {
    std::vector<int> numeri;
    for (const kzero::CardVersion& v : t.esito.removed) numeri.push_back(v.number);
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["utente"] = par.utente;
        j["scheda"] = t.esito.card;
        j["id"] = t.id;
        j["riferimento"] = par.riferimento;
        j["data"] = kzero::text_from_day(*par.data_sblocco);
        j["scritto"] = scritto;
        j["versioni_tolte"] = numeri;
        Json el = Json::array();
        for (const kzero::CardVersion& v : t.esito.removed)
            el.push_back({{"versione", v.number}, {"servizio", v.service}, {"gruppo", v.group}, {"corsa", v.trip}, {"calendario", v.calendar}});
        j["elementi_tolti"] = el;
        j["scheda_tolta"] = t.esito.card_gone;
        j["versione_ripresa"] = t.esito.restored ? Json{{"versione", t.esito.restored_version}, {"al", kzero::text_from_day(t.esito.restored->to)}}
                                                 : Json(nullptr);
        Json av = Json::array();
        for (const accesso::AvvisoMostrato& a : avvisi)
            av.push_back({{"codice", a.codice.empty() ? Json(nullptr) : Json(a.codice)}, {"messaggio", a.messaggio}});
        j["avvisi"] = av;
        j["cassaforte"] = {{"id", r.id}, {"generazione", r.generazione}, {"voci_registro", voci}};
        return {testo_json(j), 0};
    }
    std::string vv;
    for (int n : numeri) vv += (vv.empty() ? "" : ", ") + std::to_string(n);
    std::string t_ = std::string(scritto ? "" : "Anteprima (niente e' stato scritto: per scrivere, --scrivi). ") + "Scheda " + t.esito.card +
                     " (ID " + t.id + "): " + (numeri.size() == 1 ? "tolta la versione " : "tolte le versioni ") + vv + ", riferimento \"" +
                     par.riferimento + "\", data " + kzero::text_from_day(*par.data_sblocco) + ".\n";
    for (const kzero::CardVersion& v : t.esito.removed)
        t_ += "  Versione " + std::to_string(v.number) + ": tolti servizio " + v.service + ", gruppo " + v.group + ", corsa " + v.trip +
              ", calendario " + v.calendar + ".\n";
    if (t.esito.card_gone) t_ += "La scheda non c'e' piu' nel registro (restano scritti il riferimento e la data; i numeri delle versioni non si danno piu').\n";
    if (t.esito.restored)
        t_ += "La versione " + std::to_string(t.esito.restored_version) + " riprende la sua fine: vale fino al " +
              kzero::text_from_day(t.esito.restored->to) + ".\n";
    t_ += "Cassaforte " + r.id + ", chiusura numero " + std::to_string(r.generazione) + "; registro degli accessi: " + std::to_string(voci) +
          (voci == 1 ? " voce.\n" : " voci.\n");
    for (const accesso::AvvisoMostrato& a : avvisi) t_ += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return {t_, 0};
}

}  // namespace

Risultato esegui_togli_scheda(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    const bool scrivere = !par.scrivi.empty();
    if (scrivere) elenco::stessa_cartella(cartella.string(), par.scrivi);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    permessi_prima(cartella, par.utente, comando.nome);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const std::string nota = "scheda dei servizi da togliere: " + par.scheda.substr(0, 64) + (par.tutte ? " (tutte le versioni)" : "");
    if (!scrivere) {
        accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome,
                                            Visibilita{par.includi_dati_personali, par.includi_dati_particolari}, "anteprima: " + nota,
                                            accesso::ambiente());
        permessi_schede(a.cassaforte.permessi(), par.utente, comando.nome);
        const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
        const Togli t = calcola_togli(leggi_pacchetto(CartellaSuDisco(cartella), &v), par, cartella.string());
        const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
        return uscita_togli(comando, par, t, false, r, a.registro.voci.size(), elenco::avvisi_di(r, par.utente));
    }
    std::optional<Togli> t;
    const cassaforte::Riepilogo r = elenco::scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        t = calcola_togli(vecchia, par, cartella.string());
        std::string vv;
        for (const kzero::CardVersion& v : t->esito.removed) vv += (vv.empty() ? "" : ", ") + std::to_string(v.number);
        return std::make_pair(t->nuova, "scheda dei servizi tolta: " + t->esito.card + ", versioni " + vv);
    });
    return uscita_togli(comando, par, *t, true, r, r.registro_voci, elenco::avvisi_di(r, par.utente));
}

}  // namespace kzero::api
