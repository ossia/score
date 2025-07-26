if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(eigen 3.4 GLOBAL CONFIG)
endif()

if(NOT TARGET Eigen3::Eigen)
  add_subdirectory(3rdparty/eigen)
endif()
