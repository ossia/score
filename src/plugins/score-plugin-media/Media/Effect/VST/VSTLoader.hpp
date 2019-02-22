#pragma once
//  Taken and refactored from ofxVstHostPluginLoader.h
//  https://github.com/Meach/ofxVstHost

#include <Media/Effect/VST/vst-compat.hpp>

#include <string>

namespace Media::VST
{
using PluginEntryProc = AEffect* (*)(audioMasterCallback audioMaster);

struct VSTModule
{
  std::string path;
  void* module{};

  VSTModule(std::string fileName);
  ~VSTModule();
  PluginEntryProc getMain();
};

#if defined(__APPLE__)
static const constexpr auto default_path = "/Library/Audio/Plug-Ins/VST";
static const constexpr auto default_filter = "*.vst *.dylib *.component";
#elif defined(__linux__)
static const constexpr auto default_path{"/usr/lib/vst"};
static const constexpr auto default_filter = "*.so";
#elif defined(_WIN32)
static const constexpr auto default_path = "c:\\vst";
static const constexpr auto default_filter = "*.dll";
#else
static const constexpr auto default_path = "";
static const constexpr auto default_filter = "";
#endif
}
