// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "MappingInspectorWidget.hpp"

#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Mapping/Commands/ChangeAddresses.hpp>
#include <Mapping/Commands/MinMaxCommands.hpp>
#include <Mapping/MappingModel.hpp>
#include <State/Address.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QAbstractSpinBox>
#include <QFormLayout>
#include <QWidget>

#include <list>
#include <vector>

namespace Mapping
{
InspectorWidget::InspectorWidget(
    const ProcessModel& mappingModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{mappingModel, parent}, m_dispatcher{doc.commandStack}
{
  using namespace Device;
  setObjectName("MappingInspectorWidget");
  setParent(parent);

  auto lay = new QFormLayout;
  lay->setSpacing(2);
  lay->setMargin(2);
  lay->setContentsMargins(0, 0, 0, 0);

  {
    // Source
    lay->addWidget(new TextLabel{tr("Source")});

    m_sourceLineEdit = new AddressAccessorEditWidget{doc, this};

    m_sourceLineEdit->setAddress(process().sourceAddress());
    con(process(),
        &ProcessModel::sourceAddressChanged,
        m_sourceLineEdit,
        &AddressAccessorEditWidget::setAddress);

    connect(
        m_sourceLineEdit,
        &AddressAccessorEditWidget::addressChanged,
        this,
        &InspectorWidget::on_sourceAddressChange);

    lay->addWidget(m_sourceLineEdit);

    // Min / max
    auto minmaxwid = new QWidget;
    auto minmaxlay = new QFormLayout{minmaxwid};
    lay->addWidget(minmaxwid);
    minmaxlay->setSpacing(0);
    minmaxlay->setContentsMargins(0, 0, 0, 0);

    m_sourceMin = new score::SpinBox<float>;
    m_sourceMax = new score::SpinBox<float>;
    m_sourceMin->setValue(process().sourceMin());
    m_sourceMax->setValue(process().sourceMax());
    minmaxlay->addRow(tr("Min"), m_sourceMin);
    minmaxlay->addRow(tr("Max"), m_sourceMax);

    con(process(), &ProcessModel::sourceMinChanged, m_sourceMin, &QDoubleSpinBox::setValue);
    con(process(), &ProcessModel::sourceMaxChanged, m_sourceMax, &QDoubleSpinBox::setValue);

    connect(
        m_sourceMin,
        &QAbstractSpinBox::editingFinished,
        this,
        &InspectorWidget::on_sourceMinValueChanged);
    connect(
        m_sourceMax,
        &QAbstractSpinBox::editingFinished,
        this,
        &InspectorWidget::on_sourceMaxValueChanged);
  }

  {
    // target
    lay->addWidget(new TextLabel{tr("Target")});

    m_targetLineEdit = new AddressAccessorEditWidget{doc, this};

    m_targetLineEdit->setAddress(process().targetAddress());
    con(process(),
        &ProcessModel::targetAddressChanged,
        m_targetLineEdit,
        &AddressAccessorEditWidget::setAddress);

    connect(
        m_targetLineEdit,
        &AddressAccessorEditWidget::addressChanged,
        this,
        &InspectorWidget::on_targetAddressChange);

    lay->addWidget(m_targetLineEdit);

    // Min / max
    auto minmaxwid = new QWidget;
    auto minmaxlay = new QFormLayout{minmaxwid};
    lay->addWidget(minmaxwid);
    minmaxlay->setSpacing(0);
    minmaxlay->setContentsMargins(0, 0, 0, 0);

    m_targetMin = new score::SpinBox<float>;
    m_targetMax = new score::SpinBox<float>;
    m_targetMin->setValue(process().targetMin());
    m_targetMax->setValue(process().targetMax());
    minmaxlay->addRow(tr("Min"), m_targetMin);
    minmaxlay->addRow(tr("Max"), m_targetMax);

    con(process(), &ProcessModel::targetMinChanged, m_targetMin, &QDoubleSpinBox::setValue);
    con(process(), &ProcessModel::targetMaxChanged, m_targetMax, &QDoubleSpinBox::setValue);

    connect(
        m_targetMin,
        &QAbstractSpinBox::editingFinished,
        this,
        &InspectorWidget::on_targetMinValueChanged);
    connect(
        m_targetMax,
        &QAbstractSpinBox::editingFinished,
        this,
        &InspectorWidget::on_targetMaxValueChanged);
  }

  this->setLayout(lay);
}

void InspectorWidget::on_sourceAddressChange(const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address == process().sourceAddress())
    return;

  auto cmd = new ChangeSourceAddress{process(), newAddr};

  m_dispatcher.submit(cmd);
}

void InspectorWidget::on_sourceMinValueChanged()
{
  auto newVal = m_sourceMin->value();
  if (newVal != process().sourceMin())
  {
    auto cmd = new SetMappingSourceMin{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}

void InspectorWidget::on_sourceMaxValueChanged()
{
  auto newVal = m_sourceMax->value();
  if (newVal != process().sourceMax())
  {
    auto cmd = new SetMappingSourceMax{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}

void InspectorWidget::on_targetAddressChange(const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address == process().targetAddress())
    return;

  auto cmd = new ChangeTargetAddress{process(), newAddr};

  m_dispatcher.submit(cmd);
}

void InspectorWidget::on_targetMinValueChanged()
{
  auto newVal = m_targetMin->value();
  if (newVal != process().targetMin())
  {
    auto cmd = new SetMappingTargetMin{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}

void InspectorWidget::on_targetMaxValueChanged()
{
  auto newVal = m_targetMax->value();
  if (newVal != process().targetMax())
  {
    auto cmd = new SetMappingTargetMax{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}
}
