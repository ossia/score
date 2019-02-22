#pragma once
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <QObject>

#include <wobjectdefs.h>

#include <vector>

namespace score
{
class InterfaceListBase;
class PanelFactory;
} // namespace score

// RENAMEME
class score_plugin_inspector : public score::Plugin_QtInterface,
                               public score::FactoryInterface_QtInterface,
                               public score::FactoryList_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "0ed1520f-e120-458e-a5a8-b3f05f3b6b6c")
public:
  score_plugin_inspector();
  ~score_plugin_inspector() override;

  // Panel interface
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  // Factory for inspector widgets
  std::vector<std::unique_ptr<score::InterfaceListBase>>
  factoryFamilies() override;
};
