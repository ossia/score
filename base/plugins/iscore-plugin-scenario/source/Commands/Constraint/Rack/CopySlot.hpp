#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class SlotModel;
class RackModel;
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
        class CopySlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL_OBSOLETE("CopySlot", "CopySlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(CopySlot, "ScenarioControl")
                CopySlot(Path<SlotModel>&& slotToCopy,
                         Path<RackModel>&& targetRackPath);

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<SlotModel> m_slotPath;
                Path<RackModel> m_targetRackPath;

                Id<SlotModel> m_newSlotId;
        };
    }
}
