#pragma once

#include <core/presenter/command/SerializableCommand.hpp>
#include <QVector>

namespace Scenario
{
	namespace Command
	{
		// TODO generalize this into a generic GroupCommand (maybe it already exists in qt?)
		class RemoveMultipleElements : public iscore::SerializableCommand
		{
			public:
				RemoveMultipleElements();
				RemoveMultipleElements(QVector<SerializableCommand*> elementsToDelete);
				virtual void undo() override;
				virtual void redo() override;
				virtual int id() const override;
				virtual bool mergeWith(const QUndoCommand* other) override;

			protected:
				virtual void serializeImpl(QDataStream&) override;
				virtual void deserializeImpl(QDataStream&) override;

			private:
				QVector<QPair<
				QPair<QString, QString>, // Meta-data
				QByteArray>>
				m_serializedCommands;
		};

	}
}
