#pragma once
#include <tools/NamedObject.hpp>

#include <QVector>
#include <QCompleter>
#include <QMenu>

class Node;
class DeviceCompleter : public QCompleter
{
	public:
		DeviceCompleter(QObject* parent);

		QString pathFromIndex(const QModelIndex& index) const;
		QStringList splitPath(const QString& path) const;
};


QMenu* nodeToQMenu(const Node* n);
QMenu* rootNodeToQMenu();

class DeviceList : public NamedObject
{
	public:
		static QStringList getDevices();
		static QStringList getAddresses(QString device);

};
