// K-Zero API - l'elenco utenti del committente (K-019): kzero leggi-elenco e kzero sblocco. Uscite: schemi "elenco" e "sblocco".
//
// Tutti e due lavorano solo su un pacchetto chiuso (i dati personali stanno solo nella cassaforte) e chiedono, per ora, i
// permessi dei dati personali e particolari (come richiudi): il controllo sta in un punto solo, permessi_per_l_elenco, dove
// arrivera' la scrittura con i soli dati personali (docs/cassaforte.md). Chi scrive nomina la cartella che cambia con
// --scrivi, che deve essere quella del pacchetto. La scrittura e' quella di richiudi (riscrittura.hpp): il pacchetto nuovo si
// prepara in memoria, si riapre con la password e deve dare gli stessi dati, poi cassaforte e file normali.
#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "accesso.hpp"
#include "chiusura.hpp"
#include "controlli_catalogo.hpp"
#include "elenco.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "excel.hpp"
#include "riscrittura.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

namespace fs = std::filesystem;
constexpr const char* kElenco = "elenco_utenti.json";

// --- Permessi e cartelle ------------------------------------------------------------------------------------------------

// Il controllo dei permessi dell'elenco, in un punto solo. Oggi: tutti e due (la lettura scrive nomi e case nella cassaforte, e
// un utente legato a un luogo sensibile sta tra i legati, che si cifrano con le due chiavi). Domani la scrittura con i soli dati
// personali passera' da qui: riscrivere la sola sezione dei personali, copiare cosi' come sono quelle dei particolari e dei
// legati, e chiedere ancora tutti e due i permessi se una novita' tocca un utente legato.
void permessi_per_l_elenco(const cassaforte::Permessi& p, const std::string& utente, const std::string& comando) {
    if (p.tutti()) return;
    throw ErroreKZero(Errore::PermessoMancante, "kzero " + comando + " scrive nella cassaforte nomi e case degli utenti dell'elenco: "
                                                "per ora lo puo' fare solo chi ha i dati personali e particolari (l'utente " + utente +
                                                " ha " + cassaforte::testo_permessi(p) + ").");
}

// Prima della password, dall'intestazione ancora da autenticare: solo per fermarsi subito (si ricontrolla dopo l'apertura).
void permessi_prima_della_password(const fs::path& cartella, const std::string& utente, const std::string& comando) {
    std::vector<cassaforte::UtenteVisibile> utenti;
    try {
        utenti = cassaforte::leggi_intestazione(leggi_dal_disco(cartella, cassaforte::kFile, kMassimoByteFile).testo).utenti;
    } catch (const cassaforte::ErroreCassaforte&) {
        return;   // lo dira' l'apertura
    }
    for (const cassaforte::UtenteVisibile& u : utenti)
        if (u.codice == utente) permessi_per_l_elenco(u.permessi, utente, comando);
}

// --scrivi deve nominare la cartella del pacchetto (stessa cartella, anche scritta in un altro modo).
void stessa_cartella(const std::string& pacchetto, const std::string& scrivi) {
    std::error_code e1, e2;
    const fs::path a = fs::weakly_canonical(fs::absolute(pacchetto), e1);
    const fs::path b = fs::weakly_canonical(fs::absolute(scrivi), e2);
    if (e1 || e2 || a != b)
        throw ErroreKZero(Errore::ScriviDiversa, "--scrivi deve nominare la cartella del pacchetto che cambia ('" + pacchetto +
                                                     "'), invece e' '" + scrivi + "': niente e' stato scritto.");
}

// --- Il file del committente ----------------------------------------------------------------------------------------------

struct FileDelCommittente {
    std::string byte;
    std::string impronta;   // BLAKE2b-256 in esadecimale: del file si tiene solo questa, mai il nome
};

FileDelCommittente leggi_file(const std::string& percorso) {
    const FileLetto f = leggi_file_esterno(percorso, excel::limiti().file_byte);
    switch (f.esito) {
        case FileLetto::Esito::Letto: break;
        case FileLetto::Esito::TroppoGrande:
            throw ErroreKZero(Errore::FileExcelNonValido, "Il file e' piu' grande del massimo (" +
                                                              std::to_string(excel::limiti().file_byte / (1024 * 1024)) +
                                                              " MB): non e' un normale elenco di utenti.");
        case FileLetto::Esito::NonNormale:
            throw ErroreKZero(Errore::FileDaLeggereIlleggibile, "Quello indicato non e' un file normale (es. una cartella o un "
                                                                "dispositivo): indicare il file .xlsx del committente.");
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

excel::Foglio foglio_da(const FileDelCommittente& f, const std::string& nome_foglio) {
    try {
        return excel::leggi_foglio(f.byte, nome_foglio);
    } catch (const excel::ErroreExcel& e) {
        if (e.motivo == excel::ErroreExcel::Motivo::VecchioXls) throw ErroreKZero(Errore::FormatoXls, e.what());
        throw ErroreKZero(Errore::FileExcelNonValido, std::string(e.what()) + " Niente e' stato letto.");
    }
}

// --- Il documento elenco_utenti.json --------------------------------------------------------------------------------------

std::string nome_giorno(kzero::Weekday g) {
    static const char* nomi[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
    return nomi[static_cast<int>(g)];
}

JsonDati utente_json(const kzero::ListUser& u) {
    JsonDati j = JsonDati::object();
    j["codice"] = u.code;
    j["nome"] = u.name;
    if (u.home) {
        JsonDati c = JsonDati::object();
        if (!u.home->town.empty()) c["comune"] = u.home->town;
        if (!u.home->hamlet.empty()) c["frazione"] = u.home->hamlet;
        if (!u.home->street.empty()) c["via"] = u.home->street;
        if (!u.home->number.empty()) c["civico"] = u.home->number;
        j["casa"] = c;
    }
    j["scuola"] = u.school.code;
    if (u.class_number) j["classe"] = *u.class_number;
    if (!u.section.empty()) j["sezione"] = u.section;
    JsonDati stato = {{"tipo", u.active ? "attivo" : "sospeso"}};
    if (!u.status_note.empty()) stato["nota"] = u.status_note;
    j["stato"] = stato;
    if (u.stop) j["fermata"] = {{"luogo", u.stop->code}, {"riconosciuta", u.stop_match == kzero::StopMatch::Name ? "nome" : "codice_esterno"}};
    else if (!u.stop_text.empty()) j["fermata"] = {{"testo", u.stop_text}};
    if (!u.legs.empty()) {
        JsonDati v = JsonDati::array();
        for (const kzero::ListUserLeg& l : u.legs) {
            JsonDati x = {{"sigla", l.leg}};
            if (l.company) x["azienda"] = l.company->code;
            else x["altra_ditta"] = l.other_company;
            v.push_back(x);
        }
        j["tratte"] = v;
    }
    if (!u.times.empty()) {
        JsonDati v = JsonDati::array();
        for (const kzero::ListUserTime& t : u.times) {
            JsonDati x = {{"tratta", t.leg}, {"giorno", nome_giorno(t.day)}, {"ora", kzero::text_from_minutes(t.time)}};
            if (!t.period.empty()) x["periodo"] = t.period;
            x["dal"] = kzero::text_from_day(t.dates.from);
            x["al"] = kzero::text_from_day(t.dates.to);
            v.push_back(x);
        }
        j["orari"] = v;
    }
    if (!u.unlocks.empty()) {
        JsonDati v = JsonDati::array();
        for (const kzero::ListUserUnlock& s : u.unlocks)
            v.push_back({{"tratta", s.leg}, {"riferimento", s.reference}, {"data", kzero::text_from_day(s.date)}});
        j["sblocchi"] = v;
    }
    if (u.out_of_list) j["fuori_elenco"] = true;
    return j;
}

JsonDati elenco_json(const kzero::ClientList& l) {
    JsonDati j = JsonDati::object();
    j["committente"] = l.client.code;
    j["profilo"] = l.profile.code;
    j["anno_scolastico"] = std::to_string(l.first_year) + "-" + std::to_string(l.first_year + 1);
    if (l.has_last_reading) j["ultima_lettura"] = {{"impronta", l.last_fingerprint}, {"elenco", l.last_complete ? "completo" : "parziale"}};
    JsonDati utenti = JsonDati::array();
    for (const kzero::ListUser& u : l.users) utenti.push_back(utente_json(u));
    j["utenti"] = utenti;
    return j;
}

// Il documento nuovo: quello di prima (se c'e') con l'elenco di quel committente sostituito o aggiunto, in ordine di committente.
JsonDati documento_con(const Lettura& vecchia, const kzero::ClientList& l, int ultimo_numero) {
    JsonDati d = vecchia.documenti.count(kElenco)
                     ? vecchia.documenti.at(kElenco)
                     : JsonDati{{"schema", "elenco_utenti"}, {"schema_versione", 1}, {"ultimo_numero", 0}, {"elenchi", JsonDati::array()}};
    d["ultimo_numero"] = ultimo_numero;
    JsonDati elenchi = JsonDati::array();
    bool messo = false;
    for (const JsonDati& e : d.at("elenchi")) {
        if (e.at("committente").get<std::string>() == l.client.code) {
            elenchi.push_back(elenco_json(l));
            messo = true;
        } else {
            elenchi.push_back(e);
        }
    }
    if (!messo) elenchi.push_back(elenco_json(l));
    std::stable_sort(elenchi.begin(), elenchi.end(), [](const JsonDati& a, const JsonDati& b) {
        return a.at("committente").get<std::string>() < b.at("committente").get<std::string>();
    });
    d["elenchi"] = elenchi;
    return d;
}

// Il pacchetto in chiaro, in memoria, con un file cambiato. Un documento preparato dal programma che non rispetta lo schema e'
// un errore del programma (uscita 3), mai un pacchetto scritto a meta'.
Lettura con_documento(const Lettura& vecchia, const std::string& file, const JsonDati& doc) {
    PacchettoInMemoria m;
    for (const auto& [f, d] : vecchia.documenti)
        m.file[f] = chiusura::testo_file(f == "pacchetto.json" ? chiusura::manifesto_in_chiaro(d) : d);
    m.file[file] = chiusura::testo_file(doc);
    Lettura l = leggi_pacchetto(m);
    if (!l.forma_valida()) throw std::logic_error("il documento preparato non rispetta lo schema");
    return l;
}

const kzero::ClientList* elenco_di(const kzero::Package& p, const std::string& committente) {
    for (const kzero::ClientList& l : p.client_lists)
        if (l.client.code == committente) return &l;
    return nullptr;
}

// --- Calcolo della lettura ------------------------------------------------------------------------------------------------

struct Calcolo {
    std::string committente;
    kzero::ListRead letto;
    kzero::ListMerge confronto;
    Lettura vecchia;
    Lettura nuova;
};

const kzero::ReadingProfile& profilo_di(const Lettura& l, const std::string& codice) {
    for (const kzero::ReadingProfile& p : l.pacchetto.reading_profiles)
        if (p.code == codice) return p;
    throw ErroreKZero(Errore::ProfiloNonTrovato, "Il profilo di lettura '" + codice + "' non c'e' nel pacchetto (profili_elenchi.json).");
}

void pacchetto_completo(const Lettura& l, const std::string& cartella, const std::string& utente) {
    if (!l.usato("pacchetto.json") || !l.forma_valida())
        throw ErroreKZero(Errore::PacchettoConErrori, "Il pacchetto ha errori nella forma dei file: prima vanno corretti (kzero controlla " +
                                                          cartella + " --utente " + utente + ").");
    if (l.pacchetto.manifest.level == kzero::Level::NumbersOnly)
        throw ErroreKZero(Errore::ElencoSoloNumeri, "Il pacchetto e' di livello \"solo numeri\": l'elenco utenti (nomi e case uno per "
                                                    "uno) sta solo in un pacchetto di livello \"completo\" (docs/formato-dati.md).");
}

Calcolo calcola(Lettura vecchia, const Parametri& par, const kzero::Table& tabella, const std::string& impronta, const std::string& cartella) {
    pacchetto_completo(vecchia, cartella, par.utente);
    const kzero::ReadingProfile& profilo = profilo_di(vecchia, par.profilo);
    const kzero::Index idx(vecchia.pacchetto);
    Calcolo c;
    c.committente = profilo.client.code;
    c.letto = kzero::read_list(tabella, profilo, idx, par.anno_scolastico, elenco_di(vecchia.pacchetto, profilo.client.code));
    if (c.letto.fault != kzero::TableFault::None) {
        const std::string t = c.letto.fault_title;
        throw ErroreKZero(Errore::ColonnaMancante,
                          c.letto.fault == kzero::TableFault::NoRows ? std::string("Il foglio e' vuoto: manca anche la riga dei titoli.")
                          : c.letto.fault == kzero::TableFault::MissingTitle
                              ? "Nella prima riga del foglio manca la colonna \"" + t + "\" del profilo " + profilo.code +
                                    ": forse il committente ha cambiato il file, o il profilo e' sbagliato. Niente e' stato letto."
                              : "Nella prima riga del foglio la colonna \"" + t + "\" del profilo " + profilo.code +
                                    " c'e' piu' di una volta: non si sa quale leggere. Niente e' stato letto.");
    }
    c.confronto = kzero::merge_list(vecchia.pacchetto, profilo, par.anno_scolastico, c.letto, par.elenco == "completo", impronta);
    c.nuova = con_documento(vecchia, kElenco, documento_con(vecchia, c.confronto.list, c.confronto.last_number));
    c.vecchia = std::move(vecchia);
    return c;
}

// --- Uscita di leggi-elenco -------------------------------------------------------------------------------------------------

const char* nome_esito(kzero::ListChange::Kind k) {
    switch (k) {
        case kzero::ListChange::Kind::New: return "nuovo";
        case kzero::ListChange::Kind::Changed: return "cambiato";
        case kzero::ListChange::Kind::Unchanged: return "invariato";
        case kzero::ListChange::Kind::Exited: return "uscito";
        case kzero::ListChange::Kind::KeptForUnlock: return "tenuto_per_sblocco";
        case kzero::ListChange::Kind::NotInFile: return "non_nel_file";
        case kzero::ListChange::Kind::Unread: return "riga_illeggibile";
    }
    return "?";
}

const char* testo_esito(kzero::ListChange::Kind k) {
    switch (k) {
        case kzero::ListChange::Kind::New: return "nuovo";
        case kzero::ListChange::Kind::Changed: return "cambiato";
        case kzero::ListChange::Kind::Unchanged: return "invariato";
        case kzero::ListChange::Kind::Exited: return "uscito (tolto dall'elenco)";
        case kzero::ListChange::Kind::KeptForUnlock: return "uscito, ma resta \"fuori elenco\" per uno sblocco (mai nel piano)";
        case kzero::ListChange::Kind::NotInFile: return "non e' nel file: resta com'e' (uscite non applicate)";
        case kzero::ListChange::Kind::Unread: return "la sua riga non si legge: resta com'e'";
    }
    return "?";
}

struct Problema {
    std::string codice, livello, messaggio;
    int riga = 0, colonna = 0;
};

Problema mostra(const Presentatore& pr, const kzero::Problem& p) {
    const DefinizioneControllo& d = definizione_controllo(p.check);
    return {d.codice, nome_livello(d.livello), pr.messaggio(p), p.where.line, p.where.column};
}

Json numero_o_null(int n) { return n > 0 ? Json(n) : Json(nullptr); }

Risultato uscita_lettura(const DefinizioneComando& comando, const Parametri& par, const Calcolo& c, const std::string& impronta,
                         bool scritto, const cassaforte::Riepilogo& r, std::size_t voci, const std::vector<accesso::AvvisoMostrato>& avvisi) {
    const Visibilita v{par.includi_dati_personali, par.includi_dati_particolari};
    // I nomi: con --includi-dati-personali; di chi e' legato a un luogo sensibile solo anche con --includi-dati-particolari.
    const kzero::Index iv(c.vecchia.pacchetto), in(c.nuova.pacchetto);
    const LegameSensibile lv(&iv), ln(&in);
    std::map<std::string, std::string> nomi;
    for (const Lettura* l : {&c.vecchia, &c.nuova})
        for (const kzero::ClientList& x : l->pacchetto.client_lists)
            for (const kzero::ListUser& u : x.users) nomi[u.code] = u.name;
    const auto nome = [&](const std::string& codice) -> std::optional<std::string> {
        if (!v.personali || !nomi.count(codice)) return std::nullopt;
        if ((lv.utente(codice) || ln.utente(codice)) && !v.particolari) return std::nullopt;
        return nomi.at(codice);
    };
    // I problemi: le righe del file, il confronto, e gli avvisi sull'elenco nuovo di questo committente.
    const Presentatore pr(&c.nuova.pacchetto, &c.nuova.versioni, v);
    std::vector<Problema> problemi;
    for (const kzero::Problem& p : c.letto.problems) problemi.push_back(mostra(pr, p));
    for (const kzero::Problem& p : c.confronto.warnings) problemi.push_back(mostra(pr, p));
    std::set<std::string> codici;
    for (const kzero::ListUser& u : c.confronto.list.users) codici.insert(u.code);
    std::vector<kzero::Problem> sull_elenco;
    kzero::check_lists(c.nuova.pacchetto, in, sull_elenco);
    for (const kzero::Problem& p : sull_elenco) {
        if (p.where.file != kElenco) continue;
        bool di_questo = false;
        for (const kzero::Param& x : p.params) di_questo = di_questo || (x.name == "utente" && codici.count(x.text));
        if (di_questo) problemi.push_back(mostra(pr, p));
    }
    std::map<std::string, int> conti;
    for (const kzero::ListChange& x : c.confronto.changes) ++conti[nome_esito(x.kind)];
    const int illeggibili = static_cast<int>(c.letto.unreadable_rows.size());
    const std::string anno = std::to_string(par.anno_scolastico) + "-" + std::to_string(par.anno_scolastico + 1);
    const auto conto = [&](const char* k) { return conti.count(k) ? conti.at(k) : 0; };
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["utente"] = par.utente;
        j["pacchetto"] = {{"codice", c.nuova.pacchetto.manifest.code}, {"nome", c.nuova.pacchetto.manifest.name}};
        j["profilo"] = par.profilo;
        j["committente"] = c.committente;
        j["anno_scolastico"] = anno;
        j["elenco"] = par.elenco;
        j["impronta"] = impronta;
        j["scritto"] = scritto;
        j["uscite_applicate"] = c.confronto.exits_applied;
        j["righe"] = {{"dati", c.letto.data_rows}, {"lette", c.letto.data_rows - illeggibili}, {"illeggibili", illeggibili}};
        j["conteggi"] = {{"nuovi", conto("nuovo")}, {"cambiati", conto("cambiato")}, {"invariati", conto("invariato")},
                         {"usciti", conto("uscito")}, {"tenuti_per_sblocco", conto("tenuto_per_sblocco")},
                         {"non_nel_file", conto("non_nel_file")}, {"righe_illeggibili", conto("riga_illeggibile")}};
        Json utenti = Json::array();
        for (const kzero::ListChange& x : c.confronto.changes) {
            Json u = {{"codice", x.code}, {"esito", nome_esito(x.kind)}, {"riga", numero_o_null(x.row)}};
            u["campi"] = x.fields;
            const auto n = nome(x.code);
            u["nome"] = n ? Json(*n) : Json(nullptr);
            utenti.push_back(u);
        }
        j["utenti"] = utenti;
        Json pj = Json::array();
        for (const Problema& p : problemi)
            pj.push_back({{"codice", p.codice}, {"livello", p.livello}, {"messaggio", p.messaggio}, {"riga", numero_o_null(p.riga)},
                          {"colonna", numero_o_null(p.colonna)}});
        j["problemi"] = pj;
        Json av = Json::array();
        for (const accesso::AvvisoMostrato& a : avvisi)
            av.push_back({{"codice", a.codice.empty() ? Json(nullptr) : Json(a.codice)}, {"messaggio", a.messaggio}});
        j["avvisi"] = av;
        j["cassaforte"] = {{"id", r.id}, {"generazione", r.generazione}, {"voci_registro", voci}};
        return {testo_json(j), 0};
    }
    std::string t;
    t += std::string(scritto ? "Elenco utenti letto e scritto nella cassaforte" : "Anteprima (niente e' stato scritto: per scrivere, --scrivi)") +
         ": committente " + c.committente + ", profilo " + par.profilo + ", anno scolastico " + anno + ", elenco " + par.elenco + ".\n";
    t += "File: impronta " + impronta + " (del file si tiene solo l'impronta).\n";
    t += "Righe: " + std::to_string(c.letto.data_rows) + " (lette " + std::to_string(c.letto.data_rows - illeggibili) + ", illeggibili " +
         std::to_string(illeggibili) + ").\n";
    t += "Utenti: " + std::to_string(conto("nuovo")) + " nuovi, " + std::to_string(conto("cambiato")) + " cambiati, " +
         std::to_string(conto("invariato")) + " invariati, " + std::to_string(conto("uscito")) + " usciti";
    if (conto("tenuto_per_sblocco")) t += ", " + std::to_string(conto("tenuto_per_sblocco")) + " usciti tenuti per uno sblocco";
    if (conto("non_nel_file")) t += ", " + std::to_string(conto("non_nel_file")) + " non nel file (restano)";
    if (conto("riga_illeggibile")) t += ", " + std::to_string(conto("riga_illeggibile")) + " con la riga illeggibile (restano)";
    t += ".\n";
    if (par.elenco == "parziale") t += "Elenco parziale: nessuno esce.\n";
    else if (!c.confronto.exits_applied) t += "Elenco completo con righe illeggibili: le uscite non si applicano.\n";
    for (const kzero::ListChange& x : c.confronto.changes) {
        if (x.kind == kzero::ListChange::Kind::Unchanged) continue;
        const auto n = nome(x.code);
        t += "  " + x.code + (n ? " \"" + *n + "\"" : "") + (x.row ? " (riga " + std::to_string(x.row) + ")" : "") + ": " + testo_esito(x.kind);
        if (x.kind == kzero::ListChange::Kind::Changed) {
            std::string campi;
            for (const std::string& f : x.fields) campi += (campi.empty() ? "" : ", ") + f;
            t += " (" + campi + ")";
        }
        t += ".\n";
    }
    for (const Problema& p : problemi) t += (p.livello == "errore" ? "Errore " : "Avviso ") + p.codice + ": " + p.messaggio + "\n";
    if (!v.personali) t += "Nomi e case non mostrati: --includi-dati-personali.\n";
    t += "Cassaforte " + r.id + ", chiusura numero " + std::to_string(r.generazione) + "; registro degli accessi: " + std::to_string(voci) +
         (voci == 1 ? " voce.\n" : " voci.\n");
    for (const accesso::AvvisoMostrato& a : avvisi) t += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return {t, 0};
}

std::vector<accesso::AvvisoMostrato> avvisi_di(const cassaforte::Riepilogo& r, const std::string& utente) {
    std::vector<accesso::AvvisoMostrato> out;
    for (const kzero::Problem& p : accesso::avvisi_dell_apertura(r, utente)) out.push_back(accesso::mostra(p));
    return out;
}

// Scrive un pacchetto nuovo, preparato in memoria da "prepara" con la cassaforte aperta: lo riapre, poi cassaforte e file
// normali (riscrittura.hpp). prepara restituisce la lettura in chiaro nuova e la nota del registro.
cassaforte::Riepilogo scrivi(const fs::path& cartella, const ManifestoLetto& manifesto, const std::string& utente,
                             const cripto::Segreto& password, const std::string& comando,
                             const std::function<std::pair<Lettura, std::string>(const Lettura& vecchia)>& prepara) {
    std::optional<chiusura::Separazione> separazione;
    std::uint32_t generazione = 0;
    const cassaforte::Riepilogo r = accesso::cambia(
        cartella, manifesto, utente, password, comando,
        [&](cassaforte::Cassaforte& c) {
            permessi_per_l_elenco(c.permessi(), utente, comando);
            const ValoriCassaforte v = chiusura::valori_di(c);
            const Lettura vecchia = leggi_pacchetto(CartellaSuDisco(cartella), &v);
            auto [nuova, nota] = prepara(vecchia);
            separazione = chiusura::separa(nuova);
            c.sostituisci_contenuto(separazione->contenuto);
            generazione = c.riepilogo().generazione;
            riscrittura::controlla_riapertura(riscrittura::chiuso_in_memoria(*separazione, manifesto.id, generazione, c.file(), ""), nuova,
                                              utente, password, false);
            return nota;
        },
        accesso::ambiente());
    riscrittura::scrivi_normali(cartella, *separazione, manifesto.id, generazione);
    return r;
}

}  // namespace

// --- leggi-elenco ---------------------------------------------------------------------------------------------------------

Risultato esegui_leggi_elenco(const DefinizioneComando& comando, const Parametri& par) {
    const std::string& percorso_file = par.argomenti.at(0);
    const fs::path cartella(par.argomenti.at(1));
    const bool scrivere = !par.scrivi.empty();
    if (scrivere) stessa_cartella(cartella.string(), par.scrivi);
    // Prima il file (e i suoi errori), senza password: un file rovinato non apre niente.
    const FileDelCommittente file = leggi_file(percorso_file);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    permessi_prima_della_password(cartella, par.utente, comando.nome);
    // Il profilo e' fatto di dati comuni (file normale): serve gia' per il foglio da leggere.
    const Lettura senza_valori = leggi_pacchetto(CartellaSuDisco(cartella));
    pacchetto_completo(senza_valori, cartella.string(), par.utente);
    const kzero::Table tabella = tabella_da(foglio_da(file, profilo_di(senza_valori, par.profilo).sheet));

    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const std::string nota_base = "elenco utenti, profilo " + par.profilo + ", file " + file.impronta.substr(0, 16) + ", elenco " + par.elenco;
    if (!scrivere) {
        accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome,
                                            Visibilita{par.includi_dati_personali, par.includi_dati_particolari},
                                            "anteprima della lettura dell'" + nota_base, accesso::ambiente());
        permessi_per_l_elenco(a.cassaforte.permessi(), par.utente, comando.nome);
        const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
        const Calcolo c = calcola(leggi_pacchetto(CartellaSuDisco(cartella), &v), par, tabella, file.impronta, cartella.string());
        const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
        return uscita_lettura(comando, par, c, file.impronta, false, r, a.registro.voci.size(), avvisi_di(r, par.utente));
    }
    std::optional<Calcolo> calcolo;
    const cassaforte::Riepilogo r = scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        calcolo = calcola(vecchia, par, tabella, file.impronta, cartella.string());
        std::map<std::string, int> conti;
        for (const kzero::ListChange& x : calcolo->confronto.changes) ++conti[nome_esito(x.kind)];
        std::string numeri;
        for (const auto& [k, n] : conti) numeri += (numeri.empty() ? "" : ", ") + k + " " + std::to_string(n);
        return std::make_pair(calcolo->nuova, "lettura dell'" + nota_base + ": " + numeri + ", righe illeggibili " +
                                                  std::to_string(calcolo->letto.unreadable_rows.size()));
    });
    return uscita_lettura(comando, par, *calcolo, file.impronta, true, r, r.registro_voci, avvisi_di(r, par.utente));
}

// --- sblocco ------------------------------------------------------------------------------------------------------------------

Risultato esegui_sblocco(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    stessa_cartella(cartella.string(), par.scrivi);
    if (!par.togli && (par.riferimento.empty() || !par.data_sblocco))
        throw ErroreKZero(Errore::ParametroObbligatorio, "Per aggiungere uno sblocco servono --riferimento (es. la mail del committente) "
                                                         "e --data; per toglierlo, --togli.");
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    permessi_prima_della_password(cartella, par.utente, comando.nome);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    std::size_t restanti = 0;
    bool fuori = false;
    std::string committente;
    const cassaforte::Riepilogo r = scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        pacchetto_completo(vecchia, cartella.string(), par.utente);
        const kzero::Index idx(vecchia.pacchetto);
        const kzero::ListUser* u = idx.list_user(par.utente_elenco);
        if (!u) throw ErroreKZero(Errore::UtenteElencoNonTrovato, "L'utente '" + par.utente_elenco + "' non c'e' nell'elenco utenti del pacchetto.");
        const kzero::ClientList* lista = nullptr;
        for (const kzero::ClientList& l : vecchia.pacchetto.client_lists)
            for (const kzero::ListUser& x : l.users)
                if (x.code == u->code) lista = &l;
        const kzero::ReadingProfile* profilo = idx.reading_profile(lista->profile.code);
        if (!profilo || !profilo->leg(par.tratta))
            throw ErroreKZero(Errore::SbloccoNonAmmesso, "La tratta '" + par.tratta + "' non e' una tratta del profilo dell'elenco.");
        kzero::ClientList nuova_lista = *lista;
        kzero::ListUser* w = nullptr;
        for (kzero::ListUser& x : nuova_lista.users)
            if (x.code == u->code) w = &x;
        const auto it = std::find_if(w->unlocks.begin(), w->unlocks.end(), [&](const kzero::ListUserUnlock& s) { return s.leg == par.tratta; });
        if (par.togli) {
            if (it == w->unlocks.end())
                throw ErroreKZero(Errore::SbloccoNonAmmesso, "L'utente " + w->code + " non ha uno sblocco della tratta '" + par.tratta + "'.");
            w->unlocks.erase(it);
        } else {
            if (w->out_of_list)
                throw ErroreKZero(Errore::SbloccoNonAmmesso, "L'utente " + w->code + " non e' piu' nell'elenco del committente: non si "
                                                             "aggiungono sblocchi (si possono solo togliere).");
            if (w->active)
                throw ErroreKZero(Errore::SbloccoNonAmmesso, "L'utente " + w->code + " e' attivo: ha gia' diritto al servizio, lo sblocco "
                                                             "non serve.");
            const bool ha_tratta = std::any_of(w->legs.begin(), w->legs.end(), [&](const kzero::ListUserLeg& l) { return l.leg == par.tratta; });
            if (!ha_tratta)
                throw ErroreKZero(Errore::SbloccoNonAmmesso, "Il committente non da' all'utente " + w->code + " la tratta '" + par.tratta +
                                                             "' (percorsi del file): non c'e' niente da sbloccare.");
            if (it != w->unlocks.end())
                throw ErroreKZero(Errore::SbloccoNonAmmesso, "L'utente " + w->code + " ha gia' uno sblocco della tratta '" + par.tratta +
                                                             "': per cambiarlo, prima toglierlo (--togli).");
            w->unlocks.push_back({profilo->leg(par.tratta)->code, par.riferimento, *par.data_sblocco, {}});
        }
        restanti = w->unlocks.size();
        fuori = w->out_of_list;
        committente = nuova_lista.client.code;
        Lettura nuova = con_documento(vecchia, kElenco, documento_con(vecchia, nuova_lista, vecchia.pacchetto.list_last_number));
        return std::make_pair(std::move(nuova), std::string(par.togli ? "sblocco tolto" : "sblocco aggiunto") + ": utente " + w->code +
                                                    ", tratta " + par.tratta);
    });
    const std::vector<accesso::AvvisoMostrato> avvisi = avvisi_di(r, par.utente);
    std::string fatto = std::string(par.togli ? "Sblocco tolto" : "Sblocco aggiunto") + ": utente " + par.utente_elenco + " (committente " +
                        committente + "), tratta " + par.tratta + "; sblocchi che restano all'utente: " + std::to_string(restanti) + ".";
    if (fuori && restanti == 0)
        fatto += " L'utente non e' piu' nell'elenco del committente e non ha piu' sblocchi: non viene tolto adesso, se ne andra' alla "
                 "prossima lettura completa dell'elenco (kzero leggi-elenco --elenco completo).";
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["utente"] = par.utente;
        j["utente_elenco"] = par.utente_elenco;
        j["committente"] = committente;
        j["tratta"] = par.tratta;
        j["azione"] = par.togli ? "tolto" : "aggiunto";
        j["riferimento"] = par.togli ? Json(nullptr) : Json(par.riferimento);
        j["data"] = par.togli ? Json(nullptr) : Json(kzero::text_from_day(*par.data_sblocco));
        j["sblocchi_restanti"] = restanti;
        j["fuori_elenco"] = fuori;
        j["fatto"] = fatto;
        Json av = Json::array();
        for (const accesso::AvvisoMostrato& a : avvisi)
            av.push_back({{"codice", a.codice.empty() ? Json(nullptr) : Json(a.codice)}, {"messaggio", a.messaggio}});
        j["avvisi"] = av;
        j["cassaforte"] = {{"id", r.id}, {"generazione", r.generazione}, {"voci_registro", r.registro_voci}};
        return {testo_json(j), 0};
    }
    std::string t = fatto + "\nCassaforte " + r.id + ", chiusura numero " + std::to_string(r.generazione) + "; registro degli accessi: " +
                    std::to_string(r.registro_voci) + (r.registro_voci == 1 ? " voce.\n" : " voci.\n");
    for (const accesso::AvvisoMostrato& a : avvisi) t += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return {t, 0};
}

}  // namespace kzero::api
