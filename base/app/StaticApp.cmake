if(SCORE_STATIC_QT)
  find_package(Qt5 5.3 REQUIRED COMPONENTS Core Network Svg Xml DBus Qml Quick QuickControls2 Gui Widgets WebSockets)

  get_filename_component(QT_ROOT_FOLDER "${_qt5_root_dir}/../.." ABSOLUTE)
  set(QT_LIB_FOLDER "${QT_ROOT_FOLDER}/lib")

  # Taken and modified from what's in the Qt CMake config files
  macro(process_prl_file prl_file_location Library Configuration lib_deps link_flags)
      set(_lib_deps)
      set(_link_flags)
      if(EXISTS "${prl_file_location}")
          file(STRINGS "${prl_file_location}" _prl_strings REGEX "QMAKE_PRL_LIBS[ \t]*=")
          string(REGEX REPLACE "QMAKE_PRL_LIBS[ \t]*=[ \t]*([^\n]*)" "\\1" _static_depends ${_prl_strings})
          string(REGEX REPLACE "[ \t]+" ";" _static_depends ${_static_depends})
          set(_search_paths ${QT_LIB_FOLDER})

          foreach(_flag ${_static_depends})
              if(_flag MATCHES "^-l(.*)$")
                  set(_lib "${CMAKE_MATCH_1}")
                  if(_lib MATCHES "^pthread$")
                    find_package(Threads REQUIRED)
                    list(APPEND _lib_deps Threads::Threads)
                  else()
                    if(_search_paths)
                        find_library(_${Library}_${Configuration}_${_lib}_PATH ${_lib} HINTS ${_search_paths} NO_DEFAULT_PATH)
                    endif()
                    find_library(_${Library}_${Configuration}_${_lib}_PATH ${_lib})
                    mark_as_advanced(_${Library}_${Configuration}_${_lib}_PATH)
                    if(_${Library}_${Configuration}_${_lib}_PATH)
                        list(APPEND _lib_deps
                            ${_${Library}_${Configuration}_${_lib}_PATH}
                        )
                    else()
                        message(FATAL_ERROR "Library not found: ${_lib}")
                    endif()
                  endif()
              elseif(_flag MATCHES "^-L(.*)$")
                  if("${CMAKE_MATCH_1}" STREQUAL "\$\$[QT_INSTALL_LIBS]")
                    list(APPEND _search_paths "${QT_LIB_FOLDER}")
                  else()
                    list(APPEND _search_paths "${CMAKE_MATCH_1}")
                  endif()
              else()
                  list(APPEND _link_flags ${_flag})
              endif()
          endforeach()
      endif()

      string(REPLACE ";" " " _link_flags "${_link_flags}")
      set(${lib_deps} ${_lib_deps} PARENT_SCOPE)
      set(${link_flags} "SHELL:${_link_flags}" PARENT_SCOPE)
  endmacro()

  # Covers most cases
  function(process_qt_plugin plugin_prl plugin_name)
      process_prl_file("${QT_ROOT_FOLDER}/${plugin_prl}"
          ${plugin_name}
          RELEASE
          _${plugin_name}_STATIC_RELEASE_LIB_DEPENDENCIES
          _${plugin_name}_STATIC_RELEASE_LINK_FLAGS
       )
  endfunction()

  # Needed because Qt still finds stuff in /usr/lib first
  function(cleanup_library_list the_list)
    set(ok_list)
    foreach(_lib ${${the_list}})
        if("${_lib}" MATCHES ".*Qt5([a-zA-Z0-9]*)${CMAKE_DYNAMIC_LIBRARY_SUFFIX}")
          list(APPEND ok_list "${QT_LIB_FOLDER}/${CMAKE_STATIC_LIBRARY_PREFIX}Qt5${CMAKE_MATCH_1}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        else()
          list(APPEND ok_list "${_lib}")
        endif()
    endforeach()
    set("${the_list}" "${ok_list}" PARENT_SCOPE)
  endfunction()

  # First we generate _FOO_STATIC_RELEASE_LIB_DEPENDENCIES variables by parsing the .prl files
  process_qt_plugin("plugins/imageformats/${CMAKE_STATIC_LIBRARY_PREFIX}qsvg.prl" qsvg_plugin)
  process_qt_plugin("plugins/imageformats/${CMAKE_STATIC_LIBRARY_PREFIX}qjpeg.prl" qjpeg_plugin)
  process_qt_plugin("plugins/iconengines/${CMAKE_STATIC_LIBRARY_PREFIX}qsvgicon.prl" qsvgicon_plugin)
  process_qt_plugin("qml/QtQuick.2/${CMAKE_STATIC_LIBRARY_PREFIX}qtquick2plugin.prl" qtquick2_plugin)

  if(APPLE)
  elseif(WIN32)
  elseif(UNIX)
    process_qt_plugin("plugins/platforms/${CMAKE_STATIC_LIBRARY_PREFIX}qxcb.prl" qxcb_plugin)
    process_qt_plugin("plugins/xcbglintegrations/${CMAKE_STATIC_LIBRARY_PREFIX}qxcb-glx-integration.prl" qxcbglx_plugin)
  endif()

  set(QT_LIBS_VARIABLES
      Qt5QuickControls2
      Qt5Quick
      Qt5Qml
      Qt5Widgets
      Qt5Gui
      Qt5WebSockets
      Qt5DBus
      Qt5Svg
      Qt5Xml
      Qt5Core
      qjpeg_plugin
      qsvgicon_plugin
      qtquick2_plugin
  )

  if(APPLE)
  elseif(WIN32)
  elseif(UNIX)
    list(APPEND QT_LIBS_VARIABLES
      qxcb_plugin
      qxcbglx_plugin
    )
  endif()

  # Then we generate the actual list of linked libraries
  set(QT_LIBS_DEPS)
  foreach(var ${QT_LIBS_VARIABLES})
      cleanup_library_list(_${var}_STATIC_RELEASE_LIB_DEPENDENCIES)
      set(QT_LIBS_DEPS "${QT_LIBS_DEPS}" "${_${var}_STATIC_RELEASE_LIB_DEPENDENCIES}")
  endforeach()
endif()

function(static_link_qt _target)
  if(SCORE_STATIC_QT)
    target_link_libraries(
        ${_target} PRIVATE
        Qt5::Core Qt5::Gui Qt5::Widgets Qt5::DBus Qt5::Network Qt5::Svg Qt5::Xml
        Qt5::Qml Qt5::Quick Qt5::QuickControls2
        Qt5::WebSockets
        Qt5::QXcbIntegrationPlugin
        Qt5::QXcbGlxIntegrationPlugin
        Qt5::QSvgPlugin
        Qt5::QJpegPlugin
        Qt5::QSvgIconPlugin
        ${QT_ROOT_FOLDER}/qml/QtQuick.2/${CMAKE_STATIC_LIBRARY_PREFIX}qtquick2plugin${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${QT_LIBS_DEPS}
    )
  endif()
endfunction()
