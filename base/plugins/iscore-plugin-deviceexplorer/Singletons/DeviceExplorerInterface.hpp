#pragma once
#include <QString>
#include <QJsonObject>
#include <QModelIndex>

#include <State/Message.hpp>
#include <DeviceExplorer/Node/Node.hpp>

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

    iscore::Address addressFromNode(const iscore::Node& index);
    iscore::Message messageFromNode(const iscore::Node& index);
}
