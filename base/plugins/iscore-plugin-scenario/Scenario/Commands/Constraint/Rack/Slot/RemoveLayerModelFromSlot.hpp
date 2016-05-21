#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QByteArray>

#include <iscore/tools/SettableIdentifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process { class LayerModel; }

namespace Scenario
{
class SlotModel;
namespace Command
{
/**
         * @brief The RemoveLayerFromSlot class
         *
         * Removes a process view from a slot.
         */
class RemoveLayerModelFromSlot final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveLayerModelFromSlot, "Remove a layer from a slot")
        public:

            RemoveLayerModelFromSlot(
                Path<SlotModel>&& slotPath,
                Id<Process::LayerModel> layerId);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<SlotModel> m_path;
        Id<Process::LayerModel> m_layerId {};

        QByteArray m_serializedLayerData;
};
}
}
