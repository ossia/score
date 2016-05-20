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
class SlotModel;
namespace Command
{
class SwapSlots final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapSlots, "Swap slots")
        public:
            SwapSlots(
                Path<RackModel>&& rack,
                Id<SlotModel> first,
                Id<SlotModel> second);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<RackModel> m_rackPath;
        Id<SlotModel> m_first, m_second;
};
}
}
