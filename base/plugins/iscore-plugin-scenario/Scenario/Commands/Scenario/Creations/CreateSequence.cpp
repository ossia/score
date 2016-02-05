#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Commands/Cohesion/CreateCurveFromStates.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include  <Scenario/Process/ScenarioModel.hpp>

#include <boost/optional/optional.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>
#include <QtGlobal>
#include <QList>
#include <QString>
#include <algorithm>
#include <iterator>
#include <list>
#include <utility>
#include <vector>

#include "CreateSequence.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/Domain.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Process/State/MessageNode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Cohesion/InterpolateMacro.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Process { class LayerModel; }
namespace Process { class ProcessModel; }
class SlotModel;


namespace Scenario
{
namespace Command
{
CreateSequence::CreateSequence(
        const Scenario::ScenarioModel& scenario,
        const Id<StateModel>& startState,
        const TimeValue& date,
        double endStateY):
    m_command{scenario,
              startState,
              date,
              endStateY}
{
    // TESTME

    // We get the device explorer, and we fetch the new states.
    auto& scenar = m_command.scenarioPath().find();
    const auto& startMessages = flatten(scenar.state(startState).messages().rootNode());

    std::vector<Device::FullAddressSettings> endAddresses;
    endAddresses.reserve(startMessages.size());
    std::transform(startMessages.begin(), startMessages.end(), std::back_inserter(endAddresses),
                   [] (const auto& mess) { return Device::FullAddressSettings::make(mess); });

    auto& devPlugin = iscore::IDocument::documentContext(scenario).plugin<Explorer::DeviceDocumentPlugin>();
    auto& rootNode = devPlugin.rootNode();

    auto it = endAddresses.begin();
    while(it != endAddresses.end())
    {
        auto& mess = *it;

        auto node = Device::try_getNodeFromAddress(rootNode, mess.address);

        if(node && node->is<Device::AddressSettings>())
        {
            devPlugin.updateProxy.refreshRemoteValue(mess.address);
            const auto& nodeImpl = node->get<Device::AddressSettings>();
            static_cast<Device::AddressSettingsCommon&>(mess) = static_cast<const Device::AddressSettingsCommon&>(nodeImpl);
            ++it;
        }
        else
        {
            it = endAddresses.erase(it);
        }
    }

    QList<State::Message> endMessages;
    endMessages.reserve(endAddresses.size());
    std::transform(endAddresses.begin(), endAddresses.end(), std::back_inserter(endMessages),
                   [] (const auto& addr) { return State::Message{addr.address, addr.value}; });

    updateTreeWithMessageList(m_stateData, endMessages);

    // We also create relevant curves.
    std::vector<std::pair<const State::Message*, const Device::FullAddressSettings*>> matchingMessages;
    // First we filter the messages
    for(auto& message : startMessages)
    {
        if(!message.value.val.isNumeric())
            continue;

        auto addr_it = std::find_if(std::begin(endAddresses),
                                    std::end(endAddresses),
                                    [&] (const auto& arg) {
            return message.address == arg.address
                  //  && message.value.val.impl().which() == arg.value.val.impl().which()
                  // TODO this does not work because of the int -> float conversion that happens after a curve.
                    // Investigate more and comment the other uses.
                    && message.value != arg.value; });

        if(addr_it != std::end(endAddresses))
        {
            matchingMessages.emplace_back(&message, &*addr_it);
        }
    }

    // Then, if there are correct messages we can actually do our interpolation.
    if(!matchingMessages.empty())
    {
        m_addedProcessCount = matchingMessages.size();
        auto constraint = Path<Scenario::ScenarioModel>{scenario}.extend(m_command.createdConstraint());

        {
            AddMultipleProcessesToConstraintMacro interpolateMacro{Path<ConstraintModel>{constraint}};
            m_interpolations.slotsToUse = interpolateMacro.slotsToUse;
            m_interpolations.commands() = interpolateMacro.commands();
        }

        // Generate brand new ids for the processes
        auto process_ids = getStrongIdRange<Process::ProcessModel>(matchingMessages.size());
        auto layers_ids = getStrongIdRange<Process::LayerModel>(matchingMessages.size());

        int i = 0;
        // Here we know that there is nothing yet, so we can just assign
        // ids 1, 2, 3, 4 to each process and each process view in each slot
        for(const auto& elt : matchingMessages)
        {
            std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel>>> layer_vect;
            for(const auto& slots_elt : m_interpolations.slotsToUse)
            {
                layer_vect.push_back(std::make_pair(slots_elt.first, layers_ids[i]));
            }


            auto start = State::convert::value<double>(elt.first->value);
            auto end = State::convert::value<double>(elt.second->value);
            double min = (elt.second->domain.min.val.which() != State::ValueType::NoValue)
                           ? std::min(State::convert::value<double>(elt.second->domain.min), std::min(start, end))
                           : std::min(start, end);
            double max = (elt.second->domain.max.val.which() != State::ValueType::NoValue)
                         ? std::max(State::convert::value<double>(elt.second->domain.max), std::max(start, end))
                         : std::max(start, end);

            auto cmd = new CreateCurveFromStates{
                       Path<ConstraintModel>{constraint},
                       layer_vect,
                       process_ids[i],
                       elt.first->address, start, end, min, max};
            m_interpolations.addCommand(cmd);
            i++;
        }
    }

}

CreateSequence::CreateSequence(
        const Path<Scenario::ScenarioModel>& scenarioPath,
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

    if(m_addedProcessCount > 0)
        m_interpolations.redo();
}

void CreateSequence::serializeImpl(DataStreamInput& s) const
{
    s << m_command.serialize()
      << m_interpolations.serialize()
      << m_stateData
      << m_addedProcessCount;
}

void CreateSequence::deserializeImpl(DataStreamOutput& s)
{
    QByteArray command, interp;
    s >> command
      >> interp
      >> m_stateData
      >> m_addedProcessCount;
    m_command.deserialize(command);
    m_interpolations.deserialize(interp);
}
}
}
