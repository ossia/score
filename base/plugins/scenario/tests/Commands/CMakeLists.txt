cmake_minimum_required(VERSION 2.8)

project(commands_test_project)
include(CTest)
include(CppcheckTargets)

addIScoreQtTest(addprocessstointerval_command_test "${CMAKE_CURRENT_SOURCE_DIR}/AddProcessToIntervalCommandTest.cpp")