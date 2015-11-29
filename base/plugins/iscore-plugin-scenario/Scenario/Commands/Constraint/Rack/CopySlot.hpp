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
        /**
         * @brief The CopyLayerModel class
         *
         * Copy a slot, in any Rack of its parent constraint.
         * The process view models are recursively copied.
         * The Slot is put at the end.
         */
        class CopySlot final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CopySlot, "Copy a slot")
            public:
                CopySlot(Path<SlotModel>&& slotToCopy,
                         Path<RackModel>&& targetRackPath);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<SlotModel> m_slotPath;
                Path<RackModel> m_targetRackPath;

                Id<SlotModel> m_newSlotId;
        };
    }
}
