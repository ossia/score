cmake_minimum_required(VERSION 2.8)

project(interval_test_project)
addIScoreQtTest(interval_test "${CMAKE_CURRENT_SOURCE_DIR}/IntervalModelTests.cpp")
