macro(_unity_addInclude include)
  if("${include}" MATCHES "\\$<BUILD_INTERFACE:(.+)>")
    list(APPEND INCLUDES "${CMAKE_MATCH_1}")
  elseif(NOT ("${include}" MATCHES "\\$<[A-Z_]+:.+>"))
    list(APPEND INCLUDES "${include}")
  endif()
endmacro()

macro(_unity_addDefine define)
  if("${define}" MATCHES "\\$<\\$<BOOL:True>:(.+)>")
     list(APPEND DEFINES "${CMAKE_MATCH_1}")
  elseif(NOT ("${define}" MATCHES "\\$<[A-Z_]+:.+>"))
    list(APPEND DEFINES "${define}")
  endif()
endmacro()

macro(_unity_addLib lib)
  if(TARGET ${lib})
      # ignore
  elseif("${lib}" MATCHES "^${QT_PREFIX}::")
    # ignore
  elseif("${lib}" MATCHES "^[A-Za-z0-9]+::")
    # ignore
  elseif("${lib}" MATCHES "^score_")
    # ignore
  elseif("${lib}" MATCHES "ossia")
    # ignore)
  elseif("${lib}" MATCHES "^\\$<.*>")
    # ignore
  elseif("${lib}" MATCHES "Threads::Threads")
    list(APPEND ACTUAL_LIBS "-pthread")
  elseif("${lib}" MATCHES "-.*")
    list(APPEND ACTUAL_LIBS "${lib}")
  elseif(IS_ABSOLUTE "${lib}")
    list(APPEND ACTUAL_LIBS "${lib}")
  elseif("${lib}" MATCHES "portaudio_static")
    list(APPEND ACTUAL_LIBS "/opt/score-sdk/portaudio/lib/libportaudio.a")
  else()
    list(APPEND ACTUAL_LIBS "-l${lib}")
  endif()
endmacro()

function(GenerateUnity targets)
  set(BUILD "#!/bin/bash\n")
  set(UNITY "")
  set(INCLUDES "")
  set(SOURCES "")
  set(C_SOURCES "")
  set(RESSOURCES "")
  set(DEFINES "")
  set(LIBS "")
  set(ACTUAL_LIBS "")
  foreach(target ${targets})

    get_target_property(_src ${target} SOURCES)
    get_target_property(_srcdir ${target} SOURCE_DIR)
    get_target_property(_bindir ${target} BINARY_DIR)
    get_target_property(_includes ${target} INCLUDE_DIRECTORIES)
    get_target_property(_defines ${target} COMPILE_DEFINITIONS)
    get_target_property(_libs ${target} LINK_LIBRARIES)

    foreach(source ${_src})
      if("${source}" MATCHES ".*/qrc_.*")
        list(APPEND RESSOURCES "${source}")
        continue()
      endif()

      if("${source}" MATCHES ".*\\.hpp")
        continue()
      endif()

      if("${source}" MATCHES ".*\\.h")
        continue()
      endif()

      if("${source}" MATCHES ".*\\.c$")
        if(IS_ABSOLUTE "${source}")
          list(APPEND C_SOURCES "${source}")
        else()
          list(APPEND C_SOURCES "${_srcdir}/${source}")
        endif()
      else()
        if(IS_ABSOLUTE "${source}")
          list(APPEND SOURCES "${source}")
        else()
          list(APPEND SOURCES "${_srcdir}/${source}")
        endif()
      endif()
    endforeach()

    foreach(include ${_includes})
      _unity_addInclude("${include}")
    endforeach()
    list(APPEND INCLUDES "${_srcdir}")
    list(APPEND INCLUDES "${_bindir}")

    foreach(define ${_defines})
      _unity_addDefine("${define}")
    endforeach()

    if(_libs)
      foreach(lib ${_libs})
        list(APPEND LIBS "${lib}")
      endforeach()
    endif()
  endforeach()

  list(REMOVE_DUPLICATES LIBS)
  foreach(lib ${LIBS})
    if(TARGET ${lib})
      if("${lib}" MATCHES "^score_")
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
            _unity_addLib(${_lib})
          else()
            get_target_property(_lib ${lib} IMPORTED_LOCATION_RELEASE)
            if(_lib)
              _unity_addLib(${_lib})
            endif()
          endif()
        else()
          # interface
        endif()

        foreach(inc ${_includes})
          _unity_addInclude("${inc}")
        endforeach()

        if(_defines)
          _unity_addDefine(${_defines})
        endif()

        if(_libs)
          foreach(_lib ${_libs})
            _unity_addLib("${_lib}")
          endforeach()
        endif()

      endif()
    else()
      _unity_addLib("${lib}")
    endif()
  endforeach()

  list(REMOVE_DUPLICATES INCLUDES)
  list(REMOVE_ITEM INCLUDES /usr/include)

  ## Build script ##
  # C build line
  set(BUILD "${BUILD}$CC -c unity.c -o unity_c.o -fPIC \\\n")
  foreach(include ${INCLUDES})
    set(BUILD "${BUILD} -I${include} \\\n")
  endforeach()

  # C++ build line
  set(BUILD "${BUILD}\n")
  set(BUILD "${BUILD}$CXX -mrtm -Wfatal-errors -c unity.cpp -o unity.o -fPIC -std=c++1z \\\n")
  foreach(include ${INCLUDES})
    set(BUILD "${BUILD} -I${include} \\\n")
  endforeach()

  # Qt resources
  set(i 0)
  set(res_obj "")
  foreach(res ${RESSOURCES})
    set(BUILD "${BUILD}\n$CXX -fPIC -std=c++1z -c ${res} -o res_${i}.o")
    set(res_obj "${res_obj} res_${i}.o")
    math(EXPR i "${i}+1")
  endforeach()

  # Link line
  set(BUILD "${BUILD}\n$CXX -Wfatal-errors unity_c.o unity.o ${res_obj} -fPIC -std=c++1z")

  foreach(lib ${ACTUAL_LIBS})
    set(BUILD "${BUILD} ${lib} \\\n")
  endforeach()
  set(BUILD "${BUILD} -ltbb \n")

  ## Unity source ##
  list(REMOVE_DUPLICATES DEFINES)
  foreach(define ${DEFINES})
    if("${define}" MATCHES "^([A-Za-z0-9_]+)=(.*)")
      set(UNITY "${UNITY}#define ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
      set(UNITY_C "${UNITY}#define ${CMAKE_MATCH_1} ${CMAKE_MATCH_2}\n")
    else()
      set(UNITY "${UNITY}#define ${define} 1\n")
      set(UNITY_C "${UNITY}#define ${define} 1\n")
    endif()
  endforeach()

  set(UNITY "${UNITY}#define QT_NO_KEYWORDS 1\n")
  set(UNITY "${UNITY}#define SCORE_ALL_UNITY 1\n")

  set(UNITY "${UNITY}\n\n")

  foreach(source ${SOURCES})
    set(UNITY "${UNITY}#include \"${source}\"\n")
  endforeach()

  foreach(source ${C_SOURCES})
    set(UNITY_C "${UNITY_C}#include \"${source}\"\n")
  endforeach()


  set(UNITY "${UNITY}" PARENT_SCOPE)
  set(UNITY_C "${UNITY_C}" PARENT_SCOPE)
  set(BUILD "${BUILD}" PARENT_SCOPE)
endfunction()

set(ALL_LIBS "")
if(TARGET artnet)
  list(APPEND ALL_LIBS artnet)
endif()

list(APPEND ALL_LIBS "${SCORE_LIBRARIES_LIST};${SCORE_PLUGINS_LIST};score;ossia")
list(REMOVE_DUPLICATES ALL_LIBS)

GenerateUnity("${ALL_LIBS}")
score_write_file("${CMAKE_BINARY_DIR}/unity.c" "${UNITY_C}")
score_write_file("${CMAKE_BINARY_DIR}/unity.cpp" "${UNITY}")
score_write_file("${CMAKE_BINARY_DIR}/build.sh" "${BUILD}")
