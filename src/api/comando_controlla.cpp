// K-Zero API - comando "controlla": controlla un pacchetto dati e ne scrive il resoconto, in testo o in JSON.
// Il resoconto va sempre sull'uscita standard: e' il risultato del comando anche quando trova errori nei dati
// (allora il codice di uscita e' 1). Un errore del programma (es. cartella inesistente) va invece sull'uscita
// di errore con un codice K-ERR, e il resoconto resta vuoto.
#include <string>

#include "accesso.hpp"
#include "controllo.hpp"
#include "errori.hpp"
#include "esecutori.hpp"
#include "formato_dati.hpp"
#include "riservatezza.hpp"

namespace kzero::api {

namespace {

std::string quanti(std::size_t n, const char* uno, const char* molti) {
    return std::to_string(n) + " " + (n == 1 ? uno : molti);
}

// Numero di elementi, oppure "aziende non lette" se il file non e' stato letto.
std::string quanti_opz(const std::optional<std::size_t>& n, const char* uno, const char* molti, const char* non_letti) {
    return n ? quanti(*n, uno, molti) : std::string(molti) + " " + non_letti;
}

const char* nome_livello_dati(kzero::Level l) { return l == kzero::Level::Full ? "completo" : "solo_numeri"; }

// "persone.json, /persone/2", con la nota se la posizione completa e' riservata.
std::string posizione_testo(const ProblemaPresentato& p) {
    const kzero::Source& s = p.posizione;
    std::string t = s.file;
    if (!s.pointer.empty()) t += ", " + s.pointer;
    if (s.line > 0) t += ", riga " + std::to_string(s.line) + ", colonna " + std::to_string(s.column);
    if (p.posizione_ridotta)
        t += std::string(t.empty() ? "" : "; ") + "il punto preciso con " + parametro_per(*p.posizione_ridotta);
    return t;
}

// "1 errore e 2 avvisi", "nessun errore e 1 avviso".
std::string errori_e_avvisi(int errori, int avvisi) {
    const auto n = [](int x, const char* uno, const char* molti) {
        return x == 0 ? std::string("nessun ") + uno : std::to_string(x) + " " + (x == 1 ? uno : molti);
    };
    return n(errori, "errore", "errori") + " e " + n(avvisi, "avviso", "avvisi");
}

Json posizione_json(const kzero::Source& s) {
    Json j = Json::object();
    j["file"] = s.file;
    if (!s.pointer.empty()) j["puntatore"] = s.pointer;
    if (s.line > 0) {
        j["riga"] = s.line;
        j["colonna"] = s.column;
    }
    return j;
}

struct Conteggi {
    std::optional<std::size_t> aziende, luoghi, servizi, corse, mezzi, persone, turni;
};

Conteggi conteggi(const Resoconto& r) {
    const kzero::Package& p = r.lettura.pacchetto;
    return {r.quanti("pacchetto.json", p.manifest.companies.size()), r.quanti("luoghi.json", p.places.size()),
            r.quanti("servizi.json", p.services.size()),          r.quanti("corse.json", p.trips.size()),
            r.quanti("mezzi.json", p.vehicles.size()),            r.quanti("persone.json", p.persons.size()),
            r.quanti("turni.json", p.shifts.size())};
}

// Quali dati personali e particolari contiene il pacchetto (quanti valori, mai i valori): utile per sapere cosa e'
// coinvolto se il pacchetto finisce nelle mani sbagliate (docs/sicurezza.md). Di base solo i totali; file per file con
// --includi-dati-personali (il nome di un file di soli dati personali, es. assenze.json, dice gia' qualcosa).
// Con la cassaforte (K-010): quelli contati alla chiusura (tutti, anche delle classi che l'utente non apre) piu' quelli
// scritti in chiaro nei file normali.
Lettura::ValoriRiservati totali_riservati(const Lettura& l, const accesso::LetturaPerComando* c = nullptr) {
    Lettura::ValoriRiservati t;
    if (c && c->cassaforte) {
        t.personali = c->cassaforte->valori.personali + l.in_chiaro.personali;
        t.particolari = c->cassaforte->valori.particolari + l.in_chiaro.particolari;
        return t;
    }
    for (const auto& [file, v] : l.riservati) {
        t.personali += v.personali;
        t.particolari += v.particolari;
    }
    return t;
}

const char* nome_classe(Classe c) { return c == Classe::Particolare ? "particolari" : "personali"; }

std::string valori_riservati_testo(const Lettura& l, const Visibilita& visibilita, const accesso::LetturaPerComando& cf) {
    const Lettura::ValoriRiservati tot = totali_riservati(l, &cf);
    if (tot.personali + tot.particolari == 0) return "nessuno";
    std::string t = std::to_string(tot.personali) + " valori personali e " + std::to_string(tot.particolari) + " particolari";
    if (cf.cassaforte) {
        t += ", nella cassaforte";
        if (l.in_chiaro.personali + l.in_chiaro.particolari > 0)
            t += " (" + std::to_string(l.in_chiaro.personali + l.in_chiaro.particolari) + " in chiaro nei file normali)";
    }
    if (!visibilita.personali) return t + " (file per file: --includi-dati-personali)";
    std::string per_file;
    for (const FileDati& f : file_dati()) {
        const auto it = l.riservati.find(f.file);
        if (it == l.riservati.end()) continue;
        per_file += (per_file.empty() ? "" : "; ") + f.file + " " + std::to_string(it->second.personali) + " personali e " +
                    std::to_string(it->second.particolari) + " particolari";
    }
    return t + ": " + per_file;
}

std::string in_testo(const Resoconto& r, const Visibilita& visibilita, const accesso::LetturaPerComando& cf) {
    std::string t;
    const kzero::Manifest& m = r.lettura.pacchetto.manifest;
    if (r.lettura.usato("pacchetto.json")) {
        t += "Controllo del pacchetto " + m.code + (m.name.empty() ? "" : " \"" + m.name + "\"") + " (livello " +
             (m.level == kzero::Level::Full ? "completo" : "solo numeri") + ")\n";
    } else {
        t += "Controllo del pacchetto (manifesto non letto)\n";
    }
    if (cf.cassaforte)
        t += "Cassaforte aperta dall'utente " + cf.utente + " (" + cassaforte::testo_permessi_breve(cf.permessi) +
             "); conservazione dei dati fino al " + cf.cassaforte->conservazione + (cf.scaduta ? " (data passata)" : "") + ".\n";
    // Una riga sola per i problemi che rivelano un dato particolare: senza codice ne' posizione, solo quanti sono.
    if (r.errori_riservati + r.avvisi_riservati > 0)
        t += "Problemi sui dati particolari (salute), non elencati uno per uno: " +
             errori_e_avvisi(r.errori_riservati, r.avvisi_riservati) + ". Per vederli: --includi-dati-particolari.\n";
    for (const ProblemaPresentato& p : r.problemi) {
        t += std::string(p.livello == Livello::Errore ? "Errore " : "Avviso ") + p.codice;
        const std::string pos = posizione_testo(p);
        if (!pos.empty()) t += " (" + pos + ")";
        t += ": " + p.messaggio + "\n";
    }
    const int non_verificati = r.turni_non_verificati ? r.turni_non_verificati->first + r.turni_non_verificati->second : 0;
    if (r.problemi.empty() && r.errori_riservati + r.avvisi_riservati == 0 && non_verificati == 0) t += "Nessun problema.\n";
    if (!r.contenuto_controllato)
        t += "Controlli sul contenuto non eseguiti: prima vanno corretti gli errori nella forma dei file.\n";
    for (const Resoconto::NonVerificati& nv : r.non_verificati) {
        std::string codici;
        for (const std::string& x : nv.controlli) codici += (codici.empty() ? "" : ", ") + x;
        t += std::string("Non verificato (servono i dati ") + nome_classe(nv.classe) + ", che l'utente non puo' aprire): i " +
             "controlli sui dati " + nome_classe(nv.classe) + " stessi e quelli che li usano (" + codici +
             "). I loro risultati non sono ne' tra i problemi ne' nel riepilogo.\n";
    }
    // K-017: i controlli dei turni per chi non apre i dati particolari: la riga c'e' sempre, che ce ne siano o no.
    if (r.turni_non_verificati)
        t += "Controlli dei turni: si vedono i problemi certi; quelli che dipendono dal tipo di servizio (dato particolare, che "
             "l'utente non puo' aprire) non sono verificati e non sono ne' tra i problemi ne' nel riepilogo: " +
             errori_e_avvisi(r.turni_non_verificati->first, r.turni_non_verificati->second) + ".\n";
    t += "Dati personali e particolari nel pacchetto: " + valori_riservati_testo(r.lettura, visibilita, cf) + ".\n";
    const Conteggi c = conteggi(r);
    t += "Riepilogo: " + quanti_opz(c.aziende, "azienda", "aziende", "non lette") + ", " +
         quanti_opz(c.luoghi, "luogo", "luoghi", "non letti") + ", " + quanti_opz(c.servizi, "servizio", "servizi", "non letti") +
         ", " + quanti_opz(c.corse, "corsa", "corse", "non lette") + ", " + quanti_opz(c.mezzi, "mezzo", "mezzi", "non letti") +
         ", " + quanti_opz(c.persone, "persona", "persone", "non lette") + ", " + quanti_opz(c.turni, "turno", "turni", "non letti") +
         "; " +
         quanti(static_cast<std::size_t>(r.errori), "errore", "errori") +
         (r.errori_del_piano > 0 ? " (" + std::to_string(r.errori_del_piano) + " del piano degli orari)" : std::string()) + ", " +
         quanti(static_cast<std::size_t>(r.avvisi), "avviso", "avvisi") + ".\n";
    return t;
}

std::string in_json(const DefinizioneComando& comando, const Resoconto& r, const Visibilita& v, const accesso::LetturaPerComando& cf) {
    Json j = documento(comando.schema);
    if (r.lettura.usato("pacchetto.json")) {
        const kzero::Manifest& m = r.lettura.pacchetto.manifest;
        Json p = Json::object();
        p["codice"] = m.code;
        if (!m.name.empty()) p["nome"] = m.name;
        p["livello"] = nome_livello_dati(m.level);
        j["pacchetto"] = p;
    } else {
        j["pacchetto"] = nullptr;
    }
    j["dati_personali_inclusi"] = v.personali;
    j["dati_particolari_inclusi"] = v.particolari;
    if (cf.cassaforte) {
        Json x = Json::object();
        x["utente"] = cf.utente;
        x["permessi"] = accesso::classi(cf.permessi.personali, cf.permessi.particolari);
        x["conservazione"] = cf.cassaforte->conservazione;
        x["conservazione_scaduta"] = cf.scaduta;
        j["cassaforte"] = x;
    } else {
        j["cassaforte"] = nullptr;
    }
    Json nv = Json::array();
    for (const Resoconto::NonVerificati& x : r.non_verificati) nv.push_back({{"servono", nome_classe(x.classe)}, {"controlli", x.controlli}});
    j["non_verificato"] = nv;
    j["controlli_sul_contenuto"] = r.contenuto_controllato;
    j["problemi_riservati"] = {{"errori", r.errori_riservati}, {"avvisi", r.avvisi_riservati}};
    j["problemi_dei_turni_non_verificati"] =
        r.turni_non_verificati ? Json{{"errori", r.turni_non_verificati->first}, {"avvisi", r.turni_non_verificati->second}} : Json(nullptr);
    Json problemi = Json::array();
    for (const ProblemaPresentato& p : r.problemi) {
        Json x = Json::object();
        x["livello"] = nome_livello(p.livello);
        x["natura"] = p.piano ? "piano" : "dati";
        x["codice"] = p.codice;
        x["messaggio"] = p.messaggio;
        if (!p.posizione.file.empty()) x["posizione"] = posizione_json(p.posizione);
        if (p.posizione_ridotta) x["posizione_completa_con"] = parametro_per(*p.posizione_ridotta);
        if (!p.giorni.empty()) {
            Json giorni = Json::array();
            for (const auto& [dal, al] : p.giorni)
                giorni.push_back({{"dal", kzero::text_from_day(dal)}, {"al", kzero::text_from_day(al)}});
            x["giorni"] = giorni;
        }
        problemi.push_back(x);
    }
    j["problemi"] = problemi;
    const Conteggi c = conteggi(r);
    const auto numero = [](const std::optional<std::size_t>& n) { return n ? Json(*n) : Json(nullptr); };
    Json riepilogo = Json::object();
    riepilogo["aziende"] = numero(c.aziende);
    riepilogo["luoghi"] = numero(c.luoghi);
    riepilogo["servizi"] = numero(c.servizi);
    riepilogo["corse"] = numero(c.corse);
    riepilogo["mezzi"] = numero(c.mezzi);
    riepilogo["persone"] = numero(c.persone);
    riepilogo["turni"] = numero(c.turni);
    riepilogo["errori"] = r.errori;
    riepilogo["errori_del_piano"] = r.errori_del_piano;
    riepilogo["avvisi"] = r.avvisi;
    j["riepilogo"] = riepilogo;
    const Lettura::ValoriRiservati tot = totali_riservati(r.lettura, &cf);
    Json riservati = Json::object();
    riservati["personali"] = tot.personali;
    riservati["particolari"] = tot.particolari;
    Json per_file = Json::array();
    for (const FileDati& f : file_dati()) {
        const auto it = r.lettura.riservati.find(f.file);
        if (it != r.lettura.riservati.end())
            per_file.push_back({{"file", f.file}, {"personali", it->second.personali}, {"particolari", it->second.particolari}});
    }
    riservati["per_file"] = v.personali ? per_file : Json(nullptr);
    j["dati_riservati_presenti"] = riservati;
    return testo_json(j);
}

}  // namespace

Risultato esegui_controlla(const DefinizioneComando& comando, const Parametri& parametri) {
    return controlla_su(comando, parametri, accesso::leggi_per_comando(parametri.argomenti.at(0), parametri, comando.nome, ""));
}

Risultato controlla_su(const DefinizioneComando& comando, const Parametri& parametri, accesso::LetturaPerComando lc) {
    const Visibilita visibilita = lc.visibilita;
    const Resoconto r = controlla_lettura(std::move(lc.lettura), visibilita);
    Risultato risultato;
    risultato.uscita = parametri.formato == Formato::Json ? in_json(comando, r, visibilita, lc) : in_testo(r, visibilita, lc);
    risultato.codice = r.errori > 0 ? static_cast<int>(CodiceUscita::ErroreDati) : static_cast<int>(CodiceUscita::Riuscito);
    return risultato;
}

}  // namespace kzero::api
