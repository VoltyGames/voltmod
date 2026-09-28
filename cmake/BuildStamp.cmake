# cmake -DSOURCE=<dir> -DOUTPUT=<header> [-DFALLBACK=<commit>] -P BuildStamp.cmake
# FALLBACK is for a tree without git history, such as the Conan cache.

# Plugins sharing a repository build at once; keep git off the index lock.
set(ENV{GIT_OPTIONAL_LOCKS} 0)
execute_process(
    COMMAND git -C "${SOURCE}" describe --always --dirty --abbrev=7 --exclude=*
    OUTPUT_VARIABLE stamp
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE failed
)
if(failed OR stamp STREQUAL "")
    set(stamp "${FALLBACK}")
endif()
if(stamp STREQUAL "")
    set(stamp "unknown")
endif()

# Rewritten only when the stamp changes, so nothing relinks for an unchanged one.
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT "#pragma once\n#define VOLTMOD_BUILD_STAMP \"${stamp}\"\n")
