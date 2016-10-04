
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <iscore/widgets/SpinBoxes.hpp>
#include <QBoxLayout>
#include <QFormLayout>

#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QStringList>
#include <QWidget>
#include <algorithm>
#include <list>
#include <vector>
#include <QCheckBox>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/ChangeAddress.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Automation/Commands/SetAutomationMin.hpp>
#include "AutomationInspectorWidget.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <State/Widgets/UnitWidget.hpp>
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
    InspectorWidgetDelegate_T {automationModel, parent},
    m_dispatcher{doc.commandStack}
{
    using namespace Explorer;
    setObjectName("AutomationInspectorWidget");
    setParent(parent);

    auto vlay = new QFormLayout;
    vlay->setSpacing(2);
    vlay->setMargin(2);
    vlay->setContentsMargins(0,0,0,0);

    // LineEdit
    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    // TODO instead of doing this, just make an address line edit factory.
    auto plug = doc.findPlugin<DeviceDocumentPlugin>();
    DeviceExplorerModel* explorer{};
    if(plug)
        explorer = &plug->explorer();
    m_lineEdit = new AddressAccessorEditWidget{explorer, this};

    m_lineEdit->setAddress(process().address().address);
    con(process(), &ProcessModel::addressChanged,
        m_lineEdit, [=] (const State::AddressAccessor& addr) { m_lineEdit->setAddress(addr); });

    connect(m_lineEdit, &AddressAccessorEditWidget::addressChanged,
            this, &InspectorWidget::on_addressChange);

    vlay->addRow(tr("Address"), m_lineEdit);

    // Tween
    m_tween = new QCheckBox{this};
    vlay->addRow(tr("Tween"), m_tween);
    m_tween->setChecked(process().tween());
    con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
    connect(m_tween, &QCheckBox::toggled,
            this, &InspectorWidget::on_tweenChanged);

    // Min / max
    m_minsb = new iscore::SpinBox<float>{this};
    m_maxsb = new iscore::SpinBox<float>{this};
    m_minsb->setValue(process().min());
    m_maxsb->setValue(process().max());

    m_uw = new State::UnitWidget{{}, this};

    vlay->addRow(tr("Min"), m_minsb);
    vlay->addRow(tr("Max"), m_maxsb);
    vlay->addRow(tr("Unit"), m_uw);

    con(process(), &ProcessModel::minChanged, m_minsb, &QDoubleSpinBox::setValue);
    con(process(), &ProcessModel::maxChanged, m_maxsb, &QDoubleSpinBox::setValue);


    connect(m_minsb, &QAbstractSpinBox::editingFinished,
            this, &InspectorWidget::on_minValueChanged);
    connect(m_maxsb, &QAbstractSpinBox::editingFinished,
            this, &InspectorWidget::on_maxValueChanged);
    connect(m_uw, &State::UnitWidget::unitChanged,
            this, [=] (ossia::unit_t) { on_unitChanged(); });

    this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const ::State::AddressAccessor& newAddr)
{
    // Various checks
    if(newAddr == process().address())
        return;

    if(newAddr.address.path.isEmpty())
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
void InspectorWidget::on_tweenChanged()
{
    bool newVal = m_tween->checkState();
    if(newVal != process().tween())
    {
        auto cmd = new SetTween{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}
void InspectorWidget::on_unitChanged()
{
    qDebug() << "Unit changed ! " << QString::fromStdString(ossia::get_pretty_unit_text(m_uw->unit()));

}
}
