#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <verdigris>

class score_plugin_analysis final : public score::FactoryInterface_QtInterface,
                              public score::Plugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "dd999d63-9a2d-4f3b-b3a1-804f8326e76b")
public:
  score_plugin_analysis();
  ~score_plugin_analysis() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext&,
      const score::InterfaceKey& factoryName) const override;

  std::vector<score::PluginKey> required() const override;
};
