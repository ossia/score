project(score_lib_state LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/State/AddressSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/MessageSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Address.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Domain.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Value.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ValueConversion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Relation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Expression.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ExpressionParser.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/ExpressionSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/OSSIASerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/ValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/TypeComboBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/CharValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/StringValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/BoolValueWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressLineEdit.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressValidator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/UnitWidget.cpp"
)
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_lib_state.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/ValueParser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/AddressParser.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Address.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/DomainSerializationImpl.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Domain.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Expression.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Message.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/MessageListSerialization.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/OSSIASerializationImpl.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Relation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Unit.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/UpdateAddress.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/ValueConversion.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Value.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/ValueSerialization.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/ValueSerializationImpl.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressFragmentLineEdit.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressLineEdit.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/AddressValidator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/UnitWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/BoolValueWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/CharValueWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/NumericValueWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/StringValueWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/TypeComboBox.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/ValueWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/State/Widgets/Values/VecWidgets.hpp"
)

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets score_lib_base)
setup_score_library(${PROJECT_NAME})

#setup_score_tests(Tests)

