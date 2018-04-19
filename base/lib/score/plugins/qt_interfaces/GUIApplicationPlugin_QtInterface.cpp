// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GUIApplicationPlugin_QtInterface.hpp"

namespace score
{
ApplicationPlugin_QtInterface::~ApplicationPlugin_QtInterface() = default;

ApplicationPlugin* ApplicationPlugin_QtInterface::make_applicationPlugin(
    const ApplicationContext& app)
{
  return nullptr;
}

GUIApplicationPlugin* ApplicationPlugin_QtInterface::make_guiApplicationPlugin(
    const GUIApplicationContext& app)
{
  return nullptr;
}
}
