#pragma once
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
        class ShowRackInViewModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("ShowRackInViewModel", "ShowRackInViewModel")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ShowRackInViewModel, "ScenarioControl")
                ShowRackInViewModel(
                        ModelPath<ConstraintViewModel>&& constraint_path,
                        id_type<RackModel> rackId);
                ShowRackInViewModel(
                        const ConstraintViewModel& vm,
                        const id_type<RackModel>& rackId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<ConstraintViewModel> m_constraintViewModelPath;
                id_type<RackModel> m_rackId {};
                id_type<RackModel> m_previousRackId {};

        };
    }
}
