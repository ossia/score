#include "DisplayedStateModel.hpp"

#include "StateView.hpp"

#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

DisplayedStateModel::DisplayedStateModel(const id_type<DisplayedStateModel>& id,
                                         const id_type<EventModel>& eventId,
                                         double yPos,
                                         QObject *parent):
    IdentifiedObject<DisplayedStateModel> {id, "DisplayedStateModel", parent},
    m_eventId{eventId},
    m_heightPercentage{yPos}
{

}

void DisplayedStateModel::initView(AbstractConstraintView *parentView)
{
    m_view = new StateView{*this, parentView};
}

DisplayedStateModel::DisplayedStateModel(const DisplayedStateModel &copy,
                                         const id_type<DisplayedStateModel> &id,
                                         QObject *parent):
    DisplayedStateModel(id, copy.eventId(), copy.heightPercentage(), copy.parent())
{
    m_states = copy.states();
    //TODO : view
}

const ScenarioModel *DisplayedStateModel::parentScenario() const
{
    return  (dynamic_cast<ScenarioModel*>(parent()));
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

void DisplayedStateModel::setPos(qreal y)
{
    m_view->setPos({0, y});
}
const id_type<EventModel> &DisplayedStateModel::eventId() const
{
    return m_eventId;
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

