#pragma once
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Address.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <utility>
#include <vector>

class ConstraintModel;
class DataStreamInput;
class DataStreamOutput;
class LayerModel;
class Process;
class SlotModel;
template <typename Object> class Path;
template <typename tag, typename impl> class id_base_t;

class CreateCurveFromStates final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateCurveFromStates, "CreateCurveFromStates")
    public:
        CreateCurveFromStates(
                Path<ConstraintModel>&& constraint,
                const std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>>& slotList,
                const Id<Process>& curveId,
                const iscore::Address &address,
                double start,
                double end,
                double min, double max);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        AddOnlyProcessToConstraint m_addProcessCmd;
        std::vector<Scenario::Command::AddLayerModelToSlot> m_slotsCmd;

        iscore::Address m_address;

        double m_start{}, m_end{};
        double m_min{}, m_max{};

};
