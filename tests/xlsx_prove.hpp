#pragma once
// K-Zero - file Excel (.xlsx) costruiti nelle prove e nel fuzzing da parti di testo leggibili (K-019): nessun file binario nel
// deposito (cosi' anche il controllo dei nomi vede tutto il contenuto). Un archivio ZIP scritto qui, a mano, per poterlo anche
// rovinare apposta (cifrato, su piu' dischi, nomi ripetuti, troncato, impronte sbagliate) e una "bomba" di compressione.
// Due modi di comprimere: "nessuno" e deflate con i codici fissi (solo lettere, RFC 1951 3.2.6); la bomba usa un blocco
// deflate con i codici dinamici costruito per ripetere un byte (circa 1.000 byte veri per byte compresso).
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kzero::prove::xlsx {

inline std::uint32_t crc32(const std::string& d) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (unsigned char b : d) {
        c ^= b;
        for (int k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

// Scrive bit per bit, dal bit meno significativo (come vuole deflate).
class Bit {
public:
    void scrivi(std::uint32_t valore, int n) {   // n bit di valore, dal meno significativo
        for (int i = 0; i < n; ++i) uno((valore >> i) & 1u);
    }
    void codice(std::uint32_t codice, int n) {   // un codice di Huffman: dal bit piu' significativo
        for (int i = n - 1; i >= 0; --i) uno((codice >> i) & 1u);
    }
    std::string fine() {
        if (n_ > 0) out_ += static_cast<char>(b_);
        n_ = 0;
        b_ = 0;
        return out_;
    }

private:
    void uno(std::uint32_t bit) {
        b_ |= static_cast<unsigned char>(bit << n_);
        if (++n_ == 8) {
            out_ += static_cast<char>(b_);
            n_ = 0;
            b_ = 0;
        }
    }
    std::string out_;
    unsigned char b_ = 0;
    int n_ = 0;
};

// Deflate con i codici fissi, solo lettere (niente ripetizioni): piu' grande dell'originale, ma e' deflate vero.
inline std::string deflate_fisso(const std::string& d) {
    Bit b;
    b.scrivi(1, 1);   // ultimo blocco
    b.scrivi(1, 2);   // codici fissi
    for (unsigned char c : d) {
        if (c <= 143) b.codice(0x30u + c, 8);
        else b.codice(0x190u + (c - 144u), 9);
    }
    b.codice(0, 7);   // fine del blocco (256)
    return b.fine();
}

// Una bomba: il byte c ripetuto 1 + 258 x ripetizioni volte. Blocco con codici dinamici: lettera c (2 bit), fine (2 bit),
// lunghezza 258 (1 bit), distanza 1 (1 bit): ogni ripetizione costa 2 bit.
inline std::string deflate_bomba(unsigned char c, std::uint64_t ripetizioni) {
    Bit b;
    b.scrivi(1, 1);
    b.scrivi(2, 2);    // codici dinamici
    b.scrivi(29, 5);   // HLIT: 286 codici
    b.scrivi(1, 5);    // HDIST: 2 codici
    b.scrivi(14, 4);   // HCLEN: 18 lunghezze del codice delle lunghezze
    // Ordine: 16 17 18 0 8 7 9 6 10 5 11 4 12 3 13 2 14 1. Usati: 18 (1 bit), 1 e 2 (2 bit).
    const int lunghezze[18] = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2};
    for (int l : lunghezze) b.scrivi(static_cast<std::uint32_t>(l), 3);
    // Codici canonici del codice delle lunghezze: 18 -> 0, 1 -> 10, 2 -> 11.
    const auto zeri = [&](int n) {   // n zeri (11-138) con il simbolo 18
        b.codice(0, 1);
        b.scrivi(static_cast<std::uint32_t>(n - 11), 7);
    };
    const auto zeri_lunghi = [&](int n) {
        while (n > 0) {
            int k = n > 138 ? 138 : n;
            if (n - k > 0 && n - k < 11) k = n - 11;
            zeri(k);
            n -= k;
        }
    };
    const auto lunghezza = [&](int l) { b.codice(l == 1 ? 2u : 3u, 2); };
    // Lettere e lunghezze: c -> 2, 256 -> 2, 285 -> 1; distanze: 0 -> 1, 1 -> 1.
    zeri_lunghi(c);
    lunghezza(2);
    zeri_lunghi(256 - c - 1);
    lunghezza(2);
    zeri_lunghi(285 - 257);
    lunghezza(1);
    lunghezza(1);
    lunghezza(1);
    // Codici canonici: 285 -> 0; c -> 10; 256 -> 11. Distanza 0 -> 0.
    b.codice(2, 2);
    for (std::uint64_t i = 0; i < ripetizioni; ++i) {
        b.codice(0, 1);
        b.codice(0, 1);
    }
    b.codice(3, 2);
    return b.fine();
}

struct Parte {
    std::string nome;
    std::string dati;              // decompressi
    int metodo = 8;                // 0 nessuno, 8 deflate
    std::string compressi;         // se non vuoto: i byte compressi da scrivere cosi' come sono (es. una bomba)
    std::uint64_t veri = 0;        // con "compressi": quanti byte veri dichiarare
    std::uint32_t crc = 0;         // con "compressi": l'impronta da dichiarare
    std::uint16_t bit_in_piu = 0;  // es. 1 = cifrato
    bool crc_sbagliato = false;
    std::int64_t veri_in_piu = 0;  // dimensione decompressa dichiarata diversa da quella vera
    std::uint32_t posizione_in_piu = 0;   // la directory centrale dice una posizione dell'intestazione diversa (es. oltre il file)
};

inline Parte parte(std::string nome, std::string dati, int metodo = 8) {
    Parte p;
    p.nome = std::move(nome);
    p.dati = std::move(dati);
    p.metodo = metodo;
    return p;
}

struct Archivio {
    std::vector<Parte> parti;
    std::uint16_t disco = 0;       // numero del disco nella fine della directory (diverso da 0 = piu' dischi)

    std::string scrivi() const {
        std::string out, centrale;
        const auto u16 = [](std::string& s, std::uint32_t v) {
            s += static_cast<char>(v & 0xFF);
            s += static_cast<char>((v >> 8) & 0xFF);
        };
        const auto u32 = [&](std::string& s, std::uint32_t v) {
            u16(s, v & 0xFFFF);
            u16(s, v >> 16);
        };
        for (const Parte& p : parti) {
            const std::string comp = !p.compressi.empty() ? p.compressi : p.metodo == 8 ? deflate_fisso(p.dati) : p.dati;
            const std::uint32_t crc = (!p.compressi.empty() ? p.crc : crc32(p.dati)) ^ (p.crc_sbagliato ? 1u : 0u);
            const std::uint64_t veri = (!p.compressi.empty() ? p.veri : p.dati.size()) + static_cast<std::uint64_t>(p.veri_in_piu);
            const std::uint32_t posizione = static_cast<std::uint32_t>(out.size());
            const auto testata = [&](std::string& s, bool centrale_) {
                u32(s, centrale_ ? 0x02014b50u : 0x04034b50u);
                if (centrale_) u16(s, 20);
                u16(s, 20);
                u16(s, p.bit_in_piu);
                u16(s, static_cast<std::uint32_t>(p.metodo));
                u16(s, 0);
                u16(s, 0x21);
                u32(s, crc);
                u32(s, static_cast<std::uint32_t>(comp.size()));
                u32(s, static_cast<std::uint32_t>(veri));
                u16(s, static_cast<std::uint32_t>(p.nome.size()));
                u16(s, 0);
                if (centrale_) {
                    u16(s, 0);
                    u16(s, 0);
                    u16(s, 0);
                    u32(s, 0);
                    u32(s, posizione + p.posizione_in_piu);
                }
                s += p.nome;
            };
            testata(out, false);
            out += comp;
            testata(centrale, true);
        }
        const std::uint32_t inizio = static_cast<std::uint32_t>(out.size());
        out += centrale;
        u32(out, 0x06054b50u);
        u16(out, disco);
        u16(out, disco);
        u16(out, static_cast<std::uint32_t>(parti.size()));
        u16(out, static_cast<std::uint32_t>(parti.size()));
        u32(out, static_cast<std::uint32_t>(centrale.size()));
        u32(out, inizio);
        u16(out, 0);
        return out;
    }
};

// Testo XML con i caratteri speciali al loro posto.
inline std::string xml(const std::string& t) {
    std::string out;
    for (char c : t) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

inline std::string colonna(std::size_t i) {   // 0 -> A
    std::string s;
    std::size_t n = i + 1;
    while (n > 0) {
        s.insert(s.begin(), static_cast<char>('A' + (n - 1) % 26));
        n = (n - 1) / 26;
    }
    return s;
}

// Una cella: testo (condiviso o in linea) oppure numero ("#21" = numero 21, "!" = errore di Excel, "=" davanti = formula
// con il valore salvato dopo il segno, es. "=SOMMA|12").
struct Foglio {
    std::string nome = "Foglio1";
    std::vector<std::vector<std::string>> righe;
    bool testi_in_linea = false;
};

// Le parti di un file .xlsx come le scrive Excel (ridotte all'essenziale), dai fogli.
inline std::vector<Parte> parti_xlsx(const std::vector<Foglio>& fogli, int metodo = 8) {
    std::vector<std::string> condivisi;
    std::vector<Parte> parti;
    std::string tipi = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                       "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                       "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                       "<Default Extension=\"xml\" ContentType=\"application/xml\"/></Types>";
    parti.push_back(parte("[Content_Types].xml", tipi, metodo));
    parti.push_back(parte("_rels/.rels",
                     "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                     "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                     "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
                     "officeDocument\" Target=\"xl/workbook.xml\"/></Relationships>",
                     metodo));
    std::string cartella = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                           "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                           "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><sheets>";
    std::string rel = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                      "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    for (std::size_t f = 0; f < fogli.size(); ++f) {
        const std::string n = std::to_string(f + 1);
        cartella += "<sheet name=\"" + xml(fogli[f].nome) + "\" sheetId=\"" + n + "\" r:id=\"rId" + n + "\"/>";
        rel += "<Relationship Id=\"rId" + n + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
               "worksheet\" Target=\"worksheets/sheet" + n + ".xml\"/>";
        std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\"><sheetData>";
        for (std::size_t r = 0; r < fogli[f].righe.size(); ++r) {
            const std::string nr = std::to_string(r + 1);
            s += "<row r=\"" + nr + "\">";
            for (std::size_t c = 0; c < fogli[f].righe[r].size(); ++c) {
                const std::string& v = fogli[f].righe[r][c];
                if (v.empty()) continue;
                const std::string ref = colonna(c) + nr;
                if (v[0] == '#') {
                    s += "<c r=\"" + ref + "\"><v>" + xml(v.substr(1)) + "</v></c>";
                } else if (v == "!") {
                    s += "<c r=\"" + ref + "\" t=\"e\"><v>#N/A</v></c>";
                } else if (v[0] == '=') {
                    const std::size_t bar = v.find('|');
                    s += "<c r=\"" + ref + "\" t=\"str\"><f>" + xml(v.substr(1, bar - 1)) + "</f><v>" + xml(v.substr(bar + 1)) +
                         "</v></c>";
                } else if (fogli[f].testi_in_linea) {
                    s += "<c r=\"" + ref + "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" + xml(v) + "</t></is></c>";
                } else {
                    s += "<c r=\"" + ref + "\" t=\"s\"><v>" + std::to_string(condivisi.size()) + "</v></c>";
                    condivisi.push_back(v);
                }
            }
            s += "</row>";
        }
        s += "</sheetData></worksheet>";
        parti.push_back(parte("xl/worksheets/sheet" + n + ".xml", s, metodo));
    }
    cartella += "</sheets></workbook>";
    rel += "<Relationship Id=\"rIdS\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings\" "
           "Target=\"sharedStrings.xml\"/></Relationships>";
    std::string sst = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                      "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" count=\"" +
                      std::to_string(condivisi.size()) + "\" uniqueCount=\"" + std::to_string(condivisi.size()) + "\">";
    for (const std::string& t : condivisi) sst += "<si><t xml:space=\"preserve\">" + xml(t) + "</t></si>";
    sst += "</sst>";
    parti.push_back(parte("xl/workbook.xml", cartella, metodo));
    parti.push_back(parte("xl/_rels/workbook.xml.rels", rel, metodo));
    parti.push_back(parte("xl/sharedStrings.xml", sst, metodo));
    return parti;
}

inline std::string file_xlsx(const std::vector<Foglio>& fogli, int metodo = 8) { return Archivio{parti_xlsx(fogli, metodo), 0}.scrivi(); }
inline std::string file_righe(const std::vector<std::vector<std::string>>& righe, int metodo = 8) {
    return file_xlsx(std::vector<Foglio>{Foglio{"Foglio1", righe, false}}, metodo);
}

}  // namespace kzero::prove::xlsx
