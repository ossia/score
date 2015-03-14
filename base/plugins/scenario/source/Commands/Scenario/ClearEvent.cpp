#include "ClearEvent.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Process/ScenarioModel.hpp"

#include <State/State.hpp>
#include <iscore/tools/utilsCPP11.hpp>

using namespace iscore;
using namespace Scenario::Command;

ClearEvent::ClearEvent(ObjectPath&& eventPath) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(eventPath) }
{
    /*
    auto event = m_path.find<EventModel>();
    QDataStream s(&m_serializedStates, QIODevice::WriteOnly);
    s << event->states();
    */
}

void ClearEvent::undo()
{
    /*
    auto event = m_path.find<EventModel>();
    QList<State> states;
    QDataStream s(m_serializedStates);
    s >> states;

    for(auto&& state : states)
    {
        event->addState(std::move(state));
    }
    */
}

void ClearEvent::redo()
{
    /*
    auto event = m_path.find<EventModel>();
    event->replaceStates({});
    */
}

bool ClearEvent::mergeWith(const Command* other)
{
    return false;
}

void ClearEvent::serializeImpl(QDataStream& s) const
{
    s << m_path << m_serializedStates;
}

void ClearEvent::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_serializedStates;
}
