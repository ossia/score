#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
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
        class AddSlotToRack final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddSlotToRack, "Create a slot")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                AddSlotToRack(Path<RackModel>&& rackPath);

                void undo() const override;
                void redo() const override;

                const auto& createdSlot() const
                { return m_createdSlotId; }

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<RackModel> m_path;

                Id<SlotModel> m_createdSlotId {};
        };
    }
}
