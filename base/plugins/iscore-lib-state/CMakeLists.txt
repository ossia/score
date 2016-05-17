cmake_minimum_required(VERSION 3.1)
project(iscore_lib_state LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/State/AddressSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ValueSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/MessageSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Address.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Value.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ValueConversion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Relation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Expression.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ExpressionParser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ExpressionSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/ValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/CharValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/StringValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/BoolValueWidget.cpp"
)

file(GLOB_RECURSE HDRS
     "${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp")

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt5::Core Qt5::Widgets iscore_lib_base)

setup_iscore_library(${PROJECT_NAME})

setup_iscore_tests(Tests)

