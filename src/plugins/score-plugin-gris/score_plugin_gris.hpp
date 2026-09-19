#pragma once
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <QObject>

#include <score_plugin_gris_export.h>

#include <utility>
#include <vector>

class SCORE_PLUGIN_GRIS_EXPORT score_plugin_gris final
    : public QObject
    , public score::Plugin_QtInterface
    , public score::FactoryInterface_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "0a4dc8ef-5c2b-4a7e-9f13-6d8b2e05c7a1")
public:
  score_plugin_gris();
  ~score_plugin_gris() override;

private:
  std::vector<score::InterfaceBase*> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override;

  std::vector<score::PluginKey> required() const override;
};
