#include "FullViewConstraintViewModel.hpp"

FullViewConstraintViewModel::FullViewConstraintViewModel (id_type<AbstractConstraintViewModel> id,
        ConstraintModel* model,
        QObject* parent) :
    AbstractConstraintViewModel {id,
                                "FullViewConstraintViewModel",
                                model,
                                parent
}
{

}

FullViewConstraintViewModel* FullViewConstraintViewModel::clone (id_type<AbstractConstraintViewModel> id,
        ConstraintModel* cm,
        QObject* parent)
{
    auto cstr = new FullViewConstraintViewModel {id, cm, parent};
    cstr->showBox (this->shownBox() );

    return cstr;
}
