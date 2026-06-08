# nv-dvp-bridge: NVIDIA "GPUDirect for Video" runtime-loaded shim +
# C API wrapper. Cross-platform: Windows uses dvp.dll; Linux uses
# libdvp.so.1. Consumers (AJA, planned DeckLink) link this target via
# its score-plugin-gfx parent.
#
# The bridge itself has no NVIDIA-SDK headers as a build dependency —
# all DVP entry points are dlsym'd at runtime from the NVIDIA library.

add_library(score_nv_dvp_bridge STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/nv_dvp_bridge.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/nv_dvp_bridge.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/dvpapi_shim.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/dvpapi_shim.h"
)

target_include_directories(score_nv_dvp_bridge
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(WIN32)
  # D3D11 path uses ID3D11Device / ID3D11Texture2D via d3d11.h.
  target_link_libraries(score_nv_dvp_bridge PRIVATE d3d11)
else()
  # Linux dlopen path needs libdl.
  target_link_libraries(score_nv_dvp_bridge PRIVATE ${CMAKE_DL_LIBS})
endif()

target_compile_definitions(score_nv_dvp_bridge PUBLIC
  SCORE_HAS_NV_DVP_BRIDGE=1
  # Bridge is built as a STATIC lib and linked into the consumer. The
  # INLINE define makes NV_DVP_API empty (no dllexport/dllimport
  # decoration) for both the bridge build and its consumers on Windows.
  NV_DVP_BRIDGE_INLINE=1
)

set_property(TARGET score_nv_dvp_bridge PROPERTY POSITION_INDEPENDENT_CODE ON)

# Optional dvp.dll fetch on Windows. Mirrors the previous AJA addon's
# SCORE_FETCH_DVP_DLL option so existing build invocations keep working.
if(WIN32 AND SCORE_FETCH_DVP_DLL)
  set(_dvp_dll "${CMAKE_BINARY_DIR}/3rdparty/dvp/dvp.dll")
  if(NOT EXISTS "${_dvp_dll}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/3rdparty/dvp")
    message(STATUS "nv-dvp-bridge: fetching dvp.dll v1.70 from PlusToolkit/PlusLib")
    file(DOWNLOAD
      "https://github.com/PlusToolkit/PlusLib/raw/master/Tools/NVidia/dvp170/bin/x64/dvp.dll"
      "${_dvp_dll}"
      EXPECTED_HASH SHA256=602f2cefecb9b7c67d4ffebb3b15e11291bacf5a7342fcec6d52dd0118cbaddd
      STATUS _dvp_dl_status
      SHOW_PROGRESS)
    list(GET _dvp_dl_status 0 _dvp_dl_rc)
    if(NOT _dvp_dl_rc EQUAL 0)
      message(WARNING "nv-dvp-bridge: dvp.dll download failed: ${_dvp_dl_status}")
    endif()
  endif()
  if(EXISTS "${_dvp_dll}")
    # Expose path so the consuming addon can copy_if_different next to its DLL.
    set(SCORE_NV_DVP_DLL "${_dvp_dll}" CACHE INTERNAL "Fetched dvp.dll path")
  endif()
elseif(WIN32 AND AJA_DVP_DLL AND EXISTS "${AJA_DVP_DLL}")
  set(SCORE_NV_DVP_DLL "${AJA_DVP_DLL}" CACHE INTERNAL "User-supplied dvp.dll path")
endif()

message(STATUS "score-plugin-gfx: nv-dvp-bridge enabled")
