
if(SCORE_UNITY_BUILD)

        qt5_add_resources(QRCS1 "${CMAKE_SOURCE_DIR}/base/lib/resources/score.qrc"
 "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-deviceexplorer/Explorer/DeviceExplorer.qrc"
 "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-scenario/Scenario/resources/ScenarioResources.qrc")
        IScoreWriteStaticPluginsHeader()

        set(UNITY_SOURCES ${UNITY_SOURCES} ${SRCS} ${QRCS1})
        foreach(plugin ${SCORE_LIBRARIES_LIST})
                get_target_property(LOCAL_SRCS ${plugin} SOURCES)
                set(UNITY_SOURCES ${UNITY_SOURCES} ${LOCAL_SRCS})
        endforeach()
        foreach(plugin ${SCORE_PLUGINS_LIST})
                get_target_property(LOCAL_SRCS ${plugin} SOURCES)
                set(UNITY_SOURCES ${UNITY_SOURCES} ${LOCAL_SRCS})
        endforeach()

        list(REMOVE_ITEM UNITY_SOURCES
                ${CMAKE_BINARY_DIR}/base/plugins/score-plugin-scenario/qrc_ScenarioResources.cpp
                ${CMAKE_BINARY_DIR}/base/plugins/score-plugin-deviceexplorer/qrc_DeviceExplorer.cpp
)
        add_executable(IscoreCustomUnity ${UNITY_SOURCES})
        set_property(TARGET IscoreCustomUnity PROPERTY COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
        target_include_directories(IscoreCustomUnity
                PRIVATE "${CMAKE_SOURCE_DIR}/base/lib"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-lib-state"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-lib-device"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-lib-process"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-deviceexplorer"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-curve"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-automation"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-mapping"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-cohesion"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-inspector"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-js"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-scenario"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-pluginsettings"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-space"
                PRIVATE "${CMAKE_SOURCE_DIR}/base/plugins/score-plugin-ossia")
        target_link_libraries(IscoreCustomUnity PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::WebSockets Qt5::Xml Qt5::Qml  )
        cotire(IscoreCustomUnity)
endif()
