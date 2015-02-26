#include "TemporalConstraintViewModel.hpp"

TemporalConstraintViewModel::TemporalConstraintViewModel(id_type<AbstractConstraintViewModel> id,
        ConstraintModel* model,
        QObject* parent) :
    AbstractConstraintViewModel {id,
                                "TemporalConstraintViewModel",
                                model,
                                parent
}
{

}

TemporalConstraintViewModel* TemporalConstraintViewModel::clone(id_type<AbstractConstraintViewModel> id,
        ConstraintModel* cm,
        QObject* parent)
{
    auto cstr = new TemporalConstraintViewModel {id, cm, parent};
    cstr->showBox(this->shownBox());

    return cstr;
}
