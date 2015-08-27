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
         * @brief The AddSlotToRack class
         *
         * Adds an empty slot at the end of a Rack.
         */
        class AddSlotToRack : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("AddSlotToRack", "AddSlotToRack")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddSlotToRack, "ScenarioControl")
                AddSlotToRack(ModelPath<RackModel>&& rackPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<RackModel> m_path;

                id_type<SlotModel> m_createdSlotId {};
        };
    }
}
