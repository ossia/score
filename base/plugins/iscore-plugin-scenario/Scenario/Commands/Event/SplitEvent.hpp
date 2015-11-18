#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class EventModel;
class StateModel;
class ScenarioModel;

namespace Scenario
{
    namespace Command
    {
        class SplitEvent final : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SplitEvent, "SplitEvent")
            public:

            SplitEvent(
                const Path<ScenarioModel>& scenario,
                const Id<EventModel>& event,
                const QVector<Id<StateModel>>& movingstates);

            void undo() const override;
            void redo() const override;

            protected:
            void serializeImpl(DataStreamInput&) const override;
            void deserializeImpl(DataStreamOutput&) override;

            private:
            Path<ScenarioModel> m_scenarioPath;

            Id<EventModel> m_originalEvent;
            Id<EventModel> m_newEvent;
            QString m_createdName;
            QVector<Id<StateModel>> m_movingStates;
        };
    }
}
