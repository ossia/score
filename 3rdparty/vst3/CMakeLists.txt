cmake_minimum_required (VERSION 3.16)

# Minimal VST3 SDK build for ossia score: we only need the hosting bits.
#
# Since VST3 SDK 3.8.0 the split repositories (vst3_base, vst3_pluginterfaces,
# vst3_public_sdk) no longer ship their own CMakeLists.txt: the targets are
# created by functions defined in vst3_cmake's SMTG_VST3_SDK.cmake instead.

set(SMTG_USE_STDATOMIC_H 0)
set(SMTG_USE_STDATOMIC_H 0 CACHE INTERNAL "")

# Nothing here builds an actual plug-in bundle, so make sure the SDK never
# tries to install one in the system-wide VST3 folder.
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_TARGET_VARS_LOG OFF CACHE BOOL "" FORCE)

#-------------------------------------------------------------------------------
# Includes
#-------------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(SMTG_VST3_SDK)

#-------------------------------------------------------------------------------
# SDK Project
#-------------------------------------------------------------------------------
project(vstsdk)

smtg_setup_platform_toolset()

set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Here you can define where the VST 3 SDK is located
set(SDK_ROOT "${ROOT}")
set(public_sdk_SOURCE_DIR ${SDK_ROOT}/public.sdk)
set(pluginterfaces_SOURCE_DIR ${SDK_ROOT}/pluginterfaces)

include_directories(SYSTEM ${ROOT} ${SDK_ROOT})

#-------------------------------------------------------------------------------
# Projects
#-------------------------------------------------------------------------------
set(SDK_IDE_LIBS_FOLDER FOLDER "Libraries")

# used for pluginterfaces and public.sdk modules which provide only a subset of
# them for the VST-SDK
set(VST_SDK TRUE)

smtg_create_lib_base_target()
smtg_create_pluginterfaces_target()
smtg_create_public_sdk_common_target()
smtg_create_public_sdk_hosting_target()

#-------------------------------------------------------------------------------
# IDE sorting
#-------------------------------------------------------------------------------
foreach(_smtg_target base pluginterfaces sdk_common sdk_hosting)
  set_target_properties(${_smtg_target} PROPERTIES ${SDK_IDE_LIBS_FOLDER})
endforeach()
