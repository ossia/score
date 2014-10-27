#pragma once
#include <string>
#include <QObject>

namespace iscore
{
	class Command;
}

class RemoteActionReceiver : public QObject
{
		Q_OBJECT
	public:
		using QObject::QObject;
		virtual void onReceive(std::string, std::string, const char*, int) = 0;

	signals:
		void receivedCommand(QString, QString, QByteArray);

	public slots:
		virtual void applyCommand(iscore::Command*) = 0;
};
