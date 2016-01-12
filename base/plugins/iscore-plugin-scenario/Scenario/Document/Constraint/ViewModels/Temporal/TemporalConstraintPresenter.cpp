#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QGraphicsScene>
#include <QList>

#include <Process/ModelMetadata.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintHeader.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include "TemporalConstraintHeader.hpp"
#include "TemporalConstraintPresenter.hpp"
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/Todo.hpp>

class QColor;
class QObject;
class QString;

namespace Scenario
{
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
    auto& v = *Scenario::view(this);
    con(v, &TemporalConstraintView::constraintHoverEnter,
        this,       &TemporalConstraintPresenter::constraintHoverEnter);

    con(v, &TemporalConstraintView::constraintHoverLeave,
        this,       &TemporalConstraintPresenter::constraintHoverLeave);

    const auto& metadata = m_viewModel.model().metadata;
    con(metadata, &ModelMetadata::labelChanged,
        &v, &TemporalConstraintView::setLabel);

    con(metadata,   &ModelMetadata::colorChanged,
        &v, [&] (const QColor& c) {
        v.setLabelColor(c);
        v.setColor(c);
    });

    con(metadata, &ModelMetadata::nameChanged,
        this, [&] (const QString& name) { m_header->setText(name); });


    v.setLabel(metadata.label());
    v.setLabelColor(metadata.color());
    v.setColor(metadata.color());
    m_header->setText(metadata.name());

    con(m_viewModel.model().selection, &Selectable::changed,
        &v, &TemporalConstraintView::setFocused);
    con(m_viewModel.model(), &ConstraintModel::focusChanged,
        &v, &TemporalConstraintView::setFocused);

    // Change to full view when header is double-clicked
    connect(static_cast<TemporalConstraintHeader*>(m_header), &TemporalConstraintHeader::doubleClicked,
            this, [this] () {
        using namespace iscore::IDocument;
        auto& base = get<ScenarioDocumentModel> (*documentFromObject(m_viewModel.model()));

        base.setDisplayedConstraint(m_viewModel.model());
    });
}

TemporalConstraintPresenter::~TemporalConstraintPresenter()
{

    auto view = Scenario::view(this);
    // TODO deleteGraphicsObject
    if(view)
    {
        auto sc = view->scene();

        if(sc && sc->items().contains(view))
        {
            sc->removeItem(view);
        }

        view->deleteLater();
    }
}
}
