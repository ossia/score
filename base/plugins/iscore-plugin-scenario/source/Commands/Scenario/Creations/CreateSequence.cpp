#include "CreateSequence.hpp"

#include  <Process/ScenarioModel.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Commands/Cohesion/CreateCurveFromStates.hpp>
using namespace Scenario::Command;

CreateSequence::CreateSequence(
        const ScenarioModel& scenario,
        const Id<StateModel>& startState,
        const TimeValue& date,
        double endStateY):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_command{scenario,
              startState,
              date,
              endStateY}
{
    // TESTME

    // We get the device explorer, and we fetch the new states.
    auto& scenar = m_command.scenarioPath().find();
    const auto& startMessages = scenar.state(startState).messages().flatten();

    auto endMessages = startMessages;
    auto& devPlugin = *iscore::IDocument::documentFromObject(scenario)->model().pluginModel<DeviceDocumentPlugin>();
    auto& rootNode = devPlugin.rootNode();

    auto it = endMessages.begin();
    while(it != endMessages.end())
    {
        auto& mess = *it;

        auto node = iscore::try_getNodeFromAddress(rootNode, mess.address);

        if(node && node->is<iscore::AddressSettings>())
        {
            devPlugin.updateProxy.refreshRemoteValue(mess.address);
            mess.value = node->get<iscore::AddressSettings>().value;
            ++it;
        }
        else
        {
            it = endMessages.erase(it);
        }
    }

    updateTreeWithMessageList(m_stateData, endMessages);

    // We also create relevant curves.
    // TODO refactor this with a new constructor to Path<> that takes an object identifier and an existing path.
    Path<ScenarioModel> scenarioPath{scenario};
    auto vec = scenarioPath.unsafePath().vec();
    vec.push_back({ConstraintModel::className, m_command.createdConstraint()});
    Path<ConstraintModel> constraint{ObjectPath{std::move(vec)}, Path<ConstraintModel>::UnsafeDynamicCreation{}};

    m_interpolations = InterpolateMacro{Path<ConstraintModel>{constraint}};

    for(auto& message : startMessages)
    {
        if(!message.value.val.isNumeric())
            continue;

        auto it = std::find_if(std::begin(endMessages),
                               std::end(endMessages),
                               [&] (const iscore::Message& arg) {
            return message.address == arg.address
                    && arg.value.val.isNumeric()
                    && message.value.val.impl().which() == arg.value.val.impl().which()
                    && message.value != arg.value; });

        if(it != std::end(endMessages))
        {
            auto cmd = new CreateCurveFromStates{
                    Path<ConstraintModel>{constraint},
                    m_interpolations.slotsToUse,
                    message.address,
                    iscore::convert::value<double>(message.value),
                    iscore::convert::value<double>((*it).value)};
            m_interpolations.addCommand(cmd);
        }
    }

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

    m_interpolations.redo();
}

void CreateSequence::serializeImpl(QDataStream& s) const
{
    s << m_command.serialize() << m_interpolations.serialize() << m_stateData;
}

void CreateSequence::deserializeImpl(QDataStream& s)
{
    QByteArray command, interp;
    s >> command >> interp >> m_stateData;
    m_command.deserialize(command);
    m_interpolations.deserialize(interp);
}
