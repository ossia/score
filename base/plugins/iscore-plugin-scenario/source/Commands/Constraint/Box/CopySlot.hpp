#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>

class SlotModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The CopyProcessViewModel class
         *
         * Copy a slot, in any Box of its parent constraint.
         * The process view models are recursively copied.
         * The Slot is put at the end.
         */
        class CopySlot : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CopySlot, "ScenarioControl")
                CopySlot(ObjectPath&& slotToCopy,
                         ObjectPath&& targetBoxPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_slotPath;
                ObjectPath m_targetBoxPath;

                id_type<SlotModel> m_newSlotId;
        };
    }
}
