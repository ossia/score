#include "MappingInspectorWidget.hpp"
#include "Mapping/MappingModel.hpp"
#include <Inspector/InspectorSectionWidget.hpp>

#include <Explorer/Widgets/DeviceCompleter.hpp>
#include <Explorer/Widgets/DeviceExplorerMenuButton.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/PanelBase/DeviceExplorerPanelModel.hpp>

#include <Mapping/Commands/ChangeAddresses.hpp>
#include <Mapping/Commands/MinMaxCommands.hpp>
#include <State/Widgets/AddressLineEdit.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <QLabel>

#include <iscore/widgets/SpinBoxes.hpp>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QApplication>

MappingInspectorWidget::MappingInspectorWidget(
        const MappingModel& MappingModel,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {MappingModel, doc, parent},
    m_model {MappingModel}
{
    setObjectName("MappingInspectorWidget");
    setParent(parent);

    QVector<QWidget*> vec;

    // LineEdit
    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    m_explorer = iscore::IDocument::documentFromObject(m_model)->model().panel<DeviceExplorerPanelModel>()->deviceExplorer();
    {
        // Source
        auto widg = new QWidget;
        auto vlay = new QVBoxLayout{widg};
        vlay->setSpacing(0);
        vlay->setContentsMargins(0,0,0,0);

        vec.push_back(widg);
        vlay->addWidget(new QLabel{tr("Source")});

        m_sourceLineEdit = new AddressEditWidget{m_explorer, this};

        m_sourceLineEdit->setAddress(m_model.sourceAddress());
        con(m_model, &MappingModel::sourceAddressChanged,
            m_sourceLineEdit, &AddressEditWidget::setAddress);

        connect(m_sourceLineEdit, &AddressEditWidget::addressChanged,
                this, &MappingInspectorWidget::on_sourceAddressChange);

        vlay->addWidget(m_sourceLineEdit);

        // Min / max
        auto minmaxwid = new QWidget;
        auto minmaxlay = new QFormLayout{minmaxwid};
        vlay->addWidget(minmaxwid);
        minmaxlay->setSpacing(0);
        minmaxlay->setContentsMargins(0, 0, 0, 0);

        m_sourceMin = new iscore::SpinBox<float>;
        m_sourceMax = new iscore::SpinBox<float>;
        m_sourceMin->setValue(m_model.sourceMin());
        m_sourceMax->setValue(m_model.sourceMax());
        minmaxlay->addRow(tr("Min"), m_sourceMin);
        minmaxlay->addRow(tr("Max"), m_sourceMax);

        con(m_model, &MappingModel::sourceMinChanged,
            m_sourceMin, &QDoubleSpinBox::setValue);
        con(m_model, &MappingModel::sourceMaxChanged,
            m_sourceMax, &QDoubleSpinBox::setValue);

        connect(m_sourceMin, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_sourceMinValueChanged);
        connect(m_sourceMax, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_sourceMaxValueChanged);

        // TODO in AutomationInspectorWidget, remove all Qt4-style connects.
    }

    {
        // target
        auto widg = new QWidget;
        auto vlay = new QVBoxLayout{widg};
        vlay->setSpacing(0);
        vlay->setContentsMargins(0,0,0,0);

        vec.push_back(widg);
        vlay->addWidget(new QLabel{tr("Target")});

        m_targetLineEdit = new AddressEditWidget{m_explorer, this};

        m_targetLineEdit->setAddress(m_model.targetAddress());
        con(m_model, &MappingModel::targetAddressChanged,
            m_targetLineEdit, &AddressEditWidget::setAddress);

        connect(m_targetLineEdit, &AddressEditWidget::addressChanged,
                this, &MappingInspectorWidget::on_targetAddressChange);

        vlay->addWidget(m_targetLineEdit);

        // Min / max
        auto minmaxwid = new QWidget;
        auto minmaxlay = new QFormLayout{minmaxwid};
        vlay->addWidget(minmaxwid);
        minmaxlay->setSpacing(0);
        minmaxlay->setContentsMargins(0, 0, 0, 0);

        m_targetMin = new iscore::SpinBox<float>;
        m_targetMax = new iscore::SpinBox<float>;
        m_targetMin->setValue(m_model.targetMin());
        m_targetMax->setValue(m_model.targetMax());
        minmaxlay->addRow(tr("Min"), m_targetMin);
        minmaxlay->addRow(tr("Max"), m_targetMax);

        con(m_model, &MappingModel::targetMinChanged,
            m_targetMin, &QDoubleSpinBox::setValue);
        con(m_model, &MappingModel::targetMaxChanged,
            m_targetMax, &QDoubleSpinBox::setValue);

        connect(m_targetMin, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_targetMinValueChanged);
        connect(m_targetMax, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_targetMaxValueChanged);
    }

    // Add it to a new slot
    auto display = new QPushButton{"~"};
    connect(display,    &QPushButton::clicked,
            [ = ]()
    {
        createViewInNewSlot(QString::number(m_model.id_val()));
    });
    vec.push_back(display);

    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void MappingInspectorWidget::on_sourceAddressChange(const iscore::Address& newAddr)
{
    // Various checks
    if(newAddr == m_model.sourceAddress())
        return;

    if(newAddr.path.isEmpty())
        return;

    auto cmd = new ChangeSourceAddress{m_model, newAddr};

    commandDispatcher()->submitCommand(cmd);
}

void MappingInspectorWidget::on_sourceMinValueChanged()
{
    auto newVal = m_sourceMin->value();
    if(newVal != m_model.sourceMin())
    {
        auto cmd = new SetMappingSourceMin{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}

void MappingInspectorWidget::on_sourceMaxValueChanged()
{
    auto newVal = m_sourceMax->value();
    if(newVal != m_model.sourceMax())
    {
        auto cmd = new SetMappingSourceMax{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}


void MappingInspectorWidget::on_targetAddressChange(const iscore::Address& newAddr)
{
    // Various checks
    if(newAddr == m_model.targetAddress())
        return;

    if(newAddr.path.isEmpty())
        return;

    auto cmd = new ChangeTargetAddress{m_model, newAddr};

    commandDispatcher()->submitCommand(cmd);
}

void MappingInspectorWidget::on_targetMinValueChanged()
{
    auto newVal = m_targetMin->value();
    if(newVal != m_model.targetMin())
    {
        auto cmd = new SetMappingTargetMin{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}

void MappingInspectorWidget::on_targetMaxValueChanged()
{
    auto newVal = m_targetMax->value();
    if(newVal != m_model.targetMax())
    {
        auto cmd = new SetMappingTargetMax{m_model, newVal};

        commandDispatcher()->submitCommand(cmd);
    }
}
