#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_avnd final
    : public score::FactoryInterface_QtInterface
    , public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "2e61dd71-e469-4084-bdda-64bd65761519")
public:
  score_plugin_avnd();
  ~score_plugin_avnd() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
