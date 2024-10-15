project(score_plugin_js LANGUAGES CXX)

if(NOT TARGET score_plugin_engine)
    return()
endif()

if(SCORE_FAST_DEV_BUILD)
  return()
endif()

score_common_setup()

# Packages
find_package(${QT_VERSION} OPTIONAL_COMPONENTS Qml QmlIntegration Quick)
if(NOT TARGET ${QT_PREFIX}::Qml)
  return()
endif()

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_js.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Commands/EditScript.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Commands/ScriptMacro.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Commands/JSCommandFactory.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/Component.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/ExecutionHelpers.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/CPUNode.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/GPUNode.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/AddressItem.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/DeviceContext.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/QmlObjects.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/ValueTypes.${QT_PREFIX}.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/Metatypes.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/PortSource.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/Utils.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/JS/ApplicationPlugin.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/ConsolePanel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/DropHandler.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessMetadata.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessModel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/JS/LibraryHandler.hpp"
)

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/JSProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/Component.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/CPUNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Executor/GPUNode.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/JS/Commands/JSCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/AddressItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/DeviceContext.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.curve.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.device.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.metaobject.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.ossia.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.port.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.scenario.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/EditContext.ui.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/PortSource.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/QmlObjects.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/ValueTypes.${QT_PREFIX}.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/Qml/Utils.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/JS/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/JS/ConsolePanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_js.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
# TODO make this automatic in some way, and extend to other components
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

target_include_directories(${PROJECT_NAME} PUBLIC ${${QT_PREFIX}Qml_PRIVATE_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PUBLIC
                     ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets ${QT_PREFIX}::Qml ${QT_PREFIX}::QmlPrivate
                     score_lib_base score_plugin_engine score_lib_process score_lib_inspector score_lib_device
                     score_plugin_deviceexplorer score_plugin_protocols
                     )

if(TARGET score_plugin_media)
  target_link_libraries(${PROJECT_NAME} PUBLIC score_plugin_media)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC 1)
if(TARGET "${QT_PREFIX}::Quick")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${QT_PREFIX}::Quick)
    if(TARGET score_plugin_gfx AND ${Qt6_VERSION} VERSION_GREATER_EQUAL 6.5.0)
      target_link_libraries(${PROJECT_NAME} PUBLIC ${QT_PREFIX}::QuickPrivate score_plugin_gfx)
      target_compile_definitions(${PROJECT_NAME} PRIVATE SCORE_HAS_GPU_JS)
      list(APPEND SCORE_FEATURES_LIST gpu_js)
    endif()
endif()
if(TARGET ${QT_PREFIX}::QmlIntegration)
  target_link_libraries(${PROJECT_NAME} PUBLIC ${QT_PREFIX}::QmlIntegration)
endif()

setup_score_plugin(${PROJECT_NAME})
