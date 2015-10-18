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
                ISCORE_COMMAND_DECL("ScenarioControl", "ShowRackInViewModel", "ShowRackInViewModel")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ShowRackInViewModel)
                ShowRackInViewModel(
                        Path<ConstraintViewModel>&& constraint_path,
                        Id<RackModel> rackId);
                ShowRackInViewModel(
                        const ConstraintViewModel& vm,
                        const Id<RackModel>& rackId);

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<ConstraintViewModel> m_constraintViewPath;
                Id<RackModel> m_rackId {};
                Id<RackModel> m_previousRackId {};

        };
    }
}
