#pragma once

#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

class ScenarioModel;
class EventModel;
class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class MergeTimeNodes : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL_OBSOLETE("MergeTimeNodes", "MergeTimeNodes")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(MergeTimeNodes, "ScenarioControl")
                MergeTimeNodes(Path<ScenarioModel>&& path, Id<TimeNodeModel> aimedTimeNode, Id<TimeNodeModel> movingTimeNode);
                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<ScenarioModel> m_path;

                Id<TimeNodeModel> m_aimedTimeNodeId;
                Id<TimeNodeModel> m_movingTimeNodeId;

                QByteArray m_serializedTimeNode;
         };
    }
}
