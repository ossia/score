#include "OSSIAStateElement.hpp"
#include <iscore/tools/Todo.hpp>
OSSIAStateElement::OSSIAStateElement(
        const StateModel *element,
        QObject *parent):
    QObject{parent},
    m_iscore_state{element}
{
}

const StateModel *OSSIAStateElement::iscoreState() const
{
    return m_iscore_state;
}

const QHash<iscore::State, std::shared_ptr<OSSIA::State>>& OSSIAStateElement::states() const
{
    return m_states;
}


void OSSIAStateElement::addState(const iscore::State& is, std::shared_ptr<OSSIA::State> os)
{
    m_states.insert(is, os);
}

void OSSIAStateElement::removeState(const iscore::State& s)
{
    auto it = m_states.find(s);
    if(it != m_states.end())
        m_states.erase(it);
}
