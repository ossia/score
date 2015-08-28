#include "FullViewConstraintViewModel.hpp"

FullViewConstraintViewModel::FullViewConstraintViewModel(
        const Id<ConstraintViewModel>& id,
        const ConstraintModel& model,
        QObject* parent) :
    ConstraintViewModel {id,
                                "FullViewConstraintViewModel",
                                model,
                                parent
}
{

}

FullViewConstraintViewModel* FullViewConstraintViewModel::clone(
        const Id<ConstraintViewModel>& id,
        const ConstraintModel& cm,
        QObject* parent)
{
    auto cstr = new FullViewConstraintViewModel {id, cm, parent};
    cstr->showRack(this->shownRack());

    return cstr;
}

QString FullViewConstraintViewModel::type() const
{
    return "FullView";
}
