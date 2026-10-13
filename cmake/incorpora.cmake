# K-Zero - incorpora nel programma i file indicati (schemi dei dati e pacchetto dimostrativo):
# genera un file C++ con il contenuto di ogni file, cosi' il programma non dipende da dove viene installato.
# Uso: cmake -DSORGENTI=<cartella del repository> -DUSCITA=<file .cpp> "-DFILE=<file;file;...>" -P incorpora.cmake
cmake_minimum_required(VERSION 3.25)

set(dati "")
set(voci "")
set(numero 0)
list(SORT FILE)
foreach(f IN LISTS FILE)
  file(RELATIVE_PATH nome "${SORGENTI}" "${f}")
  file(READ "${f}" esadecimale HEX)
  string(LENGTH "${esadecimale}" cifre)
  math(EXPR byte "${cifre} / 2")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," corpo "${esadecimale}")
  # A capo ogni 16 byte (le espressioni di CMake non conoscono le ripetizioni {n}).
  string(REGEX REPLACE "(0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],)" "\\1\n" corpo "${corpo}")
  # Uno zero in fondo: anche un file vuoto diventa un elenco valido in C++.
  string(APPEND dati "const unsigned char file_${numero}[] = {\n${corpo}0x00};\n")
  string(APPEND voci "    {\"${nome}\", file_${numero}, ${byte}},\n")
  math(EXPR numero "${numero} + 1")
endforeach()

file(WRITE "${USCITA}.nuovo"
"// Generato da cmake/incorpora.cmake: non modificare a mano.
#include \"risorse.hpp\"

namespace kzero::api::risorse {

namespace {

${dati}
}  // namespace

const std::vector<Risorsa>& elenco() {
    static const std::vector<Risorsa> tutte = {
${voci}    };
    return tutte;
}

}  // namespace kzero::api::risorse
")
# Riscrive il file solo se cambia: niente ricompilazioni inutili.
file(COPY_FILE "${USCITA}.nuovo" "${USCITA}" ONLY_IF_DIFFERENT)
file(REMOVE "${USCITA}.nuovo")
