#include "TemporalConstraintViewModel.hpp"

TemporalConstraintViewModel::TemporalConstraintViewModel(
        const id_type<AbstractConstraintViewModel>& id,
        const ConstraintModel& model,
        QObject* parent) :
    AbstractConstraintViewModel {id,
                                "TemporalConstraintViewModel",
                                model,
                                parent
}
{

}

TemporalConstraintViewModel* TemporalConstraintViewModel::clone(
        const id_type<AbstractConstraintViewModel>& id,
        const ConstraintModel& cm,
        QObject* parent)
{
    auto cstr = new TemporalConstraintViewModel {id, cm, parent};
    cstr->showBox(this->shownBox());

    return cstr;
}
