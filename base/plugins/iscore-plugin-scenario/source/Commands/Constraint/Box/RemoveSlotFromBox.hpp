#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class SlotModel;
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The RemoveSlotFromBox class
         *
         * Removes a slot. All the function views will be deleted.
         */
        class RemoveSlotFromBox : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveSlotFromBox, "ScenarioControl")
                RemoveSlotFromBox(ObjectPath&& slotPath);
                RemoveSlotFromBox(ObjectPath&& boxPath, id_type<SlotModel> slotId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;
                id_type<SlotModel> m_slotId {};
                int m_position {};

                QByteArray m_serializedSlotData; // Should be done in the constructor
        };
    }
}
