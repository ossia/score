# READLINE_FOUND
# READLINE_INCLUDE_DIRS
# READLINE_LIBRARIES

if (READLINE_INCLUDE_DIRS AND READLINE_LIBRARIES)
	set(READLINE_FIND_QUIETLY TRUE)
endif()

include(CheckCXXSourceCompiles)

find_path(READLINE_INCLUDE_DIRS NAMES readline/readline.h)
if (NOT READLINE_INCLUDE_DIRS AND NOT READLINE_FIND_QUIETLY)
	message(ERROR "couldn't find the readline/readline.h header")
endif()
find_library(READLINE_LIBRARIES NAMES libreadline readline)
if (NOT READLINE_LIBRARIES AND NOT READLINE_FIND_QUIETLY)
	message(ERROR "couldn't find libreadline")
endif()

if (READLINE_INCLUDE_DIRS AND READLINE_LIBRARIES)
	set(_save_req_includes ${CMAKE_REQUIRED_INCLUDES})
	set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${READLINE_INCLUDE_DIR})
	check_cxx_source_compiles("
		#include <stdio.h>
		#include <readline/readline.h>
		#if !defined(RL_VERSION_MAJOR) || !defined(RL_VERSION_MINOR)
		#error Ancient version of readline
		#endif
		int main() { return 0; }
		"
		_rl_version_check)
	set(CMAKE_REQUIRED_INCLUDES ${_save_req_includes})
	if (NOT _rl_version_check)
		set(READLINE_INCLUDE_DIRS READLINE-NOTFOUND)
		set(READLINE_LIBRARIES READLINE-NOTFOUND)
		if (NOT READLINE_FIND_QUIETLY)
			message(ERROR "Ancient/unsupported version of readline")
		endif()
	endif()
endif()

if (NOT READLINE_INCLUDE_DIRS OR NOT READLINE_LIBRARIES)
	set(READLINE_LIBRARIES READLINE-NOTFOUND)
	set(READLINE_INCLUDE_DIRS READLINE-NOTFOUND)
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(READLINE REQUIRED_VARS READLINE_INCLUDE_DIRS READLINE_LIBRARIES)

