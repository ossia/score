cmake_minimum_required(VERSION 3.25)
project(llfio_debug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")

add_executable(llfio_test llfio_test.cpp)

# These defines must match the ones in llfio_test.cpp for consistency,
# though they are already there.
target_compile_definitions(llfio_test PRIVATE 
    LLFIO_HEADERS_ONLY=1
    LLFIO_EXPERIMENTAL_STATUS_CODE=1
    LLFIO_DISABLE_OPENSSL=1
    QUICKCPPLIB_USE_STD_SPAN=1
)

target_include_directories(llfio_test SYSTEM PRIVATE 
    "${3RDPARTY_FOLDER}/quickcpplib/include"
    "${3RDPARTY_FOLDER}/outcome/include"
    "${3RDPARTY_FOLDER}/llfio/include"
)

if(WIN32)
  # LLFIO on Windows often needs these
  target_link_libraries(llfio_test PRIVATE ntdll advapi32)
  if(MINGW)
    target_compile_options(llfio_test PRIVATE "-Wa,-mbig-obj")
  endif()
endif()

# Mock install target to avoid breakage in ci scripts that call 'cmake --build build --target install'
install(TARGETS llfio_test DESTINATION bin)
