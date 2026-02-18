add_executable(mcp-server main.cpp)
target_compile_features(mcp-server PRIVATE cxx_std_23)
target_link_libraries(mcp-server PRIVATE Boost::headers ossia)
