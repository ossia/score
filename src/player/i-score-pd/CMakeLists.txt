project("score-pd")

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_MACOSX_RPATH Off)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-redundant-decls")

if(MSVC)
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/pd.lib")
    message("Downloading and extracting PureData windows binary")
    # On windows we have to create a .lib file for Pd since the original one is incomplete

    # First download Pd.
    file(DOWNLOAD "http://msp.ucsd.edu/Software/pd-0.47-1.msw.zip" "${CMAKE_CURRENT_BINARY_DIR}/pd.zip")

    # Extract Pd.
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_CURRENT_BINARY_DIR}/pd.zip"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/")

    # Then we follow this process :
    # https://adrianhenke.wordpress.com/2008/12/05/create-lib-file-from-dll
    execute_process(
        COMMAND dumpbin /exports "${CMAKE_CURRENT_BINARY_DIR}/pd/bin/pd.dll"
        OUTPUT_VARIABLE PD_WIN32_DEF)

    # Convert to list
    string(REPLACE "\n" ";" PD_WIN32_DEF "${PD_WIN32_DEF}")

    # Only keep the relevant lines
    list(FILTER PD_WIN32_DEF INCLUDE REGEX " *([0-9]+) *([A-F0-9]+) ([A-F0-9]+) (.*)")

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}\\pd.def" "EXPORTS\n")

    # Filter each line to get only the function name
    foreach(PD_FUN_LINE ${PD_WIN32_DEF})
        string(REGEX REPLACE " *([0-9]+) *([A-F0-9]+) ([A-F0-9]+) " ""  PD_FUN_LINE "${PD_FUN_LINE}")
        string(STRIP "${PD_FUN_LINE}" PD_FUN_LINE)
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}\\pd.def" "${PD_FUN_LINE}\n")
    endforeach()

    # Finally, create a .lib from the .def
    if(${CMAKE_GENERATOR} MATCHES "64")
        set(PD_LIB_MACHINE X64)
    else()
        set(PD_LIB_MACHINE X86)
    endif()

    execute_process(
        COMMAND lib "/MACHINE:${PD_LIB_MACHINE}" "/def:${CMAKE_CURRENT_BINARY_DIR}\\pd.def" "/OUT:${CMAKE_CURRENT_BINARY_DIR}/pd.lib")
endif()
endif()

set(OSSIA_PD_PATH "${OSSIA_3RDPARTY_FOLDER}/pure-data")
set(OSSIA_CICM_PATH "${OSSIA_3RDPARTY_FOLDER}/CicmWrapper")
include_directories("${OSSIA_PD_PATH}/src")

set(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
if(NOT TARGET CicmWrapper)
  add_subdirectory("${OSSIA_CICM_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/Cicm-build")
endif()
set(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS})

include_directories("${OSSIA_CICM_PATH}/Sources")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_LIST_DIR}/..")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_BINARY_DIR}/..") # needed for score_player_export.h

#set(OSSIA_3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty")
#include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot/include")

set(PROJECT_SRCS score-pd.cpp )

add_library(${PROJECT_NAME} SHARED ${PROJECT_SRCS})
use_gold(${PROJECT_NAME})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)

target_link_libraries(${PROJECT_NAME} PRIVATE CicmWrapper)
target_link_libraries(${PROJECT_NAME} PRIVATE score_player)
if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}\\pd.lib")
endif()

add_linker_warnings_external(${PROJECT_NAME})

if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -std=c++1z)
endif()

# Win32 uses DLL as a suffix for pd externals
set_target_properties(${PROJECT_NAME}
  PROPERTIES
    PREFIX ""
    OUTPUT_NAME "score")
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_darwin")
elseif(UNIX)
    if(${OSSIA_ARCHITECTURE} MATCHES "arm")
        set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".l_arm")
    elseif(${OSSIA_ARCHITECTURE} MATCHES "x86")
        set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".l_i386")
    elseif(${OSSIA_ARCHITECTURE} MATCHES "amd64")
        set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".l_ia64")
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pd_linux")
    endif()
endif()
