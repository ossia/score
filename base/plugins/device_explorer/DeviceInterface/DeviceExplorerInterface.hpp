#pragma once
#include <QString>
#include <QJsonObject>
#include <QModelIndex>
class QObject;
class DeviceExplorerModel;

namespace DeviceExplorer
{
	QString panelName();
	QString explorerName();

	// Object inside a document.
	DeviceExplorerModel* getModel(QObject* object);

	QJsonObject toJson(DeviceExplorerModel* deviceExplorer);

	QString addressFromModelIndex(const QModelIndex& index);
}
