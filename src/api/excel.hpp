#pragma once
// K-Zero API - lettura di un foglio di un file Excel (.xlsx) che arriva da fuori (K-019: l'elenco utenti del committente).
// Piano 2: un lettore di file esterni, che da' una tabella di testi; cosa vogliono dire lo decide un profilo nei dati
// (elenco.hpp). Un .xlsx e' un archivio ZIP con dentro file XML: lo si legge tutto in memoria, mai estratto sul disco.
//
// Sicurezza (docs/sicurezza.md, "File Excel"): il file e' del committente e puo' essere rovinato o preparato apposta.
// - Archivio (miniz): solo in memoria; archivi cifrati, su piu' dischi, con nomi di parte ripetuti (senza distinguere
//   maiuscole, come vuole il formato) o con metodi di compressione diversi da "nessuna" e "deflate" rifiutati; i limiti si
//   controllano sui byte veri decompressi, mai sulle dimensioni scritte nell'archivio (che puo' mentire): si decomprime a
//   pezzi e ci si ferma appena si passa un limite. Si decomprimono solo le parti che servono.
// - XML (Expat, compilata senza DTD ne' entita'): un DOCTYPE si rifiuta appena compare (un foglio Excel non ne ha);
//   nessuna entita' esterna; profondita' massima; mai un testo piu' lungo del massimo di una cella tenuto in memoria.
// - Formule: solo il valore salvato nel file, mai calcolate. Celle senza valore (solo formattazione) ignorate.
// - Colori e formattazione non sono dati: si ignorano.
// I messaggi degli errori non contengono mai il contenuto delle celle.
//
// I limiti stanno nel programma, mai nei dati: un file preparato non deve poterli alzare (DECISIONI.md, K-019).

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace kzero::api::excel {

struct Limiti {
    std::uint64_t file_byte = 20ull * 1024 * 1024;               // il file .xlsx
    std::uint64_t decompresso_totale = 200ull * 1024 * 1024;     // tutte le parti lette, byte veri
    std::uint64_t decompresso_parte = 100ull * 1024 * 1024;      // una parte, byte veri
    std::uint64_t rapporto_compressione = 200;                   // byte veri per byte compresso, oltre la soglia
    std::uint64_t soglia_rapporto = 1024 * 1024;                 // sotto questi byte veri il rapporto non conta
    std::size_t parti = 200;                                     // parti dell'archivio (voci della directory centrale)
    std::uint32_t righe = 100000;                                // numero di riga piu' alto con un valore
    std::uint32_t colonne = 200;                                 // colonna piu' a destra con un valore
    std::size_t cella = 2000;                                    // caratteri (byte UTF-8) di una cella
    std::size_t stringhe_condivise = 1000000;                    // testi nella tabella dei testi condivisi
    int profondita = 64;                                         // annidamento degli elementi XML
};

// I limiti del programma (gli unici: le prove interne ne usano di piu' piccoli solo per provarli).
const Limiti& limiti();

struct Cella {
    enum class Tipo {
        Testo,
        Numero,      // il numero come scritto nel file (es. "21", "3.5")
        VeroFalso,   // "0" o "1"
        Errore,      // una cella con un errore di Excel (es. #N/D): nessun valore
        TroppoLunga, // piu' lunga del massimo: il testo non si tiene
    };
    std::uint32_t colonna = 0;   // da 1 (A)
    Tipo tipo = Tipo::Testo;
    std::string testo;
};

struct Riga {
    std::uint32_t numero = 0;   // da 1, come in Excel
    std::vector<Cella> celle;   // solo le celle con un valore, in ordine di colonna
};

struct Foglio {
    std::vector<Riga> righe;    // solo le righe con almeno un valore, in ordine
};

class ErroreExcel : public std::runtime_error {
public:
    enum class Motivo {
        NonXlsx,           // non e' un archivio ZIP
        VecchioXls,        // formato .xls (Excel 97-2003)
        ArchivioRotto,     // archivio troncato o rovinato, dati che non tornano (CRC, dimensioni)
        Cifrato,           // archivio o parte cifrati
        PiuDischi,         // archivio diviso su piu' dischi
        NomiRipetuti,      // due parti con lo stesso nome
        CompressioneNonAmmessa,
        TroppeParti,
        TroppoGrande,      // un limite sulla dimensione (file, decompresso, parte)
        Bomba,             // rapporto di compressione oltre il limite
        ParteMancante,     // manca una parte che serve (es. il foglio, la cartella di lavoro)
        XmlMalformato,
        Doctype,           // un DOCTYPE (DTD) in una parte
        FoglioMancante,    // il foglio indicato non c'e', o il file non ha fogli
        TroppeRighe,
        TroppeColonne,
        TroppiTesti,       // tabella dei testi condivisi oltre il limite
        CellaRotta,        // riferimento di cella o indice di un testo condiviso sbagliato, cella ripetuta
    };
    ErroreExcel(Motivo m, const std::string& messaggio) : std::runtime_error(messaggio), motivo(m) {}
    Motivo motivo;
};

// Legge il foglio "nome_foglio" (vuoto = il primo della cartella di lavoro) dal contenuto di un file .xlsx. Lancia ErroreExcel.
Foglio leggi_foglio(std::string_view file, const std::string& nome_foglio, const Limiti& l = limiti());

// Le parti di un archivio ZIP in memoria, con i controlli e i limiti di sopra (per le prove e il fuzzing dell'archivio).
// nomi = le parti da decomprimere (le altre non si toccano); restituisce il contenuto nello stesso ordine ("" se manca e
// mancanti_ammesse, altrimenti ErroreExcel ParteMancante).
std::vector<std::string> parti_dell_archivio(std::string_view file, const std::vector<std::string>& nomi, bool mancanti_ammesse,
                                             const Limiti& l = limiti());

// I nomi delle parti dell'archivio, dopo i controlli (per le prove).
std::vector<std::string> nomi_delle_parti(std::string_view file, const Limiti& l = limiti());

// Il foglio da una parte XML gia' decompressa e dalla tabella dei testi condivisi (per le prove e il fuzzing dell'XML).
Foglio foglio_da_xml(std::string_view xml, const std::vector<std::string>& condivisi, const Limiti& l = limiti());
std::vector<std::string> testi_condivisi_da_xml(std::string_view xml, const Limiti& l = limiti());

// Decodifica le sequenze "_xHHHH_" dei testi del formato (es. "_x000D_" = a capo).
std::string decodifica_escape(std::string_view testo);

}  // namespace kzero::api::excel
