#include "TemporalConstraintViewModel.hpp"
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
TemporalConstraintViewModel::TemporalConstraintViewModel(
    const Id<ConstraintViewModel>& id, ConstraintModel& model, QObject* parent)
    : ConstraintViewModel{id, "TemporalConstraintViewModel", model, parent}
{
}

TemporalConstraintViewModel* TemporalConstraintViewModel::clone(
    const Id<ConstraintViewModel>& id, ConstraintModel& cm, QObject* parent)
{
  auto cstr = new TemporalConstraintViewModel{id, cm, parent};
  cstr->showRack(this->shownRack());

  return cstr;
}

QString TemporalConstraintViewModel::type() const
{
  return "Temporal";
}
}
