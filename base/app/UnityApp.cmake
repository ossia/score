
if(ISCORE_UNITY_BUILD)

        qt5_add_resources(QRCS1 "${CMAKE_SOURCE_DIR}/base/lib/resources/iscore.qrc"
 "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-deviceexplorer/Explorer/DeviceExplorer.qrc"
"${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-automation/Automation/Resources/AutomationResources.qrc"
"${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-mapping/Mapping/Resources/MappingResources.qrc"
 "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-scenario/Scenario/resources/ScenarioResources.qrc")
        IScoreWriteStaticPluginsHeader()

        set(UNITY_SOURCES ${UNITY_SOURCES} ${SRCS} ${QRCS1})
        foreach(plugin ${ISCORE_LIBRARIES_LIST})
                get_target_property(LOCAL_SRCS ${plugin} SOURCES)
                set(UNITY_SOURCES ${UNITY_SOURCES} ${LOCAL_SRCS})
        endforeach()
        foreach(plugin ${ISCORE_PLUGINS_LIST})
                get_target_property(LOCAL_SRCS ${plugin} SOURCES)
                set(UNITY_SOURCES ${UNITY_SOURCES} ${LOCAL_SRCS})
        endforeach()

        list(REMOVE_ITEM UNITY_SOURCES
                ${CMAKE_BINARY_DIR}/base/lib/iscore-plugin-mapping/qrc_MappingResources.cpp
                ${CMAKE_BINARY_DIR}/base/plugins/iscore-plugin-mapping/qrc_MappingResources.cpp
                ${CMAKE_BINARY_DIR}/base/plugins/iscore-plugin-scenario/qrc_ScenarioResources.cpp
                ${CMAKE_BINARY_DIR}/base/plugins/iscore-plugin-automation/qrc_AutomationResources.cpp
                ${CMAKE_BINARY_DIR}/base/plugins/iscore-plugin-deviceexplorer/qrc_DeviceExplorer.cpp
)
        add_executable(IscoreCustomUnity ${UNITY_SOURCES})
        set_property(TARGET IscoreCustomUnity PROPERTY COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
        target_include_directories(IscoreCustomUnity
                PRIVATE "${CMAKE_SOURCE_DIR}/base/lib"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-lib-state"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-lib-device"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-lib-process"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-deviceexplorer"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-curve"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-automation"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-mapping"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-cohesion"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-inspector"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-js"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-scenario"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-pluginsettings"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-space"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/iscore-plugin-ossia")
        target_link_libraries(IscoreCustomUnity PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::WebSockets Qt5::Xml Qt5::Qml QRecentFilesMenu )
        cotire(IscoreCustomUnity)
endif()
