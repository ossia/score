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
                ISCORE_COMMAND_DECL("CopySlot", "CopySlot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CopySlot, "ScenarioControl")
                CopySlot(ModelPath<SlotModel>&& slotToCopy,
                         ModelPath<RackModel>&& targetRackPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<SlotModel> m_slotPath;
                ModelPath<RackModel> m_targetRackPath;

                id_type<SlotModel> m_newSlotId;
        };
    }
}
