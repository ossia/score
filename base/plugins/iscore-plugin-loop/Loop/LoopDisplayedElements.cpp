#include "LoopDisplayedElements.hpp"
#include "LoopProcessModel.hpp"
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
namespace Loop
{
bool DisplayedElementsProvider::matches(
        const Scenario::ConstraintModel& cst) const
{
    return dynamic_cast<Loop::ProcessModel*>(cst.parent());
}

Scenario::DisplayedElementsContainer DisplayedElementsProvider::make(
        const Scenario::ConstraintModel& cst) const
{
    if(auto parent_base = dynamic_cast<Loop::ProcessModel*>(cst.parent()))
    {
        return Scenario::DisplayedElementsContainer{
            cst,
            parent_base->startState(),
            parent_base->endState(),

            parent_base->startEvent(),
            parent_base->endEvent(),

            parent_base->startTimeNode(),
            parent_base->endTimeNode()
        };
    }

    return {};
}
Scenario::DisplayedElementsPresenterContainer DisplayedElementsProvider::make_presenters(
        const Scenario::ConstraintModel& m,
        QGraphicsObject* view_parent,
        QObject* parent) const
{
    if(auto bs = dynamic_cast<Loop::ProcessModel*>(m.parent()))
    {
        return Scenario::DisplayedElementsPresenterContainer{
            new Scenario::FullViewConstraintPresenter {
                *m.fullView(),
                view_parent,
                parent},
            new Scenario::StatePresenter{bs->startState(), view_parent, parent},
            new Scenario::StatePresenter{bs->endState(), view_parent, parent},
            new Scenario::EventPresenter{bs->startEvent(), view_parent, parent},
            new Scenario::EventPresenter{bs->endEvent(), view_parent, parent},
            new Scenario::TimeNodePresenter{bs->startTimeNode(), view_parent, parent},
            new Scenario::TimeNodePresenter{bs->endTimeNode(), view_parent, parent}
        };
    }
    return {};
}
}
