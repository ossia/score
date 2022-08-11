#pragma once
//  Taken and refactored from ofxVstHostPluginLoader.h
//  https://github.com/Meach/ofxVstHost

#include <Vst/vst-compat.hpp>

#include <QString>

#include <string>

namespace vst
{
using PluginEntryProc = AEffect* (*)(audioMasterCallback audioMaster);

struct Module
{
  std::string path;
  void* module{};

  Module(std::string fileName);
  ~Module();
  PluginEntryProc getMain();

  int use_count{};
};

#if defined(__APPLE__)
static const constexpr auto default_path = "/Library/Audio/Plug-Ins/VST";
static const constexpr auto default_filter = "*.vst *.dylib *.component";
#elif defined(__linux__)
static const auto default_path = QStringLiteral("/usr/lib/vst");
static const constexpr auto default_filter = "*.so";
#elif defined(_WIN32)
static const constexpr auto default_path = "c:\\vst";
static const constexpr auto default_filter = "*.dll";
#else
static const constexpr auto default_path = "";
static const constexpr auto default_filter = "";
#endif
}
