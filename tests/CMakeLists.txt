cmake_minimum_required(VERSION 2.8)

project(iscore_tests)
include (CTest)
include(CppcheckTargets)
find_package(Qt5Core)
find_package(Qt5Test)


add_executable(iscore_test main.cpp)

target_link_libraries(iscore_test Qt5::Core Qt5::Test)

add_test(iscore_test1 iscore_test)
add_cppcheck(iscore_test) 

