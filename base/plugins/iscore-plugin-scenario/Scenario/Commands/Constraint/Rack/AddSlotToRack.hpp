#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
class DataStreamInput;
class DataStreamOutput;

namespace Scenario
{
class RackModel;
class SlotModel;

namespace Command
{
/**
         * @brief The AddSlotToRack class
         *
         * Adds an empty slot at the end of a Rack.
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT AddSlotToRack final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddSlotToRack, "Create a slot")
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
