// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "AutomationInspectorWidget.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/ChangeAddress.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Device/Widgets/AddressAccessorEditWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <State/Address.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SpinBoxes.hpp>
#include <score/widgets/TextLabel.hpp>

#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QWidget>

#include <list>
#include <vector>

namespace Automation
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}, m_dispatcher{doc.commandStack}
{

  using namespace Device;
  setObjectName("AutomationInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{doc, this};

  m_lineEdit->setAddress(process().address());
  con(process(),
      &ProcessModel::addressChanged,
      m_lineEdit,
      &AddressAccessorEditWidget::setAddress);

  connect(
      m_lineEdit,
      &AddressAccessorEditWidget::addressChanged,
      this,
      &InspectorWidget::on_addressChange);

  vlay->addRow(tr("Address"), m_lineEdit);

  // Tween
  m_tween = new QCheckBox{tr("Tween"), this};
  vlay->addRow(m_tween);
  m_tween->setChecked(process().tween());
  con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
  connect(m_tween, &QCheckBox::toggled, this, &InspectorWidget::on_tweenChanged);

  // Min / max
  m_minsb = new score::SpinBox<float>{this};
  m_maxsb = new score::SpinBox<float>{this};
  m_minsb->setValue(process().min());
  m_maxsb->setValue(process().max());

  vlay->addRow(tr("Min"), m_minsb);
  vlay->addRow(tr("Max"), m_maxsb);

  con(process(), &ProcessModel::minChanged, m_minsb, &QDoubleSpinBox::setValue);
  con(process(), &ProcessModel::maxChanged, m_maxsb, &QDoubleSpinBox::setValue);

  connect(m_minsb, &QAbstractSpinBox::editingFinished, this, &InspectorWidget::on_minValueChanged);
  connect(m_maxsb, &QAbstractSpinBox::editingFinished, this, &InspectorWidget::on_maxValueChanged);

  this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address == process().address())
    return;

  if (newAddr.address.address.path.isEmpty())
    return;

  auto cmd = new ChangeAddress{process(), newAddr};

  m_dispatcher.submit(cmd);
}

void InspectorWidget::on_minValueChanged()
{
  auto newVal = m_minsb->value();
  if (newVal != process().min())
  {
    auto cmd = new SetMin{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}

void InspectorWidget::on_maxValueChanged()
{
  auto newVal = m_maxsb->value();
  if (newVal != process().max())
  {
    auto cmd = new SetMax{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}
void InspectorWidget::on_tweenChanged()
{
  bool newVal = m_tween->checkState();
  if (newVal != process().tween())
  {
    auto cmd = new SetTween{process(), newVal};

    m_dispatcher.submit(cmd);
  }
}
}

namespace Gradient
{
InspectorWidget::InspectorWidget(
    const ProcessModel& proc,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{proc, parent}, m_dispatcher{doc.commandStack}
{

  setObjectName("GradientInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  /*
  auto port = new PortTooltip{doc, *proc.outlets};
  vlay->addWidget(port);
  */
  /*
    m_lineEdit = new AddressAccessorEditWidget{
        doc.plugin<DeviceDocumentPlugin>().explorer(), this};

    m_lineEdit->setAddress(process().address());
    con(process(), &ProcessModel::addressChanged, m_lineEdit,
        &AddressAccessorEditWidget::setAddress);

    connect(
        m_lineEdit, &AddressAccessorEditWidget::addressChanged, this,
        &InspectorWidget::on_addressChange);


    vlay->addRow(tr("Address"), m_lineEdit);
  */

  // Tween
  m_tween = new QCheckBox{tr("Tween"), this};
  vlay->addRow(m_tween);
  m_tween->setChecked(process().tween());
  con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
  connect(m_tween, &QCheckBox::toggled, this, &InspectorWidget::on_tweenChanged);

  this->setLayout(vlay);
}

void InspectorWidget::on_tweenChanged()
{
  bool newVal = m_tween->checkState();
  if (newVal != process().tween())
  {
    auto cmd = new Gradient::SetGradientTween(process(), newVal);

    m_dispatcher.submit(cmd);
  }
}
}

namespace Spline
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}, m_dispatcher{doc.commandStack}
{
  using namespace Device;

  setObjectName("SplineInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{doc, this};

  m_lineEdit->setAddress(process().address());
  con(process(),
      &ProcessModel::addressChanged,
      m_lineEdit,
      &AddressAccessorEditWidget::setAddress);

  connect(
      m_lineEdit,
      &AddressAccessorEditWidget::addressChanged,
      this,
      &InspectorWidget::on_addressChange);

  vlay->addRow(tr("Address"), m_lineEdit);

  /*
  // Tween
  m_tween = new QCheckBox{this};
  vlay->addRow(tr("Tween"), m_tween);
  m_tween->setChecked(process().tween());
  con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
  connect(
      m_tween, &QCheckBox::toggled, this, &InspectorWidget::on_tweenChanged);

  */
  this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address == process().address())
    return;

  if (newAddr.address.address.path.isEmpty())
    return;

  auto cmd = new ChangeSplineAddress{process(), newAddr.address};

  m_dispatcher.submit(cmd);
}

void InspectorWidget::on_tweenChanged()
{
  bool newVal = m_tween->checkState();
  if (newVal != process().tween())
  {
    // auto cmd = new SetTween{process(), newVal};

    // m_dispatcher.submit(cmd);
  }
}
}

namespace Metronome
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}, m_dispatcher{doc.commandStack}
{
  using namespace Device;

  setObjectName("MetronomeInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{doc, this};

  m_lineEdit->setAddress(State::AddressAccessor{process().address()});
  con(process(), &ProcessModel::addressChanged, m_lineEdit, [=](const State::Address& addr) {
    m_lineEdit->setAddress(State::AddressAccessor{addr});
  });

  connect(
      m_lineEdit,
      &AddressAccessorEditWidget::addressChanged,
      this,
      &InspectorWidget::on_addressChange);

  vlay->addRow(tr("Address"), m_lineEdit);

  this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const Device::FullAddressAccessorSettings& newAddr)
{
  // Various checks
  if (newAddr.address.address == process().address())
    return;

  if (newAddr.address.address.path.isEmpty())
    return;

  auto cmd = new ChangeMetronomeAddress{process(), newAddr.address.address};

  m_dispatcher.submit(cmd);
}
}
