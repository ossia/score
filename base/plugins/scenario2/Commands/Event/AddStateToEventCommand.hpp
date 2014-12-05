#pragma once
#include <core/presenter/command/SerializableCommand.hpp>
#include <QNamedObject>

class AddStateToEventCommand : public iscore::SerializableCommand
{
	public:
		AddStateToEventCommand();
		AddStateToEventCommand(ObjectPath&& eventPath, QString message);
		virtual void undo() override;
		virtual void redo() override;
		virtual int id() const override;
		virtual bool mergeWith(const QUndoCommand* other) override;

	protected:
		virtual void serializeImpl(QDataStream&) override;
		virtual void deserializeImpl(QDataStream&) override;

	private:
		ObjectPath m_path;
		QString m_message;
};
