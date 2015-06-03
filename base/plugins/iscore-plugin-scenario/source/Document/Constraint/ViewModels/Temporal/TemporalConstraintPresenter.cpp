#include "TemporalConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <QGraphicsScene>

TemporalConstraintPresenter::TemporalConstraintPresenter(
        const TemporalConstraintViewModel& cstr_model,
        QGraphicsObject *parentobject,
        QObject* parent) :
    AbstractConstraintPresenter {"TemporalConstraintPresenter",
                                 cstr_model,
                                 new TemporalConstraintView{*this, parentobject},
                                 parent}
{
    connect(::view(this), &TemporalConstraintView::constraintHoverEnter,
            this,       &TemporalConstraintPresenter::constraintHoverEnter);

    connect(::view(this), &TemporalConstraintView::constraintHoverLeave,
            this,       &TemporalConstraintPresenter::constraintHoverLeave);

    if(viewModel(this)->isBoxShown())
    {
        on_boxShown(viewModel(this)->shownBox());
    }
    ::view(this)->setLabel(cstr_model.model().metadata.label());

    connect(&cstr_model.model().metadata, &ModelMetadata::labelChanged,
            ::view(this), &TemporalConstraintView::setLabel);
    connect(&cstr_model.model().metadata,   &ModelMetadata::colorChanged,
            ::view(this),   &TemporalConstraintView::setLabelColor);

    updateHeight();
}

TemporalConstraintPresenter::~TemporalConstraintPresenter()
{
    if(::view(this))
    {
        auto sc = ::view(this)->scene();

        if(sc && sc->items().contains(::view(this)))
        {
            sc->removeItem(::view(this));
        }

        ::view(this)->deleteLater();
    }
}
