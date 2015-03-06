#pragma once
#include <QString>
#include <QJsonObject>
#include <QModelIndex>
class QObject;
class DeviceExplorerModel;
namespace iscore
{
    class Document;
}

namespace DeviceExplorer
{
    QString panelName();
    QString explorerName();

    // Object inside a document.
    DeviceExplorerModel* getModel(QObject* object);
    DeviceExplorerModel* getModel(iscore::Document* object);

    QJsonObject toJson(DeviceExplorerModel* deviceExplorer);

    QString addressFromModelIndex(const QModelIndex& index);
}
