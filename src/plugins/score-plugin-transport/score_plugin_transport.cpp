// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_transport.hpp"

#include <Transport/TransportInterface.hpp>
#include <Transport/ApplicationPlugin.hpp>

#include <score/plugins/FactorySetup.hpp>

score_plugin_transport::score_plugin_transport() { }

score_plugin_transport::~score_plugin_transport() { }

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_transport::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase,
      Transport::TransportInterfaceList>();
}
std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_transport::guiFactories(
    const score::GUIApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<score::GUIApplicationContext, FW<Transport::TransportInterface, Transport::DirectTransport>>(ctx, key);
}

score::GUIApplicationPlugin* score_plugin_transport::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Transport::ApplicationPlugin{app};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_transport)
