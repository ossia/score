
#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <Mapping/Commands/ChangeAddresses.hpp>
#include <Mapping/Commands/MinMaxCommands.hpp>
#include <QAbstractSpinBox>
#include <QBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/widgets/SpinBoxes.hpp>
#include <iscore/widgets/TextLabel.hpp>

#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>
#include <algorithm>
#include <list>
#include <vector>

#include "MappingInspectorWidget.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Mapping/MappingModel.hpp>
#include <State/Address.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/Todo.hpp>

namespace Mapping
{
InspectorWidget::InspectorWidget(
    const ProcessModel& mappingModel,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{mappingModel, parent}
    , m_dispatcher{doc.commandStack}
{
  using namespace Explorer;
  setObjectName("MappingInspectorWidget");
  setParent(parent);

  auto lay = new QVBoxLayout;

  auto& explorer = doc.plugin<DeviceDocumentPlugin>().explorer();

  {
    // Source
    auto widg = new QWidget;
    auto vlay = new QVBoxLayout{widg};
    vlay->setSpacing(0);
    vlay->setContentsMargins(0, 0, 0, 0);

    vlay->addWidget(new TextLabel{tr("Source")});

    m_sourceLineEdit = new AddressAccessorEditWidget{explorer, this};

    m_sourceLineEdit->setAddress(process().sourceAddress());
    con(process(), &ProcessModel::sourceAddressChanged, m_sourceLineEdit,
        &AddressAccessorEditWidget::setAddress);

    connect(
        m_sourceLineEdit, &AddressAccessorEditWidget::addressChanged, this,
        &InspectorWidget::on_sourceAddressChange);

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

    con(process(), &ProcessModel::sourceMinChanged, m_sourceMin,
        &QDoubleSpinBox::setValue);
    con(process(), &ProcessModel::sourceMaxChanged, m_sourceMax,
        &QDoubleSpinBox::setValue);

    connect(
        m_sourceMin, &QAbstractSpinBox::editingFinished, this,
        &InspectorWidget::on_sourceMinValueChanged);
    connect(
        m_sourceMax, &QAbstractSpinBox::editingFinished, this,
        &InspectorWidget::on_sourceMaxValueChanged);

    // TODO in AutomationInspectorWidget, remove all Qt4-style connects.
    lay->addWidget(widg);
  }

  {
    // target
    auto widg = new QWidget;
    auto vlay = new QVBoxLayout{widg};
    vlay->setSpacing(0);
    vlay->setContentsMargins(0, 0, 0, 0);

    vlay->addWidget(new TextLabel{tr("Target")});

    m_targetLineEdit = new AddressAccessorEditWidget{explorer, this};

    m_targetLineEdit->setAddress(process().targetAddress());
    con(process(), &ProcessModel::targetAddressChanged, m_targetLineEdit,
        &AddressAccessorEditWidget::setAddress);

    connect(
        m_targetLineEdit, &AddressAccessorEditWidget::addressChanged, this,
        &InspectorWidget::on_targetAddressChange);

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

    con(process(), &ProcessModel::targetMinChanged, m_targetMin,
        &QDoubleSpinBox::setValue);
    con(process(), &ProcessModel::targetMaxChanged, m_targetMax,
        &QDoubleSpinBox::setValue);

    connect(
        m_targetMin, &QAbstractSpinBox::editingFinished, this,
        &InspectorWidget::on_targetMinValueChanged);
    connect(
        m_targetMax, &QAbstractSpinBox::editingFinished, this,
        &InspectorWidget::on_targetMaxValueChanged);
    lay->addWidget(widg);
  }

  this->setLayout(lay);
}

void InspectorWidget::on_sourceAddressChange(
    const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address == process().sourceAddress())
    return;

  if (newAddr.address.address.path.isEmpty())
    return;

  auto cmd = new ChangeSourceAddress{process(), newAddr};

  m_dispatcher.submitCommand(cmd);
}

void InspectorWidget::on_sourceMinValueChanged()
{
  auto newVal = m_sourceMin->value();
  if (newVal != process().sourceMin())
  {
    auto cmd = new SetMappingSourceMin{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}

void InspectorWidget::on_sourceMaxValueChanged()
{
  auto newVal = m_sourceMax->value();
  if (newVal != process().sourceMax())
  {
    auto cmd = new SetMappingSourceMax{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}

void InspectorWidget::on_targetAddressChange(
    const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address == process().targetAddress())
    return;

  if (newAddr.address.address.path.isEmpty())
    return;

  auto cmd = new ChangeTargetAddress{process(), newAddr};

  m_dispatcher.submitCommand(cmd);
}

void InspectorWidget::on_targetMinValueChanged()
{
  auto newVal = m_targetMin->value();
  if (newVal != process().targetMin())
  {
    auto cmd = new SetMappingTargetMin{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}

void InspectorWidget::on_targetMaxValueChanged()
{
  auto newVal = m_targetMax->value();
  if (newVal != process().targetMax())
  {
    auto cmd = new SetMappingTargetMax{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}
}
