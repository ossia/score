### Add-on architecture keys ###
#
# Standalone because ScoreExternalAddon.sdk.cmake cannot include
# ScoreFunctions.cmake, and both the in-tree and the SDK add-on paths need this.
#
# localaddon.json indexes the add-on library by an architecture key that
# score::makeAddon() looks up with score::addonArchitecture()
# (src/lib/score/plugins/Addon.cpp), so this must stay in sync with that function:
# a key that does not match makes makeAddon() discard the add-on silently.
#
# Sets ${out_var} to the keys the built binary is valid for: one normally, several
# for an Apple multi-architecture build.
function(score_addon_architectures out_var)
  if(WIN32)
    set(_os "windows")
  elseif(APPLE)
    set(_os "darwin")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(_os "freebsd")
  elseif(UNIX)
    set(_os "linux")
  else()
    set(_os "unknown")
  endif()

  if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    set(_processors "${CMAKE_OSX_ARCHITECTURES}")
  else()
    set(_processors "${CMAKE_SYSTEM_PROCESSOR}")
  endif()

  set(_keys "")
  foreach(_proc IN LISTS _processors)
    string(TOLOWER "${_proc}" _proc)
    if(_proc MATCHES "^(x86_64|amd64)$")
      set(_arch "x86_64")
    elseif(_proc MATCHES "^(aarch64|arm64)$")
      set(_arch "aarch64")
    elseif(_proc MATCHES "^(i[3-6]86|x86)$")
      set(_arch "x86")
    elseif(_proc MATCHES "^arm")
      set(_arch "arm")
    elseif(_proc MATCHES "^(ppc64|powerpc64)")
      set(_arch "ppc64")
    elseif(_proc MATCHES "^(ppc|powerpc)")
      set(_arch "ppc")
    elseif(_proc MATCHES "^riscv")
      set(_arch "riscv")
    else()
      # addonArchitecture() appends nothing for a processor boost/predef does not
      # name, so its key is the OS part and a trailing dash. Matching that keeps
      # s390x and mips64el loadable, at the cost of one key for both.
      set(_arch "")
    endif()
    list(APPEND _keys "${_os}-${_arch}")
  endforeach()

  list(REMOVE_DUPLICATES _keys)
  set(${out_var} "${_keys}" PARENT_SCOPE)
endfunction()
