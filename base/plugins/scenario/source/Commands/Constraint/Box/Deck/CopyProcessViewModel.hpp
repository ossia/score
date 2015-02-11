#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class ProcessViewModelInterface;
class ProcessSharedModelInterface;
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The CopyProcessViewModel class
		 *
		 * Copy a process view from a Deck to another.
		 * Note : this must be in the same constraint.
		 */
		class CopyProcessViewModel : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				CopyProcessViewModel();
				CopyProcessViewModel(ObjectPath&& pvmToCopy,
									 ObjectPath&& targetDeck);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_pvmPath;
				ObjectPath m_targetDeckPath;

				id_type<ProcessViewModelInterface> m_newProcessViewModelId;
		};
	}
}
