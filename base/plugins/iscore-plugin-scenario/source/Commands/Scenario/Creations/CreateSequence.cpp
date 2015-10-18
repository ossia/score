#include "CreateSequence.hpp"

#include  <Process/ScenarioModel.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
using namespace Scenario::Command;

CreateSequence::CreateSequence(
        const ScenarioModel& scenario,
        const Id<StateModel>& startState,
        const TimeValue& date,
        double endStateY):
    iscore::SerializableCommand{"ScenarioControl", commandName(), description()},
    m_command{scenario,
              startState,
              date,
              endStateY}
{
    // TESTME

    // We get the device explorer, and we fetch the new states.
    auto& scenar = m_command.scenarioPath().find();
    auto messages = scenar.state(startState).messages().flatten();

    const auto& rootNode = iscore::IDocument::documentFromObject(scenario)->model().pluginModel<DeviceDocumentPlugin>()->rootNode();
    auto it = messages.begin();
    while(it != messages.end())
    {
        auto& mess = *it;
        auto node = iscore::try_getNodeFromAddress(rootNode, mess.address);
        if(node && node->is<iscore::AddressSettings>())
        {
            mess.value = node->get<iscore::AddressSettings>().value;
            ++it;
        }
        else
        {
            it = messages.erase(it);
        }
    }

    updateTreeWithMessageList(m_stateData, messages);
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

void CreateSequence::undo() const
{
    m_command.undo();
}

void CreateSequence::redo() const
{
    m_command.redo();

    auto& scenar = m_command.scenarioPath().find();
    auto& endstate = scenar.state(m_command.createdState());

    endstate.messages() = m_stateData;
}

void CreateSequence::serializeImpl(QDataStream& s) const
{
    s << m_command.serialize() << m_stateData;
}

void CreateSequence::deserializeImpl(QDataStream& s)
{
    QByteArray b;
    s >> b >> m_stateData;
    m_command.deserialize(b);
}
