#cmake_minimum_required(VERSION 3.1)

#include(CppcheckTargets)

#set (TEST2_HDRS  "${CMAKE_SOURCE_DIR}/base/lib/presenter/command/Command.hpp"
#				 "${CMAKE_SOURCE_DIR}/base/lib/presenter/command/CommandQueue.hpp")
#set (TEST2_SRCS "test_command.cpp"
#				"${CMAKE_SOURCE_DIR}/base/lib/presenter/command/Command.cpp"
#				"${CMAKE_SOURCE_DIR}/base/lib/presenter/command/CommandQueue.cpp")

#add_executable(score_test2 ${TEST2_SRCS} ${TEST2_HDRS})

#target_link_libraries(score_test1 Qt5::Core Qt5::Test)
#target_link_libraries(score_test2 Qt5::Core Qt5::Widgets Qt5::Test)

#add_test(score_test1 score_test1)
#add_cppcheck(score_test1)
#add_test(score_test2 score_test2)
#add_cppcheck(score_test2)

