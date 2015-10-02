#pragma once
#include <QString>
#include <QJsonObject>
#include <QModelIndex>

#include <State/Message.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

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

    iscore::Address addressFromModelIndex(const QModelIndex& index);
    iscore::Message messageFromModelIndex(const QModelIndex& index);
}
