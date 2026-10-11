# K-Zero - prova sugli schemi: un documento JSON dichiara il proprio schema ("schema", "schema_versione");
# lo strumento standard check-jsonschema controlla che lo rispetti davvero.
# Il file dello schema si ricava sempre cosi': docs/schema/<schema>.v<schema_versione>.schema.json
# Uso: cmake -DVALIDATORE=<check-jsonschema> -DSORGENTI=<cartella del repository> -DDOCUMENTO=<file JSON> -P schema.cmake
cmake_minimum_required(VERSION 3.25)

file(READ "${DOCUMENTO}" testo)
string(JSON nome ERROR_VARIABLE problema GET "${testo}" schema)
if(problema)
  message(FATAL_ERROR "${DOCUMENTO}: JSON senza \"schema\": ${problema}")
endif()
string(JSON versione ERROR_VARIABLE problema GET "${testo}" schema_versione)
if(problema)
  message(FATAL_ERROR "${DOCUMENTO}: JSON senza \"schema_versione\": ${problema}")
endif()

set(file_schema "${SORGENTI}/docs/schema/${nome}.v${versione}.schema.json")
if(NOT EXISTS "${file_schema}")
  message(FATAL_ERROR "${DOCUMENTO} dichiara lo schema ${nome} versione ${versione}, ma manca ${file_schema}")
endif()

execute_process(
  COMMAND "${VALIDATORE}" --schemafile "${file_schema}" "${DOCUMENTO}"
  RESULT_VARIABLE risultato
  OUTPUT_VARIABLE resoconto
  ERROR_VARIABLE resoconto
)
if(risultato)
  message(FATAL_ERROR "${DOCUMENTO} non rispetta ${file_schema}:\n${resoconto}")
endif()
message(STATUS "${DOCUMENTO} rispetta docs/schema/${nome}.v${versione}.schema.json")
