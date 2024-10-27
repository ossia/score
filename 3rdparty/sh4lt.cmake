if(NOT UNIX)
  return()
endif()

if(EMSCRIPTEN OR BSD)
  return()
endif()

if(SCORE_USE_SYSTEM_LIBRARIES)
  find_package(sh4lt GLOBAL)
else()
  add_library(sh4lt STATIC
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/c/cfollower.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/c/clogger.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/c/cshtype.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/c/cwriter.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/follower.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/infotree/information-tree.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/infotree/json-serializer.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/infotree/key-val-serializer.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/file-monitor.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/reader.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/sysv-sem.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/sysv-shm.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/unix-socket-client.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/unix-socket-protocol.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/unix-socket-server.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/ipcs/unix-socket.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/jsoncpp/jsoncpp.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/shtype/shtype-from-gst-caps.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/shtype/shtype.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/time.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/utils/any.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/utils/bool-log.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/utils/safe-bool-log.cpp"
      "${CMAKE_CURRENT_LIST_DIR}/sh4lt/sh4lt/writer.cpp"
  )

  # target_compile_options(sh4lt PUBLIC -DSH4LT_VERSION_STRING="1.3.60" )
  target_include_directories(sh4lt SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/sh4lt")

  if(has_w_unneeded_internal_declaration_flag)
    target_compile_options(sh4lt PRIVATE -Wno-unneeded-internal-declaration)
  endif()
endif()
