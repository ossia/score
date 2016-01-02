#include <Explorer/PanelBase/DeviceExplorerPanelModel.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <iscore/widgets/SpinBoxes.hpp>
#include <QBoxLayout>
#include <QFormLayout>

#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>
#include <algorithm>
#include <list>
#include <vector>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/ChangeAddress.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Automation/Commands/SetAutomationMin.hpp>
#include "AutomationInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <State/Address.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/Todo.hpp>

namespace Automation
{
InspectorWidget::InspectorWidget(
        const ProcessModel& automationModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    ProcessInspectorWidgetDelegate_T {automationModel, parent},
    m_dispatcher{doc.commandStack}
{
    setObjectName("AutomationInspectorWidget");
    setParent(parent);

    auto vlay = new QVBoxLayout;
    vlay->setSpacing(0);
    vlay->setContentsMargins(0,0,0,0);

    // LineEdit
    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    // TODO instead of doing this, just make an address line edit factory.
    auto plug = doc.findPlugin<DeviceDocumentPlugin>();
    DeviceExplorerModel* explorer{};
    if(plug)
        explorer = plug->updateProxy.deviceExplorer;
    m_lineEdit = new AddressEditWidget{explorer, this};

    m_lineEdit->setAddress(process().address());
    con(process(), &ProcessModel::addressChanged,
            m_lineEdit, &AddressEditWidget::setAddress);

    connect(m_lineEdit, &AddressEditWidget::addressChanged,
            this, &InspectorWidget::on_addressChange);

    vlay->addWidget(m_lineEdit);

    // Min / max
    auto minmaxwid = new QWidget;
    auto minmaxlay = new QFormLayout{minmaxwid};
    vlay->addWidget(minmaxwid);
    minmaxlay->setSpacing(0);
    minmaxlay->setContentsMargins(0, 0, 0, 0);

    m_minsb = new iscore::SpinBox<float>;
    m_maxsb = new iscore::SpinBox<float>;
    m_minsb->setValue(process().min());
    m_maxsb->setValue(process().max());
    minmaxlay->addRow(tr("Min"), m_minsb);
    minmaxlay->addRow(tr("Max"), m_maxsb);

    con(process(), &ProcessModel::minChanged, m_minsb, &QDoubleSpinBox::setValue);
    con(process(), &ProcessModel::maxChanged, m_maxsb, &QDoubleSpinBox::setValue);

    connect(m_minsb, &QAbstractSpinBox::editingFinished,
            this, &InspectorWidget::on_minValueChanged);
    connect(m_maxsb, &QAbstractSpinBox::editingFinished,
            this, &InspectorWidget::on_maxValueChanged);

    this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const ::State::Address& newAddr)
{
    // Various checks
    if(newAddr == process().address())
        return;

    if(newAddr.path.isEmpty())
        return;

    auto cmd = new ChangeAddress{process(), newAddr};

    m_dispatcher.submitCommand(cmd);
}
void InspectorWidget::on_minValueChanged()
{
    auto newVal = m_minsb->value();
    if(newVal != process().min())
    {
        auto cmd = new SetMin{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}

void InspectorWidget::on_maxValueChanged()
{
    auto newVal = m_maxsb->value();
    if(newVal != process().max())
    {
        auto cmd = new SetMax{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}
}
