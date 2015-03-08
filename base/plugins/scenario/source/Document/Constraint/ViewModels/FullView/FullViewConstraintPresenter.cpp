#include "FullViewConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <public_interface/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

FullViewConstraintPresenter::FullViewConstraintPresenter(
    FullViewConstraintViewModel* cstr_model,
    FullViewConstraintView* cstr_view,
    QObject* parent) :
    AbstractConstraintPresenter {"FullViewConstraintPresenter", cstr_model, cstr_view, parent}
{
    if(viewModel(this)->isBoxShown())
    {
        on_boxShown(viewModel(this)->shownBox());
    }

    updateHeight();
}

FullViewConstraintPresenter::~FullViewConstraintPresenter()
{
    if(view(this))
    {
        auto sc = view(this)->scene();

        if(sc && sc->items().contains(view(this)))
        {
            sc->removeItem(view(this));
        }

        view(this)->deleteLater();
    }
}
