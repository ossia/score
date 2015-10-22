#pragma once
#include <Commands/ScenarioCommandFactory.hpp>
#include <Commands/Constraint/AddProcessToConstraint.hpp>
#include <Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <State/Address.hpp>

class ConstraintModel;

class CreateCurveFromStates : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(), CreateCurveFromStates, "CreateCurveFromStates")
    public:
        CreateCurveFromStates(
                Path<ConstraintModel>&& constraint,
                const std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>>& slotList,
                const Id<Process>& curveId,
                const iscore::Address &address,
                double start,
                double end);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        AddOnlyProcessToConstraint m_addProcessCmd;
        std::vector<Scenario::Command::AddLayerModelToSlot> m_slotsCmd;

        iscore::Address m_address;

        double m_start{}, m_end{};

};
