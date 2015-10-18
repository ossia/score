#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class EventModel;
class StateModel;
class ScenarioModel;

namespace Scenario
{
    namespace Command
    {
        class SplitEvent : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("ScenarioControl", "SplitEvent", "SplitEvent")
            public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SplitEvent)

            SplitEvent(
                const Path<ScenarioModel>& scenario,
                const Id<EventModel>& event,
                const QVector<Id<StateModel>>& movingstates);

            void undo() const override;
            void redo() const override;

            protected:
            virtual void serializeImpl(QDataStream&) const override;
            virtual void deserializeImpl(QDataStream&) override;

            private:
            Path<ScenarioModel> m_scenarioPath;

            Id<EventModel> m_originalEvent;
            Id<EventModel> m_newEvent;
            QString m_createdName;
            QVector<Id<StateModel>> m_movingStates;
        };
    }
}
