#pragma once

#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ObjectPath.hpp>

class EventModel;
class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class SplitTimeNode : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(SplitTimeNode, "ScenarioControl")
                SplitTimeNode(ObjectPath&& path, QVector<id_type<EventModel> > eventsInNewTimeNode);
                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

                id_type<TimeNodeModel> createdTimeNode()
                {
                    return m_newTimeNodeId;
                }

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                QVector<id_type<EventModel> > m_eventsInNewTimeNode;

                id_type<TimeNodeModel> m_originalTimeNodeId;
                id_type<TimeNodeModel> m_newTimeNodeId;
         };
    }
}
