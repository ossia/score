#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

class ConstraintModel;
class RackModel;
class ConstraintViewModel;

class ShowRackInAllViewModels final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ShowRackInAllViewModels, "Show a rack everywhere")
    public:
        ShowRackInAllViewModels(
                Path<ConstraintModel>&& constraint_path,
                const Id<RackModel>& rackId);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<ConstraintModel> m_constraintPath;
        Id<RackModel> m_rackId {};

        QMap<Id<ConstraintViewModel>, Id<RackModel>> m_previousRacks;

};
