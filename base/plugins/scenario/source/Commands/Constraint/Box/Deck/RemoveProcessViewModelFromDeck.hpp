#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The RemoveProcessViewFromDeck class
		 *
		 * Removes a process view from a deck.
		 */
		class RemoveProcessViewModelFromDeck : public iscore::SerializableCommand
		{
			public:
				RemoveProcessViewModelFromDeck(ObjectPath&& deckPath, int processViewId);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;
				int m_processViewId{};
				int m_sharedModelId{};

				QByteArray m_serializedProcessViewData;
		};
	}
}
