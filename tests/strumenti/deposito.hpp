#pragma once
// K-Zero - controllo del deposito pubblico (K-015). Il deposito e' pubblico dal 5/11/2026: nessun dato vero entra nei suoi
// file (CLAUDE.md, Confini). Questo controllo e' una rete, non la difesa (docs/sicurezza.md): cerca
//   - quattro nomi che non si scrivono nei file del deposito ne' nei messaggi dei commit e nei titoli delle proposte
//     (a, b, c, d: chi sono e cosa si scrive al loro posto e' detto nella tabella di deposito.cpp);
//   - fuori da tests/ ed esempi/, i file dei formati dei dati (fogli di calcolo, documenti, cassaforte, registro) e i
//     manifesti di un pacchetto (pacchetto.json);
//   - (K-016) i file che non sono testo UTF-8 ne' binari dell'elenco dei binari ammessi;
//   - (K-016 aggiunta 1) i testi con un byte zero fuori dalle cartelle di un elenco (oggi tests/fuzz/); in ogni testo con
//     byte zero i nomi si cercano anche con i byte zero tolti.
// I nomi non sono scritti in chiaro: il controllo conosce solo l'impronta SHA-256 di ogni nome in minuscolo (libsodium).
// Ogni parola dei testi (lettere e cifre sono parole diverse) si porta in minuscolo e se ne calcola l'impronta; per un
// nome di due parole l'impronta delle due parole vicine, anche a capo o con un segno di commento in mezzo. I messaggi dicono file, riga e quale nome (a-d), mai
// il nome. Le impronte non sono un segreto: chi prova le parole (una sigla di tre lettere in pochi istanti) le ritrova;
// servono solo a non scrivere i nomi in chiaro.
// Strumento delle prove: non fa parte del programma kzero.

#include <array>
#include <cstddef>
#include <filesystem>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace kzero::deposito {

using Impronta = std::array<unsigned char, 32>;   // SHA-256

// SHA-256 di un testo (libsodium, crypto_hash_sha256). Chiama sodium_init la prima volta.
Impronta impronta(std::string_view testo);

// Un nome da non scrivere: la sua etichetta, chi e', cosa si scrive al suo posto, l'impronta del nome in minuscolo e
// quante parole ha (2 = due parole separate da un solo spazio prima di calcolare l'impronta).
struct Nome {
    char etichetta = '?';
    std::string chi;
    std::string sostituto;
    Impronta impronta{};
    int parole = 1;
};

// I quattro nomi del deposito (solo le impronte).
const std::vector<Nome>& nomi_del_deposito();

// Un problema trovato: dove (file o testo), riga (0 = nessuna riga: il percorso o il file intero) e cosa, senza il nome.
struct Problema {
    std::string dove;
    std::size_t riga = 0;
    std::string cosa;
};

// Cerca i nomi in un testo (UTF-8; i byte che non sono UTF-8 valgono come separatori). Parola intera: una parola e' una
// fila di lettere e segni Unicode, oppure di cifre e segni: il passaggio tra una lettera e una cifra, nei due versi,
// separa due parole (K-016: "<nome>2027" e "78<nome>" si vedono); maiuscole e minuscole contano tutte. Un nome di due
// parole vale anche a capo o con in mezzo soli spazi e segni di commento (/ * # - > e le virgolette).
std::vector<Problema> cerca_nomi(std::string_view testo, std::string_view dove, const std::vector<Nome>& nomi);

// Come cerca_nomi, ma in un testo con byte zero (K-016 aggiunta 1) cerca i nomi nel testo com'e' e anche nel testo con i
// byte zero tolti (un nome in UTF-16 o UTF-32 senza BOM, di lettere semplici, ha un byte zero tra ogni lettera). Le righe
// restano quelle del file (si tolgono solo i byte zero, mai gli a capo); un nome trovato in tutti e due i modi alla
// stessa riga si segnala una volta sola (per ogni riga e nome conta il numero piu' alto dei due modi).
std::vector<Problema> cerca_nomi_anche_senza_zeri(std::string_view testo, std::string_view dove,
                                                  const std::vector<Nome>& nomi);

// I binari ammessi (K-016): ogni file del deposito e' testo UTF-8 valido, tranne i file con una di queste estensioni
// (scritte cosi', in minuscolo) dentro una di queste cartelle in cima. L'elenco si allarga solo con un mandato
// (CLAUDE.md, Confini).
struct BinarioAmmesso {
    std::string estensione;
    std::vector<std::string> cartelle;
};
const std::vector<BinarioAmmesso>& binari_ammessi();

// Vero se il file (percorso relativo alla cima del deposito, con "/") sta nell'elenco dei binari ammessi.
bool binario_ammesso(std::string_view relativo);

// Le cartelle in cui un testo puo' contenere un byte zero (K-016 aggiunta 1): conta l'inizio del percorso (es.
// "tests/fuzz/"), a ogni profondita' sotto. Oggi solo tests/fuzz/ (i semi e i casi del fuzzing). Fuori di li' un testo
// UTF-8 con un byte zero e' un problema. L'elenco si allarga solo con un mandato (CLAUDE.md, Confini).
const std::vector<std::string>& cartelle_con_byte_zero();

// Vero se il file (percorso relativo, con "/") sta sotto una delle cartelle_con_byte_zero.
bool byte_zero_ammesso(std::string_view relativo);

// Resoconto del controllo di una cartella.
struct Resoconto {
    std::vector<Problema> problemi;
    std::size_t letti = 0;                 // file letti come testo (anche i testi UTF-16 e UTF-32 con BOM, decodificati)
    std::vector<std::string> binari;       // binari ammessi non UTF-8 (contenuto non letto, percorso controllato)
    std::vector<std::string> collegamenti; // collegamenti simbolici: non seguiti, percorso controllato
};

// Le cartelle che il controllo salta a ogni livello, oltre a .git e alle cartelle escluse (K-016): esattamente quelle che
// .gitignore ignora, "build" e "build-*" (l'asterisco vale per qualsiasi fila di caratteri, anche vuota). Chi cambia una
// delle due cambia anche l'altra: la prova Deposito.CartelleSaltateComeGitignore le confronta.
const std::vector<std::string>& cartelle_saltate();

// Vero se il nome di una cartella (solo il nome, senza percorso) e' tra le cartelle saltate. Maiuscole e minuscole
// contano, come per git su Linux.
bool cartella_saltata(std::string_view nome);

// Le righe di un .gitignore che riguardano le cartelle: tutte le righe attive (non vuote, non commenti) tranne le
// eccezioni ("!...") e i formati dei file ("*...."), senza gli spazi in fondo, nell'ordine del file.
std::vector<std::string> righe_delle_cartelle(std::string_view gitignore);

// Controlla tutti i file della cartella, tranne .git (a ogni livello), le cartelle saltate (a ogni livello) e le cartelle
// "escluse" (es. la cartella della costruzione, se sta dentro). Ogni file che e' UTF-8 valido si legge come testo (K-016),
// anche se ha un'estensione dei binari ammessi; se ha un byte zero e non sta in una delle cartelle_con_byte_zero e' un
// problema, e i nomi si cercano anche senza i byte zero (cerca_nomi_anche_senza_zeri). Un file che non e' UTF-8 valido: se e' un binario ammesso
// se ne controlla solo il percorso (i byte casuali di una cassaforte possono contenere per caso una sigla); altrimenti e'
// un problema, e se comincia con il BOM di UTF-16 o UTF-32 lo si decodifica e si cercano anche i nomi. Fuori da tests/
// ed esempi/ (in cima) i formati dei dati e i manifesti di un pacchetto sono un problema. Ordine stabile: file in ordine
// di percorso, poi riga.
Resoconto controlla_cartella(const std::filesystem::path& radice, const std::vector<std::filesystem::path>& escluse,
                             const std::vector<Nome>& nomi);

// Il programma controlla_deposito (argomenti senza il nome del programma):
//   --cartella <cartella> [--escludi <cartella>]...   controlla una cartella (il deposito)
//   --testo <dove>                                     controlla il testo dell'ingresso standard (messaggi, titoli)
// Scrive il resoconto su "uscita"; codici: 0 nessun problema, 1 problemi, 2 argomenti sbagliati, 3 errore di lettura.
int esegui(const std::vector<std::string>& argomenti, std::istream& ingresso, const std::vector<Nome>& nomi,
           std::ostream& uscita);

}  // namespace kzero::deposito
