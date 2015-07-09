#include "BaseScenarioPresenter.hpp"
#include "BaseScenarioModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp"
void BaseScenarioPresenter::on_displayedConstraintChanged(ConstraintModel* m)
{
    const auto& constraintViewModel = m->fullView();

    delete m_displayedConstraintPresenter;
    m_displayedConstraintPresenter = new FullViewConstraintPresenter {constraintViewModel,
            this->view()->baseItem(),
            this};

    /*
    m_mainTimeRuler->setStartPoint(- m_displayedConstraintPresenter->model().startDate());
    m_localTimeRuler->setDuration(TimeValue{std::chrono::milliseconds(0)});
    m_localTimeRuler->setStartPoint(TimeValue{std::chrono::milliseconds(0)});
    */

    // Set a new zoom ratio, such that the displayed constraint takes the whole screen.
    on_zoomSliderChanged(0);
    on_askUpdate();

    connect(m_displayedConstraintPresenter,	&FullViewConstraintPresenter::askUpdate,
            this,					        &BaseElementPresenter::on_askUpdate);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::heightChanged,
            this, [&] () { updateRect({0,
                                       0,
                                       m_displayedConstraint->defaultDuration().toPixels(m_millisecondsPerPixel),
                                       height()});} );

    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::pressed,
            this, &BaseElementPresenter::displayedConstraintPressed);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::moved,
            this, &BaseElementPresenter::displayedConstraintMoved);
    connect(m_displayedConstraintPresenter, &FullViewConstraintPresenter::released,
            this, &BaseElementPresenter::displayedConstraintReleased);

}

void BaseScenarioPresenter::showConstraint()
{

    // We set the focus on the main scenario.
    // TODO crash happens when we load an empty score
    if(m_displayedConstraintPresenter->rack() &&
            !m_displayedConstraintPresenter->rack()->getSlots().empty())
    {
        SlotPresenter* slot = *m_displayedConstraintPresenter->rack()->getSlots().begin();
        model()->focusManager().setFocusedPresenter(
                    slot->processes().front().first);
    }
}
