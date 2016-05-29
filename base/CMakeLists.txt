cmake_minimum_required(VERSION 3.0)

project(iscore_base LANGUAGES CXX)
enable_testing()
add_subdirectory(lib)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)
add_subdirectory(plugins)
if(NOT ISCORE_DISABLE_ADDONS)
add_subdirectory(addons)
endif()

if(ISCORE_MERGE_COMPONENTS)
  add_subdirectory(component-wrapper)
endif()

if(ANDROID)
    add_subdirectory(app-android)
elseif(NACL)
    add_subdirectory(app-nacl)
else()
  add_subdirectory(app)
endif()

add_subdirectory(app-remote)

if(ISCORE_COVERAGE)
    add_subdirectory(app-test)
endif()
