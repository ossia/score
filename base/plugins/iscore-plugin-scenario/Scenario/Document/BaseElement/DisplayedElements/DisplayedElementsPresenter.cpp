#include "DisplayedElementsPresenter.hpp"
#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>

#include <Scenario/Process/ScenarioModel.hpp>

DisplayedElementsPresenter::DisplayedElementsPresenter(BaseElementPresenter *parent):
    QObject{parent},
    BaseScenarioPresenter<DisplayedElementsModel, FullViewConstraintPresenter>{parent->model().displayedElements},
    m_model{parent}
{

}

void DisplayedElementsPresenter::on_displayedConstraintChanged(const ConstraintModel& m)
{
    for(auto& con : m_connections)
        QObject::disconnect(con);

    m_connections.clear();
    delete m_constraintPresenter;
    delete m_startStatePresenter;
    delete m_endStatePresenter;
    delete m_startEventPresenter;
    delete m_endEventPresenter;
    delete m_startNodePresenter;
    delete m_endNodePresenter;

    m_constraintPresenter = new FullViewConstraintPresenter {
            *m.fullView(),
            m_model->view().baseItem(),
            m_model};

    // Create states / events
    // TODO this needs to be virtual instead and call ScenarioInterface...
    if(auto bs = dynamic_cast<BaseScenario*>(m.parent()))
    {
        m_startStatePresenter = new StatePresenter{bs->startState(), m_model->view().baseItem(), this};
        m_endStatePresenter = new StatePresenter{bs->endState(), m_model->view().baseItem(), this};

        m_startEventPresenter = new EventPresenter{bs->startEvent(), m_model->view().baseItem(), this};
        m_endEventPresenter = new EventPresenter{bs->endEvent(), m_model->view().baseItem(), this};

        m_startNodePresenter = new TimeNodePresenter{bs->startTimeNode(), m_model->view().baseItem(), this};
        m_endNodePresenter = new TimeNodePresenter{bs->endTimeNode(), m_model->view().baseItem(), this};

    }
    else if(auto sm = dynamic_cast<Scenario::ScenarioModel*>(m.parent()))
    {
        const auto& startState = sm->states.at(m.startState());
        const auto& endState = sm->states.at(m.endState());
        const auto& startEvent = sm->events.at(startState.eventId());
        const auto& endEvent = sm->events.at(endState.eventId());
        const auto& startNode = sm->timeNodes.at(startEvent.timeNode());
        const auto& endNode = sm->timeNodes.at(endEvent.timeNode());
        m_startStatePresenter = new StatePresenter{startState, m_model->view().baseItem(), this};
        m_endStatePresenter = new StatePresenter{endState, m_model->view().baseItem(), this};

        m_startEventPresenter = new EventPresenter{startEvent, m_model->view().baseItem(), this};
        m_endEventPresenter = new EventPresenter{endEvent, m_model->view().baseItem(), this};

        m_startNodePresenter = new TimeNodePresenter{startNode, m_model->view().baseItem(), this};
        m_endNodePresenter = new TimeNodePresenter{endNode, m_model->view().baseItem(), this};
    }

    m_connections.push_back(con(m_constraintPresenter->model().duration, &ConstraintDurations::defaultDurationChanged,
        this, &DisplayedElementsPresenter::on_displayedConstraintDurationChanged));
    m_connections.push_back(connect(m_constraintPresenter, &FullViewConstraintPresenter::askUpdate,
            m_model,              &BaseElementPresenter::on_askUpdate));
    m_connections.push_back(connect(m_constraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () {
        on_displayedConstraintHeightChanged(m_constraintPresenter->view()->height());
    }));

    auto elements = std::make_tuple(
                m_constraintPresenter,
                m_startStatePresenter,
                m_endStatePresenter,
                m_startEventPresenter,
                m_endEventPresenter,
                m_startNodePresenter,
                m_endNodePresenter);

    for_each_in_tuple(elements, [&] (auto elt) {
        using elt_t = std::remove_reference_t<decltype(*elt)>;
        m_connections.push_back(connect(elt, &elt_t::pressed,  m_model, &BaseElementPresenter::pressed));
        m_connections.push_back(connect(elt, &elt_t::moved,    m_model, &BaseElementPresenter::moved));
        m_connections.push_back(connect(elt, &elt_t::released, m_model, &BaseElementPresenter::released));
    });

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

void DisplayedElementsPresenter::on_zoomRatioChanged(ZoomRatio r)
{
    updateLength(m_constraintPresenter->abstractConstraintViewModel().model().duration.defaultDuration().toPixels(r));

    m_constraintPresenter->on_zoomRatioChanged(r);
}

void DisplayedElementsPresenter::on_displayedConstraintDurationChanged(TimeValue t)
{
    updateLength(t.toPixels(m_model->zoomRatio()));
}

void DisplayedElementsPresenter::on_displayedConstraintHeightChanged(double size)
{
    m_model->updateRect(
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
