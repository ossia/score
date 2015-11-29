#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "iscore/tools/SettableIdentifier.hpp"

class DataStreamInput;
class DataStreamOutput;
class RackModel;
class SlotModel;

namespace Scenario
{
    namespace Command
    {
        class SwapSlots final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), SwapSlots, "Swap slots")
            public:
                SwapSlots(
                    Path<RackModel>&& rack,
                    const Id<SlotModel>& first,
                    const Id<SlotModel>& second);

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
