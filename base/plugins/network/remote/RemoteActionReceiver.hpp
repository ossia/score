#pragma once
#include <string>
#include <QObject>
#include <Repartition/session/Session.h>

namespace iscore
{
	class SerializableCommand;
}

class RemoteActionReceiver : public QObject
{
		Q_OBJECT
	public:
		RemoteActionReceiver(Session*);

	signals:
		void undo();
		void redo();

		void commandReceived(QString, QString, QByteArray);

	protected:
		virtual void handle__edit_command(osc::ReceivedMessageArgumentStream);
		virtual void handle__edit_undo(osc::ReceivedMessageArgumentStream);
		virtual void handle__edit_redo(osc::ReceivedMessageArgumentStream);

		virtual Session* session() = 0;
};
