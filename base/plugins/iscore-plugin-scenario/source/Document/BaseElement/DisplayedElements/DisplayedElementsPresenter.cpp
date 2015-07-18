#include "DisplayedElementsPresenter.hpp"
#include "Document/BaseElement/BaseScenario/BaseScenarioModel.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp"

#include "Document/Constraint/Rack/RackPresenter.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"

#include "Document/State/DisplayedStateModel.hpp"
#include "Document/State/StatePresenter.hpp"

#include "Process/ScenarioModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
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

    m_constraintPresenter = new FullViewConstraintPresenter {
            *m.fullView(),
            m_parent->view()->baseItem(),
            m_parent};

    // Create states / events
    if(auto bs = dynamic_cast<BaseScenario*>(m.parent()))
    {
        m_startStatePresenter = new StatePresenter(bs->startState(), m_parent->view()->baseItem(), this);
        m_endStatePresenter = new StatePresenter(bs->endState(), m_parent->view()->baseItem(), this);
    }
    else if(auto sm = dynamic_cast<ScenarioModel*>(m.parent()))
    {
        m_startStatePresenter = new StatePresenter(sm->state(m.startState()), m_parent->view()->baseItem(), this);
        m_endStatePresenter = new StatePresenter(sm->state(m.endState()), m_parent->view()->baseItem(), this);
    }

    // Manage the selection
    // The full view constraint presenter does not need it.
    connect(m_startStatePresenter, &StatePresenter::pressed, this, [&] (const QPointF&)
           {
        m_parent->m_selectionDispatcher.setAndCommit({&m_startStatePresenter->model()});
    });
    connect(m_endStatePresenter, &StatePresenter::pressed, this, [&] (const QPointF&)
           {
        m_parent->m_selectionDispatcher.setAndCommit({&m_endStatePresenter->model()});
    });

    ISCORE_TODO;
    /*
    m_mainTimeRuler->setStartPoint(- m_displayedConstraintPresenter->model().startDate());
    m_localTimeRuler->setDuration(TimeValue{std::chrono::milliseconds(0)});
    m_localTimeRuler->setStartPoint(TimeValue{std::chrono::milliseconds(0)});
    */

    connect(m_constraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            m_parent,                       &BaseElementPresenter::on_askUpdate);
    connect(m_constraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () { m_parent->updateRect({0, 0,
                                       m.defaultDuration().toPixels(m_parent->zoomRatio()),
                                       m_constraintPresenter->view()->height()});} );

    connect(m_constraintPresenter, &FullViewConstraintPresenter::pressed,
            m_parent, &BaseElementPresenter::displayedConstraintPressed);
    connect(m_constraintPresenter, &FullViewConstraintPresenter::moved,
            m_parent, &BaseElementPresenter::displayedConstraintMoved);
    connect(m_constraintPresenter, &FullViewConstraintPresenter::released,
            m_parent, &BaseElementPresenter::displayedConstraintReleased);

    showConstraint();
}

void DisplayedElementsPresenter::showConstraint()
{
    // We set the focus on the main scenario.
    if(m_constraintPresenter->rack() && !m_constraintPresenter->rack()->getSlots().empty())
    {
        const auto& slot = *m_constraintPresenter->rack()->getSlots().begin();
        m_parent->model()->focusManager().setFocusedPresenter(
                    slot.processes().front().first);
    }
}

void DisplayedElementsPresenter::on_zoomRatioChanged(ZoomRatio r)
{
    m_startStatePresenter->view()->setPos(0, 0);
    m_endStatePresenter->view()->setPos({m_constraintPresenter->abstractConstraintViewModel().model().defaultDuration().toPixels(r), 0});

    m_constraintPresenter->on_zoomRatioChanged(r);
}
