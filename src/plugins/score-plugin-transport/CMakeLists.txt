project(score_plugin_transport LANGUAGES CXX)

score_common_setup()

set(HDRS
    score_plugin_transport.hpp
    Transport/TransportInterface.hpp
    Transport/DocumentPlugin.hpp
    Transport/ApplicationPlugin.hpp
)
set(SRCS
    score_plugin_transport.cpp
    Transport/TransportInterface.cpp
    Transport/DocumentPlugin.cpp
)

add_library(score_plugin_transport ${SRCS} ${HDRS})

target_link_libraries(score_plugin_transport
  PUBLIC
      ${QT_PREFIX}::Core
      score_lib_base ossia)

setup_score_plugin(score_plugin_transport)
