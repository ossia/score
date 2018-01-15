#pragma once
#include <score/widgets/ClearLayout.hpp>
#include <score/widgets/TextLabel.hpp>
#include <score/document/DocumentContext.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Dataflow/Commands/EditPort.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/widgets/MarginLess.hpp>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <score_plugin_scenario_export.h>

#include <Process/Dataflow/Port.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
namespace Dataflow
{
class SCORE_PLUGIN_SCENARIO_EXPORT PortWidget :
    public QWidget
{
    Q_OBJECT
    score::MarginLess<QHBoxLayout> m_lay;
    QPushButton m_remove;
  public:
    PortWidget(Explorer::DeviceExplorerModel& model, QWidget* parent);

    QLineEdit localName;
    Explorer::AddressAccessorEditWidget accessor;

  Q_SIGNALS:
    void removeMe();
};

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
        const QList<const QObject*>& sourceElements,
        const score::DocumentContext& doc,
        QWidget* parent) const override;

    bool matches(const QList<const QObject*>& objects) const override;
};
class SCORE_PLUGIN_SCENARIO_EXPORT DataflowWidget:
    public QWidget
{
    Q_OBJECT
    const Process::ProcessModel& m_proc;
    Explorer::DeviceExplorerModel& m_explorer;
    CommandDispatcher<> m_disp;
    score::MarginLess<QVBoxLayout> m_lay;
    QPushButton* m_addInlet{};
    QPushButton* m_addOutlet{};
    std::vector<PortWidget*> m_inlets;
    std::vector<PortWidget*> m_outlets;
  public:
    DataflowWidget(
        const score::DocumentContext& doc,
        const Process::ProcessModel& proc,
        QWidget* parent);

    void reinit();

};
}
