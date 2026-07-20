# Device node serialization test. Boots a MinimalApplication (via Utils.hpp),
# so it needs the APP mode (offscreen + runtime plugin discovery).
#
# score_plugin_deviceexplorer is linked explicitly: JSONReader::read(DeviceSettings)
# aborts without the Device::ProtocolFactoryList interface list it registers.
# With dynamic plugins (Linux) the plugin is discovered at runtime from
# <build>/plugins, but in static-plugin builds (macOS SDK)
# score_init_static_plugins() only registers plugins whose headers are visible
# to the test target, i.e. linked ones.
score_add_test(test_device_serialization
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp"
  APP
  PLUGINS score_lib_device score_lib_state score_plugin_deviceexplorer)
