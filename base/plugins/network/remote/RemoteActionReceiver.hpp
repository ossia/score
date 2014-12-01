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
		void receivedCommand(QString, QString, QByteArray);
		void undo();
		void redo();

	public slots:
		void applyCommand(iscore::SerializableCommand*);

	protected:
		virtual void handle__edit_command(osc::ReceivedMessageArgumentStream);
		virtual void handle__edit_undo(osc::ReceivedMessageArgumentStream);
		virtual void handle__edit_redo(osc::ReceivedMessageArgumentStream);

		virtual Session* session() = 0;
};
