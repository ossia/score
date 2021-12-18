if(NOT UNIX)
  return()
endif()

add_library(shmdata STATIC
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/cfollower.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/clogger.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/cwriter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/file-monitor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/follower.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/reader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/sysv-sem.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/sysv-shm.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/type.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket-client.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket-protocol.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket-server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/writer.cpp
)

target_compile_options(shmdata PUBLIC -DSHMDATA_VERSION_STRING="1.3.60" )
target_include_directories(shmdata PUBLIC "${CMAKE_CURRENT_LIST_DIR}/shmdata")

set(HEADER_INCLUDES
    abstract-logger.hpp
    cfollower.h
    clogger.h
    cwriter.h
    console-logger.hpp
    file-monitor.hpp
    follower.hpp
    reader.hpp
    safe-bool-idiom.hpp
    sysv-sem.hpp
    sysv-shm.hpp
    type.hpp
    unix-socket.hpp
    unix-socket-client.hpp
    unix-socket-protocol.hpp
    unix-socket-server.hpp
    writer.hpp
    )
