#include <Explorer/PanelBase/DeviceExplorerPanelModel.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <core/document/Document.hpp>
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

AutomationInspectorWidget::AutomationInspectorWidget(
        const AutomationModel& automationModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    InspectorWidgetBase {automationModel, doc, parent},
    m_model {automationModel}
{
    setObjectName("AutomationInspectorWidget");
    setParent(parent);

    std::list<QWidget*> vec;

    auto widg = new QWidget;
    auto vlay = new QVBoxLayout{widg};
    vlay->setSpacing(0);
    vlay->setContentsMargins(0,0,0,0);
    auto hlay = new QHBoxLayout{};
    hlay->setSpacing(0);
    hlay->setContentsMargins(0,0,0,0);

    vec.push_back(widg);

    // LineEdit
    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    auto plug = doc.findPlugin<DeviceDocumentPlugin>();
    DeviceExplorerModel* explorer{};
    if(plug)
        explorer = plug->updateProxy.deviceExplorer;
    m_lineEdit = new AddressEditWidget{explorer, this};

    m_lineEdit->setAddress(m_model.address());
    con(m_model, &AutomationModel::addressChanged,
            m_lineEdit, &AddressEditWidget::setAddress);

    connect(m_lineEdit, &AddressEditWidget::addressChanged,
            this, &AutomationInspectorWidget::on_addressChange);

    vlay->addWidget(m_lineEdit);

    // Min / max
    auto minmaxwid = new QWidget;
    auto minmaxlay = new QFormLayout{minmaxwid};
    vec.push_back(minmaxwid);
    minmaxlay->setSpacing(0);
    minmaxlay->setContentsMargins(0, 0, 0, 0);

    m_minsb = new iscore::SpinBox<float>;
    m_maxsb = new iscore::SpinBox<float>;
    m_minsb->setValue(m_model.min());
    m_maxsb->setValue(m_model.max());
    minmaxlay->addRow(tr("Min"), m_minsb);
    minmaxlay->addRow(tr("Max"), m_maxsb);

    con(m_model, SIGNAL(minChanged(double)), m_minsb, SLOT(setValue(double)));
    con(m_model, SIGNAL(maxChanged(double)), m_maxsb, SLOT(setValue(double)));

    connect(m_minsb, SIGNAL(editingFinished()), this, SLOT(on_minValueChanged()));
    connect(m_maxsb, SIGNAL(editingFinished()), this, SLOT(on_maxValueChanged()));


    // Add it to a new slot
    auto display = new QPushButton{"~"};
    hlay->addWidget(display);
    connect(display,    &QPushButton::clicked,
            [ = ]()
    {
        createViewInNewSlot(QString::number(m_model.id_val()));
    });

    vlay->addLayout(hlay);


    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void AutomationInspectorWidget::on_addressChange(const iscore::Address& newAddr)
{
    // Various checks
    if(newAddr == m_model.address())
        return;

    if(newAddr.path.isEmpty())
        return;

    auto cmd = new ChangeAddress{m_model, newAddr};

    commandDispatcher()->submitCommand(cmd);
}

void AutomationInspectorWidget::on_minValueChanged()
{
    auto newVal = m_minsb->value();
    if(newVal != m_model.min())
    {
        auto cmd = new SetAutomationMin{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}

void AutomationInspectorWidget::on_maxValueChanged()
{
    auto newVal = m_maxsb->value();
    if(newVal != m_model.max())
    {
        auto cmd = new SetAutomationMax{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}
