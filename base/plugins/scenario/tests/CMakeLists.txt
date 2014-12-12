cmake_minimum_required(VERSION 3.0)


include_directories("${CMAKE_SOURCE_DIR}/base")

find_package(Qt5Core)
find_package(Qt5Test)

function(addIScoreQtTest TESTNAME TESTSRCS)
	add_executable(${TESTNAME} ${TESTSRCS})
	target_link_libraries(${TESTNAME} Qt5::Core Qt5::Test scenario_process)
	add_test(${TESTNAME}_target ${TESTNAME})
	#add_cppcheck(${TESTNAME})
endFunction()


add_subdirectory(Interval)
add_subdirectory(Commands)

include(CTest)
