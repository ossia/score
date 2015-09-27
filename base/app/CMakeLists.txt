cmake_minimum_required(VERSION 3.0)
project(iscore_app)

enable_testing()
# TODO strip symbols after deployment build
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")

set(SRCS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
if(APPLE)
        set(ICON_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../lib/resources/i-score.icns")
        set_source_files_properties(${ICON_SRC} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif()

if(DEPLOYMENT_BUILD)
    add_executable(${APPNAME} WIN32 MACOSX_BUNDLE ${SRCS} ${ICON_SRC})
else()
    add_executable(${APPNAME} WIN32 ${SRCS})
endif()

target_link_libraries(${APPNAME} iscore_lib_base)

if(ISCORE_STATIC_PLUGINS)
    set(ISCORE_PLUGINS_FILE "${CMAKE_CURRENT_BINARY_DIR}/iscore_static_plugins.hpp")
    file(WRITE "${ISCORE_PLUGINS_FILE}" "#pragma once\n")
    foreach(plugin ${ISCORE_PLUGINS_LIST})
        message("${plugin}")
        file(APPEND "${ISCORE_PLUGINS_FILE}" "Q_IMPORT_PLUGIN(${plugin})\n")
    endforeach()

    target_compile_definitions(${APPNAME}
                               PUBLIC ISCORE_STATIC_PLUGINS)

    target_link_libraries(${APPNAME} ${ISCORE_PLUGINS_LIST})
endif()

setup_iscore_common_features(${APPNAME})

install(TARGETS ${APPNAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        COMPONENT Runtime)

include(IScoreDeployment)
