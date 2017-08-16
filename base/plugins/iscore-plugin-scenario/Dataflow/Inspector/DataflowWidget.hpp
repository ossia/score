#pragma once
#include <Process/Dataflow/DataflowProcess.hpp>
#include <iscore/widgets/ClearLayout.hpp>
#include <iscore/widgets/TextLabel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Dataflow/Commands/EditPort.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <iscore_plugin_scenario_export.h>
namespace Dataflow
{
class PortWidget :
    public QWidget
{
  Q_OBJECT
  iscore::MarginLess<QHBoxLayout> m_lay;
  QPushButton m_remove;
public:
  PortWidget(Explorer::DeviceExplorerModel& model, QWidget* parent);

  QLineEdit localName;
  Explorer::AddressAccessorEditWidget accessor;

signals:
  void removeMe();
};

class ISCORE_PLUGIN_SCENARIO_EXPORT DataflowWidget:
    public QWidget
{
  Q_OBJECT
  const Process::DataflowProcess& m_proc;
  Explorer::DeviceExplorerModel& m_explorer;
  CommandDispatcher<> m_disp;
  iscore::MarginLess<QVBoxLayout> m_lay;
  QPushButton* m_addInlet{};
  QPushButton* m_addOutlet{};
  std::vector<PortWidget*> m_inlets;
  std::vector<PortWidget*> m_outlets;
public:
  DataflowWidget(
      const iscore::DocumentContext& doc,
      const Process::DataflowProcess& proc,
      QWidget* parent);

  void reinit();

};
}
