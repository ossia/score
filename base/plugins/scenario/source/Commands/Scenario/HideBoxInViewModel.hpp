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
		 * @brief The HideBoxInViewModel class
		 *
		 * For a given constraint view model, hides the box.
		 * Can only be called if a box was being displayed.
		 */
		class HideBoxInViewModel : public iscore::SerializableCommand
		{
			public:
				HideBoxInViewModel(AbstractConstraintViewModel* constraint);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_constraintViewModelPath;

				bool m_constraintPreviousId{};
		};
	}
}
