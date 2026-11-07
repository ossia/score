add_compile_options(-fno-exceptions)
add_library(directory_options STATIC "${_src}")
set_property(GLOBAL APPEND PROPERTY SCORE_AVND_PCH_CONSUMERS directory_options)
