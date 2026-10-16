include_guard(GLOBAL)

get_filename_component(_s2sdk_pkg "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
# INTERNAL shares the root across directories and replaces stale package revisions.
set(S2SDK_ROOT "${_s2sdk_pkg}" CACHE INTERNAL "s2sdk package root")
unset(_s2sdk_pkg)

# PCH and unity builds disturb these sources' include order and namespace state.
function(_s2sdk_add_sources target)
    target_sources("${target}" PRIVATE ${ARGN})
    set_source_files_properties(${ARGN} PROPERTIES
        COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>"
        SKIP_PRECOMPILE_HEADERS ON
        SKIP_UNITY_BUILD_INCLUSION ON
    )
endfunction()

# entitysystem.cpp needs NameMatches from entityidentity.cpp; convar.cpp stays per plugin.
function(s2sdk_attach_engine_sources target)
    _s2sdk_add_sources("${target}"
        "${S2SDK_ROOT}/entity2/entityidentity.cpp"
        "${S2SDK_ROOT}/entity2/entitykeyvalues.cpp"
        "${S2SDK_ROOT}/entity2/entitysystem.cpp"
        "${S2SDK_ROOT}/tier0_perproject/keyvalues3.cpp"
    )
endfunction()

# Consumer-side compilation avoids a compiler ABI dependency in the package.
function(s2sdk_attach_generated_sources target)
    file(GLOB _generated
        "${S2SDK_ROOT}/generated/public/*.pb.cc"
        "${S2SDK_ROOT}/generated/game-shared/*.pb.cc"
    )
    if(NOT _generated)
        message(FATAL_ERROR "s2sdk shipped no generated protobuf sources at ${S2SDK_ROOT}/generated")
    endif()
    _s2sdk_add_sources("${target}" ${_generated})
endfunction()

# Allocator overrides and ConVar registration state must remain module-local.
function(s2sdk_attach_plugin_support target)
    _s2sdk_add_sources("${target}"
        "${S2SDK_ROOT}/public/tier0/memoverride.cpp"
        "${S2SDK_ROOT}/tier0_perproject/convar.cpp"
    )
endfunction()
