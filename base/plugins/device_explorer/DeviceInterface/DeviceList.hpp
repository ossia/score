#pragma once
#include <tools/NamedObject.hpp>

#include <QVector>
#include <QCompleter>

class DeviceCompleter : public QCompleter
{
	public:
		DeviceCompleter(QObject* parent);

		QString pathFromIndex(const QModelIndex& index) const;
		QStringList splitPath(const QString& path) const;
};

class DeviceList : public NamedObject
{
	public:
		static QStringList getDevices();
		static QStringList getAddresses(QString device);

};
