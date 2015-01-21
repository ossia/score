#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
class DeckModel;
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The AddDeckToBox class
		 *
		 * Adds an empty deck to a constraint.
		 */
		class AddDeckToBox : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				AddDeckToBox();
				AddDeckToBox(ObjectPath&& boxPath);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;

				id_type<DeckModel> m_createdDeckId{};
		};
	}
}
