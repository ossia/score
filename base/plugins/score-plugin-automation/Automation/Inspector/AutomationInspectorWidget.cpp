// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Explorer/Widgets/AddressAccessorEditWidget.hpp>
#include <QBoxLayout>
#include <QFormLayout>
#include <score/widgets/SpinBoxes.hpp>

#include "AutomationInspectorWidget.hpp"
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/ChangeAddress.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>
#include <State/Address.hpp>
#include <State/Widgets/UnitWidget.hpp>
#include <algorithm>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/Todo.hpp>
#include <score/widgets/TextLabel.hpp>
#include <list>
#include <vector>


namespace Automation
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}
    , m_dispatcher{doc.commandStack}
{
  using namespace Explorer;
  setObjectName("AutomationInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  QString name = tr("Automation");
  m_label = new TextLabel{name, this};
  // TODO use the same style as InspectorWidgetBase
  m_label->setStyleSheet("font-weight: bold; font-size: 18");
  vlay->addWidget(m_label);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{
      doc.plugin<DeviceDocumentPlugin>().explorer(), this};

  m_lineEdit->setAddress(process().address());
  con(process(), &ProcessModel::addressChanged, m_lineEdit,
      &AddressAccessorEditWidget::setAddress);

  connect(
      m_lineEdit, &AddressAccessorEditWidget::addressChanged, this,
      &InspectorWidget::on_addressChange);

  vlay->addRow(tr("Address"), m_lineEdit);

  // Tween
  m_tween = new QCheckBox{this};
  vlay->addRow(tr("Tween"), m_tween);
  m_tween->setChecked(process().tween());
  con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
  connect(
      m_tween, &QCheckBox::toggled, this, &InspectorWidget::on_tweenChanged);

  // Min / max
  m_minsb = new score::SpinBox<float>{this};
  m_maxsb = new score::SpinBox<float>{this};
  m_minsb->setValue(process().min());
  m_maxsb->setValue(process().max());

  m_uw = new State::UnitWidget{{}, this};
  m_uw->setUnit(process().unit());

  vlay->addRow(tr("Min"), m_minsb);
  vlay->addRow(tr("Max"), m_maxsb);
  vlay->addRow(tr("Unit"), m_uw);

  con(process(), &ProcessModel::minChanged, m_minsb,
      &QDoubleSpinBox::setValue);
  con(process(), &ProcessModel::maxChanged, m_maxsb,
      &QDoubleSpinBox::setValue);
  con(process(), &ProcessModel::unitChanged, m_uw,
      &State::UnitWidget::setUnit);

  connect(
      m_minsb, &QAbstractSpinBox::editingFinished, this,
      &InspectorWidget::on_minValueChanged);
  connect(
      m_maxsb, &QAbstractSpinBox::editingFinished, this,
      &InspectorWidget::on_maxValueChanged);
  connect(
      m_uw, &State::UnitWidget::unitChanged, this,
      [=](const State::Unit&) { on_unitChanged(); });

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

  m_dispatcher.submitCommand(cmd);
}

void InspectorWidget::on_minValueChanged()
{
  auto newVal = m_minsb->value();
  if (newVal != process().min())
  {
    auto cmd = new SetMin{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}

void InspectorWidget::on_maxValueChanged()
{
  auto newVal = m_maxsb->value();
  if (newVal != process().max())
  {
    auto cmd = new SetMax{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}
void InspectorWidget::on_tweenChanged()
{
  bool newVal = m_tween->checkState();
  if (newVal != process().tween())
  {
    auto cmd = new SetTween{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}
void InspectorWidget::on_unitChanged()
{
  auto newVal = m_uw->unit();
  if (newVal != process().unit())
  {
    auto cmd = new SetUnit{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}
}



namespace Gradient
{
InspectorWidget::InspectorWidget(
    const ProcessModel& proc,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{proc, parent}
    , m_dispatcher{doc.commandStack}
{
  using namespace Explorer;
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
  m_tween = new QCheckBox{this};
  vlay->addRow(tr("Tween"), m_tween);
  m_tween->setChecked(process().tween());
  con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
  connect(
      m_tween, &QCheckBox::toggled, this, &InspectorWidget::on_tweenChanged);

  this->setLayout(vlay);
}


void InspectorWidget::on_tweenChanged()
{
  bool newVal = m_tween->checkState();
  if (newVal != process().tween())
  {
    auto cmd = new Automation::SetTween{process(), newVal};

    m_dispatcher.submitCommand(cmd);
  }
}
}




namespace Spline
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}
    , m_dispatcher{doc.commandStack}
{
  using namespace Explorer;
  setObjectName("SplineInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{
      doc.plugin<DeviceDocumentPlugin>().explorer(), this};

  m_lineEdit->setAddress(process().address());
  con(process(), &ProcessModel::addressChanged, m_lineEdit,
      &AddressAccessorEditWidget::setAddress);

  connect(
      m_lineEdit, &AddressAccessorEditWidget::addressChanged, this,
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

  m_dispatcher.submitCommand(cmd);
}

void InspectorWidget::on_tweenChanged()
{
  bool newVal = m_tween->checkState();
  if (newVal != process().tween())
  {
    //auto cmd = new SetTween{process(), newVal};

    //m_dispatcher.submitCommand(cmd);
  }
}
}


namespace Metronome
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}
    , m_dispatcher{doc.commandStack}
{
  using namespace Explorer;
  setObjectName("MetronomeInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(2);
  vlay->setMargin(2);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{
      doc.plugin<DeviceDocumentPlugin>().explorer(), this};

  m_lineEdit->setAddress(State::AddressAccessor{process().address()});
  con(process(), &ProcessModel::addressChanged, m_lineEdit,
      [=] (const State::Address& addr) {
    m_lineEdit->setAddress(State::AddressAccessor{addr});
  });

  connect(
      m_lineEdit, &AddressAccessorEditWidget::addressChanged, this,
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

  m_dispatcher.submitCommand(cmd);
}

}
