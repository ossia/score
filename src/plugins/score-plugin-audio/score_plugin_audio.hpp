#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <vector>
class score_plugin_audio final : public score::ApplicationPlugin_QtInterface,
                                 public score::FactoryList_QtInterface,
                                 public score::FactoryInterface_QtInterface,
                                 public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "b29771b7-3f12-4255-af5d-4153b08e55cf")
public:
  score_plugin_audio();
  virtual ~score_plugin_audio();

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;

  // Contains the OSC, MIDI, Minuit factories
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
