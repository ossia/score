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
         * @brief The AddSlotToRack class
         *
         * Adds an empty slot at the end of a Rack.
         */
        class AddSlotToRack : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddSlotToRack, "ScenarioControl")
                AddSlotToRack(ObjectPath&& rackPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_path;

                id_type<SlotModel> m_createdSlotId {};
        };
    }
}
