#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class RackModel;
namespace Command
{
class DuplicateRack final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), DuplicateRack, "Duplicate a rack")
        public:
            DuplicateRack(const RackModel&);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<RackModel> m_rackPath;

        Id<RackModel> m_newRackId;
};
}
}
