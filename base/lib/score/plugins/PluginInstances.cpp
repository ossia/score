#include <score/plugins/PluginInstances.hpp>

namespace score
{
std::vector<Plugin_QtInterface*>& staticPlugins()
{
  static std::vector<Plugin_QtInterface*> objs;
  return objs;
}
}
