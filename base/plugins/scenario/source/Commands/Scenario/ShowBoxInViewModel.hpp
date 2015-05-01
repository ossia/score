#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

class AbstractConstraintViewModel;
class ConstraintModel;
class BoxModel;
#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
    namespace Command
    {
        /**
         * @brief The ShowBoxInViewModel class
         *
         * For a given constraint view model,
         * select the box that is to be shown, and show it.
         */
        class ShowBoxInViewModel : public iscore::SerializableCommand
        {
                ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(ShowBoxInViewModel, "ScenarioControl")
                ShowBoxInViewModel(ObjectPath&& constraint_path,
                                   id_type<BoxModel> boxId);
                ShowBoxInViewModel(const AbstractConstraintViewModel* constraint,
                                   id_type<BoxModel> boxId);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_constraintViewModelPath;
                id_type<BoxModel> m_boxId {};
                id_type<BoxModel> m_previousBoxId {};

        };
    }
}
