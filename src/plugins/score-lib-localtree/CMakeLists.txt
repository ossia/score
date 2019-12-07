project(score_lib_localtree LANGUAGES CXX)

score_common_setup()

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/LocalTreeComponent.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/BaseProperty.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/Property.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/SetProperty.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/GetProperty.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/NameProperty.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/BaseCallbackWrapper.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/BaseProperty.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${MAPPER_SRCS})

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          score_lib_base score_lib_device
)

setup_score_plugin(${PROJECT_NAME})
