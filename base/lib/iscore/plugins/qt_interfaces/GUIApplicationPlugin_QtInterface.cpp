#include "GUIApplicationPlugin_QtInterface.hpp"

namespace iscore
{
ApplicationPlugin_QtInterface::
    ~ApplicationPlugin_QtInterface()
    = default;

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
