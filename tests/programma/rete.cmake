# K-Zero - prova "niente rete" (docs/sicurezza.md): il programma lavora sul PC e oggi nessun comando usa la rete.
# Il futuro adattatore delle distanze sara' l'unica eccezione: un programma separato, con un suo mandato.
#   MODO=programma: il programma compilato non importa nessuna funzione di rete (socket, connect, getaddrinfo...) e non
#                   e' collegato a librerie di rete (Linux, simboli ELF letti con nm e objdump);
#   MODO=sorgenti:  nessun file del codice (src, prove, CMake) include intestazioni di rete o si collega a librerie
#                   di rete.
# Uso: cmake -DMODO=programma -DKZERO=<programma> -DNM=<nm> -DOBJDUMP=<objdump> -P rete.cmake
#      cmake -DMODO=sorgenti -DSORGENTI=<cartella del repository> -P rete.cmake
cmake_minimum_required(VERSION 3.25)

if(MODO STREQUAL "programma")
  execute_process(COMMAND "${NM}" -D --undefined-only "${KZERO}" OUTPUT_VARIABLE elenco RESULT_VARIABLE esito)
  if(esito)
    message(FATAL_ERROR "nm non riesce a leggere ${KZERO} (${esito})")
  endif()
  # Una riga per simbolo importato: "                 U socket@GLIBC_2.2.5" -> "socket".
  string(REGEX REPLACE "[ \t]*[Uw] " "" elenco "${elenco}")
  string(REGEX REPLACE "@[^\n]*" "" elenco "${elenco}")
  string(REPLACE "\n" ";" simboli "${elenco}")
  list(LENGTH simboli quanti)
  # La prova non deve passare per sbaglio: il programma importa di sicuro l'operatore new della libreria C++.
  if(quanti LESS 20 OR NOT "_Znwm" IN_LIST simboli)
    message(FATAL_ERROR "elenco dei simboli importati da ${KZERO} non letto correttamente (${quanti} simboli)")
  endif()
  set(vietati socket socketpair connect bind listen accept accept4 shutdown setsockopt getsockopt getpeername getsockname
      send sendto sendmsg sendmmsg recv recvfrom recvmsg recvmmsg
      getaddrinfo freeaddrinfo gai_strerror getnameinfo gethostbyname gethostbyname2 gethostbyname_r gethostbyname2_r
      gethostbyaddr gethostbyaddr_r gethostent getservbyname getservbyport getprotobyname
      inet_aton inet_addr inet_ntoa inet_pton inet_ntop if_nametoindex res_init res_query res_search __res_query)
  foreach(s IN LISTS simboli)
    if(s IN_LIST vietati OR s MATCHES "^(curl_|SSL_|TLS_|BIO_|gnutls_|nghttp2_|WSA)")
      message(FATAL_ERROR "il programma importa la funzione di rete '${s}': nessun comando deve usare la rete")
    endif()
  endforeach()
  execute_process(COMMAND "${OBJDUMP}" -p "${KZERO}" OUTPUT_VARIABLE intestazioni RESULT_VARIABLE esito)
  if(esito)
    message(FATAL_ERROR "objdump non riesce a leggere ${KZERO} (${esito})")
  endif()
  string(REGEX MATCHALL "NEEDED[ \t]+[^\n]+" librerie "${intestazioni}")
  foreach(l IN LISTS librerie)
    if(l MATCHES "(curl|ssl|crypto|gnutls|resolv|nsl|socket|nghttp|microhttpd|websocket)")
      message(FATAL_ERROR "il programma e' collegato a una libreria di rete: ${l}")
    endif()
  endforeach()
  message(STATUS "niente rete: ${quanti} simboli importati controllati, librerie: ${librerie}")
elseif(MODO STREQUAL "sorgenti")
  file(GLOB_RECURSE sorgenti "${SORGENTI}/src/*" "${SORGENTI}/tests/*.cpp" "${SORGENTI}/tests/*.hpp")
  file(GLOB cmake_file "${SORGENTI}/cmake/*.cmake")
  list(APPEND cmake_file "${SORGENTI}/CMakeLists.txt" "${SORGENTI}/tests/CMakeLists.txt")
  set(intestazioni "#[ \t]*include[ \t]*[<\"](sys/socket\\.h|netinet/|arpa/inet\\.h|netdb\\.h|sys/un\\.h|ifaddrs\\.h|winsock|ws2tcpip\\.h|wininet\\.h|winhttp\\.h|iphlpapi\\.h|curl/|openssl/|gnutls/|boost/asio|asio\\.hpp|httplib\\.h|cpr/)")
  set(librerie "(ws2_32|wsock32|wininet|winhttp|iphlpapi|libcurl|CURL::|OpenSSL::|GnuTLS|find_package\\((CURL|OpenSSL|GnuTLS|Boost))")
  set(controllati 0)
  foreach(f IN LISTS sorgenti cmake_file)
    file(READ "${f}" testo)
    math(EXPR controllati "${controllati} + 1")
    if(testo MATCHES "${intestazioni}")
      message(FATAL_ERROR "${f} include un'intestazione di rete (${CMAKE_MATCH_0}): nessun comando deve usare la rete")
    endif()
    if(f MATCHES "\\.cmake$|CMakeLists\\.txt$" AND testo MATCHES "${librerie}")
      message(FATAL_ERROR "${f} usa una libreria di rete (${CMAKE_MATCH_0}): nessun comando deve usare la rete")
    endif()
  endforeach()
  if(controllati LESS 40)
    message(FATAL_ERROR "troppo pochi file controllati (${controllati}): la ricerca dei sorgenti non funziona")
  endif()
  message(STATUS "niente rete nei sorgenti: ${controllati} file controllati")
else()
  message(FATAL_ERROR "MODO sconosciuto: ${MODO}")
endif()
