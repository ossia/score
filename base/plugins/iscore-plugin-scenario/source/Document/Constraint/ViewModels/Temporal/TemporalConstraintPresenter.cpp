#include "TemporalConstraintPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include "Document/BaseElement/BaseElementModel.hpp"

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

    con(m_viewModel.model().metadata, &ModelMetadata::labelChanged,
            ::view(this), &TemporalConstraintView::setLabel);
    con(m_viewModel.model().metadata,   &ModelMetadata::colorChanged,
            ::view(this), [&] (const QColor& c) {
        ::view(this)->setLabelColor(c);
        ::view(this)->setColor(c);
    });

    con(m_viewModel.model().selection, &Selectable::changed,
        ::view(this), &TemporalConstraintView::setFocused);
    con(m_viewModel.model(), &ConstraintModel::focusChanged,
        ::view(this), &TemporalConstraintView::setFocused);

    m_header->setText(m_viewModel.model().metadata.name());
    con(m_viewModel.model().metadata, &ModelMetadata::nameChanged,
            this, [&] (const QString& name) { m_header->setText(name); });

    // Change to full view when header is double-clicked
    connect(static_cast<TemporalConstraintHeader*>(m_header), &TemporalConstraintHeader::doubleClicked,
            this, [this] () {
        using namespace iscore::IDocument;
        auto& base = get<BaseElementModel> (*documentFromObject(m_viewModel.model()));

        base.setDisplayedConstraint(m_viewModel.model());
    });
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
