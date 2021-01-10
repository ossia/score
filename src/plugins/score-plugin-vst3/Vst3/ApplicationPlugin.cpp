#include <Vst3/ApplicationPlugin.hpp>


namespace vst3
{

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& ctx):
  score::ApplicationPlugin{ctx}
{

}

void ApplicationPlugin::rescan()
{

}

}
