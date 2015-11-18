#include "TemporalConstraintPresenter.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>

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

    const auto& metadata = m_viewModel.model().metadata;
    con(metadata, &ModelMetadata::labelChanged,
            ::view(this), &TemporalConstraintView::setLabel);

    con(metadata,   &ModelMetadata::colorChanged,
            ::view(this), [&] (const QColor& c) {
        ::view(this)->setLabelColor(c);
        ::view(this)->setColor(c);
    });

    con(metadata, &ModelMetadata::nameChanged,
        this, [&] (const QString& name) { m_header->setText(name); });


    ::view(this)->setLabel(metadata.label());
    ::view(this)->setLabelColor(metadata.color());
    ::view(this)->setColor(metadata.color());
    m_header->setText(metadata.name());

    con(m_viewModel.model().selection, &Selectable::changed,
        ::view(this), &TemporalConstraintView::setFocused);
    con(m_viewModel.model(), &ConstraintModel::focusChanged,
        ::view(this), &TemporalConstraintView::setFocused);

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
