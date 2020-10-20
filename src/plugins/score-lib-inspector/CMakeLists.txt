project(score_lib_inspector LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

# Inspector interface
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorSectionWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetBase.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetFactoryInterface.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetList.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorLayout.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/score_lib_inspector.hpp"
)

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorSectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/InspectorWidgetBase.cpp"
)

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets score_lib_base)
setup_score_library(score_lib_inspector)
