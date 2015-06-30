#include "DisplayedStateModel.hpp"

#include "StateView.hpp"

DisplayedStateModel::DisplayedStateModel(id_type<DisplayedStateModel> id,
                                         double yPos,
                                         StateView* view,
                                         QObject *parent):
    IdentifiedObject<DisplayedStateModel> {id, "DisplayedStateModel", parent},
    m_heightPercentage{yPos},
    m_view{view}
{

}

DisplayedStateModel::DisplayedStateModel(const DisplayedStateModel &copy,
                                         const id_type<DisplayedStateModel> &id,
                                         QObject *parent):
    DisplayedStateModel(id, copy.heightPercentage(), copy.view(), copy.parent())
{
    m_states = copy.states();
//TODO : view
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

