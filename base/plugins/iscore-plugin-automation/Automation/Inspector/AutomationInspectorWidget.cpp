#include "AutomationInspectorWidget.hpp"
#include "Automation/AutomationModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>
#include "Automation/Commands/ChangeAddress.hpp"
#include "Automation/Commands/SetCurveMin.hpp"
#include "Automation/Commands/SetCurveMax.hpp"

#include <Singletons/DeviceExplorerInterface.hpp>
#include <Plugin/Widgets/DeviceCompleter.hpp>
#include <Plugin/Widgets/DeviceExplorerMenuButton.hpp>
#include <Plugin/Widgets/AddressEditWidget.hpp>
#include <Plugin/Panel/DeviceExplorerModel.hpp>
#include <Plugin/PanelBase/DeviceExplorerPanelModel.hpp>

#include <State/Widgets/AddressLineEdit.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <iscore/widgets/SpinBoxes.hpp>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QApplication>

AutomationInspectorWidget::AutomationInspectorWidget(
        const AutomationModel& automationModel,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {automationModel, doc, parent},
    m_model {automationModel}
{
    setObjectName("AutomationInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;

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
    m_explorer = iscore::IDocument::documentFromObject(m_model)->model().panel<DeviceExplorerPanelModel>()->deviceExplorer();
    m_lineEdit = new AddressEditWidget{m_explorer, this};

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
        auto cmd = new SetCurveMin{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}

void AutomationInspectorWidget::on_maxValueChanged()
{
    auto newVal = m_maxsb->value();
    if(newVal != m_model.max())
    {
        auto cmd = new SetCurveMax{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}
