#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <tools/ObjectPath.hpp>

#include <QString>

namespace Scenario
{
	namespace Command
	{
		/**
		 * @brief The ResizeDeckVerticallyCommand class
		 *
		 * Changes a deck's vertical size
		 */
		class ResizeDeckVertically : public iscore::SerializableCommand
		{
			public:
				ResizeDeckVertically();
				ResizeDeckVertically(ObjectPath&& deckPath,
											int newSize);

				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				ObjectPath m_path;

				int m_originalSize{};
				int m_newSize{};
		};
	}
}