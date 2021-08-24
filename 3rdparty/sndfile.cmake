function(disable_var VAR)
  if(${${VAR}})
    set(${VAR}_prev "${${VAR}}" CACHE "" INTERNAL FORCE)
  endif()
  set(${VAR} 0 PARENT_SCOPE)
  set(${VAR} 0 CACHE "" INTERNAL FORCE)
endfunction()

function(restore_var VAR)
  if(${${VAR}_prev})
    set(${VAR} "${${VAR}_prev}" PARENT_SCOPE)
    set(${VAR} "${${VAR}_prev}" CACHE "" INTERNAL FORCE)
  endif()
endfunction()


disable_var(BUILD_PROGRAMS)
disable_var(BUILD_EXAMPLES)
disable_var(ENABLE_PACKAGE_CONFIG)
disable_var(INSTALL_PKGCONFIG_MODULE)
disable_var(ENABLE_EXPERIMENTAL)
disable_var(BUILD_REGTEST)
disable_var(BUILD_TESTING)
disable_var(BUILD_SHARED_LIBS)

add_subdirectory(3rdparty/libsndfile)

if(TARGET sndfile)
  set_target_properties(sndfile PROPERTIES UNITY_BUILD 0)
endif()

restore_var(BUILD_SHARED_LIBS)
restore_var(BUILD_TESTING)
