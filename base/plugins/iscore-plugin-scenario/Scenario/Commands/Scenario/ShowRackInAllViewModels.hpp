#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>

class ConstraintModel;
class RackModel;
class ConstraintViewModel;

class ShowRackInAllViewModels : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(
                ScenarioCommandFactoryName(),
                ShowRackInAllViewModels,
                "ShowRackInAllViewModels")
    public:
        ShowRackInAllViewModels(
                Path<ConstraintModel>&& constraint_path,
                const Id<RackModel>& rackId);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<ConstraintModel> m_constraintPath;
        Id<RackModel> m_rackId {};

        QMap<Id<ConstraintViewModel>, Id<RackModel>> m_previousRacks;

};
