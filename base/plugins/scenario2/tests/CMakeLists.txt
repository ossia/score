cmake_minimum_required(VERSION 2.8)

project(iscore_tests)
include (CTest)
include(CppcheckTargets)
find_package(Qt5Core)
find_package(Qt5Test)
find_package(Qt5Widgets)

include_directories("${CMAKE_SOURCE_DIR}/base")

set(TESTNAME interval_test)

set (TESTHDRS  ""
#	"${CMAKE_SOURCE_DIR}/base/core/presenter/command/Command.hpp"
)
set (TESTSRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/Interval/IntervalModelTests.cpp"
#	"${CMAKE_SOURCE_DIR}/base/core/presenter/command/Command.cpp"
#	"${CMAKE_SOURCE_DIR}/base/core/presenter/command/CommandQueue.cpp"
)

add_executable("${TESTNAME}" ${TESTSRCS} ${TESTHDRS})

target_link_libraries(${TESTNAME} Qt5::Core Qt5::Widgets Qt5::Test scenario_process)

add_test(${TESTNAME}_target ${TESTNAME})
add_cppcheck(${TESTNAME})

