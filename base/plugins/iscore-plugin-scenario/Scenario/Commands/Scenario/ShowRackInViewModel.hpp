#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class ConstraintViewModel;
class ConstraintModel;
class RackModel;
#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The ShowRackInViewModel class
         *
         * For a given constraint view model,
         * select the rack that is to be shown, and show it.
         */
        class ShowRackInViewModel final : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ShowRackInViewModel, "ShowRackInViewModel")
            public:
                ShowRackInViewModel(
                        Path<ConstraintViewModel>&& constraint_path,
                        const Id<RackModel>& rackId);
                ShowRackInViewModel(
                        const ConstraintViewModel& vm,
                        const Id<RackModel>& rackId);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<ConstraintViewModel> m_constraintViewPath;
                Id<RackModel> m_rackId {};
                Id<RackModel> m_previousRackId {};

        };
    }
}
