cmake_minimum_required(VERSION 3.0)

project(iscore_base)
enable_testing()

include_directories(lib)

add_subdirectory(lib)
add_subdirectory(plugins)

if(ANDROID)
    add_subdirectory(app-android)
elseif(NACL)
    add_subdirectory(app-nacl)
else()
    add_subdirectory(app)
endif()

if(ISCORE_COVERAGE)
    add_subdirectory(app-test)
endif()
