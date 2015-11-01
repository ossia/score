#include "DeviceCompleter.hpp"
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <QStringList>
#include <QApplication>

DeviceCompleter::DeviceCompleter(DeviceExplorerModel* treemodel,
                                 QObject* parent) :
    QCompleter {parent}
{
    setModel(treemodel);

    setCompletionColumn(0);
    setCompletionRole(Qt::DisplayRole);
    setCaseSensitivity(Qt::CaseInsensitive);
}

QString DeviceCompleter::pathFromIndex(const QModelIndex& index) const
{
    QString path;

    QModelIndex iter = index;

    while(iter.isValid())
    {
        auto node = static_cast<iscore::Node*>(iter.internalPointer());
        if(node && node->is<iscore::DeviceSettings>())
        {
            path = QString {"%1:/"} .arg(iter.data(0).toString()) + path;
        }
        else
        {
            path = QString {"%1/"} .arg(iter.data(0).toString()) + path;
        }

        iter = iter.parent();
    }

    return path.remove(path.length() - 1, 1);
}

QStringList DeviceCompleter::splitPath(const QString& path) const
{
    QString p2 = path;

    if(p2.at(0) == QChar('/'))
    {
        p2.remove(0, 1);
    }

    QStringList split = p2.split("/");
    split.first().remove(":");
    return split;
}
