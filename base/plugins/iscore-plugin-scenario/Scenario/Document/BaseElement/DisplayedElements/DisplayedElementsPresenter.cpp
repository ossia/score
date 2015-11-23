#include "DisplayedElementsPresenter.hpp"
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeView.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>

#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>

#include <Scenario/Document/State/StatePresenter.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

DisplayedElementsPresenter::DisplayedElementsPresenter(BaseElementPresenter *parent):
    QObject{parent},
    m_parent{parent}
{

}

void DisplayedElementsPresenter::on_displayedConstraintChanged(const ConstraintModel& m)
{
    delete m_constraintPresenter;
    delete m_startStatePresenter;
    delete m_endStatePresenter;
    delete m_startEventPresenter;
    delete m_endEventPresenter;
    delete m_startNodePresenter;
    delete m_endNodePresenter;

    m_constraintPresenter = new FullViewConstraintPresenter {
            *m.fullView(),
            m_parent->view().baseItem(),
            m_parent};

    // Create states / events
    // TODO this needs to be virtual instead and call ScenarioInterface...
    if(auto bs = dynamic_cast<BaseScenario*>(m.parent()))
    {
        m_startStatePresenter = new StatePresenter{bs->startState(), m_parent->view().baseItem(), this};
        m_endStatePresenter = new StatePresenter{bs->endState(), m_parent->view().baseItem(), this};

        m_startEventPresenter = new EventPresenter{bs->startEvent(), m_parent->view().baseItem(), this};
        m_endEventPresenter = new EventPresenter{bs->endEvent(), m_parent->view().baseItem(), this};

        m_startNodePresenter = new TimeNodePresenter{bs->startTimeNode(), m_parent->view().baseItem(), this};
        m_endNodePresenter = new TimeNodePresenter{bs->endTimeNode(), m_parent->view().baseItem(), this};

    }
    else if(auto sm = dynamic_cast<ScenarioModel*>(m.parent()))
    {
        const auto& startState = sm->states.at(m.startState());
        const auto& endState = sm->states.at(m.endState());
        const auto& startEvent = sm->events.at(startState.eventId());
        const auto& endEvent = sm->events.at(endState.eventId());
        const auto& startNode = sm->timeNodes.at(startEvent.timeNode());
        const auto& endNode = sm->timeNodes.at(endEvent.timeNode());
        m_startStatePresenter = new StatePresenter{startState, m_parent->view().baseItem(), this};
        m_endStatePresenter = new StatePresenter{endState, m_parent->view().baseItem(), this};

        m_startEventPresenter = new EventPresenter{startEvent, m_parent->view().baseItem(), this};
        m_endEventPresenter = new EventPresenter{endEvent, m_parent->view().baseItem(), this};

        m_startNodePresenter = new TimeNodePresenter{startNode, m_parent->view().baseItem(), this};
        m_endNodePresenter = new TimeNodePresenter{endNode, m_parent->view().baseItem(), this};
    }

    con(m_constraintPresenter->model().duration, &ConstraintDurations::defaultDurationChanged,
        this, &DisplayedElementsPresenter::on_displayedConstraintDurationChanged);

    // Manage the selection
    // The full view constraint presenter does not need it.
    /*
    connect(m_startStatePresenter, &StatePresenter::pressed, this, [&] (const QPointF&)
           {
        m_parent->m_selectionDispatcher.setAndCommit({&m_startStatePresenter->model()});
    });
    connect(m_endStatePresenter, &StatePresenter::pressed, this, [&] (const QPointF&)
           {
        m_parent->m_selectionDispatcher.setAndCommit({&m_endStatePresenter->model()});
    });*/

    connect(m_constraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            m_parent,               &BaseElementPresenter::on_askUpdate);
    connect(m_constraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () {
        on_displayedConstraintHeightChanged(m_constraintPresenter->view()->height());
    });

    connect(m_constraintPresenter, &FullViewConstraintPresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_constraintPresenter, &FullViewConstraintPresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_constraintPresenter, &FullViewConstraintPresenter::released,
            m_parent, &BaseElementPresenter::released);

    connect(m_startStatePresenter, &StatePresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_startStatePresenter, &StatePresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_startStatePresenter, &StatePresenter::released,
            m_parent, &BaseElementPresenter::released);
    connect(m_endStatePresenter, &StatePresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_endStatePresenter, &StatePresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_endStatePresenter, &StatePresenter::released,
            m_parent, &BaseElementPresenter::released);

    connect(m_startEventPresenter, &EventPresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_startEventPresenter, &EventPresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_startEventPresenter, &EventPresenter::released,
            m_parent, &BaseElementPresenter::released);
    connect(m_endEventPresenter, &EventPresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_endEventPresenter, &EventPresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_endEventPresenter, &EventPresenter::released,
            m_parent, &BaseElementPresenter::released);

    connect(m_startNodePresenter, &TimeNodePresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_startNodePresenter, &TimeNodePresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_startNodePresenter, &TimeNodePresenter::released,
            m_parent, &BaseElementPresenter::released);
    connect(m_endNodePresenter, &TimeNodePresenter::pressed,
            m_parent, &BaseElementPresenter::pressed);
    connect(m_endNodePresenter, &TimeNodePresenter::moved,
            m_parent, &BaseElementPresenter::moved);
    connect(m_endNodePresenter, &TimeNodePresenter::released,
            m_parent, &BaseElementPresenter::released);
    showConstraint();

    on_zoomRatioChanged(m_constraintPresenter->zoomRatio());
}

void DisplayedElementsPresenter::showConstraint()
{
    // We set the focus on the main scenario.
    if(m_constraintPresenter->rack() && !m_constraintPresenter->rack()->getSlots().empty())
    {
        const auto& slot = *m_constraintPresenter->rack()->getSlots().begin();
        if(slot.processes().size() > 0)
        {
            const auto& slot_process = slot.processes().front().processes;
            if(slot_process.size() > 0)
                emit requestFocusedPresenterChange(slot_process.front().first);
        }
    }
}

const EventPresenter& DisplayedElementsPresenter::event(const Id<EventModel>& id) const
{
    const auto& de = m_parent->model().displayedElements;
    if(id == de.startEvent().id())
        return *m_startEventPresenter;
    else if(id == de.endEvent().id())
        return *m_endEventPresenter;
    ISCORE_ABORT;
}

const TimeNodePresenter& DisplayedElementsPresenter::timeNode(const Id<TimeNodeModel>& id) const
{
    const auto& de = m_parent->model().displayedElements;
    if(id == de.startNode().id())
        return *m_startNodePresenter;
    else if(id == de.endNode().id())
        return *m_endNodePresenter;
    ISCORE_ABORT;
}

const FullViewConstraintPresenter& DisplayedElementsPresenter::constraint(const Id<ConstraintModel>& id) const
{
    const auto& de = m_parent->model().displayedElements;
    if(id == de.displayedConstraint().id())
        return *m_constraintPresenter;
    ISCORE_ABORT;
}

const StatePresenter& DisplayedElementsPresenter::state(const Id<StateModel>& id) const
{
    const auto& de = m_parent->model().displayedElements;
    if(id == de.startState().id())
        return *m_startStatePresenter;
    else if(id == de.endState().id())
        return *m_endStatePresenter;
    ISCORE_ABORT;
}

const TimeNodeModel&DisplayedElementsPresenter::startTimeNode() const
{
    return m_startNodePresenter->model();
}

void DisplayedElementsPresenter::on_zoomRatioChanged(ZoomRatio r)
{
    updateLength(m_constraintPresenter->abstractConstraintViewModel().model().duration.defaultDuration().toPixels(r));

    m_constraintPresenter->on_zoomRatioChanged(r);
}

void DisplayedElementsPresenter::on_displayedConstraintDurationChanged(TimeValue t)
{
    updateLength(t.toPixels(m_constraintPresenter->model().fullView()->zoom()));
}

void DisplayedElementsPresenter::on_displayedConstraintHeightChanged(double size)
{
    m_parent->updateRect(
    {
        0,
        0,
        m_constraintPresenter->abstractConstraintViewModel().model().duration.defaultDuration().toPixels(m_constraintPresenter->model().fullView()->zoom()),
        size
    });

    m_startEventPresenter->view()->setExtent({0, size * .2});
    m_startNodePresenter->view()->setExtent({0, size* .4});
    m_endEventPresenter->view()->setExtent({0, size * .2});
    m_endNodePresenter->view()->setExtent({0, size* .4});
}

void DisplayedElementsPresenter::updateLength(double length)
{
    m_endStatePresenter->view()->setPos({length, 0});
    m_endEventPresenter->view()->setPos({length, 0});
    m_endNodePresenter->view()->setPos({length, 0});
}
