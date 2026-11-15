// K-Zero API - le modifiche a mano dell'elenco utenti del committente (K-020): kzero aggiungi-in-elenco, cambia-in-elenco,
// togli-da-elenco, rimetti-in-elenco e cancella-da-elenco. Uscita: schema "modifica_elenco".
//
// Le novita' che il committente manda per mail, una alla volta, senza tirare fuori i dati in chiaro: stessa scrittura, stessi
// permessi e stessa cartella di --scrivi di kzero leggi-elenco (elenco_scrittura.hpp). Senza --scrivi mostrano cosa cambierebbe
// (solo la voce del registro). Ogni modifica ha un riferimento (es. la mail) e una data scritta dall'utente (niente orologio). I
// valori dei campi si leggono dall'ingresso standard, mai dagli argomenti (campi_ingresso.hpp); le regole sono nel nucleo
// (elenco.hpp, "Modifiche a mano").
#include <filesystem>
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
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

namespace fs = std::filesystem;

enum class Azione { Aggiungi, Cambia, Togli, Rimetti, Cancella };

const char* nome_azione(Azione a) {
    switch (a) {
        case Azione::Aggiungi: return "aggiunto";
        case Azione::Cambia: return "cambiato";
        case Azione::Togli: return "tolto";
        case Azione::Rimetti: return "rimesso";
        case Azione::Cancella: return "cancellato";
    }
    return "?";
}

// --- I campi dall'ingresso ----------------------------------------------------------------------------------------------

std::vector<std::string> righe_dei_campi() {
    std::function<int()> byte = accesso::ambiente().valori;
    if (!byte) {
#if defined(KZERO_ISATTY)
        if (::isatty(0))
            std::fputs("Scrivi i campi, una riga per campo (campo: valore, es. \"via: via Roma\"); una riga vuota per finire.\n", stderr);
#endif
        byte = campi::ingresso_standard();
    }
    const campi::Righe r = campi::leggi(byte);
    if (r.motivo != campi::Motivo::Nessuno)
        throw ErroreKZero(Errore::CampiScrittiMale, "Campi dall'ingresso, riga " + std::to_string(r.riga) + ": " + campi::spiegazione(r.motivo) +
                                                        ". Niente e' cambiato.");
    return r.righe;
}

std::string spiegazione_riga(kzero::RowFault f) {
    switch (f) {
        case kzero::RowFault::SchoolUnknown: return "la scuola non e' una scuola del pacchetto (nessuna ha questo codice esterno del profilo)";
        case kzero::RowFault::SchoolAmbiguous: return "la scuola vale per piu' scuole del pacchetto (stesso codice esterno)";
        case kzero::RowFault::StatusUnknown: return "lo stato non e' una delle parole del profilo";
        case kzero::RowFault::RoutesForm: return "i percorsi sono scritti in un modo che il profilo non conosce";
        case kzero::RowFault::RoutesLegTwice: return "i percorsi hanno la stessa tratta due volte";
        case kzero::RowFault::TimesForm: return "gli orari sono scritti in un modo che il profilo non conosce";
        case kzero::RowFault::TimesLegUnknown: return "gli orari hanno una sigla di tratta che il profilo non conosce";
        case kzero::RowFault::TimesDayUnknown: return "gli orari hanno una sigla di giorno che il profilo non conosce";
        case kzero::RowFault::TimesPeriodUnknown: return "gli orari hanno una sigla di periodo che il profilo non conosce";
        case kzero::RowFault::TimesHour: return "gli orari hanno un'ora scritta male o che non esiste";
        case kzero::RowFault::TimesDate: return "gli orari hanno una data che non esiste";
        case kzero::RowFault::TimesDatesReversed: return "gli orari hanno un periodo che finisce prima di cominciare";
        case kzero::RowFault::ClassNotNumber: return "la classe non e' un numero da 0 a 20";
        default: return "il valore non si legge con il profilo";
    }
}

[[noreturn]] void errore_dei_campi(const kzero::ManualError& e, const std::string& profilo) {
    const std::string riga = e.line > 0 ? "riga " + std::to_string(e.line) + ": " : "";
    std::string m;
    switch (e.fault) {
        case kzero::ManualFault::UnknownField:
            m = "la riga non comincia con il nome di un campo seguito da \":\" (nome, scuola, stato, percorsi, fermata, comune, frazione, "
                "via, civico, classe, sezione, \"orari <titolo della colonna>\")";
            break;
        case kzero::ManualFault::RepeatedField: m = "il campo \"" + e.field + "\" e' scritto due volte"; break;
        case kzero::ManualFault::NotInProfile:
            m = "il profilo " + profilo + " non ha il campo \"" + e.field + "\"" +
                (e.field == "orari" ? " con quel titolo (le colonne degli orari si scrivono con il loro titolo nel profilo)" : "");
            break;
        case kzero::ManualFault::HomePartMissing:
            m = "per cambiare la casa si scrivono tutte le sue parti che il profilo ha: manca \"" + e.field +
                "\" (una parte non scritta non si svuota; per lasciarla vuota, scriverla vuota)";
            break;
        case kzero::ManualFault::RequiredEmpty: m = "il campo \"" + e.field + "\" non puo' essere vuoto"; break;
        case kzero::ManualFault::RequiredMissing: m = "per aggiungere un utente servono nome, scuola e stato: manca \"" + e.field + "\""; break;
        case kzero::ManualFault::NoFields: m = "nessun campo scritto (una riga per campo, \"campo: valore\")"; break;
        case kzero::ManualFault::Value:
            m = "\"" + e.field + "\": " + spiegazione_riga(e.row_fault.value_or(kzero::RowFault::UnreadableCell));
            break;
        case kzero::ManualFault::None: break;
    }
    throw ErroreKZero(Errore::CampiScrittiMale, "Campi dall'ingresso, " + riga + m + ". Niente e' cambiato.");
}

[[noreturn]] void rifiuto(const kzero::ManualOutcome& o, const std::string& codice) {
    std::string m;
    switch (o.refusal) {
        case kzero::ManualRefusal::NothingChanges: m = "i valori scritti sono quelli che l'utente " + codice + " ha gia': niente da cambiare"; break;
        case kzero::ManualRefusal::DateBeforeLast:
            m = "la data e' prima di quella dell'ultima modifica a mano " + (o.field.empty() ? std::string("dell'utente") : "del campo \"" + o.field + "\"") +
                " (" + kzero::text_from_day(o.last_date) + "): le modifiche si scrivono in ordine di data (lo stesso giorno va bene)";
            break;
        case kzero::ManualRefusal::Homonym:
            m = "nome e scuola sarebbero quelli dell'utente " + o.other + " (anche di prima di una correzione a mano): due utenti con lo "
                "stesso nome nella stessa scuola non si distinguerebbero alla lettura del file";
            break;
        case kzero::ManualRefusal::TimesWithoutColumn:
            m = "gli orari dell'utente " + codice + " vengono da una lettura della versione 1, che non ricorda la colonna di ogni orario: "
                "\"" + o.field + "\" si potra' cambiare dopo la prossima lettura del file (kzero leggi-elenco); gli altri campi si'";
            break;
        case kzero::ManualRefusal::UserRemoved: m = "l'utente " + codice + " e' tolto a mano: prima rimetterlo (kzero rimetti-in-elenco)"; break;
        case kzero::ManualRefusal::AlreadyRemoved: m = "l'utente " + codice + " e' gia' tolto a mano"; break;
        case kzero::ManualRefusal::NotRemovedNorOut:
            m = "l'utente " + codice + " e' gia' nell'elenco: non e' ne' tolto a mano ne' fuori elenco";
            break;
        case kzero::ManualRefusal::None: break;
    }
    throw ErroreKZero(Errore::ModificaNonAmmessa, "Modifica a mano non fatta: " + m + ". Niente e' cambiato.");
}

// --- Il calcolo, sulla lettura in chiaro della cassaforte aperta ------------------------------------------------------------

struct Calcolo {
    Lettura vecchia;
    Lettura nuova;
    std::string committente;
    std::string codice;
    kzero::ManualOutcome esito;
    bool fermata_non_riconosciuta = false;
    std::string nota;   // per il registro: solo codici e nomi dei campi
};

Calcolo calcola(Azione azione, const Lettura& vecchia, const Parametri& par, const std::vector<std::string>& righe, const std::string& cartella) {
    elenco::pacchetto_completo(vecchia, cartella, par.utente);
    Calcolo c;
    const kzero::Index idx(vecchia.pacchetto);
    kzero::ClientList lista;
    const kzero::ReadingProfile* profilo = nullptr;
    const kzero::ListUser* attuale = nullptr;
    if (azione == Azione::Aggiungi) {
        profilo = idx.reading_profile(par.profilo);
        if (!profilo)
            throw ErroreKZero(Errore::ProfiloNonTrovato, "Il profilo di lettura '" + par.profilo + "' non c'e' nel pacchetto (profili_elenchi.json).");
        if (const kzero::ClientList* l = elenco::elenco_di(vecchia.pacchetto, profilo->client.code)) {
            if (l->profile.code != profilo->code)
                throw ErroreKZero(Errore::ModificaNonAmmessa, "L'elenco del committente " + profilo->client.code + " e' letto con il profilo " +
                                                                  l->profile.code + ", non " + profilo->code + ": i campi si scrivono con le "
                                                                  "notazioni del profilo dell'elenco. Niente e' cambiato.");
            if (par.anno_scolastico && par.anno_scolastico != l->first_year)
                throw ErroreKZero(Errore::ModificaNonAmmessa, "L'elenco del committente " + profilo->client.code + " e' dell'anno scolastico " +
                                                                  std::to_string(l->first_year) + "-" + std::to_string(l->first_year + 1) +
                                                                  ": --anno-scolastico non va scritto, o va scritto uguale. Niente e' cambiato.");
            lista = *l;
        } else {
            if (!par.anno_scolastico)
                throw ErroreKZero(Errore::ModificaNonAmmessa, "Il committente " + profilo->client.code + " non ha ancora un elenco nel pacchetto: "
                                                              "per il primo utente serve anche --anno-scolastico (es. 2026-2027). Niente e' cambiato.");
            lista.client = kzero::Ref{profilo->client.code, {}};
            lista.profile = kzero::Ref{profilo->code, {}};
            lista.first_year = par.anno_scolastico;
        }
    } else {
        attuale = idx.list_user(par.utente_elenco);
        if (!attuale)
            throw ErroreKZero(Errore::UtenteElencoNonTrovato, "L'utente '" + par.utente_elenco + "' non c'e' nell'elenco utenti del pacchetto.");
        for (const kzero::ClientList& l : vecchia.pacchetto.client_lists)
            for (const kzero::ListUser& x : l.users)
                if (x.code == attuale->code) lista = l;
        profilo = idx.reading_profile(lista.profile.code);
        if (!profilo && azione == Azione::Cambia)
            throw ErroreKZero(Errore::ProfiloNonTrovato, "Il profilo di lettura '" + lista.profile.code + "' dell'elenco non c'e' nel pacchetto: "
                                                         "i campi si leggono con il profilo.");
    }
    c.committente = lista.client.code;
    const kzero::ManualMark segno{par.riferimento, par.data_sblocco.value_or(0)};
    int ultimo = vecchia.pacchetto.list_last_number;
    kzero::ManualParse in;
    if (azione == Azione::Aggiungi || azione == Azione::Cambia) {
        in = kzero::parse_manual(righe, *profilo, idx, lista.first_year, attuale, azione == Azione::Aggiungi);
        if (in.error.fault != kzero::ManualFault::None) errore_dei_campi(in.error, profilo->code);
        for (const auto& [campo, colonna] : in.fields)
            if (campo == kzero::ListField::Stop && !in.values.stop && !in.values.stop_text.empty()) c.fermata_non_riconosciuta = true;
    }
    switch (azione) {
        case Azione::Aggiungi: c.esito = kzero::manual_add(vecchia.pacchetto, lista, in, segno); break;
        case Azione::Cambia: c.esito = kzero::manual_change(lista, par.utente_elenco, in, segno); break;
        case Azione::Togli: c.esito = kzero::manual_remove(lista, par.utente_elenco, segno); break;
        case Azione::Rimetti: c.esito = kzero::manual_restore(lista, par.utente_elenco, segno); break;
        case Azione::Cancella: c.esito = kzero::manual_delete(lista, par.utente_elenco); break;
    }
    if (c.esito.refusal != kzero::ManualRefusal::None) rifiuto(c.esito, azione == Azione::Aggiungi ? std::string("nuovo") : par.utente_elenco);
    if (azione == Azione::Aggiungi) ultimo = c.esito.last_number;
    c.codice = c.esito.code;
    c.nuova = elenco::con_documento(vecchia, elenco::kFile, elenco::documento_con(vecchia, lista, ultimo));
    std::string campi;
    for (const kzero::ManualFieldChange& x : c.esito.changes) campi += (campi.empty() ? "" : ", ") + kzero::field_name(x.field, x.column);
    c.nota = std::string("modifica a mano dell'elenco (") + nome_azione(azione) + "): utente " + c.codice + ", committente " + c.committente +
             (campi.empty() ? std::string() : ", campi: " + campi);
    c.vecchia = vecchia;
    return c;
}

// --- L'uscita ----------------------------------------------------------------------------------------------------------------

// Il valore di un campo come si mostra. nullopt = riservato (dato personale senza i parametri o i permessi che lo mostrano).
std::optional<std::string> valore(kzero::ListField f, const std::string& colonna, const kzero::ListUser& u, bool personali) {
    const auto o_vuoto = [](const std::string& s) { return s.empty() ? std::string("(vuoto)") : s; };
    switch (f) {
        case kzero::ListField::Name:
            if (!personali) return std::nullopt;
            return o_vuoto(u.name);
        case kzero::ListField::Home: {
            if (!personali) return std::nullopt;
            if (!u.home) return std::string("(vuoto)");
            std::string t;
            for (const std::string* x : {&u.home->street, &u.home->number, &u.home->hamlet, &u.home->town})
                if (!x->empty()) t += (t.empty() ? "" : x == &u.home->number ? " " : ", ") + *x;
            return o_vuoto(t);
        }
        case kzero::ListField::School: return o_vuoto(u.school.code);
        case kzero::ListField::Class:
            if (!personali) return std::nullopt;
            return u.class_number ? std::to_string(*u.class_number) : std::string("(vuoto)");
        case kzero::ListField::Section:
            if (!personali) return std::nullopt;
            return o_vuoto(u.section);
        case kzero::ListField::Status: {
            std::string t = u.active ? "attivo" : "sospeso";
            if (!u.status_note.empty()) t += personali ? " - " + u.status_note : std::string(" - (nota riservata)");
            return t;
        }
        case kzero::ListField::Stop:
            if (u.stop) return u.stop->code + (u.stop_match == kzero::StopMatch::Name ? " (riconosciuta dal nome)" : " (dal codice esterno)");
            if (u.stop_text.empty()) return std::string("(vuoto)");
            if (!personali) return std::nullopt;
            return "\"" + u.stop_text + "\" (non riconosciuta)";
        case kzero::ListField::Routes: {
            std::string t;
            for (const kzero::ListUserLeg& l : u.legs)
                t += (t.empty() ? "" : ", ") + l.leg + ": " + (l.company ? l.company->code : "altra ditta \"" + l.other_company + "\"");
            return o_vuoto(t);
        }
        case kzero::ListField::Times: {
            static const char* giorni[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
            std::string t;
            for (const kzero::ListUserTime& x : u.times) {
                if (x.column != colonna) continue;
                t += (t.empty() ? "" : "; ") + x.leg + " " + giorni[static_cast<int>(x.day)] + " " + kzero::text_from_minutes(x.time) +
                     (x.period.empty() ? "" : " (" + x.period + ")") + " dal " + kzero::text_from_day(x.dates.from) + " al " +
                     kzero::text_from_day(x.dates.to);
            }
            return o_vuoto(t);
        }
        case kzero::ListField::OutOfList: return std::string(u.out_of_list ? "fuori elenco" : "nell'elenco");
    }
    return std::nullopt;
}

const char* memoria(kzero::ManualFieldChange::Memory m, Azione a) {
    if (a != Azione::Cambia) return nullptr;
    switch (m) {
        case kzero::ManualFieldChange::Memory::New: return "ricordato";
        case kzero::ManualFieldChange::Memory::Updated: return "gia_ricordato";
        case kzero::ManualFieldChange::Memory::Dropped: return "tolto";
    }
    return nullptr;
}

const char* testo_memoria(kzero::ManualFieldChange::Memory m) {
    switch (m) {
        case kzero::ManualFieldChange::Memory::New:
            return "il valore di prima resta ricordato finche' un file del committente non conferma o cambia il campo";
        case kzero::ManualFieldChange::Memory::Updated:
            return "il campo era gia' cambiato a mano: resta ricordato il valore di prima, quello del file";
        case kzero::ManualFieldChange::Memory::Dropped:
            return "e' di nuovo il valore di prima: la modifica a mano si toglie, non resta niente da confermare";
    }
    return "";
}

struct Problema {
    std::string codice, livello, messaggio;
};

Risultato uscita(Azione azione, const DefinizioneComando& comando, const Parametri& par, const Calcolo& c, bool scritto,
                 const cassaforte::Riepilogo& r, std::size_t voci, const std::vector<accesso::AvvisoMostrato>& avvisi) {
    const Visibilita v{par.includi_dati_personali, par.includi_dati_particolari};
    const kzero::Index iv(c.vecchia.pacchetto), in(c.nuova.pacchetto);
    const LegameSensibile lv(&iv), ln(&in);
    // Di chi e' legato a un luogo sensibile (prima o dopo) i dati personali solo con tutti e due i parametri.
    const bool legato = lv.utente(c.codice) || ln.utente(c.codice);
    const bool personali = v.personali && (!legato || v.particolari);
    std::optional<std::string> nome;
    for (const Lettura* l : {&c.vecchia, &c.nuova})
        for (const kzero::ClientList& x : l->pacchetto.client_lists)
            for (const kzero::ListUser& u : x.users)
                if (u.code == c.codice && personali) nome = u.name;
    // Gli avvisi sull'elenco nuovo per questo utente (lo stato delle modifiche a mano, gli sblocchi...).
    const Presentatore pr(&c.nuova.pacchetto, &c.nuova.versioni, v);
    std::vector<Problema> problemi;
    std::vector<kzero::Problem> sull_elenco;
    kzero::check_lists(c.nuova.pacchetto, in, sull_elenco);
    for (const kzero::Problem& p : sull_elenco) {
        if (p.where.file != elenco::kFile) continue;
        bool suo = false;
        for (const kzero::Param& x : p.params) suo = suo || (x.name == "utente" && x.text == c.codice);
        if (!suo) continue;
        const DefinizioneControllo& d = definizione_controllo(p.check);
        problemi.push_back({d.codice, nome_livello(d.livello), pr.messaggio(p)});
    }
    const std::string data = kzero::text_from_day(par.data_sblocco.value_or(0));
    std::string fatto;
    switch (azione) {
        case Azione::Aggiungi: fatto = "Utente aggiunto a mano all'elenco: " + c.codice; break;
        case Azione::Cambia: fatto = "Utente cambiato a mano: " + c.codice; break;
        case Azione::Togli: fatto = "Utente tolto a mano dall'elenco: " + c.codice; break;
        case Azione::Rimetti: fatto = "Utente rimesso a mano nell'elenco: " + c.codice; break;
        case Azione::Cancella: fatto = "Utente cancellato da tutto il pacchetto: " + c.codice; break;
    }
    fatto += " (committente " + c.committente + "), riferimento \"" + par.riferimento + "\", data " + data + ".";
    std::vector<std::string> note;
    if (azione == Azione::Aggiungi)
        note.push_back("Codice nuovo dato dal programma (mai ricavato dal nome, mai riusato). Resta \"aggiunto a mano\" finche' un file "
                       "del committente lo porta: da li' vale il file (tranne i campi cambiati a mano dopo l'aggiunta).");
    if (azione == Azione::Togli)
        note.push_back(std::string("Non entra piu' nel piano. Se ne andra' davvero alla prima lettura completa del file senza di lui (e senza "
                                   "righe illeggibili)") +
                       (c.esito.unlocks ? ", con i suoi sblocchi (" + std::to_string(c.esito.unlocks) + "): e' una scelta dell'utente, non della lettura"
                                        : std::string()) +
                       ". Si annulla con kzero rimetti-in-elenco.");
    if (azione == Azione::Rimetti)
        note.push_back(c.esito.removed ? std::string("Il togliere a mano e' annullato") +
                                             (c.esito.out_of_list ? ": l'utente resta fuori elenco (per rimetterlo nell'elenco, di nuovo kzero "
                                                                    "rimetti-in-elenco)."
                                                                  : ": l'utente torna nel piano.")
                                       : std::string("L'utente era fuori elenco (uscito, tenuto per uno sblocco): torna nell'elenco e nel piano, con i "
                                                     "suoi sblocchi. Finche' un file del committente non lo riporta resta l'avviso; una lettura completa "
                                                     "senza di lui non lo toglie (il file non e' ancora aggiornato)."));
    if (azione == Azione::Cancella)
        note.push_back("Tolto subito da tutto il pacchetto" +
                       (c.esito.unlocks ? ", con i suoi sblocchi (" + std::to_string(c.esito.unlocks) + ")" : std::string()) +
                       ": dell'utente non resta niente, e il suo codice non si dara' mai a nessun altro. Se il file del committente lo ha "
                       "ancora, la prossima lettura lo aggiungera' come utente nuovo, con un codice nuovo.");
    if (c.fermata_non_riconosciuta)
        note.push_back("La fermata scritta non e' un luogo del pacchetto (ne' per il codice esterno del profilo ne' per il nome tra i luoghi "
                       "del comune della casa): resta il testo, dato personale.");
    const char* riservato = "riservato: --includi-dati-personali";
    const char* riservato_legato = "riservato: --includi-dati-personali e --includi-dati-particolari (luogo sensibile)";
    if (par.formato == Formato::Json) {
        Json j = documento(comando.schema);
        j["comando"] = comando.nome;
        j["utente"] = par.utente;
        j["azione"] = nome_azione(azione);
        j["utente_elenco"] = c.codice;
        j["committente"] = c.committente;
        j["riferimento"] = par.riferimento;
        j["data"] = data;
        j["scritto"] = scritto;
        j["nome"] = nome ? Json(*nome) : Json(nullptr);
        Json campi = Json::array();
        for (const kzero::ManualFieldChange& x : c.esito.changes) {
            Json y = {{"campo", kzero::field_name(x.field, x.column)}};
            const auto prima = azione == Azione::Aggiungi ? std::nullopt : valore(x.field, x.column, x.before, personali);
            const auto dopo = valore(x.field, x.column, x.after, personali);
            y["prima"] = azione == Azione::Aggiungi || !prima ? Json(nullptr) : Json(*prima);
            y["dopo"] = dopo ? Json(*dopo) : Json(nullptr);
            y["riservato"] = !dopo || (azione != Azione::Aggiungi && !prima);
            const char* m = memoria(x.memory, azione);
            y["valore_di_prima"] = m ? Json(m) : Json(nullptr);
            campi.push_back(y);
        }
        j["campi"] = campi;
        j["sblocchi"] = (azione == Azione::Togli || azione == Azione::Cancella) ? Json(c.esito.unlocks) : Json(nullptr);
        j["note"] = note;
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
    std::string t = std::string(scritto ? "" : "Anteprima (niente e' stato scritto: per scrivere, --scrivi). ") + fatto + "\n";
    if (nome && azione != Azione::Aggiungi) t += "  Nome: \"" + *nome + "\".\n";
    for (const kzero::ManualFieldChange& x : c.esito.changes) {
        const auto prima = valore(x.field, x.column, x.before, personali);
        const auto dopo = valore(x.field, x.column, x.after, personali);
        const std::string r_ = legato && v.personali && !v.particolari ? riservato_legato : riservato;
        t += "  " + kzero::field_name(x.field, x.column) + ": ";
        if (azione != Azione::Aggiungi) t += "prima " + (prima ? *prima : "(" + r_ + ")") + "; dopo ";
        t += dopo ? *dopo : "(" + r_ + ")";
        if (azione == Azione::Cambia) t += std::string(" (") + testo_memoria(x.memory) + ")";
        t += ".\n";
    }
    for (const std::string& n : note) t += n + "\n";
    for (const Problema& p : problemi) t += (p.livello == "errore" ? "Errore " : "Avviso ") + p.codice + ": " + p.messaggio + "\n";
    t += "Cassaforte " + r.id + ", chiusura numero " + std::to_string(r.generazione) + "; registro degli accessi: " + std::to_string(voci) +
         (voci == 1 ? " voce.\n" : " voci.\n");
    for (const accesso::AvvisoMostrato& a : avvisi) t += "Avviso" + (a.codice.empty() ? "" : " " + a.codice) + ": " + a.messaggio + "\n";
    return {t, 0};
}

// --- Il comando ----------------------------------------------------------------------------------------------------------------

Risultato esegui(Azione azione, const DefinizioneComando& comando, const Parametri& par) {
    const fs::path cartella(par.argomenti.at(0));
    const bool scrivere = !par.scrivi.empty();
    if (scrivere) elenco::stessa_cartella(cartella.string(), par.scrivi);
    if (par.riferimento.empty() || !par.data_sblocco)
        throw ErroreKZero(Errore::ParametroObbligatorio, "Ogni modifica a mano dell'elenco vuole --riferimento (es. la mail del committente) e "
                                                         "--data (scritta da chi la fa).");
    const ManifestoLetto manifesto = accesso::manifesto_con_cassaforte(cartella);
    elenco::permessi_prima_della_password(cartella, par.utente, comando.nome);
    const auto ingresso = accesso::ingresso(par);
    const cripto::Segreto password = accesso::chiedi_password(*ingresso, par.utente);
    cripto::niente_core_dump();
    // I campi dopo la password (con --password-da-ingresso la password e' la prima riga), prima di aprire la cassaforte.
    const std::vector<std::string> righe =
        azione == Azione::Aggiungi || azione == Azione::Cambia ? righe_dei_campi() : std::vector<std::string>{};
    if (!scrivere) {
        accesso::Apertura a = accesso::apri(cartella, manifesto, par.utente, password, comando.nome,
                                            Visibilita{par.includi_dati_personali, par.includi_dati_particolari},
                                            std::string("anteprima della modifica a mano dell'elenco (") + nome_azione(azione) + ")" +
                                                (par.utente_elenco.empty() ? std::string() : ": utente " + par.utente_elenco),
                                            accesso::ambiente());
        elenco::permessi(a.cassaforte.permessi(), par.utente, comando.nome);
        const ValoriCassaforte v = chiusura::valori_di(a.cassaforte);
        const Calcolo c = calcola(azione, leggi_pacchetto(CartellaSuDisco(cartella), &v), par, righe, cartella.string());
        const cassaforte::Riepilogo r = a.cassaforte.riepilogo();
        return uscita(azione, comando, par, c, false, r, a.registro.voci.size(), elenco::avvisi_di(r, par.utente));
    }
    std::optional<Calcolo> calcolo;
    const cassaforte::Riepilogo r = elenco::scrivi(cartella, manifesto, par.utente, password, comando.nome, [&](const Lettura& vecchia) {
        calcolo = calcola(azione, vecchia, par, righe, cartella.string());
        return std::make_pair(calcolo->nuova, calcolo->nota);
    });
    return uscita(azione, comando, par, *calcolo, true, r, r.registro_voci, elenco::avvisi_di(r, par.utente));
}

}  // namespace

Risultato esegui_aggiungi_in_elenco(const DefinizioneComando& c, const Parametri& p) { return esegui(Azione::Aggiungi, c, p); }
Risultato esegui_cambia_in_elenco(const DefinizioneComando& c, const Parametri& p) { return esegui(Azione::Cambia, c, p); }
Risultato esegui_togli_da_elenco(const DefinizioneComando& c, const Parametri& p) { return esegui(Azione::Togli, c, p); }
Risultato esegui_rimetti_in_elenco(const DefinizioneComando& c, const Parametri& p) { return esegui(Azione::Rimetti, c, p); }
Risultato esegui_cancella_da_elenco(const DefinizioneComando& c, const Parametri& p) { return esegui(Azione::Cancella, c, p); }

}  // namespace kzero::api
