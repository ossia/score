#include "TemporalConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"

#include "TemporalConstraintHeader.hpp"
#include <QGraphicsScene>

TemporalConstraintPresenter::TemporalConstraintPresenter(
        const TemporalConstraintViewModel& cstr_model,
        QGraphicsObject *parentobject,
        QObject* parent) :
    ConstraintPresenter {"TemporalConstraintPresenter",
                         cstr_model,
                         new TemporalConstraintView{*this, parentobject},
                         new TemporalConstraintHeader,
                         parent}
{
    connect(::view(this), &TemporalConstraintView::constraintHoverEnter,
            this,       &TemporalConstraintPresenter::constraintHoverEnter);

    connect(::view(this), &TemporalConstraintView::constraintHoverLeave,
            this,       &TemporalConstraintPresenter::constraintHoverLeave);

    ::view(this)->setLabel(cstr_model.model().metadata.label());

    connect(&m_viewModel.model().metadata, &ModelMetadata::labelChanged,
            ::view(this), &TemporalConstraintView::setLabel);
    connect(&m_viewModel.model().metadata,   &ModelMetadata::colorChanged,
            ::view(this),   &TemporalConstraintView::setLabelColor);


    m_header->setText(m_viewModel.model().metadata.name());
    connect(&m_viewModel.model().metadata, &ModelMetadata::nameChanged,
            this, [&] (const QString& name) { m_header->setText(name); });
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
