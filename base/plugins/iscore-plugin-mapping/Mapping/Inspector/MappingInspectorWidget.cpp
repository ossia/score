#include <Explorer/PanelBase/DeviceExplorerPanelModel.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <Mapping/Commands/ChangeAddresses.hpp>
#include <Mapping/Commands/MinMaxCommands.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/SpinBoxes.hpp>
#include <QAbstractSpinBox>
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>

#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>
#include <algorithm>
#include <list>
#include <vector>

#include <Inspector/InspectorWidgetBase.hpp>
#include <Mapping/MappingModel.hpp>
#include "MappingInspectorWidget.hpp"
#include <State/Address.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/Todo.hpp>

namespace Mapping
{
MappingInspectorWidget::MappingInspectorWidget(
        const MappingModel& mappingModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    ProcessInspectorWidgetDelegate_T {mappingModel, parent},
    m_dispatcher{doc.commandStack}
{
    using namespace DeviceExplorer;
    setObjectName("MappingInspectorWidget");
    setParent(parent);

    auto lay = new QVBoxLayout;
    // LineEdit
    // If there is a DeviceExplorer in the current document, use it
    // to make a widget.
    auto plug = doc.findPlugin<DeviceDocumentPlugin>();
    DeviceExplorerModel* explorer{};
    if(plug)
        explorer = plug->updateProxy.deviceExplorer;
    {
        // Source
        auto widg = new QWidget;
        auto vlay = new QVBoxLayout{widg};
        vlay->setSpacing(0);
        vlay->setContentsMargins(0,0,0,0);

        vlay->addWidget(new QLabel{tr("Source")});

        m_sourceLineEdit = new AddressEditWidget{explorer, this};

        m_sourceLineEdit->setAddress(process().sourceAddress());
        con(process(), &MappingModel::sourceAddressChanged,
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
        m_sourceMin->setValue(process().sourceMin());
        m_sourceMax->setValue(process().sourceMax());
        minmaxlay->addRow(tr("Min"), m_sourceMin);
        minmaxlay->addRow(tr("Max"), m_sourceMax);

        con(process(), &MappingModel::sourceMinChanged,
            m_sourceMin, &QDoubleSpinBox::setValue);
        con(process(), &MappingModel::sourceMaxChanged,
            m_sourceMax, &QDoubleSpinBox::setValue);

        connect(m_sourceMin, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_sourceMinValueChanged);
        connect(m_sourceMax, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_sourceMaxValueChanged);

        // TODO in AutomationInspectorWidget, remove all Qt4-style connects.
        lay->addWidget(widg);
    }

    {
        // target
        auto widg = new QWidget;
        auto vlay = new QVBoxLayout{widg};
        vlay->setSpacing(0);
        vlay->setContentsMargins(0,0,0,0);

        vlay->addWidget(new QLabel{tr("Target")});

        m_targetLineEdit = new AddressEditWidget{explorer, this};

        m_targetLineEdit->setAddress(process().targetAddress());
        con(process(), &MappingModel::targetAddressChanged,
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
        m_targetMin->setValue(process().targetMin());
        m_targetMax->setValue(process().targetMax());
        minmaxlay->addRow(tr("Min"), m_targetMin);
        minmaxlay->addRow(tr("Max"), m_targetMax);

        con(process(), &MappingModel::targetMinChanged,
            m_targetMin, &QDoubleSpinBox::setValue);
        con(process(), &MappingModel::targetMaxChanged,
            m_targetMax, &QDoubleSpinBox::setValue);

        connect(m_targetMin, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_targetMinValueChanged);
        connect(m_targetMax, &QAbstractSpinBox::editingFinished,
                this, &MappingInspectorWidget::on_targetMaxValueChanged);
        lay->addWidget(widg);
    }


    this->setLayout(lay);
}

void MappingInspectorWidget::on_sourceAddressChange(const State::Address& newAddr)
{
    // Various checks
    if(newAddr == process().sourceAddress())
        return;

    if(newAddr.path.isEmpty())
        return;

    auto cmd = new ChangeSourceAddress{process(), newAddr};

    m_dispatcher.submitCommand(cmd);
}

void MappingInspectorWidget::on_sourceMinValueChanged()
{
    auto newVal = m_sourceMin->value();
    if(newVal != process().sourceMin())
    {
        auto cmd = new SetMappingSourceMin{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}

void MappingInspectorWidget::on_sourceMaxValueChanged()
{
    auto newVal = m_sourceMax->value();
    if(newVal != process().sourceMax())
    {
        auto cmd = new SetMappingSourceMax{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}


void MappingInspectorWidget::on_targetAddressChange(const State::Address& newAddr)
{
    // Various checks
    if(newAddr == process().targetAddress())
        return;

    if(newAddr.path.isEmpty())
        return;

    auto cmd = new ChangeTargetAddress{process(), newAddr};

    m_dispatcher.submitCommand(cmd);
}

void MappingInspectorWidget::on_targetMinValueChanged()
{
    auto newVal = m_targetMin->value();
    if(newVal != process().targetMin())
    {
        auto cmd = new SetMappingTargetMin{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}

void MappingInspectorWidget::on_targetMaxValueChanged()
{
    auto newVal = m_targetMax->value();
    if(newVal != process().targetMax())
    {
        auto cmd = new SetMappingTargetMax{process(), newVal};

        m_dispatcher.submitCommand(cmd);
    }
}
}
