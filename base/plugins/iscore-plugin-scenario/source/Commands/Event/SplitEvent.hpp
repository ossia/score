#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class EventModel;
class StateModel;
namespace Scenario
{
    namespace Command
    {
        class SplitEvent : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("ScenarioControl", "SplitEvent", "SplitEvent")
            public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(SplitEvent)


            virtual void undo() override;
            virtual void redo() override;

            protected:
            virtual void serializeImpl(QDataStream&) const override;
            virtual void deserializeImpl(QDataStream&) override;

            private:
            Path<EventModel> m_oldPath;
            Path<EventModel> m_newPath;

    //       QVector<Path<StateModel> m_movingStates;
        };
    }
}
