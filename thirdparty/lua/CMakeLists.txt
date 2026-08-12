# Lua 5.4.8, vendored source build.
#
# Upstream Lua ships a Makefile, not CMake, so this file is the actual build
# wiring for the engine (not a stub). Built as a static library, compiled as
# C (not C++) so Lua's internal setjmp/longjmp error propagation behaves
# exactly as upstream intends. All cross-language error handling happens at
# the sol2 boundary via lua_pcall-based protected calls, never by letting
# a longjmp unwind through C++ stack frames with non-trivial destructors -
# that is undefined behavior and a real crash source if it's allowed to
# happen, so this static lib is intentionally isolated from any C++ TU.
#
# lua.c and luac.c (the CLI interpreter and bytecode compiler executables)
# are excluded: the engine embeds the Lua library only, it doesn't ship or
# depend on the standalone lua.exe/luac.exe binaries.

set(DT_LUA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(DT_LUA_SOURCES
    ${DT_LUA_DIR}/lapi.c
    ${DT_LUA_DIR}/lauxlib.c
    ${DT_LUA_DIR}/lbaselib.c
    ${DT_LUA_DIR}/lcode.c
    ${DT_LUA_DIR}/lcorolib.c
    ${DT_LUA_DIR}/lctype.c
    ${DT_LUA_DIR}/ldblib.c
    ${DT_LUA_DIR}/ldebug.c
    ${DT_LUA_DIR}/ldo.c
    ${DT_LUA_DIR}/ldump.c
    ${DT_LUA_DIR}/lfunc.c
    ${DT_LUA_DIR}/lgc.c
    ${DT_LUA_DIR}/linit.c
    ${DT_LUA_DIR}/liolib.c
    ${DT_LUA_DIR}/llex.c
    ${DT_LUA_DIR}/lmathlib.c
    ${DT_LUA_DIR}/lmem.c
    ${DT_LUA_DIR}/loadlib.c
    ${DT_LUA_DIR}/lobject.c
    ${DT_LUA_DIR}/lopcodes.c
    ${DT_LUA_DIR}/loslib.c
    ${DT_LUA_DIR}/lparser.c
    ${DT_LUA_DIR}/lstate.c
    ${DT_LUA_DIR}/lstring.c
    ${DT_LUA_DIR}/lstrlib.c
    ${DT_LUA_DIR}/ltable.c
    ${DT_LUA_DIR}/ltablib.c
    ${DT_LUA_DIR}/ltm.c
    ${DT_LUA_DIR}/lundump.c
    ${DT_LUA_DIR}/lutf8lib.c
    ${DT_LUA_DIR}/lvm.c
    ${DT_LUA_DIR}/lzio.c
)

add_library(dt_lua STATIC ${DT_LUA_SOURCES})

target_include_directories(dt_lua PUBLIC ${DT_LUA_DIR})

set_target_properties(dt_lua PROPERTIES
    LINKER_LANGUAGE C
    C_STANDARD 99
    FOLDER "thirdparty"
)

if(DT_PLATFORM_LINUX OR UNIX)
    target_compile_definitions(dt_lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(dt_lua PUBLIC m dl)
elseif(WIN32)
    target_compile_definitions(dt_lua PRIVATE LUA_USE_WINDOWS)
endif()

# Lua is compiled as C++ under MSVC when object-based exception boundaries
# matter less than build simplicity, but we explicitly force plain C
# compilation on all compilers to keep longjmp semantics correct.
if(MSVC)
    set_source_files_properties(${DT_LUA_SOURCES} PROPERTIES LANGUAGE C)
endif()
