#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>

class ScenarioModel;
class EventModel;
class TimeNodeModel;

namespace Scenario
{
    namespace Command
    {
        class MergeTimeNodes final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MergeTimeNodes, "MergeTimeNodes")
            public:
                MergeTimeNodes(Path<ScenarioModel>&& path, Id<TimeNodeModel> aimedTimeNode, Id<TimeNodeModel> movingTimeNode);
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<ScenarioModel> m_path;

                Id<TimeNodeModel> m_aimedTimeNodeId;
                Id<TimeNodeModel> m_movingTimeNodeId;

                QByteArray m_serializedTimeNode;
         };
    }
}
