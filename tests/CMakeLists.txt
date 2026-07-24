# Root of the score test tree. Included from the top-level CMakeLists.txt
# only when SCORE_TESTING is ON.
score_setup_catch2()

add_subdirectory(fixtures)
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(nodes)
add_subdirectory(hardware)

# L3 GPU render+readback suites (ISF/CSF/raw-raster/VSA/incremental).
# They compile the few non-exported engine TUs they need directly, so they
# work with both dynamic and static plug-in builds.
if(TARGET score_plugin_gfx)
  add_subdirectory(gfx)
endif()

# L3 regression guards for the 3D object subsystem.
if(TARGET score_plugin_threedim)
  add_subdirectory(threedim)
endif()
