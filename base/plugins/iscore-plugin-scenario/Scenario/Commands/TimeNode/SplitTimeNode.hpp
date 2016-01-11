#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QVector>

#include <iscore/tools/SettableIdentifier.hpp>

class DataStreamInput;
class DataStreamOutput;

namespace Scenario
{
class EventModel;
class TimeNodeModel;
namespace Command
{
class SplitTimeNode final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SplitTimeNode, "Split a timenode")
        public:
            SplitTimeNode(
                Path<TimeNodeModel>&& path,
                QVector<Id<EventModel> > eventsInNewTimeNode);
        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<TimeNodeModel> m_path;
        QVector<Id<EventModel> > m_eventsInNewTimeNode;

        Id<TimeNodeModel> m_originalTimeNodeId;
        Id<TimeNodeModel> m_newTimeNodeId;
};
}
}
