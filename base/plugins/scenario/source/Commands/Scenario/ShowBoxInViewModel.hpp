#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

class AbstractConstraintViewModel;
class ConstraintModel;
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
			public:
				ShowBoxInViewModel(AbstractConstraintViewModel* constraint,
								   int boxId);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_constraintViewModelPath;
				int m_boxId{};

				bool m_constraintPreviousId{};
				int m_constraintPreviousState{};

		};
	}
}
