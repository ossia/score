#include "BaseScenarioPresenter.hpp"
#include "BaseScenarioModel.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"

#include "Document/Constraint/Rack/RackPresenter.hpp"
#include "Document/Constraint/Rack/Slot/SlotPresenter.hpp"

#include <iscore/document/DocumentInterface.hpp>
BaseScenarioPresenter::BaseScenarioPresenter(BaseElementPresenter *parent):
    NamedObject{"BaseScenarioPresenter", parent},
    m_parent{parent}
{

}

void BaseScenarioPresenter::on_displayedConstraintChanged(const ConstraintModel* m)
{
    const auto& constraintViewModel = m->fullView();

    delete m_displayedConstraintPresenter;
    m_displayedConstraintPresenter = new FullViewConstraintPresenter {*constraintViewModel,
            m_parent->view()->baseItem(),
            m_parent};

    /*
    m_mainTimeRuler->setStartPoint(- m_displayedConstraintPresenter->model().startDate());
    m_localTimeRuler->setDuration(TimeValue{std::chrono::milliseconds(0)});
    m_localTimeRuler->setStartPoint(TimeValue{std::chrono::milliseconds(0)});
    */

    connect(m_displayedConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            m_parent,                       &BaseElementPresenter::on_askUpdate);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () { m_parent->updateRect({0, 0,
                                       m_parent->displayedConstraint()->defaultDuration().toPixels(m_parent->zoomRatio()),
                                       m_parent->height()});} );

    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::pressed,
            m_parent, &BaseElementPresenter::displayedConstraintPressed);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::moved,
            m_parent, &BaseElementPresenter::displayedConstraintMoved);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::released,
            m_parent, &BaseElementPresenter::displayedConstraintReleased);

    showConstraint();

}

void BaseScenarioPresenter::showConstraint()
{
    // We set the focus on the main scenario.
    // TODO crash happens when we load an empty score
    if(m_displayedConstraintPresenter->rack() &&
            !m_displayedConstraintPresenter->rack()->getSlots().empty())
    {
        SlotPresenter* slot = *m_displayedConstraintPresenter->rack()->getSlots().begin();
        m_parent->model()->focusManager().setFocusedPresenter(
                    slot->processes().front().first);
    }
}
