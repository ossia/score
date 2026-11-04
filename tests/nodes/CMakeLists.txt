# L1 node/process sweep tests: instantiate + serialize-roundtrip every
# registered factory. Run from the build root (APP mode) so the dynamic plugins
# in <build>/plugins are discovered at runtime and ALL factories are registered.

score_add_test(test_nodes_processes
  SOURCES Processes.cpp
  APP
  PLUGINS score_plugin_scenario score_lib_process)

# The views of every process: layers in every mode and size, nodes,
# inspectors; then the monitoring views while the document plays.
score_add_test(test_nodes_process_views
  SOURCES ProcessViews.cpp ValidationPlugins.cpp
  GUI
  APP
  PLUGINS score_plugin_scenario score_plugin_engine score_plugin_audio score_plugin_gfx score_plugin_media score_lib_process score_lib_inspector)
target_compile_definitions(test_nodes_process_views PRIVATE
  GFX_TEST_CORPUS_DIR="${SCORE_ROOT_SOURCE_DIR}/tests/gfx/corpus"
  THREEDIM_TEST_DIR="${SCORE_ROOT_SOURCE_DIR}/tests/threedim")
# The monitors of the Geometry viewer and of the texture views, header only
target_include_directories(test_nodes_process_views PRIVATE
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
  "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-ui")

# The CLAP, VST 2 and VST 3 hosts scan their plug-ins in a puppet they look
# for next to the executable: the exec sweep runs validation plug-ins
# (ValidationPlugins.hpp) through them.
if(TARGET test_nodes_process_views AND UNIX AND NOT APPLE)
  foreach(_puppet ossia-score-clappuppet ossia-score-vstpuppet ossia-score-vst3puppet)
    if(TARGET ${_puppet})
      add_dependencies(test_nodes_process_views ${_puppet})
      add_custom_command(TARGET test_nodes_process_views POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                $<TARGET_FILE:${_puppet}>
                $<TARGET_FILE_DIR:test_nodes_process_views>/$<TARGET_FILE_NAME:${_puppet}>
        VERBATIM)
    endif()
  endforeach()
endif()

# Every process run, its document ticked by the test: a test case of the same
# executable, hidden from the entry above, in shards that each fit the time a
# CI test gets. Each shard takes the factories whose uuid hashes to it (see
# sweep::in_shard). They run on the private X server rather than offscreen, so
# that the GPU processes get a device and render: hence the "gpu" label.
if(TEST test_nodes_process_views)
  set(_exec_shards 4)
  score_test_display_wrapper(_exec_wrapper)
  foreach(_p ENVIRONMENT_MODIFICATION WORKING_DIRECTORY SKIP_RETURN_CODE)
    get_test_property(test_nodes_process_views ${_p} _exec_${_p})
  endforeach()
  foreach(_shard RANGE 1 ${_exec_shards})
    set(_name test_nodes_process_views_exec_${_shard}of${_exec_shards})
    add_test(NAME ${_name}
      COMMAND ${_exec_wrapper} $<TARGET_FILE:test_nodes_process_views>
              "Every process runs with each inlet fed")
    set_tests_properties(${_name} PROPERTIES
      ENVIRONMENT "SCORE_AUDIO_BACKEND=dummy;SCORE_DISABLE_AUDIOPLUGINS=1;SWEEP_SHARD=${_shard}/${_exec_shards}"
      WORKING_DIRECTORY "${_exec_WORKING_DIRECTORY}"
      SKIP_RETURN_CODE "${_exec_SKIP_RETURN_CODE}"
      LABELS "gui;gpu")
    if(_exec_ENVIRONMENT_MODIFICATION)
      set_property(TEST ${_name} PROPERTY
        ENVIRONMENT_MODIFICATION ${_exec_ENVIRONMENT_MODIFICATION})
    endif()
  endforeach()
endif()

score_add_test(test_nodes_curves
  SOURCES Curves.cpp
  APP
  PLUGINS score_plugin_curve score_lib_process)

score_add_test(test_nodes_devices
  SOURCES Devices.cpp
  APP
  PLUGINS score_lib_device)

score_add_test(test_nodes_veltonote
  SOURCES VelToNoteTest.cpp VelToNoteBindingTest.cpp
  PLUGINS score_plugin_fx)

score_add_test(test_nodes_ossia_midi
  SOURCES "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/tests/test_ossia_midi.cpp"
  PLUGINS score_plugin_fx)

# The Point2D View's auto range, fit again to the points shown while playing.
if(EXISTS "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples/Advanced/UI/2DView.hpp")
  score_add_test(test_nodes_point2d_view_auto_range
    SOURCES Point2DViewAutoRange.cpp
    GUI
    APP
    PLUGINS score_plugin_scenario score_lib_process)
  target_include_directories(test_nodes_point2d_view_auto_range
    PRIVATE "${SCORE_ROOT_SOURCE_DIR}/3rdparty/avendish/examples/Advanced")
endif()
