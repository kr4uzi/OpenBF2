# [CHOSEN, not recovered] data/d3dx-refusals.txt ("<16 hex> A|R" per line, sorted) -> C++ table.
file(READ "${IN}" text)
string(REGEX REPLACE "#[^\n]*\n" "" text "${text}")
string(REGEX REPLACE "([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]) ([AR])" "{0x\\1ull,'\\2'}," text "${text}")
if(text MATCHES "^[ \n\t]*$")
    set(text "{0,'?'},")
endif()
file(WRITE "${OUT}" "// Generated from data/d3dx-refusals.txt by refusals.cmake.\n${text}\n")
