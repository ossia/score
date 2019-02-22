
if(SCORE_UNITY_BUILD)

        qt5_add_resources(QRCS1 "${SCORE_SRC}/lib/resources/score.qrc"
 "${SCORE_SRC}/plugins/score-plugin-deviceexplorer/Explorer/DeviceExplorer.qrc"
 "${SCORE_SRC}/plugins/score-plugin-scenario/Scenario/resources/ScenarioResources.qrc")
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
                ${CMAKE_BINARY_DIR}/src/plugins/score-plugin-scenario/qrc_ScenarioResources.cpp
                ${CMAKE_BINARY_DIR}/src/plugins/score-plugin-deviceexplorer/qrc_DeviceExplorer.cpp
)
        add_executable(IscoreCustomUnity ${UNITY_SOURCES})
        set_property(TARGET IscoreCustomUnity PROPERTY COTIRE_ENABLE_PRECOMPILED_HEADER FALSE)
        target_include_directories(IscoreCustomUnity
                PRIVATE "${SCORE_SRC}/lib"
                PRIVATE "${SCORE_SRC}/plugins/score-lib-state"
                PRIVATE "${SCORE_SRC}/plugins/score-lib-device"
                PRIVATE "${SCORE_SRC}/plugins/score-lib-process"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-deviceexplorer"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-curve"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-automation"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-mapping"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-cohesion"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-inspector"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-js"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-scenario"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-pluginsettings"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-space"
                PRIVATE "${SCORE_SRC}/plugins/score-plugin-ossia")
        target_link_libraries(IscoreCustomUnity PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Svg Qt5::WebSockets Qt5::Xml Qt5::Qml  )
        cotire(IscoreCustomUnity)
endif()
