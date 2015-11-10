cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_automation)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets PrintSupport Xml Test)

# Files & main target
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Automation/*.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationLayerSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationLayerModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/InitAutomation.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Commands/ChangeAddress.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/State/AutomationState.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Automation/AutomationColors.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_automation.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationStateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/AutomationInspectorWidget.cpp"
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/Automation/Resources/AutomationResources.qrc")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::PrintSupport
                     iscore_lib_base iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state iscore_plugin_curve)

if(TARGET iscore_lib_inspector)
    target_link_libraries(${PROJECT_NAME} iscore_lib_inspector)
endif()

setup_iscore_plugin(${PROJECT_NAME})
