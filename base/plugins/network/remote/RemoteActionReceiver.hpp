#pragma once
#include <string>
#include <QObject>
#include <Repartition/session/Session.h>

namespace iscore
{
	class Command;
}

class RemoteActionReceiver : public QObject
{
		Q_OBJECT
	public:
		RemoteActionReceiver(Session*);

	signals:
		void receivedCommand(QString, QString, QByteArray);

	public slots:
		void applyCommand(iscore::Command*);

	protected:
		virtual void handle__edit_command(osc::ReceivedMessageArgumentStream);
		virtual void handle__edit_undo(osc::ReceivedMessageArgumentStream);
		virtual void handle__edit_redo(osc::ReceivedMessageArgumentStream);

		void undo();
		void redo();

		virtual Session* session() = 0;
};
