if(NOT UNIX)
  return()
endif()

if(EMSCRIPTEN)
  return()
endif()

if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(shmdata GLOBAL)
else()
  add_library(shmdata STATIC
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/cfollower.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/clogger.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/cwriter.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/file-monitor.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/follower.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/reader.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/sysv-sem.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/sysv-shm.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/type.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket-client.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket-protocol.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/unix-socket-server.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/shmdata/shmdata/writer.cpp"
  )

  target_compile_options(shmdata PUBLIC -DSHMDATA_VERSION_STRING="1.3.60" )
  target_include_directories(shmdata PUBLIC "${CMAKE_CURRENT_LIST_DIR}/shmdata")
endif()
