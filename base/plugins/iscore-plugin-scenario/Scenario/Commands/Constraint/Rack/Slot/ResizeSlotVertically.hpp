#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class SlotModel;
namespace Command
{
/**
         * @brief The ResizeSlotVerticallyCommand class
         *
         * Changes a slot's vertical size
         */
class ISCORE_PLUGIN_SCENARIO_EXPORT ResizeSlotVertically final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ResizeSlotVertically, "Resize a slot")
        public:
            ResizeSlotVertically(
                Path<SlotModel>&& slotPath,
                double newSize);

        void undo() const override;
        void redo() const override;

        void update(unused_t, double size)
        {
            m_newSize = size;
        }

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<SlotModel> m_path;

        double m_originalSize {};
        double m_newSize {};
};
}
}
