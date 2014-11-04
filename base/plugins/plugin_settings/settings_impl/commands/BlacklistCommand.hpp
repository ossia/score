#pragma once

#include <core/presenter/command/Command.hpp>
#include <QMap>
#include <QString>
class BlacklistCommand : public iscore::Command
{
		// QUndoCommand interface
	public:
		BlacklistCommand(QString name, bool value);

		virtual void undo();
		virtual void redo();
		virtual bool mergeWith(const QUndoCommand* other);

		// Command interface
	public:
		virtual void deserialize(QByteArray);

		QMap<QString, bool> m_blacklistedState;
};
