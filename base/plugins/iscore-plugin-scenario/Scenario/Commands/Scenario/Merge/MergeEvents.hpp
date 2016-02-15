#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>
#include "MergeTimeNodes.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/StandardDisplacementPolicy.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/Algorithms/GoodOldDisplacementPolicy.hpp>

#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>


namespace Scenario
{

namespace Command
{
template < typename Scenario_T>
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeEvents : public iscore::SerializableCommand
{
    // No ISCORE_COMMAND here since it's a template.
    public:
    const CommandParentFactoryKey& parentKey() const override
    {
        return ScenarioCommandFactoryName();
    }
    static const CommandFactoryKey& static_key()
    {
        auto name = QString("MergeEvents");
        static const CommandFactoryKey kagi{std::move(name)};
        return kagi;
    }
    const CommandFactoryKey& key() const override
    {
        return static_key();
    }
    QString description() const override
    {
        return QObject::tr("Merging Events");
    }

    MergeEvents() = default;

    MergeEvents(Path<Scenario_T>&&,
               const Id<EventModel>& ,
               const Id<EventModel>& )
    {}

    void undo() const override {}
    void redo() const override {}

    void update(Path<Scenario_T> ,
            const Id<EventModel>& ,
            const Id<EventModel>& ) {}

    protected:
    void serializeImpl(DataStreamInput& s) const override
    {}

    void deserializeImpl(DataStreamOutput& s) override
    {}
};

template<>
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeEvents<ScenarioModel> : public iscore::SerializableCommand
{
    // No ISCORE_COMMAND here since it's a template.
    public:
    const CommandParentFactoryKey& parentKey() const override
    {
        return ScenarioCommandFactoryName();
    }
    static const CommandFactoryKey& static_key()
    {
        auto name = QString("MergeEvents");
        static const CommandFactoryKey kagi{std::move(name)};
        return kagi;
    }
    const CommandFactoryKey& key() const override
    {
        return static_key();
    }
    QString description() const override
    {
        return QObject::tr("Merging Events");
    }

    MergeEvents() = default;

    MergeEvents(Path<ScenarioModel>&& scenar,
               const Id<EventModel>& clickedEv,
               const Id<EventModel>& hoveredEv):
        m_scenarioPath{scenar},
        m_movingEventId{clickedEv},
        m_destinationEventId{hoveredEv}
    {
        auto& scenario = m_scenarioPath.find();
        auto& event = scenario.event(m_movingEventId);
        auto& destinantionEvent = scenario.event(m_destinationEventId);

        QByteArray arr;
        Serializer<DataStream> s{&arr};
        s.readFrom(event);
        m_serializedEvent = arr;

        m_mergeTimeNodesCommand = new MergeTimeNodes<ScenarioModel>{
                Path<ScenarioModel>{scenario},
                event.timeNode(),
                destinantionEvent.timeNode()};
    }

    void undo() const override
    {
        auto& scenar = m_scenarioPath.find();
        auto& globalEvent = scenar.event(m_destinationEventId);

        Deserializer<DataStream> s{m_serializedEvent};
        auto recreatedEvent = new EventModel{s, &scenar};

        auto states_in_event = recreatedEvent->states();
        // we remove and re-add states in recreated event
        // to ensure correct parentship between elements.
        for(auto stateId : states_in_event)
        {
            recreatedEvent->removeState(stateId);
            globalEvent.removeState(stateId);
        }
        for(auto stateId : states_in_event)
        {
            recreatedEvent->addState(stateId);
            scenar.states.at(stateId).setEventId(m_movingEventId);
        }

        scenar.events.add(recreatedEvent);

        if(recreatedEvent->timeNode() != globalEvent.timeNode())
        {
            auto& tn = scenar.timeNode(globalEvent.timeNode());
            tn.addEvent(m_movingEventId);
            m_mergeTimeNodesCommand->undo();
        }

        updateEventExtent(m_destinationEventId, scenar);

    }

    void redo() const override
    {
        auto& scenar = m_scenarioPath.find();
        auto& movingEvent = scenar.event(m_movingEventId);
        auto& destinationEvent = scenar.event(m_destinationEventId);
        auto movingStates = movingEvent.states();

        if(movingEvent.timeNode() != destinationEvent.timeNode())
            m_mergeTimeNodesCommand->redo();


        for(auto& stateId : movingStates)
        {
            movingEvent.removeState(stateId);
            destinationEvent.addState(stateId);
            scenar.states.at(stateId).setEventId(m_destinationEventId);
        }

        auto& tn = scenar.timeNode(destinationEvent.timeNode());
        tn.removeEvent(m_movingEventId);

        scenar.events.remove(m_movingEventId);
        updateEventExtent(m_destinationEventId, scenar);
    }

    void update(Path<ScenarioModel> ,
            const Id<EventModel>& ,
            const Id<EventModel>& ) {}

    protected:
    void serializeImpl(DataStreamInput& s) const override
    {
        s << m_scenarioPath << m_movingEventId << m_destinationEventId
          << m_serializedEvent << m_mergeTimeNodesCommand->serialize();
    }

    void deserializeImpl(DataStreamOutput& s) override
    {
        QByteArray cmd;

        s >> m_scenarioPath >> m_movingEventId >> m_destinationEventId
        >> m_serializedEvent >> cmd;

        m_mergeTimeNodesCommand = new MergeTimeNodes<ScenarioModel>{};
        m_mergeTimeNodesCommand->deserialize(cmd);
    }

    private:
    Path<ScenarioModel> m_scenarioPath;
    Id<EventModel> m_movingEventId;
    Id<EventModel> m_destinationEventId;

    QByteArray m_serializedEvent;
    MergeTimeNodes<ScenarioModel>* m_mergeTimeNodesCommand;
};

}
}

ISCORE_COMMAND_DECL_T(Scenario::Command::MergeEvents<Scenario::ScenarioModel>)
