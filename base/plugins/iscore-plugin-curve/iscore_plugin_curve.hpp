#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <utility>
#include <vector>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
/**
 * \namespace Curve
 * \brief Utilities and base classes for 1D curves.
 *
 * This plug-in contains the tools used in i-score to create
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

class iscore_plugin_curve final : public QObject,
                                  public iscore::Plugin_QtInterface,
                                  public iscore::FactoryInterface_QtInterface,
                                  public iscore::CommandFactory_QtInterface,
                                  public iscore::FactoryList_QtInterface
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
  Q_INTERFACES(
      iscore::Plugin_QtInterface iscore::FactoryInterface_QtInterface
          iscore::CommandFactory_QtInterface iscore::FactoryList_QtInterface)

public:
  iscore_plugin_curve();
  virtual ~iscore_plugin_curve() = default;

private:
  std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
      const iscore::ApplicationContext& ctx,
      const iscore::InterfaceKey& factoryName) const override;

  std::vector<std::unique_ptr<iscore::InterfaceListBase>>
  factoryFamilies() override;

  std::pair<const CommandParentFactoryKey, CommandGeneratorMap>
  make_commands() override;

  iscore::Version version() const override;
  UuidKey<iscore::Plugin> key() const override;
};
