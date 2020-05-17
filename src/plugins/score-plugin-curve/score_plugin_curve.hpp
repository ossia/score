#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/command/CommandGeneratorMap.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <utility>
#include <vector>
#include <verdigris>
/**
 * \namespace Curve
 * \brief Utilities and base classes for 1D curves.
 *
 * This plug-in contains the tools used in score to create
 * and edit curves.
 *
 * * The triplet Curve::CurveModel, Curve::CurvePresenter, Curve::CurveView
 * represents an editable curve.
 * * For processes that only use a single curve, base classes are provided for
 * convenience : Curve::CurveProcessModel and Curve::CurveProcessPresenter.
 * * Common curve segments types are provided.
 *
 * \todo Put the easing segments defined in the engine library in this library.
 *
 * A way to skin curves differently according to the curve type is given
 * through Curve::Style.
 */

class score_plugin_curve final : public score::Plugin_QtInterface,
                                 public score::FactoryInterface_QtInterface,
                                 public score::CommandFactory_QtInterface,
                                 public score::FactoryList_QtInterface,
                                 public score::ApplicationPlugin_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "49837ed7-dbc5-4330-9890-a130a2718b5e")
public:
  score_plugin_curve();
  ~score_plugin_curve() override;

private:
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& factoryName) const override;

  std::vector<std::unique_ptr<score::InterfaceListBase>> factoryFamilies() override;

  std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;

  score::GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app) override;
};
