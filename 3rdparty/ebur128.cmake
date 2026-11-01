if(TARGET ebur128)
  return()
endif()

add_library(ebur128 STATIC
  "${CMAKE_CURRENT_LIST_DIR}/libebur128/ebur128/ebur128.c"
)
set_target_properties(ebur128 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  UNITY_BUILD 0
  C_VISIBILITY_PRESET hidden
)
# Its own queue.h, so that MSVC and the BSDs build the same code
target_include_directories(ebur128 SYSTEM PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/libebur128/ebur128/queue"
)
target_include_directories(ebur128 SYSTEM PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/libebur128/ebur128"
)
if(MSVC)
  target_compile_definitions(ebur128 PRIVATE _USE_MATH_DEFINES)
else()
  target_compile_options(ebur128 PRIVATE -w)
  if(NOT CMAKE_CROSSCOMPILING AND CMAKE_BUILD_TYPE MATCHES ".*Debug.*")
    target_compile_options(ebur128 PRIVATE -O2)
  endif()
endif()
