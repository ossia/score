#include "InterpolationInspectorWidget.hpp"
#include <ossia/editor/dataspace/dataspace_visitors.hpp>
#include <ossia/editor/state/destination_qualifiers.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Interpolation/Commands/ChangeAddress.hpp>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
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

  this->setLayout(vlay);
}

void InspectorWidget::on_addressChange(const ::State::AddressAccessor& addr)
{
  // Various checks
  if (addr == process().address())
    return;

  if (addr.address.path.isEmpty())
    return;

  if (addr == State::AddressAccessor{})
  {
    m_dispatcher.submitCommand(new ChangeAddress{process(), {}, {}, {}, {}});
  }
  else
  {
    // Try to find a matching state in the start & end state in order to update
    // the process
    auto cst = dynamic_cast<Scenario::ConstraintModel*>(process().parent());
    if (!cst)
      return;
    auto parent_scenario
        = dynamic_cast<Scenario::ScenarioInterface*>(cst->parent());
    if (!parent_scenario)
      return;

    State::Value sv, ev;
    ossia::unit_t source_u;

    auto& ss = Scenario::startState(*cst, *parent_scenario);
    auto& es = Scenario::endState(*cst, *parent_scenario);
    const auto snodes
        = Process::try_getNodesFromAddress(ss.messages().rootNode(), addr);
    const auto enodes
        = Process::try_getNodesFromAddress(es.messages().rootNode(), addr);

    for (const Process::MessageNode* lhs : snodes)
    {
      if (!lhs->hasValue())
        continue;
      if (lhs->name.qualifiers.get().accessors
          != addr.qualifiers.get().accessors)
        continue;

      auto it = ossia::find_if(enodes, [&](auto rhs) {
        return (lhs->name.qualifiers == rhs->name.qualifiers)
               && rhs->hasValue();
      });

      if (it != enodes.end())
      {
        sv = *lhs->value();
        ev = *(*it)->value();
        source_u = lhs->name.qualifiers.get().unit;

        break; // or maybe not break ? the latest should replace maybe ?
      }
    }

    m_dispatcher.submitCommand(
        new ChangeAddress{process(), addr, sv, ev, source_u});
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
  std::list<QWidget*> vec;
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

Inspector::InspectorWidgetBase* StateInspectorFactory::makeWidget(
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
