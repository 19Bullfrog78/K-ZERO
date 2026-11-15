// K-Zero API - quello che i comandi dell'elenco utenti hanno in comune (vedi elenco_scrittura.hpp).
#include "elenco_scrittura.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include "chiusura.hpp"
#include "disco.hpp"
#include "errori.hpp"
#include "riscrittura.hpp"

namespace kzero::api::elenco {

namespace fs = std::filesystem;

void permessi(const cassaforte::Permessi& p, const std::string& utente, const std::string& comando) {
    // Oggi: tutti e due (i comandi scrivono nella cassaforte nomi e case degli utenti dell'elenco, e un utente legato a un luogo
    // sensibile sta tra i legati, che si cifrano con le due chiavi). Domani la scrittura con i soli dati personali passera' da qui:
    // riscrivere la sola sezione dei personali, copiare cosi' come sono quelle dei particolari e dei legati, e chiedere ancora
    // tutti e due i permessi se una novita' tocca un utente legato.
    if (p.tutti()) return;
    throw ErroreKZero(Errore::PermessoMancante, "kzero " + comando + " scrive nella cassaforte nomi e case degli utenti dell'elenco: "
                                                "per ora lo puo' fare solo chi ha i dati personali e particolari (l'utente " + utente +
                                                " ha " + cassaforte::testo_permessi(p) + ").");
}

void permessi_prima_della_password(const fs::path& cartella, const std::string& utente, const std::string& comando) {
    std::vector<cassaforte::UtenteVisibile> utenti;
    try {
        utenti = cassaforte::leggi_intestazione(leggi_dal_disco(cartella, cassaforte::kFile, kMassimoByteFile).testo).utenti;
    } catch (const cassaforte::ErroreCassaforte&) {
        return;   // lo dira' l'apertura
    }
    for (const cassaforte::UtenteVisibile& u : utenti)
        if (u.codice == utente) permessi(u.permessi, utente, comando);
}

void stessa_cartella(const std::string& pacchetto, const std::string& scrivi) {
    std::error_code e1, e2;
    const fs::path a = fs::weakly_canonical(fs::absolute(pacchetto), e1);
    const fs::path b = fs::weakly_canonical(fs::absolute(scrivi), e2);
    if (e1 || e2 || a != b)
        throw ErroreKZero(Errore::ScriviDiversa, "--scrivi deve nominare la cartella del pacchetto che cambia ('" + pacchetto +
                                                     "'), invece e' '" + scrivi + "': niente e' stato scritto.");
}

void pacchetto_completo(const Lettura& l, const std::string& cartella, const std::string& utente) {
    if (!l.usato("pacchetto.json") || !l.forma_valida())
        throw ErroreKZero(Errore::PacchettoConErrori, "Il pacchetto ha errori nella forma dei file: prima vanno corretti (kzero controlla " +
                                                          cartella + " --utente " + utente + ").");
    if (l.pacchetto.manifest.level == kzero::Level::NumbersOnly)
        throw ErroreKZero(Errore::ElencoSoloNumeri, "Il pacchetto e' di livello \"solo numeri\": l'elenco utenti (nomi e case uno per "
                                                    "uno) sta solo in un pacchetto di livello \"completo\" (docs/formato-dati.md).");
}

const kzero::ClientList* elenco_di(const kzero::Package& p, const std::string& committente) {
    for (const kzero::ClientList& l : p.client_lists)
        if (l.client.code == committente) return &l;
    return nullptr;
}

namespace {

std::string nome_giorno(kzero::Weekday g) {
    static const char* nomi[] = {"lunedi", "martedi", "mercoledi", "giovedi", "venerdi", "sabato", "domenica"};
    return nomi[static_cast<int>(g)];
}

JsonDati casa_json(const kzero::ListUserHome& h) {
    JsonDati c = JsonDati::object();
    if (!h.town.empty()) c["comune"] = h.town;
    if (!h.hamlet.empty()) c["frazione"] = h.hamlet;
    if (!h.street.empty()) c["via"] = h.street;
    if (!h.number.empty()) c["civico"] = h.number;
    return c;
}
JsonDati stato_json(const kzero::ListUser& u) {
    JsonDati stato = {{"tipo", u.active ? "attivo" : "sospeso"}};
    if (!u.status_note.empty()) stato["nota"] = u.status_note;
    return stato;
}
std::optional<JsonDati> fermata_json(const kzero::ListUser& u) {
    if (u.stop) return JsonDati{{"luogo", u.stop->code}, {"riconosciuta", u.stop_match == kzero::StopMatch::Name ? "nome" : "codice_esterno"}};
    if (!u.stop_text.empty()) return JsonDati{{"testo", u.stop_text}};
    return std::nullopt;
}
JsonDati tratte_json(const std::vector<kzero::ListUserLeg>& legs) {
    JsonDati v = JsonDati::array();
    for (const kzero::ListUserLeg& l : legs) {
        JsonDati x = {{"sigla", l.leg}};
        if (l.company) x["azienda"] = l.company->code;
        else x["altra_ditta"] = l.other_company;
        v.push_back(x);
    }
    return v;
}
// con_colonna: nell'elenco ogni orario dice la sua colonna; nel valore di prima di una colonna no (e' quella del campo).
JsonDati orari_json(const std::vector<kzero::ListUserTime>& times, bool con_colonna) {
    JsonDati v = JsonDati::array();
    for (const kzero::ListUserTime& t : times) {
        JsonDati x = {{"tratta", t.leg}, {"giorno", nome_giorno(t.day)}, {"ora", kzero::text_from_minutes(t.time)}};
        if (!t.period.empty()) x["periodo"] = t.period;
        x["dal"] = kzero::text_from_day(t.dates.from);
        x["al"] = kzero::text_from_day(t.dates.to);
        if (con_colonna && !t.column.empty()) x["colonna"] = t.column;
        v.push_back(x);
    }
    return v;
}
JsonDati segno_json(const kzero::ManualMark& m) { return {{"riferimento", m.reference}, {"data", kzero::text_from_day(m.date)}}; }

const char* nome_campo(kzero::ListField f) {
    switch (f) {
        case kzero::ListField::Name: return "nome";
        case kzero::ListField::Home: return "casa";
        case kzero::ListField::School: return "scuola";
        case kzero::ListField::Class: return "classe";
        case kzero::ListField::Section: return "sezione";
        case kzero::ListField::Status: return "stato";
        case kzero::ListField::Stop: return "fermata";
        case kzero::ListField::Routes: return "tratte";
        case kzero::ListField::Times: return "orari";
        case kzero::ListField::OutOfList: return "fuori_elenco";
    }
    return "?";
}

// Il valore di prima di un campo (nessun valore: prima il campo era vuoto).
std::optional<JsonDati> prima_json(const kzero::ManualField& m) {
    const kzero::ListUser& b = *m.before;
    switch (m.field) {
        case kzero::ListField::Name: return JsonDati(b.name);
        case kzero::ListField::Home:
            if (b.home && !casa_json(*b.home).empty()) return casa_json(*b.home);
            return std::nullopt;
        case kzero::ListField::School: return JsonDati(b.school.code);
        case kzero::ListField::Class:
            if (b.class_number) return JsonDati(*b.class_number);
            return std::nullopt;
        case kzero::ListField::Section:
            if (!b.section.empty()) return JsonDati(b.section);
            return std::nullopt;
        case kzero::ListField::Status: return stato_json(b);
        case kzero::ListField::Stop: return fermata_json(b);
        case kzero::ListField::Routes: return tratte_json(b.legs);
        case kzero::ListField::Times: return orari_json(b.times, false);
        case kzero::ListField::OutOfList: return JsonDati(true);
    }
    return std::nullopt;
}

JsonDati a_mano_json(const kzero::ListUser& u) {
    JsonDati j = JsonDati::object();
    if (u.added) j["aggiunto"] = segno_json(*u.added);
    if (u.removed) j["tolto"] = segno_json(*u.removed);
    if (!u.manual.empty()) {
        JsonDati campi = JsonDati::object();
        JsonDati orari = JsonDati::array();
        std::vector<const kzero::ManualField*> ordinati;
        for (const kzero::ManualField& m : u.manual) ordinati.push_back(&m);
        std::stable_sort(ordinati.begin(), ordinati.end(), [](const kzero::ManualField* a, const kzero::ManualField* b) {
            return std::make_pair(static_cast<int>(a->field), a->column) < std::make_pair(static_cast<int>(b->field), b->column);
        });
        for (const kzero::ManualField* m : ordinati) {
            JsonDati x = segno_json(m->mark);
            const std::optional<JsonDati> prima = prima_json(*m);
            if (prima) x["prima"] = *prima;
            if (m->field == kzero::ListField::Times) {
                JsonDati y = {{"colonna", m->column}};
                for (auto& [k, v] : x.items()) y[k] = v;
                orari.push_back(y);
            } else {
                campi[nome_campo(m->field)] = x;
            }
        }
        if (!orari.empty()) campi["orari"] = orari;
        j["campi"] = campi;
    }
    return j;
}

JsonDati utente_json(const kzero::ListUser& u) {
    JsonDati j = JsonDati::object();
    j["codice"] = u.code;
    j["nome"] = u.name;
    if (u.home && !casa_json(*u.home).empty()) j["casa"] = casa_json(*u.home);
    j["scuola"] = u.school.code;
    if (u.class_number) j["classe"] = *u.class_number;
    if (!u.section.empty()) j["sezione"] = u.section;
    j["stato"] = stato_json(u);
    if (const std::optional<JsonDati> f = fermata_json(u)) j["fermata"] = *f;
    if (!u.legs.empty()) j["tratte"] = tratte_json(u.legs);
    if (!u.times.empty()) j["orari"] = orari_json(u.times, true);
    if (!u.unlocks.empty()) {
        JsonDati v = JsonDati::array();
        for (const kzero::ListUserUnlock& s : u.unlocks)
            v.push_back({{"tratta", s.leg}, {"riferimento", s.reference}, {"data", kzero::text_from_day(s.date)}});
        j["sblocchi"] = v;
    }
    if (u.out_of_list) j["fuori_elenco"] = true;
    if (u.added || u.removed || !u.manual.empty()) j["a_mano"] = a_mano_json(u);
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

}  // namespace

JsonDati documento_con(const Lettura& vecchia, const kzero::ClientList& l, int ultimo_numero) {
    JsonDati d = vecchia.documenti.count(kFile) ? vecchia.documenti.at(kFile)
                                                : JsonDati{{"schema", "elenco_utenti"}, {"schema_versione", kVersione}, {"ultimo_numero", 0},
                                                           {"elenchi", JsonDati::array()}};
    // Gli altri elenchi della versione 1 si scrivono uguali: la 2 aggiunge soltanto campi che non devono esserci.
    d["schema_versione"] = kVersione;
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

Lettura con_documento(const Lettura& vecchia, const std::string& file, const JsonDati& doc) {
    PacchettoInMemoria m;
    for (const auto& [f, d] : vecchia.documenti)
        m.file[f] = chiusura::testo_file(f == "pacchetto.json" ? chiusura::manifesto_in_chiaro(d) : d);
    m.file[file] = chiusura::testo_file(doc);
    Lettura l = leggi_pacchetto(m);
    if (!l.forma_valida()) throw std::logic_error("il documento preparato non rispetta lo schema");
    return l;
}

cassaforte::Riepilogo scrivi(const fs::path& cartella, const ManifestoLetto& manifesto, const std::string& utente,
                             const cripto::Segreto& password, const std::string& comando,
                             const std::function<std::pair<Lettura, std::string>(const Lettura& vecchia)>& prepara) {
    std::optional<chiusura::Separazione> separazione;
    std::uint32_t generazione = 0;
    const cassaforte::Riepilogo r = accesso::cambia(
        cartella, manifesto, utente, password, comando,
        [&](cassaforte::Cassaforte& c) {
            permessi(c.permessi(), utente, comando);
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

std::vector<accesso::AvvisoMostrato> avvisi_di(const cassaforte::Riepilogo& r, const std::string& utente) {
    std::vector<accesso::AvvisoMostrato> out;
    for (const kzero::Problem& p : accesso::avvisi_dell_apertura(r, utente)) out.push_back(accesso::mostra(p));
    return out;
}

}  // namespace kzero::api::elenco
