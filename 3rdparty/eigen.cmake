score_use_system(use_sys eigen)
if(use_sys)
  find_package(eigen 3.4 GLOBAL CONFIG)
endif()

if(NOT TARGET Eigen3::Eigen)
  add_subdirectory(3rdparty/eigen)
endif()
