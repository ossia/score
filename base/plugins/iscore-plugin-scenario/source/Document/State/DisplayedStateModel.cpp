#include "DisplayedStateModel.hpp"

#include "StateView.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

DisplayedStateModel::DisplayedStateModel(id_type<DisplayedStateModel> id,
                                         double yPos,
                                         AbstractConstraintView *view,
                                         QObject *parent):
    IdentifiedObject<DisplayedStateModel> {id, "DisplayedStateModel", parent},
    m_heightPercentage{yPos},
    m_view{new StateView{*this, view} }
{

}

DisplayedStateModel::DisplayedStateModel(const DisplayedStateModel &copy,
                                         const id_type<DisplayedStateModel> &id,
                                         QObject *parent):
    DisplayedStateModel(id, copy.heightPercentage(), dynamic_cast<AbstractConstraintView*>(copy.view()->parentWidget()), copy.parent())
{
    m_states = copy.states();
    //TODO : view
}

const ScenarioModel *DisplayedStateModel::parentScenario() const
{
    return  &(dynamic_cast<TemporalScenarioPresenter*>(parent())->stateMachine().model());
}

double DisplayedStateModel::heightPercentage() const
{
    return m_heightPercentage;
}

StateView *DisplayedStateModel::view() const
{
    return m_view;
}

void DisplayedStateModel::setHeightPercentage(double y)
{
    if(m_heightPercentage == y)
        return;
    m_heightPercentage = y;
}

const iscore::StateList &DisplayedStateModel::states() const
{
    return m_states;
}

void DisplayedStateModel::replaceStates(const iscore::StateList &newStates)
{
    m_states = newStates;
}

void DisplayedStateModel::addState(const iscore::State &s)
{
    m_states.append(s);

}

void DisplayedStateModel::removeState(const iscore::State &s)
{
    m_states.removeOne(s);

}

