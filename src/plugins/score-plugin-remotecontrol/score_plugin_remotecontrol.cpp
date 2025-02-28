#include "score_plugin_remotecontrol.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <RemoteControl/ApplicationPlugin.hpp>
#include <RemoteControl/Controller/RemoteControlProvider.hpp>
#include <RemoteControl/Settings/Factory.hpp>
#include <RemoteControl/Websockets/Scenario/Scenario.hpp>

#include <score_plugin_deviceexplorer.hpp>
#include <score_plugin_engine.hpp>
#include <score_plugin_scenario.hpp>

score_plugin_remotecontrol::score_plugin_remotecontrol() { }

score_plugin_remotecontrol::~score_plugin_remotecontrol() { }

score::GUIApplicationPlugin* score_plugin_remotecontrol::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new RemoteControl::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_remotecontrol::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase, RemoteControl::WS::ProcessComponentFactoryList>();
}

std::vector<score::InterfaceBase*> score_plugin_remotecontrol::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<score::SettingsDelegateFactory, RemoteControl::Settings::Factory>,
      FW<RemoteControl::WS::ProcessComponentFactory,
         RemoteControl::WS::ScenarioComponentFactory>,
      FW<Process::RemoteControlProvider,
         RemoteControl::Controller::RemoteControlProvider>>(ctx, key);
}

auto score_plugin_remotecontrol::required() const -> std::vector<score::PluginKey>
{
  return {
      score_plugin_scenario::static_key(), score_plugin_deviceexplorer::static_key(),
      score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_remotecontrol)
