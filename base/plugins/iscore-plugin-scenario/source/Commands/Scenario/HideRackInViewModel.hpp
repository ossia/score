#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class ConstraintViewModel;
class ConstraintModel;
class RackModel;
#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The HideRackInViewModel class
         *
         * For a given constraint view model, hides the rack.
         * Can only be called if a rack was being displayed.
         */
        class HideRackInViewModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("HideRackInViewModel", "HideRackInViewModel")
#include <tests/helpers/FriendDeclaration.hpp>
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(HideRackInViewModel, "ScenarioControl")
                HideRackInViewModel(ObjectPath&& path);

                /**
                 * @brief HideRackInViewModel
                 * @param constraint A constraint view model.
                 *
                 * Note : this will search it and make a path from an object named "BaseConstraintModel"
                 * Hence this constructor has to be used in a Scenario.
                 */
                HideRackInViewModel(const ConstraintViewModel& constraint);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_constraintViewModelPath;

                id_type<RackModel> m_constraintPreviousId {};
        };
    }
}
