#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

class score_plugin_protocols final :
                                  public score::FactoryInterface_QtInterface,
                                  public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "d4758f8d-64ac-41b4-8aaf-1cbd6f3feb91")
public:
  score_plugin_protocols();
  virtual ~score_plugin_protocols();

private:

  // Contains the OSC, MIDI, Minuit factories
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
