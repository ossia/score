cmake_minimum_required(VERSION 3.0)
project(SpaceTests)

include_directories("${ISCORE_ROOT_SOURCE_DIR}/base")

find_package(Qt5 5.5 REQUIRED COMPONENTS Core Test)

function(addIScoreQtTest TESTNAME TESTSRCS)
                add_executable(${TESTNAME} ${TESTSRCS})
                target_link_libraries(${TESTNAME}
                  Qt5::Core Qt5::Test
                  cln ginac)
                add_test(${TESTNAME}_target ${TESTNAME})
endFunction()


addIScoreQtTest(SpaceTest "${CMAKE_CURRENT_SOURCE_DIR}/space_test.cpp")

include(CTest)
