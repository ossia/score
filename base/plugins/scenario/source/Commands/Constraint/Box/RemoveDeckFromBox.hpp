#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The RemoveDeckFromBox class
		 *
		 * Removes a deck. All the function views will be deleted.
		 */
		class RemoveDeckFromBox : public iscore::SerializableCommand
		{
			public:
				RemoveDeckFromBox(ObjectPath&& boxPath, int deckId);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;
				int m_deckId{};

				QByteArray m_serializedDeckData; // Should be done in the constructor
		};
	}
}
