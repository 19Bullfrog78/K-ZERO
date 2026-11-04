// K-Zero API - tabella dei comandi: l'unico posto dove si dichiarano comandi e parametri.
#include "comandi.hpp"

#include <algorithm>
#include <tuple>

#include "errori.hpp"
#include "esecutori.hpp"
#include "tempo.hpp"

namespace kzero::api {

namespace {

DefinizioneParametro parametro_formato() {
    DefinizioneParametro p;
    p.nome = kParametroFormato;
    p.tipo = TipoParametro::Scelta;
    p.valori = {kFormatoJson, "testo"};
    p.predefinito = "testo";
    p.descrizione = "Formato dell'uscita: testo per le persone, json per i programmi e gli assistenti AI.";
    p.applica = [](Parametri& par, const std::string& valore) {
        par.formato = valore == kFormatoJson ? Formato::Json : Formato::Testo;
    };
    return p;
}

DefinizioneParametro parametro_dati_personali() {
    DefinizioneParametro p;
    p.nome = "--includi-dati-personali";
    p.tipo = TipoParametro::Interruttore;
    p.descrizione = "Mostra i dati personali (nomi, indirizzi, case, punti porta a porta, motivi scritti a mano), "
                    "che di base sono sostituiti dal codice dell'elemento. Vale per il testo e per il JSON.";
    p.applica = [](Parametri& par, const std::string&) { par.includi_dati_personali = true; };
    return p;
}

DefinizioneParametro parametro_dati_particolari() {
    DefinizioneParametro p;
    p.nome = "--includi-dati-particolari";
    p.tipo = TipoParametro::Interruttore;
    p.descrizione = "Mostra i dati particolari sulla salute (carrozzine, assistente necessario, servizio per disabili), "
                    "che di base sono nascosti (\"riservato\"). Vale per il testo e per il JSON.";
    p.applica = [](Parametri& par, const std::string&) { par.includi_dati_particolari = true; };
    return p;
}

DefinizioneParametro parametro_data() {
    DefinizioneParametro p;
    p.nome = "--data";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Data;
    p.obbligatorio = true;
    p.descrizione = "Il giorno da mostrare, dentro il periodo del pacchetto (il programma non usa l'orologio).";
    p.applica = [](Parametri& par, const std::string& valore) { par.data = kzero::day_from_text(valore); };
    return p;
}

// Orari di lavoro (K-011): un giorno (--data) oppure un periodo (--dal e --al), dentro il periodo del pacchetto.
DefinizioneParametro parametro_data_o_periodo() {
    DefinizioneParametro p = parametro_data();
    p.obbligatorio = false;
    p.incompatibili = {"--dal", "--al"};
    p.descrizione = "Il giorno da mostrare, dentro il periodo del pacchetto; oppure un periodo con --dal e --al (il programma non "
                    "usa l'orologio).";
    return p;
}

DefinizioneParametro parametro_periodo(bool inizio) {
    DefinizioneParametro p;
    p.nome = inizio ? "--dal" : "--al";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Data;
    p.incompatibili = {"--data"};
    p.descrizione = inizio ? "Primo giorno del periodo da mostrare (con --al), dentro il periodo del pacchetto."
                           : "Ultimo giorno del periodo da mostrare (con --dal, incluso), dentro il periodo del pacchetto.";
    if (inizio) p.applica = [](Parametri& par, const std::string& valore) { par.dal = kzero::day_from_text(valore); };
    else p.applica = [](Parametri& par, const std::string& valore) { par.al = kzero::day_from_text(valore); };
    return p;
}

DefinizioneParametro parametro_persona_ore() {
    DefinizioneParametro p;
    p.nome = "--persona";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Codice;
    p.descrizione = "Solo gli orari di questa persona (codice di persone.json).";
    p.applica = [](Parametri& par, const std::string& valore) { par.persona = valore; };
    return p;
}

DefinizioneParametro parametro_persona() {
    DefinizioneParametro p;
    p.nome = "--persona";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Codice;
    p.incompatibili = {"--mezzo"};
    p.descrizione = "Solo la giornata di questa persona (codice di persone.json): i turni che fa e quelli che non puo' fare.";
    p.applica = [](Parametri& par, const std::string& valore) { par.persona = valore; };
    return p;
}

DefinizioneParametro parametro_mezzo() {
    DefinizioneParametro p;
    p.nome = "--mezzo";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Codice;
    p.incompatibili = {"--persona"};
    p.descrizione = "Solo la giornata di questo mezzo (codice di mezzi.json): i turni che lo usano e quelli che non puo' fare.";
    p.applica = [](Parametri& par, const std::string& valore) { par.mezzo = valore; };
    return p;
}

// --- Cassaforte (K-010) ---------------------------------------------------------------------------------------------

DefinizioneParametro parametro_utente(bool obbligatorio) {
    DefinizioneParametro p;
    p.nome = "--utente";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Codice;
    p.obbligatorio = obbligatorio;
    p.descrizione = obbligatorio ? "Chi apre la cassaforte (codice dell'utente); la password si chiede dopo, mai come parametro."
                                 : "Chi apre la cassaforte, se il pacchetto ne ha una (codice dell'utente); la password si chiede "
                                   "dopo, mai come parametro. I dati e i parametri --includi-dati-* valgono entro i suoi permessi.";
    p.applica = [](Parametri& par, const std::string& valore) { par.utente = valore; };
    return p;
}

DefinizioneParametro parametro_password_da_ingresso() {
    DefinizioneParametro p;
    p.nome = "--password-da-ingresso";
    p.tipo = TipoParametro::Interruttore;
    p.descrizione = "Le password dall'ingresso standard, una per riga, nell'ordine in cui il comando le chiede (per le prove e "
                    "per un'interfaccia); senza, si chiedono al terminale senza eco. Mai come parametro ne' in una variabile "
                    "d'ambiente.";
    p.applica = [](Parametri& par, const std::string&) { par.password_da_ingresso = true; };
    return p;
}

DefinizioneParametro parametro_scrivi(const std::string& descrizione) {
    DefinizioneParametro p;
    p.nome = "--scrivi";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Percorso;
    p.obbligatorio = true;
    p.descrizione = descrizione;
    p.applica = [](Parametri& par, const std::string& valore) { par.scrivi = valore; };
    return p;
}

DefinizioneParametro parametro_conservazione() {
    DefinizioneParametro p;
    p.nome = "--conservazione";
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Data;
    p.obbligatorio = true;
    p.descrizione = "Fino a quando si tengono i dati personali e particolari (GDPR art. 5 c. 1 lett. e): passata la data, ogni "
                    "apertura lo ricorda.";
    p.applica = [](Parametri& par, const std::string& valore) { par.conservazione = kzero::day_from_text(valore); };
    return p;
}

DefinizioneParametro parametro_codice(const char* nome, std::string descrizione, bool obbligatorio,
                                      void (*applica)(Parametri&, const std::string&)) {
    DefinizioneParametro p;
    p.nome = nome;
    p.tipo = TipoParametro::Valore;
    p.forma = FormaValore::Codice;
    p.obbligatorio = obbligatorio;
    p.descrizione = std::move(descrizione);
    p.applica = applica;
    return p;
}

DefinizioneParametro parametro_permessi() {
    DefinizioneParametro p;
    p.nome = "--permessi";
    p.tipo = TipoParametro::Scelta;
    p.valori = {"particolari", "personali", "tutti"};
    p.obbligatorio = true;   // una scelta sui permessi si scrive sempre: nessun valore predefinito
    p.descrizione = "I permessi del nuovo utente: i dati personali, quelli particolari (salute) o tutti e due; al massimo quelli "
                    "di chi lo aggiunge.";
    p.applica = [](Parametri& par, const std::string& valore) { par.permessi = valore; };
    return p;
}

const char* kCartellaPacchetto = "Cartella del pacchetto dati, con il manifesto pacchetto.json.";

std::vector<DefinizioneComando> comandi_cassaforte() {
    const Schema cassaforte{"cassaforte", 1};
    const auto base = [] { return std::vector<DefinizioneParametro>{parametro_formato(), parametro_password_da_ingresso()}; };
    std::vector<DefinizioneComando> c;
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella nuova in cui scrivere il pacchetto chiuso (non deve esserci gia')."));
        p.push_back(parametro_utente(true));
        p.push_back(parametro_conservazione());
        c.push_back({"chiudi",
                     "Chiude un pacchetto in chiaro nella cassaforte: scrive in una cartella nuova i file normali senza i dati "
                     "personali e particolari, la cassaforte cifrata con quei dati e il registro degli accessi. Chi chiude "
                     "diventa il primo utente (con i permessi dei dati personali e particolari) e sceglie la password, chiesta "
                     "due volte. Prima di dire fatto riapre il pacchetto e controlla che i dati siano identici. L'originale non "
                     "si tocca: va cancellato a mano.",
                     {{"cartella", "Cartella del pacchetto in chiaro da chiudere (manifesto versione 2)."}},
                     p, cassaforte, esegui_chiudi});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso da aggiornare (stessa cassaforte, stessi utenti, stesso "
                                     "registro)."));
        p.push_back(parametro_utente(true));
        c.push_back({"richiudi",
                     "Rimette nella cassaforte di un pacchetto chiuso i dati corretti di un'uscita in chiaro (kzero esporta): "
                     "sostituisce i file normali e i dati della cassaforte, tiene utenti e registro. Serve chi ha i dati personali "
                     "e particolari. Riapre e controlla prima di dire fatto.",
                     {{"cartella", "Cartella del pacchetto in chiaro con i dati corretti."}},
                     p, cassaforte, esegui_richiudi});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella nuova in cui scrivere l'uscita in chiaro (non deve esserci gia')."));
        p.push_back(parametro_utente(true));
        c.push_back({"esporta",
                     "Uscita in chiaro di un pacchetto chiuso, per correggere i dati: scrive in una cartella nuova il pacchetto "
                     "con i dati che l'utente puo' aprire, in chiaro. Scritta nel registro. La cartella in chiaro va cancellata "
                     "appena richiuso il pacchetto (kzero richiudi).",
                     {{"cartella", kCartellaPacchetto}}, p, cassaforte, esegui_esporta});
    }
    {
        auto p = base();
        p.push_back(parametro_utente(true));
        c.push_back({"cassaforte",
                     "Mostra la cassaforte di un pacchetto: codice, chiusura, data di conservazione, quanti valori personali e "
                     "particolari, utenti con i loro permessi, voci del registro. Mai i dati.",
                     {{"cartella", kCartellaPacchetto}}, p, cassaforte, esegui_cassaforte});
    }
    {
        auto p = base();
        p.push_back(parametro_utente(true));
        c.push_back({"registro",
                     "Mostra il registro degli accessi alla cassaforte, dopo averne controllato la catena: per ogni voce data e "
                     "ora (UTC), utente, comando, classi aperte e mostrate. Mai i dati. La lettura stessa e' una voce.",
                     {{"cartella", kCartellaPacchetto}}, p, Schema{"registro", 1}, esegui_registro});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso a cui aggiungere l'utente."));
        p.push_back(parametro_utente(true));
        p.push_back(parametro_codice("--nuovo-utente", "Il codice del nuovo utente.", true,
                                     [](Parametri& par, const std::string& v) { par.nuovo_utente = v; }));
        p.push_back(parametro_permessi());
        c.push_back({"aggiungi-utente",
                     "Aggiunge un utente alla cassaforte, con al massimo i permessi di chi lo aggiunge. Si chiedono la password "
                     "di chi aggiunge e quella nuova del nuovo utente (due volte al terminale): l'ha scritta chi aggiunge, e il "
                     "nuovo utente la cambia al primo uso (avviso K-CTR-100).",
                     {}, p, cassaforte, esegui_aggiungi_utente});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso da cui togliere l'utente."));
        p.push_back(parametro_utente(true));
        p.push_back(parametro_codice("--utente-da-togliere", "Il codice dell'utente da togliere.", true,
                                     [](Parametri& par, const std::string& v) { par.utente_da_togliere = v; }));
        c.push_back({"togli-utente",
                     "Toglie un utente dalla cassaforte: chi lo toglie deve avere i dati personali e particolari. Le chiavi dei "
                     "dati che aveva si rifanno e i dati si ricifrano, anche quelli di chi va in un luogo sensibile (si aprono "
                     "solo con tutti e due i permessi): l'utente tolto non apre la versione nuova. Non si toglie l'ultimo utente "
                     "ne' l'ultimo che legge i dati personali o particolari. Dopo, le password vanno cambiate (avvisi K-CTR-099 e, "
                     "a ogni apertura di chi ha ricevuto le chiavi nuove in una busta, K-CTR-098).",
                     {}, p, cassaforte, esegui_togli_utente});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso."));
        p.push_back(parametro_utente(true));
        p.push_back(parametro_codice("--per-utente",
                                     "Password nuova per un altro utente (persa la sua): serve avere tutti i suoi permessi. "
                                     "Senza, si cambia la propria.",
                                     false, [](Parametri& par, const std::string& v) { par.per_utente = v; }));
        c.push_back({"password",
                     "Cambia la propria password, oppure ne da' una nuova a un altro utente che l'ha persa (con --per-utente): "
                     "non c'e' recupero ne' chiave di ripristino. Si chiedono la password di chi la cambia e quella nuova (due "
                     "volte al terminale). In tutti e due i casi l'utente riceve anche una coppia di chiavi nuova; una password "
                     "data a un altro va cambiata al primo uso (avviso K-CTR-100).",
                     {}, p, cassaforte, esegui_password});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso."));
        p.push_back(parametro_utente(true));
        p.push_back(parametro_conservazione());
        c.push_back({"conservazione",
                     "Cambia la data di conservazione dei dati della cassaforte. Serve chi ha i dati personali e particolari.",
                     {}, p, cassaforte, esegui_conservazione});
    }
    {
        auto p = base();
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso di cui cancellare la cassaforte."));
        p.push_back(parametro_utente(true));
        c.push_back({"cancella",
                     "Cancella la cassaforte e il suo registro: prima si distruggono le chiavi (le fessure degli utenti), poi i "
                     "file. Stampa un resoconto da conservare (data, utente, quanti valori per classe, mai i dati). Serve chi ha "
                     "i dati personali e particolari. Le copie vecchie del file restano cifrate dove sono.",
                     {}, p, cassaforte, esegui_cancella});
    }
    return c;
}

std::vector<DefinizioneComando> crea_comandi() {
    std::vector<DefinizioneComando> c = {
        {"controlla",
         "Controlla un pacchetto dati: forma dei file (schemi), codici e riferimenti, calendari, orari delle corse, "
         "persone a bordo, trasbordi, coincidenze, attese e regolamenti. Elenca errori e avvisi con codice K-CTR e "
         "posizione, poi un riepilogo; esce con 1 se trova errori (gli avvisi non bloccano).",
         {{"cartella", "Cartella del pacchetto dati, con il manifesto pacchetto.json."}},
         {parametro_formato(), parametro_dati_personali(), parametro_dati_particolari(), parametro_utente(false),
          parametro_password_da_ingresso()},
         {"controlla", 5},
         esegui_controlla},
        {"descrivi",
         "Descrive il programma: comandi, argomenti, parametri, schemi JSON, file dei pacchetti dati, codici di uscita, "
         "di errore e dei controlli.",
         {},
         {parametro_formato()},
         {"descrivi", 5},
         esegui_descrivi},
        {"dimostrazione",
         "Stampa di prova con il pacchetto dimostrativo esempi/demo (dati inventati, incorporato nel programma): "
         "pacchetto, mezzi, luoghi, servizi, corse e alcune regole del mestiere applicate ai dati.",
         {},
         {parametro_formato(), parametro_dati_personali(), parametro_dati_particolari()},
         {"dimostrazione", 3},
         esegui_dimostrazione},
        {"giornata",
         "Mostra la giornata di un giorno del pacchetto: per ogni turno chi lo fa e da dove viene l'assegnazione "
         "(rotazione, servizio fisso, assegnazione del singolo giorno), il mezzo, le corse (intere o a pezzi) e le "
         "attivita' in ordine con i passaggi, i tempi fermi e i km; i turni scoperti o senza mezzo con il motivo; i "
         "totali. Con --persona o --mezzo solo quella persona o quel mezzo. Se il pacchetto ha errori nei dati non mostra "
         "nulla e rimanda a kzero controlla (gli avvisi e gli errori del piano degli orari non bloccano).",
         {{"cartella", "Cartella del pacchetto dati, con il manifesto pacchetto.json."}},
         {parametro_formato(), parametro_dati_personali(), parametro_dati_particolari(), parametro_data(), parametro_persona(),
          parametro_mezzo(), parametro_utente(false), parametro_password_da_ingresso()},
         {"giornata", 3},
         esegui_giornata},
        {"ore",
         "Mostra gli orari di lavoro delle persone, giorno per giorno, in un giorno (--data) o in un periodo (--dal e --al): "
         "turni, inizio e fine, minuti per categoria di tempo (guida, altre mansioni, disponibilita', reperibilita', "
         "interruzione), lavoro contato da ogni regime (es. lavoro effettivo, orario di lavoro), notturno, riposo prima della "
         "giornata, quali leggi e contratti collettivi valgono e perche', i rientri con la chiave dell'azienda o della persona; "
         "per settimana guida, lavoro e riposo; i totali del periodo e la media tra le persone; i problemi degli orari con gli "
         "stessi codici di kzero controlla. Le regole che guardano fuori dal periodo del pacchetto si verificano solo in parte, "
         "mai \"regolare\". Se il pacchetto ha errori nei dati non mostra nulla (gli errori del piano degli orari no).",
         {{"cartella", "Cartella del pacchetto dati, con il manifesto pacchetto.json."}},
         {parametro_formato(), parametro_dati_personali(), parametro_dati_particolari(), parametro_data_o_periodo(),
          parametro_periodo(true), parametro_periodo(false), parametro_persona_ore(), parametro_utente(false),
          parametro_password_da_ingresso()},
         {"ore", 2},
         esegui_ore},
        {"versione",
         "Mostra la versione del programma.",
         {},
         {parametro_formato()},
         {"versione", 1},
         esegui_versione},
    };
    for (DefinizioneComando& x : comandi_cassaforte()) c.push_back(std::move(x));
    for (auto& comando : c)
        std::sort(comando.parametri.begin(), comando.parametri.end(),
                  [](const DefinizioneParametro& a, const DefinizioneParametro& b) { return a.nome < b.nome; });
    std::sort(c.begin(), c.end(),
              [](const DefinizioneComando& a, const DefinizioneComando& b) { return a.nome < b.nome; });
    return c;
}

}  // namespace

const std::vector<DefinizioneComando>& comandi() {
    static const std::vector<DefinizioneComando> tabella = crea_comandi();
    return tabella;
}

const DefinizioneComando* trova_comando(const std::string& nome) {
    for (const auto& c : comandi())
        if (c.nome == nome) return &c;
    return nullptr;
}

std::string nomi_comandi() {
    std::string s;
    for (const auto& c : comandi()) s += (s.empty() ? "" : ", ") + c.nome;
    return s;
}

std::string forma_comando(const DefinizioneComando& c) {
    std::string s = std::string(kNomeProgramma) + " " + c.nome;
    for (const auto& a : c.argomenti) s += " <" + a.nome + ">";
    return s + " [parametri]";
}

const std::vector<Schema>& schemi_ritirati() {
    // Versioni non piu' prodotte: i file restano in docs/schema per controllare le uscite salvate prima.
    static const std::vector<Schema> s = {{"controlla", 1}, {"controlla", 2}, {"controlla", 3}, {"controlla", 4}, {"descrivi", 1},
                                          {"descrivi", 2},  {"descrivi", 3},  {"descrivi", 4},  {"dimostrazione", 1},
                                          {"dimostrazione", 2}, {"giornata", 1},  {"giornata", 2}, {"ore", 1}};
    return s;
}

std::vector<Schema> schemi() {
    std::vector<Schema> s;
    for (const auto& c : comandi()) s.push_back(c.schema);
    s.push_back(schema_errore());
    const auto chiave = [](const Schema& x) { return std::tie(x.nome, x.versione); };
    std::sort(s.begin(), s.end(), [&](const Schema& a, const Schema& b) { return chiave(a) < chiave(b); });
    s.erase(std::unique(s.begin(), s.end(), [&](const Schema& a, const Schema& b) { return chiave(a) == chiave(b); }),
            s.end());
    return s;
}

}  // namespace kzero::api
