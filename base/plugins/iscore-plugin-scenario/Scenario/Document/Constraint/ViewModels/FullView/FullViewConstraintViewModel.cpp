#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <boost/optional/optional.hpp>

#include "FullViewConstraintViewModel.hpp"
#include <Process/ZoomHelper.hpp>
#include "Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class QObject;

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

#include <core/document/DocumentContext.hpp>

bool FullViewConstraintViewModel::isActive()
{
    auto& ctx = iscore::IDocument::documentContext(model());
    auto& baseElt = iscore::IDocument::get<ScenarioDocumentModel>(ctx.document);

    return (this->model().id() == baseElt.displayedElements.constraint().id());
}
