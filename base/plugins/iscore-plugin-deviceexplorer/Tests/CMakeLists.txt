include (CTest)
include(CppcheckTargets)
find_package(Qt5Core)
find_package(Qt5Test)
find_package(Qt5Widgets)

add_executable(NodeTest NodeTest.cpp)
target_link_libraries(NodeTest iscore_plugin_deviceexplorer Qt5::Core Qt5::Test)
add_test(NodeTest NodeTest)
