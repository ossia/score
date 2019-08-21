#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <QObject>

#include <utility>
#include <vector>

class score_addon_remotecontrol final
    : public score::Plugin_QtInterface,
      public score::FactoryList_QtInterface,
      public score::FactoryInterface_QtInterface,
      public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "ecffb9d5-3d67-4b89-a64f-341b68cd9603")
public:
  score_addon_remotecontrol();
  virtual ~score_addon_remotecontrol();

private:
  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;

  std::vector<std::unique_ptr<score::InterfaceListBase>>
  factoryFamilies() override;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
