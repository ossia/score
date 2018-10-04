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
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <score_plugin_scenario_export.h>
#include <wobjectdefs.h>
namespace Dataflow
{
class SCORE_PLUGIN_SCENARIO_EXPORT PortWidget : public QWidget
{
  W_OBJECT(PortWidget)
  score::MarginLess<QHBoxLayout> m_lay;
  QPushButton m_remove;

public:
  PortWidget(const score::DocumentContext& model, QWidget* parent);

  QLineEdit localName;
  Device::AddressAccessorEditWidget accessor;

public:
  void removeMe() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, removeMe);
};

class CableWidget final : public Inspector::InspectorWidgetBase
{
  QComboBox m_cabletype;

public:
  CableWidget(
      const Process::Cable& cable, const score::DocumentContext& ctx,
      QWidget* parent);
};

class CableInspectorFactory final : public Inspector::InspectorWidgetFactory
{
  SCORE_CONCRETE("4b1a99aa-016e-440f-8ba6-24b961cff532")
public:
  CableInspectorFactory();

  QWidget* make(
      const InspectedObjects& sourceElements,
      const score::DocumentContext& doc, QWidget* parent) const override;

  bool matches(const InspectedObjects& objects) const override;
};
class SCORE_PLUGIN_SCENARIO_EXPORT DataflowWidget : public QWidget
{
  W_OBJECT(DataflowWidget)
  const Process::ProcessModel& m_proc;
  const score::DocumentContext& m_ctx;
  CommandDispatcher<> m_disp;
  score::MarginLess<QVBoxLayout> m_lay;
  QPushButton* m_addInlet{};
  QPushButton* m_addOutlet{};
  std::vector<PortWidget*> m_inlets;
  std::vector<PortWidget*> m_outlets;

public:
  DataflowWidget(
      const score::DocumentContext& doc, const Process::ProcessModel& proc,
      QWidget* parent);

  void reinit();
};
}
