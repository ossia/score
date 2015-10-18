#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class SlotModel;
class RackModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveSlotFromRack class
         *
         * Removes a slot. All the function views will be deleted.
         */
        class RemoveSlotFromRack : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("ScenarioControl", "RemoveSlotFromRack", "RemoveSlotFromRack")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveSlotFromRack)
                RemoveSlotFromRack(Path<SlotModel>&& slotPath);
                RemoveSlotFromRack(Path<RackModel>&& rackPath, Id<SlotModel> slotId);

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<RackModel> m_path;
                Id<SlotModel> m_slotId {};
                int m_position {};

                QByteArray m_serializedSlotData; // Should be done in the constructor
        };
    }
}
