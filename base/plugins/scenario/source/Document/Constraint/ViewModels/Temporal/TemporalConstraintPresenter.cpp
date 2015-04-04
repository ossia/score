#include "TemporalConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/BoxView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include <iscore/command/SerializableCommand.hpp>

#include <QDebug>
#include <QGraphicsScene>

TemporalConstraintPresenter::TemporalConstraintPresenter(
    TemporalConstraintViewModel* cstr_model,
    TemporalConstraintView* cstr_view,
    QObject* parent) :
    AbstractConstraintPresenter {"TemporalConstraintPresenter", cstr_model, cstr_view, parent}
{
    connect(::view(this), &TemporalConstraintView::constraintMoved,
    [&](QPointF p)
    {
        ConstraintData data {};
        data.id = viewModel(this)->model()->id();
        data.y = p.y();
        data.x = p.x();
        emit constraintMoved(data);
    });

    connect(::view(this), &TemporalConstraintView::constraintReleased,
            this,		&TemporalConstraintPresenter::constraintReleased);

    connect(::view(this), &TemporalConstraintView::constraintHoverEnter,
            this,       &TemporalConstraintPresenter::constraintHoverEnter);

    connect(::view(this), &TemporalConstraintView::constraintHoverLeave,
            this,       &TemporalConstraintPresenter::constraintHoverLeave);

    if(viewModel(this)->isBoxShown())
    {
        on_boxShown(viewModel(this)->shownBox());
    }

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
