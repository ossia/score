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
namespace Process { class ProcessModel; }

namespace Scenario
{
class ConstraintModel;
class RackModel;
class SlotModel;
namespace Command
{
/**
        * @brief The AddLayerInNewSlot class
        */
class AddLayerInNewSlot final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), AddLayerInNewSlot, "Add a new layer")
        public:
            AddLayerInNewSlot(
                Path<ConstraintModel>&& constraintPath,
                Id<Process::ProcessModel> process); // maybe should we pass the viewmodel too, if many available ?

        void undo() const override;
        void redo() const override;

        Id<Process::ProcessModel> processId() const
        {
            return m_processId;
        }

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<ConstraintModel> m_path;

        bool m_existingRack {};

        Id<Process::ProcessModel> m_processId {};
        Id<RackModel> m_createdRackId {};
        Id<SlotModel> m_createdSlotId {};
        Id<Process::LayerModel> m_createdLayerId {};
        Id<Process::ProcessModel> m_sharedProcessModelId {};

        QByteArray m_processData;
};
}
}
