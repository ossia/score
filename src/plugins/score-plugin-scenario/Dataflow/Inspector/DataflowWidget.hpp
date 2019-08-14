#pragma once
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/Commands/EditPort.hpp>
#include <Process/Dataflow/Port.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QComboBox>
#include <QWidget>

#include <score_plugin_scenario_export.h>
#include <verdigris>
namespace Dataflow
{
class CableWidget final : public Inspector::InspectorWidgetBase
{
  QComboBox m_cabletype;

public:
  CableWidget(
      const Process::Cable& cable,
      const score::DocumentContext& ctx,
      QWidget* parent);
};

class CableInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("4b1a99aa-016e-440f-8ba6-24b961cff532")
public:
  CableInspectorFactory();

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc,
      QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
}
