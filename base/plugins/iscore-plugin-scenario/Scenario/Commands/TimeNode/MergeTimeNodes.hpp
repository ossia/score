#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QByteArray>

#include <iscore/tools/SettableIdentifier.hpp>

class DataStreamInput;
class DataStreamOutput;
class TimeNodeModel;
namespace Scenario {
class ScenarioModel;
}  // namespace Scenario

namespace Scenario
{
    namespace Command
    {
        class MergeTimeNodes final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), MergeTimeNodes, "Merge timenodes")
            public:
                MergeTimeNodes(Path<Scenario::ScenarioModel>&& path, Id<TimeNodeModel> aimedTimeNode, Id<TimeNodeModel> movingTimeNode);
                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<Scenario::ScenarioModel> m_path;

                Id<TimeNodeModel> m_aimedTimeNodeId;
                Id<TimeNodeModel> m_movingTimeNodeId;

                QByteArray m_serializedTimeNode;
         };
    }
}
