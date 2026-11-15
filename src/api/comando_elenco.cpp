// K-Zero API - l'elenco utenti del committente (K-019): kzero leggi-elenco e kzero sblocco. Uscite: schemi "elenco" e "sblocco".
//
// Permessi, cartella di --scrivi, documento e scrittura nella cassaforte: elenco_scrittura.hpp (in comune con le modifiche a mano
// del K-020, comando_elenco_a_mano.cpp).
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
#include "elenco_scrittura.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

namespace fs = std::filesystem;
using elenco::kFile;

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

Calcolo calcola(Lettura vecchia, const Parametri& par, const kzero::Table& tabella, const std::string& impronta, const std::string& cartella) {
    elenco::pacchetto_completo(vecchia, cartella, par.utente);
    const kzero::ReadingProfile& profilo = profilo_di(vecchia, par.profilo);
    const kzero::Index idx(vecchia.pacchetto);
    Calcolo c;
    c.committente = profilo.client.code;
    c.letto = kzero::read_list(tabella, profilo, idx, par.anno_scolastico, elenco::elenco_di(vecchia.pacchetto, profilo.client.code));
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
    c.nuova = elenco::con_documento(vecchia, kFile, elenco::documento_con(vecchia, c.confronto.list, c.confronto.last_number));
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
        case kzero::ListChange::Kind::RemovedInFile: return "tolto_a_mano_nel_file";
        case kzero::ListChange::Kind::RemovedGone: return "tolto_davvero";
        case kzero::ListChange::Kind::ManualNotInFile: return "a_mano_non_nel_file";
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
        case kzero::ListChange::Kind::RemovedInFile: return "tolto a mano, ma il file lo ha ancora: resta fuori (mai nel piano)";
        case kzero::ListChange::Kind::RemovedGone: return "tolto a mano e non nel file: se ne va davvero, con i suoi sblocchi";
        case kzero::ListChange::Kind::ManualNotInFile: return "aggiunto o rimesso a mano, non e' nel file: resta";
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
        if (p.where.file != kFile) continue;
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
                         {"non_nel_file", conto("non_nel_file")}, {"righe_illeggibili", conto("riga_illeggibile")},
                         {"tolti_a_mano_nel_file", conto("tolto_a_mano_nel_file")}, {"tolti_davvero", conto("tolto_davvero")},
                         {"a_mano_non_nel_file", conto("a_mano_non_nel_file")}};
        Json utenti = Json::array();
        for (const kzero::ListChange& x : c.confronto.changes) {
            Json u = {{"codice", x.code}, {"esito", nome_esito(x.kind)}, {"riga", numero_o_null(x.row)}};
            u["campi"] = x.fields;
            u["confermati"] = x.confirmed;
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
    if (conto("tolto_a_mano_nel_file")) t += ", " + std::to_string(conto("tolto_a_mano_nel_file")) + " tolti a mano ancora nel file (restano fuori)";
    if (conto("tolto_davvero")) t += ", " + std::to_string(conto("tolto_davvero")) + " tolti a mano che se ne vanno davvero";
    if (conto("a_mano_non_nel_file")) t += ", " + std::to_string(conto("a_mano_non_nel_file")) + " aggiunti o rimessi a mano non nel file (restano)";
    t += ".\n";
    if (par.elenco == "parziale") t += "Elenco parziale: nessuno esce.\n";
    else if (!c.confronto.exits_applied) t += "Elenco completo con righe illeggibili: le uscite non si applicano.\n";
    for (const kzero::ListChange& x : c.confronto.changes) {
        if (x.kind == kzero::ListChange::Kind::Unchanged && x.confirmed.empty()) continue;
        const auto n = nome(x.code);
        t += "  " + x.code + (n ? " \"" + *n + "\"" : "") + (x.row ? " (riga " + std::to_string(x.row) + ")" : "") + ": " + testo_esito(x.kind);
        if (x.kind == kzero::ListChange::Kind::Changed) {
            std::string campi;
            for (const std::string& f : x.fields) campi += (campi.empty() ? "" : ", ") + f;
            t += " (" + campi + ")";
        }
        if (!x.confirmed.empty()) {
            std::string campi;
            for (const std::string& f : x.confirmed) campi += (campi.empty() ? "" : ", ") + f;
            t += "; il file conferma le modifiche a mano (" + campi + ")";
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

}  // namespace

// --- leggi-elenco ---------------------------------------------------------------------------------------------------------

Risultato esegui_leggi_elenco(const DefinizioneComando& comando, const Parametri& par) {
    const std::string& percorso_file = par.argomenti.at(0);
    const fs::path cartella(par.argomenti.at(1));
    const bool scrivere = !par.scrivi.empty();
    if (scrivere) elenco::stessa_cartella(cartella.string(), par.scrivi);
    // Prima il file (e i suoi errori), senza password: un file rovinato non apre niente.
    const FileDelCommittente file = leggi_file(percorso_file);
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    elenco::permessi_prima_della_password(cartella, par.utente, comando.nome);
    // Il profilo e' fatto di dati comuni (file normale): serve gia' per il foglio da leggere.
    const Lettura senza_valori = leggi_pacchetto(CartellaSuDisco(cartella));
    elenco::pacchetto_completo(senza_valori, cartella.string(), par.utente);
    const kzero::Table tabella = tabella_da(foglio_da(file, profilo_di(senza_valori, par.profilo).sheet));

    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    const std::string nota_base = "elenco utenti, profilo " + par.profilo + ", file " + file.impronta.substr(0, 16) + ", elenco " + par.elenco;
    if (!scrivere) {
        accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome,
                                            Visibilita{par.includi_dati_personali, par.includi_dati_particolari},
                                            "anteprima della lettura dell'" + nota_base, accesso::ambiente());
        elenco::permessi(a.cassaforte.permessi(), par.utente, comando.nome);
        const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
        const Calcolo c = calcola(leggi_pacchetto(CartellaSuDisco(cartella), &v), par, tabella, file.impronta, cartella.string());
        const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
        return uscita_lettura(comando, par, c, file.impronta, false, r, a.registro.voci.size(), elenco::avvisi_di(r, par.utente));
    }
    std::optional<Calcolo> calcolo;
    const cassaforte::Riepilogo r = elenco::scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        calcolo = calcola(vecchia, par, tabella, file.impronta, cartella.string());
        std::map<std::string, int> conti;
        for (const kzero::ListChange& x : calcolo->confronto.changes) ++conti[nome_esito(x.kind)];
        std::string numeri;
        for (const auto& [k, n] : conti) numeri += (numeri.empty() ? "" : ", ") + k + " " + std::to_string(n);
        return std::make_pair(calcolo->nuova, "lettura dell'" + nota_base + ": " + numeri + ", righe illeggibili " +
                                                  std::to_string(calcolo->letto.unreadable_rows.size()));
    });
    return uscita_lettura(comando, par, *calcolo, file.impronta, true, r, r.registro_voci, elenco::avvisi_di(r, par.utente));
}

// --- sblocco ------------------------------------------------------------------------------------------------------------------

Risultato esegui_sblocco(const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    elenco::stessa_cartella(cartella.string(), par.scrivi);
    if (!par.togli && (par.riferimento.empty() || !par.data_sblocco))
        throw ErroreKZero(Errore::ParametroObbligatorio, "Per aggiungere uno sblocco servono --riferimento (es. la mail del committente) "
                                                         "e --data; per toglierlo, --togli.");
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    elenco::permessi_prima_della_password(cartella, par.utente, comando.nome);
    const auto in = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*in, par.utente);
    cripto::niente_core_dump();
    std::size_t restanti = 0;
    bool fuori = false;
    std::string committente;
    const cassaforte::Riepilogo r = elenco::scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        elenco::pacchetto_completo(vecchia, cartella.string(), par.utente);
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
        Lettura nuova = elenco::con_documento(vecchia, kFile, elenco::documento_con(vecchia, nuova_lista, vecchia.pacchetto.list_last_number));
        return std::make_pair(std::move(nuova), std::string(par.togli ? "sblocco tolto" : "sblocco aggiunto") + ": utente " + w->code +
                                                    ", tratta " + par.tratta);
    });
    const std::vector<accesso::AvvisoMostrato> avvisi = elenco::avvisi_di(r, par.utente);
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
