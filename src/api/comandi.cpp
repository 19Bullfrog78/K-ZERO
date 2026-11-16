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

// --- Elenco utenti del committente (K-019) ---------------------------------------------------------------------------

std::vector<DefinizioneComando> comandi_elenco() {
    std::vector<DefinizioneComando> c;
    {
        std::vector<DefinizioneParametro> p{parametro_formato(), parametro_password_da_ingresso(), parametro_utente(true),
                                            parametro_dati_personali(), parametro_dati_particolari()};
        DefinizioneParametro s = parametro_scrivi("La cartella del pacchetto chiuso, la stessa del secondo argomento: con --scrivi le "
                                                  "novita' si scrivono nella cassaforte; senza, si vedono soltanto (anteprima).");
        s.obbligatorio = false;
        p.push_back(s);
        p.push_back(parametro_codice("--profilo", "Il profilo di lettura del file (codice di profili_elenchi.json).", true,
                                     [](Parametri& par, const std::string& v) { par.profilo = v; }));
        DefinizioneParametro a;
        a.nome = "--anno-scolastico";
        a.tipo = TipoParametro::Valore;
        a.forma = FormaValore::AnnoScolastico;
        a.obbligatorio = true;
        a.descrizione = "L'anno scolastico del file (es. 2026-2027): le date senza anno dal mese d'inizio del profilo a dicembre "
                        "sono del primo anno, le altre del secondo.";
        a.applica = [](Parametri& par, const std::string& v) { par.anno_scolastico = std::stoi(v.substr(0, 4)); };
        p.push_back(a);
        DefinizioneParametro e;
        e.nome = "--elenco";
        e.tipo = TipoParametro::Scelta;
        e.valori = {"completo", "parziale"};
        e.obbligatorio = true;   // nessun valore predefinito: un elenco completo fa uscire chi non c'e'
        e.descrizione = "completo = il file ha tutti gli utenti del committente: chi non c'e' esce (solo se tutte le righe si "
                        "leggono); parziale = solo alcuni (es. una zona): nessuno esce.";
        e.applica = [](Parametri& par, const std::string& v) { par.elenco = v; };
        p.push_back(e);
        c.push_back({"leggi-elenco",
                     "Legge l'elenco utenti che il committente manda in un file Excel (.xlsx), con il profilo di lettura dei dati, "
                     "e lo confronta con quello gia' nel pacchetto chiuso: nuovi, cambiati (quali campi), invariati, usciti, righe "
                     "illeggibili e avvisi. Senza --scrivi mostra soltanto cosa cambierebbe (scrive solo la voce del registro); "
                     "con --scrivi mette le novita' nella cassaforte, dopo aver riaperto in memoria il pacchetto nuovo. Serve chi "
                     "ha i dati personali e particolari. Del file si tiene solo l'impronta.",
                     {{"file", "Il file Excel (.xlsx) del committente, che non si modifica mai."},
                      {"cartella", "Cartella del pacchetto chiuso (livello completo)."}},
                     p, Schema{"elenco", 2}, esegui_leggi_elenco});
    }
    {
        std::vector<DefinizioneParametro> p{parametro_formato(), parametro_password_da_ingresso(), parametro_utente(true)};
        p.push_back(parametro_scrivi("La cartella del pacchetto chiuso, la stessa dell'argomento."));
        p.push_back(parametro_codice("--utente-elenco", "Il codice dell'utente dell'elenco (es. U-000012).", true,
                                     [](Parametri& par, const std::string& v) { par.utente_elenco = v; }));
        p.push_back(parametro_codice("--tratta", "La sigla della tratta del profilo (es. RP).", true,
                                     [](Parametri& par, const std::string& v) { par.tratta = v; }));
        DefinizioneParametro r;
        r.nome = "--riferimento";
        r.tipo = TipoParametro::Valore;
        r.forma = FormaValore::Testo;
        r.incompatibili = {"--togli"};
        r.descrizione = "Il riferimento dello sblocco (es. la mail del committente), obbligatorio per aggiungerlo. Dato comune: "
                        "senza nomi di persone.";
        r.applica = [](Parametri& par, const std::string& v) { par.riferimento = v; };
        p.push_back(r);
        DefinizioneParametro d;
        d.nome = "--data";
        d.tipo = TipoParametro::Valore;
        d.forma = FormaValore::Data;
        d.incompatibili = {"--togli"};
        d.descrizione = "La data dello sblocco, scritta dall'utente (il programma non usa l'orologio), obbligatoria per aggiungerlo.";
        d.applica = [](Parametri& par, const std::string& v) { par.data_sblocco = kzero::day_from_text(v); };
        p.push_back(d);
        DefinizioneParametro g;
        g.nome = "--togli";
        g.tipo = TipoParametro::Interruttore;
        g.incompatibili = {"--data", "--riferimento"};
        g.descrizione = "Toglie lo sblocco della tratta invece di aggiungerlo.";
        g.applica = [](Parametri& par, const std::string&) { par.togli = true; };
        p.push_back(g);
        c.push_back({"sblocco",
                     "Aggiunge (con --riferimento e --data) o toglie (con --togli) lo sblocco di una tratta di un utente "
                     "dell'elenco del committente: un utente sospeso viaggia su quella tratta. Lo scrive l'utente di K-Zero (nel file "
                     "del committente non c'e'); una lettura dell'elenco non lo cancella mai. Stessa scrittura e stessi permessi di "
                     "kzero leggi-elenco (dati personali e particolari), voce nel registro.",
                     {{"cartella", "Cartella del pacchetto chiuso."}}, p, Schema{"sblocco", 1}, esegui_sblocco});
    }
    // K-020: modifiche a mano dell'elenco. Stessi parametri di base; i valori dei campi dall'ingresso standard, mai come parametri.
    const auto base_a_mano = [](const std::string& di_che) {
        std::vector<DefinizioneParametro> p{parametro_formato(), parametro_password_da_ingresso(), parametro_utente(true),
                                            parametro_dati_personali(), parametro_dati_particolari()};
        DefinizioneParametro s = parametro_scrivi("La cartella del pacchetto chiuso, la stessa dell'argomento: con --scrivi la modifica si "
                                                  "scrive nella cassaforte; senza, si vede soltanto (anteprima).");
        s.obbligatorio = false;
        p.push_back(s);
        DefinizioneParametro r;
        r.nome = "--riferimento";
        r.tipo = TipoParametro::Valore;
        r.forma = FormaValore::Testo;
        r.obbligatorio = true;
        r.descrizione = "Il riferimento della modifica " + di_che + " (es. la mail del committente). Dato comune: senza nomi di persone.";
        r.applica = [](Parametri& par, const std::string& v) { par.riferimento = v; };
        p.push_back(r);
        DefinizioneParametro d;
        d.nome = "--data";
        d.tipo = TipoParametro::Valore;
        d.forma = FormaValore::Data;
        d.obbligatorio = true;
        d.descrizione = "La data della modifica, scritta dall'utente (il programma non usa l'orologio): non prima dell'ultima modifica a "
                        "mano dello stesso campo (lo stesso giorno va bene).";
        d.applica = [](Parametri& par, const std::string& v) { par.data_sblocco = kzero::day_from_text(v); };
        p.push_back(d);
        return p;
    };
    const auto utente_elenco = [] {
        return parametro_codice("--utente-elenco", "Il codice dell'utente dell'elenco (es. U-000012).", true,
                                [](Parametri& par, const std::string& v) { par.utente_elenco = v; });
    };
    const std::string campi_da_ingresso =
        " I campi si scrivono sull'ingresso standard (mai come parametri: finirebbero nella storia dei comandi e nell'elenco dei processi), "
        "una riga per campo, \"campo: valore\", fino alla fine o a una riga vuota (con --password-da-ingresso la password e' la prima "
        "riga): nome, scuola, stato, percorsi, fermata, comune, frazione, via, civico, classe, sezione e \"orari <titolo della "
        "colonna>\", con le notazioni del file del committente lette con il profilo dell'elenco. Serve chi ha i dati personali e "
        "particolari; senza --scrivi mostra soltanto cosa cambierebbe.";
    const std::vector<DefinizioneArgomento> cartella = {{"cartella", "Cartella del pacchetto chiuso (livello completo)."}};
    {
        auto p = base_a_mano("(aggiunta)");
        p.push_back(parametro_codice("--profilo", "Il profilo di lettura dell'elenco del committente (codice di profili_elenchi.json).", true,
                                     [](Parametri& par, const std::string& v) { par.profilo = v; }));
        DefinizioneParametro a;
        a.nome = "--anno-scolastico";
        a.tipo = TipoParametro::Valore;
        a.forma = FormaValore::AnnoScolastico;
        a.descrizione = "Solo se il committente non ha ancora un elenco nel pacchetto: il suo anno scolastico (es. 2026-2027).";
        a.applica = [](Parametri& par, const std::string& v) { par.anno_scolastico = std::stoi(v.substr(0, 4)); };
        p.push_back(a);
        c.push_back({"aggiungi-in-elenco",
                     "Aggiunge a mano un utente all'elenco del committente (es. una mail con un utente nuovo), con un codice nuovo (mai dal "
                     "nome, unico in tutto il pacchetto). Servono nome, scuola e stato. Resta \"aggiunto a mano\" finche' un file del "
                     "committente lo porta; una lettura completa senza di lui non lo toglie." + campi_da_ingresso,
                     cartella, p, Schema{"modifica_elenco", 1}, esegui_aggiungi_in_elenco});
    }
    {
        auto p = base_a_mano("(cambio)");
        p.push_back(utente_elenco());
        c.push_back({"cambia-in-elenco",
                     "Cambia a mano uno o piu' campi di un utente dell'elenco (es. una mail con un cambio di casa, di orari, di stato, di "
                     "ditta). Un campo si sostituisce intero (la casa con tutte le sue parti, gli orari una colonna alla volta); i campi "
                     "non scritti restano come sono. Si ricorda il valore di prima: alla lettura del file vince l'informazione piu' nuova, "
                     "e quando non si puo' sapere resta il valore a mano." + campi_da_ingresso,
                     cartella, p, Schema{"modifica_elenco", 1}, esegui_cambia_in_elenco});
    }
    {
        auto p = base_a_mano("(utente tolto)");
        p.push_back(utente_elenco());
        c.push_back({"togli-da-elenco",
                     "Toglie a mano un utente dall'elenco (es. una mail: lascia il servizio): esce subito dal piano e se ne va davvero, con i "
                     "suoi sblocchi, alla prima lettura completa del file senza di lui. Si annulla con kzero rimetti-in-elenco. Serve chi ha i "
                     "dati personali e particolari; senza --scrivi mostra soltanto cosa cambierebbe.",
                     cartella, p, Schema{"modifica_elenco", 1}, esegui_togli_da_elenco});
    }
    {
        auto p = base_a_mano("(utente rimesso)");
        p.push_back(utente_elenco());
        c.push_back({"rimetti-in-elenco",
                     "Annulla il togliere a mano di un utente; oppure rimette nell'elenco un utente fuori elenco (uscito, tenuto per uno "
                     "sblocco: es. una mail dice che torna), che resta finche' un file del committente lo riporta. Serve chi ha i dati "
                     "personali e particolari; senza --scrivi mostra soltanto cosa cambierebbe.",
                     cartella, p, Schema{"modifica_elenco", 1}, esegui_rimetti_in_elenco});
    }
    {
        auto p = base_a_mano("(utente cancellato)");
        p.push_back(utente_elenco());
        c.push_back({"cancella-da-elenco",
                     "Cancella subito un utente da tutto il pacchetto, con i suoi sblocchi: non resta niente di lui e il suo codice non si "
                     "da' mai a nessun altro. Se il file del committente lo ha ancora, la lettura dopo lo aggiunge come utente nuovo. Serve "
                     "chi ha i dati personali e particolari; senza --scrivi mostra soltanto cosa cambierebbe.",
                     cartella, p, Schema{"modifica_elenco", 1}, esegui_cancella_da_elenco});
    }
    return c;
}

// --- Schede dei servizi (K-021) ---------------------------------------------------------------------------------------

std::vector<DefinizioneComando> comandi_schede() {
    std::vector<DefinizioneComando> c;
    const auto testo = [](const char* nome, bool obbligatorio, std::string descrizione, void (*applica)(Parametri&, const std::string&)) {
        DefinizioneParametro x;
        x.nome = nome;
        x.tipo = TipoParametro::Valore;
        x.forma = FormaValore::Testo;
        x.obbligatorio = obbligatorio;
        x.descrizione = std::move(descrizione);
        x.applica = applica;
        return x;
    };
    const auto data = [](std::string descrizione) {
        DefinizioneParametro d;
        d.nome = "--data";
        d.tipo = TipoParametro::Valore;
        d.forma = FormaValore::Data;
        d.obbligatorio = true;
        d.descrizione = std::move(descrizione);
        d.applica = [](Parametri& par, const std::string& v) { par.data_sblocco = kzero::day_from_text(v); };
        return d;
    };
    {
        std::vector<DefinizioneParametro> p{parametro_formato(), parametro_password_da_ingresso(), parametro_utente(true),
                                            parametro_dati_personali(), parametro_dati_particolari()};
        DefinizioneParametro s = parametro_scrivi("La cartella del pacchetto chiuso, la stessa del secondo argomento: con --scrivi le "
                                                  "schede si scrivono nel pacchetto e nella cassaforte; senza, si vedono soltanto "
                                                  "(anteprima).");
        s.obbligatorio = false;
        p.push_back(s);
        p.push_back(parametro_codice("--profilo", "Il profilo delle schede (codice di profili_schede.json).", true,
                                     [](Parametri& par, const std::string& v) { par.profilo = v; }));
        p.push_back(parametro_codice("--contratto", "Il contratto dei servizi fatti dalle schede (contratti.json), del committente del "
                                                    "profilo.",
                                     true, [](Parametri& par, const std::string& v) { par.contratto = v; }));
        p.push_back(data("La presa in carico, scritta dall'utente (il programma non usa l'orologio): una scheda con l'attivazione da "
                         "questo giorno in poi vale dall'attivazione; prima, serve una scelta (--validita)."));
        p.push_back(testo("--riferimento", true,
                          "Il riferimento delle schede (es. la mail del committente). Dato comune: senza nomi di persone.",
                          [](Parametri& par, const std::string& v) { par.riferimento = v; }));
        p.push_back(testo("--validita", false,
                          "Da quando valgono le schede del file con l'attivazione prima della presa in carico: attivazione, "
                          "presa-in-carico oppure una data AAAA-MM-GG. Senza, quelle schede si fermano: il programma non sceglie.",
                          [](Parametri& par, const std::string& v) { par.validita = v; }));
        p.push_back(testo("--validita-scheda", false,
                          "La scelta per una o piu' schede, che vince su --validita: Nr=valore, separate da \";\" (es. \"4000123/3=2026-11-20\"); "
                          "una data vale anche per una scheda con l'attivazione dopo la presa in carico.",
                          [](Parametri& par, const std::string& v) { par.validita_schede = v; }));
        c.push_back({"leggi-schede",
                     "Legge le schede dei servizi del committente copiate nel modello Excel (.xlsx) di K-Zero, con il profilo delle schede "
                     "dei dati: ogni scheda diventa un servizio, un gruppo, una corsa e un calendario, controllati con i totali della "
                     "scheda (una scheda che non torna si ferma, le altre vanno avanti). Lo stesso ID gia' nel pacchetto = versione nuova "
                     "della stessa scheda, dalla data scelta: i giorni prima non cambiano mai. Senza --scrivi mostra soltanto cosa "
                     "cambierebbe (scrive solo la voce del registro); con --scrivi scrive nel pacchetto e nella cassaforte, dopo aver "
                     "riaperto in memoria il pacchetto nuovo. Serve chi ha i dati personali e particolari. Del file si tiene solo "
                     "l'impronta.",
                     {{"file", "Il file Excel (.xlsx) con le schede copiate, che non si modifica mai."},
                      {"cartella", "Cartella del pacchetto chiuso."}},
                     p, Schema{"schede", 1}, esegui_leggi_schede});
    }
    {
        std::vector<DefinizioneParametro> p{parametro_formato(), parametro_password_da_ingresso(), parametro_utente(true),
                                            parametro_dati_personali(), parametro_dati_particolari()};
        DefinizioneParametro s = parametro_scrivi("La cartella del pacchetto chiuso, la stessa dell'argomento: con --scrivi la scheda si "
                                                  "toglie; senza, si vede soltanto (anteprima).");
        s.obbligatorio = false;
        p.push_back(s);
        p.push_back(testo("--scheda", true, "La scheda da togliere: il suo codice nel registro (es. SCH-4000123-3) o il suo ID.",
                          [](Parametri& par, const std::string& v) { par.scheda = v; }));
        p.push_back(testo("--riferimento", true, "Perche' si toglie (es. \"scheda copiata per errore\"). Dato comune: senza nomi di persone.",
                          [](Parametri& par, const std::string& v) { par.riferimento = v; }));
        p.push_back(data("La data, scritta dall'utente (il programma non usa l'orologio)."));
        DefinizioneParametro t;
        t.nome = "--tutte";
        t.tipo = TipoParametro::Interruttore;
        t.descrizione = "Toglie tutte le versioni della scheda (senza: solo l'ultima, e la versione di prima riprende la sua fine).";
        t.applica = [](Parametri& par, const std::string&) { par.tutte = true; };
        p.push_back(t);
        c.push_back({"togli-scheda",
                     "Toglie una scheda scritta per errore: l'ultima versione (servizio, gruppo, corsa, calendario), e la versione di "
                     "prima riprende la fine che aveva; con --tutte tutte le versioni. Restano scritti nel registro delle schede il "
                     "riferimento e la data. Non toglie una corsa usata da un turno o da un blocco. Stessa scrittura e stessi permessi "
                     "di kzero leggi-schede; senza --scrivi mostra soltanto cosa cambierebbe.",
                     {{"cartella", "Cartella del pacchetto chiuso."}}, p, Schema{"togli_scheda", 1}, esegui_togli_scheda});
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
         {"controlla", 6},
         esegui_controlla},
        {"descrivi",
         "Descrive il programma: comandi, argomenti, parametri, schemi JSON, file dei pacchetti dati, codici di uscita, "
         "di errore e dei controlli.",
         {},
         {parametro_formato()},
         {"descrivi", 6},
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
         {"giornata", 6},
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
         {"ore", 3},
         esegui_ore},
        {"versione",
         "Mostra la versione del programma.",
         {},
         {parametro_formato()},
         {"versione", 1},
         esegui_versione},
    };
    for (DefinizioneComando& x : comandi_cassaforte()) c.push_back(std::move(x));
    for (DefinizioneComando& x : comandi_elenco()) c.push_back(std::move(x));
    for (DefinizioneComando& x : comandi_schede()) c.push_back(std::move(x));
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
    static const std::vector<Schema> s = {
        {"controlla", 1},     {"controlla", 2},     {"controlla", 3}, {"controlla", 4}, {"controlla", 5},
        {"descrivi", 1},      {"descrivi", 2},      {"descrivi", 3},  {"descrivi", 4},  {"descrivi", 5},
        {"dimostrazione", 1}, {"dimostrazione", 2},
        {"elenco", 1},
        {"giornata", 1},      {"giornata", 2},      {"giornata", 3},  {"giornata", 4},  {"giornata", 5},
        {"ore", 1},           {"ore", 2}};
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
