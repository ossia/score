#include "CreateSequence.hpp"

//#include <Document/State/StateModel.hpp>

#include  <Process/ScenarioModel.hpp>

using namespace Scenario::Command;

CreateSequence::CreateSequence(
        const ScenarioModel& scenario,
        const Id<StateModel>& startState,
        const TimeValue& date,
        double endStateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_originalState{startState},
    m_command{scenario,
              startState,
              date,
              endStateY}
{

}

CreateSequence::CreateSequence(
        const Path<ScenarioModel>& scenarioPath,
        const Id<StateModel>& startState,
        const TimeValue& date,
        double endStateY):
    CreateSequence{scenarioPath.find(),
                   startState, date, endStateY}
{

}

void CreateSequence::undo()
{
    m_command.undo();
}

void CreateSequence::redo()
{
    m_command.redo();

    auto& scenar = m_command.scenarioPath().find();
    auto& firststate = scenar.state(m_originalState);
    auto& endstate = scenar.state(m_command.createdState());

    endstate.messages() = firststate.messages();

}

void CreateSequence::serializeImpl(QDataStream& s) const
{
    s << m_command.serialize();
}

void CreateSequence::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> b;
    m_command.deserialize(b);
}
