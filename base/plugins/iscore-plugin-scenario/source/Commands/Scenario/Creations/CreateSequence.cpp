#include "CreateSequence.hpp"

#include  <Process/ScenarioModel.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Commands/Cohesion/CreateCurveFromStates.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

#include <iostream>
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
    const auto& startMessages = flatten(scenar.state(startState).messages().rootNode());

    std::vector<iscore::FullAddressSettings> endAddresses;
    endAddresses.reserve(startMessages.size());
    std::transform(startMessages.begin(), startMessages.end(), std::back_inserter(endAddresses),
                   [] (const auto& mess) { return iscore::FullAddressSettings::make(mess); });

    auto& devPlugin = *iscore::IDocument::documentFromObject(scenario)->model().pluginModel<DeviceDocumentPlugin>();
    auto& rootNode = devPlugin.rootNode();

    auto it = endAddresses.begin();
    while(it != endAddresses.end())
    {
        auto& mess = *it;

        auto node = iscore::try_getNodeFromAddress(rootNode, mess.address);

        if(node && node->is<iscore::AddressSettings>())
        {
            devPlugin.updateProxy.refreshRemoteValue(mess.address);
            const auto& nodeImpl = node->get<iscore::AddressSettings>();
            static_cast<iscore::AddressSettingsCommon&>(mess) = static_cast<const iscore::AddressSettingsCommon&>(nodeImpl);
            ++it;
        }
        else
        {
            it = endAddresses.erase(it);
        }
    }

    QList<iscore::Message> endMessages;
    endMessages.reserve(endAddresses.size());
    std::transform(endAddresses.begin(), endAddresses.end(), std::back_inserter(endMessages),
                   [] (const auto& addr) { return iscore::Message{addr.address, addr.value}; });

    updateTreeWithMessageList(m_stateData, endMessages);

    // We also create relevant curves.
    std::vector<std::pair<const iscore::Message*, const iscore::FullAddressSettings*>> matchingMessages;
    // First we filter the messages
    for(auto& message : startMessages)
    {
        if(!message.value.val.isNumeric())
            continue;

        auto it = std::find_if(std::begin(endAddresses),
                               std::end(endAddresses),
                               [&] (const auto& arg) {
            return message.address == arg.address
                  //  && message.value.val.impl().which() == arg.value.val.impl().which()
                  // TODO this does not work because of the int -> float conversion that happens after a curve.
                    // Investigate more and comment the other uses.
                    && message.value != arg.value; });

        if(it != std::end(endAddresses))
        {
            matchingMessages.emplace_back(&message, &*it);
        }
    }

    // Then, if there are correct messages we can actually do our interpolation.
    if(!matchingMessages.empty())
    {
        // TODO refactor this with a new constructor to Path<> that takes an object identifier and an existing path.
        Path<ScenarioModel> scenarioPath{scenario};
        auto vec = scenarioPath.unsafePath().vec();
        vec.push_back({ConstraintModel::className, m_command.createdConstraint()});
        Path<ConstraintModel> constraint{ObjectPath{std::move(vec)}, Path<ConstraintModel>::UnsafeDynamicCreation{}};

        m_interpolations = InterpolateMacro{Path<ConstraintModel>{constraint}};

        // Generate brand new ids for the processes
        auto process_ids = getStrongIdRange<Process>(matchingMessages.size());
        auto layers_ids = getStrongIdRange<LayerModel>(matchingMessages.size());

        int i = 0;
        // Here we know that there is nothing yet, so we can just assign
        // ids 1, 2, 3, 4 to each process and each process view in each slot
        for(const auto& elt : matchingMessages)
        {
            std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>> layer_vect;
            for(const auto& elt : m_interpolations.slotsToUse)
            {
                layer_vect.push_back(std::make_pair(elt.first, layers_ids[i]));
            }


            auto start = iscore::convert::value<double>(elt.first->value);
            auto end = iscore::convert::value<double>(elt.second->value);
            double min = (elt.second->domain.min.val.which() != iscore::ValueType::NoValue)
                           ? std::min(iscore::convert::value<double>(elt.second->domain.min), std::min(start, end))
                           : std::min(start, end);
            double max = (elt.second->domain.max.val.which() != iscore::ValueType::NoValue)
                         ? std::max(iscore::convert::value<double>(elt.second->domain.max), std::max(start, end))
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

    if(m_interpolations.count() > 0)
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
