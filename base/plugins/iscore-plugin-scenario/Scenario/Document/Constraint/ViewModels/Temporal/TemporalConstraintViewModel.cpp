#include "TemporalConstraintViewModel.hpp"

TemporalConstraintViewModel::TemporalConstraintViewModel(
        const Id<ConstraintViewModel>& id,
        const ConstraintModel& model,
        QObject* parent) :
    ConstraintViewModel {id,
                                "TemporalConstraintViewModel",
                                model,
                                parent
}
{

}

TemporalConstraintViewModel* TemporalConstraintViewModel::clone(
        const Id<ConstraintViewModel>& id,
        const ConstraintModel& cm,
        QObject* parent)
{
    auto cstr = new TemporalConstraintViewModel {id, cm, parent};
    cstr->showRack(this->shownRack());

    return cstr;
}

QString TemporalConstraintViewModel::type() const
{
    return "Temporal";
}
