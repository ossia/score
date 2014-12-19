#pragma once

#include <core/presenter/command/SerializableCommand.hpp>
#include <QVector>

// TODO generalize this into a Command Group
class DeleteMultipleElementsCommand : public iscore::SerializableCommand
{
	public:
		DeleteMultipleElementsCommand(QVector<SerializableCommand*> elementsToDelete);
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
