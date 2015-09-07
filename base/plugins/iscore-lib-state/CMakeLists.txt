cmake_minimum_required(VERSION 2.8)
project(iscore_lib_state)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/State/AddressSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ValueSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/RelationSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/MessageSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/DynamicStateSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/StateSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Address.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Value.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Relation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Expression.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ExpressionParser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ExpressionSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/StateItemModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressLineEdit.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/StringValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/BoolValueWidget.cpp"
)

file(GLOB_RECURSE HDRS
     "${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp")

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Widgets iscore_lib_base)

set_property(TARGET ${PROJECT_NAME} APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

iscore_cotire(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION static_lib/
        COMPONENT DynamicRuntime)

if(NOT DEPLOYMENT_BUILD)
#    add_subdirectory(Tests)
endif()
