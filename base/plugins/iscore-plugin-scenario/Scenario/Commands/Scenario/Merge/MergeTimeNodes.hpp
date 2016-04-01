#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEvent.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
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
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeTimeNodes : public iscore::SerializableCommand
{
        // No ISCORE_COMMAND here since it's a template.
    public:
        const CommandParentFactoryKey& parentKey() const override
        {
            return ScenarioCommandFactoryName();
        }
        static const CommandFactoryKey& static_key()
        {
            auto name = QString("MergeTimeNodes");
            static const CommandFactoryKey kagi{std::move(name)};
            return kagi;
        }
        const CommandFactoryKey& key() const override
        {
            return static_key();
        }
        QString description() const override
        {
            return QObject::tr("Merging TimeNodes");
        }

        MergeTimeNodes() = default;

        MergeTimeNodes(Path<Scenario_T>&& scenar,
                       const Id<TimeNodeModel>& clickedTn,
                       const Id<TimeNodeModel>& hoveredTn)
        {}

        void undo() const override {}
        void redo() const override {}

        void update(Path<Scenario_T> scenar,
                    const Id<TimeNodeModel>& clickedTn,
                    const Id<TimeNodeModel>& hoveredTn) {}

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {}

        void deserializeImpl(DataStreamOutput& s) override
        {}
};

template<>
class ISCORE_PLUGIN_SCENARIO_EXPORT MergeTimeNodes<ScenarioModel> : public iscore::SerializableCommand
{
        // No ISCORE_COMMAND here since it's a template.
    public:
        const CommandParentFactoryKey& parentKey() const override
        {
            return ScenarioCommandFactoryName();
        }
        static const CommandFactoryKey& static_key()
        {
            auto name = QString("MergeTimeNodes");
            static const CommandFactoryKey kagi{std::move(name)};
            return kagi;
        }
        const CommandFactoryKey& key() const override
        {
            return static_key();
        }
        QString description() const override
        {
            return QObject::tr("Merging TimeNodes");
        }

        MergeTimeNodes() = default;

        MergeTimeNodes(Path<ScenarioModel>&& scenar,
                       const Id<TimeNodeModel>& clickedTn,
                       const Id<TimeNodeModel>& hoveredTn):
            m_scenarioPath{scenar},
            m_movingTnId{clickedTn},
            m_destinationTnId{hoveredTn}
        {
            auto& scenario = m_scenarioPath.find();
            auto& tn = scenario.timeNode(m_movingTnId);
            auto& destinantionTn = scenario.timeNode(m_destinationTnId);

            QByteArray arr;
            Serializer<DataStream> s{&arr};
            s.readFrom(tn);
            m_serializedTimeNode = arr;

            m_moveCommand = new MoveEvent<GoodOldDisplacementPolicy>{
                            Path<ScenarioModel>{scenario},
                            tn.events().front(),
                            destinantionTn.date(),
                            ExpandMode::Scale};

            m_targetTrigger = destinantionTn.trigger()->expression();
            m_targetTriggerActive = destinantionTn.trigger()->active();
        }

        void undo() const override
        {
            auto& scenar = m_scenarioPath.find();
            auto& globalTn = scenar.timeNode(m_destinationTnId);

            Deserializer<DataStream> s{m_serializedTimeNode};
            auto recreatedTn = new TimeNodeModel{s, &scenar};

            auto events_in_timenode = recreatedTn->events();
            // we remove and re-add events in recreated Tn
            // to ensure correct parentship between elements.
            for(auto evId : events_in_timenode)
            {
                recreatedTn->removeEvent(evId);
                globalTn.removeEvent(evId);
            }
            for(auto evId : events_in_timenode)
            {
                recreatedTn->addEvent(evId);
            }

            scenar.timeNodes.add(recreatedTn);

            globalTn.trigger()->setExpression(m_targetTrigger);
            globalTn.trigger()->setActive(m_targetTriggerActive);

            m_moveCommand->undo();
            updateTimeNodeExtent(m_destinationTnId, scenar);

        }
        void redo() const override
        {
            m_moveCommand->redo();

            auto& scenar = m_scenarioPath.find();
            auto& movingTn = scenar.timeNode(m_movingTnId);
            auto& destinationTn = scenar.timeNode(m_destinationTnId);

            auto movingEvents = movingTn.events();
            for(auto& evId : movingEvents)
            {
                movingTn.removeEvent(evId);
                destinationTn.addEvent(evId);
            }

            destinationTn.trigger()->setActive(destinationTn.trigger()->active() || movingTn.trigger()->active());
            destinationTn.trigger()->setExpression(movingTn.trigger()->expression());

            scenar.timeNodes.remove(m_movingTnId);
            updateTimeNodeExtent(m_destinationTnId, scenar);            
        }

        void update(Path<ScenarioModel> scenar,
                    const Id<TimeNodeModel>& clickedTn,
                    const Id<TimeNodeModel>& hoveredTn) {}

    protected:
        void serializeImpl(DataStreamInput& s) const override
        {
            s << m_scenarioPath << m_movingTnId << m_destinationTnId
              << m_serializedTimeNode << m_moveCommand->serialize();
        }

        void deserializeImpl(DataStreamOutput& s) override
        {
            QByteArray cmd;

            s >> m_scenarioPath >> m_movingTnId >> m_destinationTnId
                >> m_serializedTimeNode >> cmd;

            m_moveCommand = new MoveEvent<GoodOldDisplacementPolicy>{};
            m_moveCommand->deserialize(cmd);
        }

    private:
        Path<ScenarioModel> m_scenarioPath;
        Id<TimeNodeModel> m_movingTnId;
        Id<TimeNodeModel> m_destinationTnId;

        QByteArray m_serializedTimeNode;
        MoveEvent<GoodOldDisplacementPolicy>* m_moveCommand;
        State::Trigger m_targetTrigger;
        bool m_targetTriggerActive;
};

}
}

ISCORE_COMMAND_DECL_T(Scenario::Command::MergeTimeNodes<Scenario::ScenarioModel>)
