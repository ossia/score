add_library(directory_definitions STATIC "${_src}")
set_property(GLOBAL APPEND PROPERTY SCORE_AVND_PCH_CONSUMERS directory_definitions)
# After the target: the directory property still applies to it
add_compile_definitions(HALP_FROM_DIRECTORY=1)
