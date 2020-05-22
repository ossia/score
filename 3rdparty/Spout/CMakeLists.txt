cmake_minimum_required(VERSION 3.17)
project(Spout 
	LANGUAGES CXX 
	VERSION 2.0
	HOMEPAGE_URL https://spout.zeal.co
)

if(NOT WIN32)
  message(STATUS "Spout is not supported outside of MS Windows")
  return()
endif()

set(CMAKE_INCLUDE_CURRENT_DIRS ON)

add_library(Spout
  Spout.h
  SpoutCommon.h
  SpoutCopy.h
  SpoutDirectX.h
  SpoutGLDXinterop.h
  SpoutGLextensions.h
  SpoutMemoryShare.h
  SpoutReceiver.h
  SpoutSDK.h
  SpoutSender.h
  SpoutSenderMemory.h
  SpoutSenderNames.h
  SpoutSharedMemory.h
  SpoutCopy.cpp
  SpoutDirectX.cpp
  SpoutGLDXinterop.cpp
  SpoutGLextensions.cpp
  SpoutMemoryShare.cpp
  SpoutReceiver.cpp
  SpoutSDK.cpp
  SpoutSender.cpp
  SpoutSenderMemory.cpp
  SpoutSenderNames.cpp
  SpoutSharedMemory.cpp
)

if(NOT MSVC)
  target_compile_options(Spout 
    PRIVATE
      -msse4
  )
endif()

target_compile_definitions(Spout
  PRIVATE
    SPOUT_BUILD_DLL
    SPOUTSDK_EXPORTS
)

target_link_libraries(Spout
  PRIVATE
    opengl32
    kernel32
    user32
    gdi32
    winspool
    comdlg32
    advapi32
    shell32
    ole32
    oleaut32
    uuid
    odbc32
    odbccp32
    d3d9
    d3d11
    DXGI
    shlwapi
    Version
)