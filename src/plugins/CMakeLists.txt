enable_testing()

# SCORE_PLUGINS_TO_BUILD is defined in the configuration.
foreach(plugin ${SCORE_PLUGINS_TO_BUILD})
  message(" -- score: configuring ${plugin}")
  add_subdirectory(${plugin})
endforeach()
