#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <tests/helpers/ForwardDeclaration.hpp>
namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The RemoveBoxFromConstraint class
		 *
		 * Removes a box : all the decks and function views will be removed.
		 */
		class RemoveBoxFromConstraint : public iscore::SerializableCommand
		{
#include <tests/helpers/FriendDeclaration.hpp>
			public:
				RemoveBoxFromConstraint(ObjectPath&& constraintPath, int boxId);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;
				int m_boxId{};

				QByteArray m_serializedBoxData; // Should be done in the constructor
		};
	}
}
