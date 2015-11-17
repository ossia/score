#include "FullViewConstraintViewModel.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>

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

bool FullViewConstraintViewModel::isActive()
{
    auto scenar = dynamic_cast<ScenarioModel*>(this->model().parentScenario());
    const ConstraintModel* cstr = & this->model();
    while (scenar)
    {
        cstr = dynamic_cast<ConstraintModel*>(scenar->parent());
        ISCORE_ASSERT(cstr);
        scenar = dynamic_cast<ScenarioModel*>(cstr->parentScenario());
    }
    auto baseScenar = dynamic_cast<BaseScenario*>(cstr->parentScenario());
    ISCORE_ASSERT(cstr);
    auto baseElt = dynamic_cast<BaseElementModel*>(baseScenar->parent());

    return (this->model().id() == baseElt->displayedElements.displayedConstraint().id());
}
