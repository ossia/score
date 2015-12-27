
if(ISCORE_COVERAGE)
  setup_target_for_coverage(
      iscore_coverage
      ${APPNAME}
      coverage_out
       )
endif()

if(INTEGRATION_TESTING)
    add_library(iscore_integration_lib INTERFACE)

    target_link_libraries(iscore_integration_lib INTERFACE
      ${ISCORE_PLUGINS_LIST}
      Qt5::Core Qt5::Widgets Qt5::Gui Qt5::Network Qt5::Xml Qt5::Svg Qt5::Test)
endif()

