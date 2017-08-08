// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterpolationInspectorWidget.hpp"
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <iscore/widgets/ReactiveLabel.hpp>
#include <iscore/widgets/TextLabel.hpp>

namespace Interpolation
{
InspectorWidget::InspectorWidget(
    const ProcessModel& automationModel,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetDelegate_T{automationModel, parent}
    , m_dispatcher{doc.commandStack}
{
  using namespace Explorer;
  setObjectName("InterpolationInspectorWidget");
  setParent(parent);

  auto vlay = new QFormLayout;
  vlay->setSpacing(0);
  vlay->setContentsMargins(0, 0, 0, 0);

  // Address
  m_lineEdit = new AddressAccessorEditWidget{
      doc.plugin<DeviceDocumentPlugin>().explorer(),
      this};

  m_lineEdit->setAddress(process().address());
  con(process(), &ProcessModel::addressChanged, m_lineEdit,
      &AddressAccessorEditWidget::setAddress);

  connect(
      m_lineEdit, &AddressAccessorEditWidget::addressChanged, this,
        [this] (const auto& addr) { this->on_addressChange(addr.address); });

  vlay->addRow(tr("Address"), m_lineEdit);

  // Tween
  m_tween = new QCheckBox{this};
  vlay->addRow(tr("Tween"), m_tween);
  m_tween->setChecked(process().tween());
  con(process(), &ProcessModel::tweenChanged, m_tween, &QCheckBox::setChecked);
  connect(
      m_tween, &QCheckBox::toggled, this, &InspectorWidget::on_tweenChanged);

  this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const ::State::AddressAccessor& addr)
{
  ChangeInterpolationAddress(process(), addr, m_dispatcher);
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

StateInspectorWidget::StateInspectorWidget(
    const ProcessState& object,
    const iscore::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetBase{object, doc, parent}
    , m_state{object}
    , m_label{new TextLabel}
{
  std::vector<QWidget*> vec;
  vec.push_back(m_label);

  con(m_state, &ProcessStateDataInterface::stateChanged, this,
      &StateInspectorWidget::on_stateChanged);

  on_stateChanged();

  updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void StateInspectorWidget::on_stateChanged()
{
  QString txt = State::convert::toPrettyString(m_state.message().value);
  auto unit = m_state.process().sourceUnit();
  if (auto& u = unit.get())
  {
    txt += " " + QString::fromStdString(ossia::get_pretty_unit_text(u));
  }

  m_label->setText(txt);
}

StateInspectorFactory::StateInspectorFactory() : InspectorWidgetFactory{}
{
}

QWidget* StateInspectorFactory::make(
    const QList<const QObject*>& sourceElements,
    const iscore::DocumentContext& doc,
    QWidget* parent) const
{
  return new StateInspectorWidget{
      safe_cast<const ProcessState&>(*sourceElements.first()), doc, parent};
}

bool StateInspectorFactory::matches(const QList<const QObject*>& objects) const
{
  return dynamic_cast<const ProcessState*>(objects.first());
}
}
