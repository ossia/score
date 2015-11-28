#include "FullViewConstraintViewModel.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

FullViewConstraintViewModel::FullViewConstraintViewModel(
        const Id<ConstraintViewModel>& id,
        const ConstraintModel& model,
        QObject* parent) :
    ConstraintViewModel {id,
                                "FullViewConstraintViewModel",
                                model,
                                parent
}
{

}

FullViewConstraintViewModel* FullViewConstraintViewModel::clone(
        const Id<ConstraintViewModel>& id,
        const ConstraintModel& cm,
        QObject* parent)
{
    auto cstr = new FullViewConstraintViewModel {id, cm, parent};
    cstr->showRack(this->shownRack());

    return cstr;
}

QString FullViewConstraintViewModel::type() const
{
    return "FullView";
}

ZoomRatio FullViewConstraintViewModel::zoom() const
{
    return m_zoom;
}

void FullViewConstraintViewModel::setZoom(const ZoomRatio& zoom)
{
    m_zoom = zoom;
}

QPointF FullViewConstraintViewModel::center() const
{
    return m_center;
}

void FullViewConstraintViewModel::setCenter(const QPointF& value)
{
    m_center = value;
}

#include <core/document/Document.hpp>
#include <core/document/DocumentContext.hpp>
bool FullViewConstraintViewModel::isActive()
{
    auto& ctx = iscore::IDocument::documentContext(model());
    auto& baseElt = iscore::IDocument::get<ScenarioDocumentModel>(ctx.document);

    return (this->model().id() == baseElt.displayedElements.constraint().id());
}
