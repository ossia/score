cmake_minimum_required(VERSION 2.8)
project(iscore_lib_state)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/State/AddressSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/MessageSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/StateSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Address.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/MessageWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressLineEdit.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/StateWidget.cpp"
)

file(GLOB_RECURSE HDRS
     "${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp")

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Widgets)

set_property(TARGET ${PROJECT_NAME} APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")


if(ISCORE_COTIRE)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(${PROJECT_NAME} PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
    cotire(${PROJECT_NAME})
endif()

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION static_lib/
        COMPONENT DynamicRuntime)

if(NOT DEPLOYMENT_BUILD)
    add_subdirectory(Tests)
endif()
