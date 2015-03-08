cmake_minimum_required(VERSION 2.8)

project(iscore_tests)
#include (CTest)
#include(CppcheckTargets)
find_package(Qt5Core)
find_package(Qt5Test)
find_package(Qt5Widgets)

#add_executable(SerializationTest SerializationTest.cpp)
#target_link_libraries(SerializationTest iscore_lib Qt5::Core Qt5::Test)
#add_test(SerializationTest SerializationTest)
#set (TEST2_HDRS  "${CMAKE_SOURCE_DIR}/base/lib/presenter/command/Command.hpp"
#				 "${CMAKE_SOURCE_DIR}/base/lib/presenter/command/CommandQueue.hpp")
#set (TEST2_SRCS "test_command.cpp"
#				"${CMAKE_SOURCE_DIR}/base/lib/presenter/command/Command.cpp"
#				"${CMAKE_SOURCE_DIR}/base/lib/presenter/command/CommandQueue.cpp")

#add_executable(iscore_test2 ${TEST2_SRCS} ${TEST2_HDRS})

#target_link_libraries(iscore_test1 Qt5::Core Qt5::Test)
#target_link_libraries(iscore_test2 Qt5::Core Qt5::Widgets Qt5::Test)

#add_test(iscore_test1 iscore_test1)
#add_cppcheck(iscore_test1)
#add_test(iscore_test2 iscore_test2)
#add_cppcheck(iscore_test2)

