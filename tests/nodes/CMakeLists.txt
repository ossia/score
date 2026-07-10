# L1 node/process sweep tests: instantiate + serialize-roundtrip every
# registered factory. Run from the build root (APP mode) so the dynamic plugins
# in <build>/plugins are discovered at runtime and ALL factories are registered.

score_add_test(test_nodes_processes
  SOURCES Processes.cpp
  APP
  PLUGINS score_plugin_scenario score_lib_process)

score_add_test(test_nodes_curves
  SOURCES Curves.cpp
  APP
  PLUGINS score_plugin_curve score_lib_process)

score_add_test(test_nodes_devices
  SOURCES Devices.cpp
  APP
  PLUGINS score_lib_device)
