macro(_qmake_addInclude include)
  if("${include}" MATCHES "\\$<BUILD_INTERFACE:(.+)>")
    list(APPEND INCLUDES "${CMAKE_MATCH_1}")
  elseif(NOT ("${include}" MATCHES "\\$<[A-Z_]+:.+>"))
    list(APPEND INCLUDES "${include}")
  endif()
endmacro()

macro(_qmake_addDefine define)
  if("${define}" MATCHES "\\$<\\$<BOOL:True>:(.+)>")
     list(APPEND DEFINES "${CMAKE_MATCH_1}")
  elseif(NOT ("${define}" MATCHES "\\$<[A-Z_]+:.+>"))
    list(APPEND DEFINES "${define}")
  endif()
endmacro()

macro(_qmake_addLib lib)
  if("${lib}" MATCHES "^\\$<.*>")
    # ignore
  elseif("${lib}" MATCHES "Threads::Threads")
    # ignore
  elseif("${lib}" MATCHES "Boost::Boost")
    # ignore
  elseif("${lib}" MATCHES "-.*")
    set(QMAKE "${QMAKE}LIBS += ${lib}\n")
  elseif(IS_ABSOLUTE "${lib}")
    set(QMAKE "${QMAKE}LIBS += ${lib}\n")
  elseif("${lib}" MATCHES "portaudio_static")
    list(APPEND LIBS "/opt/score-sdk/portaudio/lib/libportaudio.a")
  else()
    set(QMAKE "${QMAKE}LIBS += -l${lib}\n")
  endif()
endmacro()

function(GenerateQMake targets)
  set(QMAKE "TARGET = ossia-score
TEMPLATE = app
CONFIG += c++1z object_parallel_to_source warn_off debug
QT+=core widgets gui network xml svg websockets opengl
")
  set(INCLUDES "")
  set(DEFINES "")
  set(LIBS "")
  foreach(target ${targets})

    get_target_property(_src ${target} SOURCES)
    get_target_property(_srcdir ${target} SOURCE_DIR)
    get_target_property(_bindir ${target} BINARY_DIR)
    get_target_property(_includes ${target} INCLUDE_DIRECTORIES)
    get_target_property(_defines ${target} COMPILE_DEFINITIONS)
    get_target_property(_libs ${target} LINK_LIBRARIES)

    foreach(source ${_src})
      if("${source}" MATCHES ".*/qrc_.*")
        continue()
      endif()

      set(VAR "SOURCES")
      if("${source}" MATCHES ".*\\.hpp")
        set(VAR "HEADERS")
      endif()
      if(IS_ABSOLUTE "${source}")
        set(QMAKE "${QMAKE}${VAR} += ${source}\n")
      else()
        set(QMAKE "${QMAKE}${VAR} += ${_srcdir}/${source}\n")
      endif()
    endforeach()

    set(QMAKE "${QMAKE}\n")

    foreach(include ${_includes})
      _qmake_addInclude("${include}")
    endforeach()
    list(APPEND INCLUDES "${_srcdir}")
    list(APPEND INCLUDES "${_bindir}")

    foreach(define ${_defines})
      _qmake_addDefine("${define}")
    endforeach()

    foreach(lib ${_libs})
      list(APPEND LIBS "${lib}")
    endforeach()

  endforeach()

  list(REMOVE_DUPLICATES LIBS)
  foreach(lib ${LIBS})
    if(TARGET ${lib})
      if("${lib}" MATCHES "^${QT_PREFIX}::")
        # ignore
      elseif("${lib}" MATCHES "^score_")
        # ignore
      elseif("${lib}" MATCHES "ossia")
        # ignore
      else()
        unset(_lib)

        get_target_property(_includes ${lib} INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(_defines ${lib} INTERFACE_COMPILE_DEFINITIONS)
        get_target_property(_libs ${lib} INTERFACE_LINK_LIBRARIES)
        get_target_property(_libtype ${lib} TYPE)

        if(NOT ${_libtype} MATCHES "INTERFACE_LIBRARY")
          get_target_property(_lib ${lib} IMPORTED_LOCATION)
          if(_lib)
            _qmake_addLib(${_lib})
          endif()
        endif()

        if(_includes)
          _qmake_addInclude(${_includes})
        endif()

        if(_defines)
          _qmake_addDefine(${_defines})
        endif()

        if(_libs)
          _qmake_addLib(${_libs})
        endif()

      endif()
    else()
      _qmake_addLib("${lib}")
    endif()
  endforeach()

  list(REMOVE_DUPLICATES INCLUDES)
  list(REMOVE_ITEM INCLUDES /usr/include)

  foreach(include ${INCLUDES})
    set(QMAKE "${QMAKE}INCLUDEPATH += ${include}\n")
  endforeach()

  list(REMOVE_DUPLICATES DEFINES)
  foreach(define ${DEFINES})
    set(QMAKE "${QMAKE}DEFINES += ${define}\n")
  endforeach()


  set(QMAKE "${QMAKE}" PARENT_SCOPE)
endfunction()

set(ALL_LIBS "${SCORE_LIBRARIES_LIST};${SCORE_PLUGINS_LIST};ossia")
list(REMOVE_DUPLICATES ALL_LIBS)

GenerateQMake("${ALL_LIBS}")
score_write_file("${CMAKE_BINARY_DIR}/srcs.pro" "${QMAKE}")
