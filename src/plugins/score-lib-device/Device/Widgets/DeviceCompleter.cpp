// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceCompleter.hpp"

#include <Device/ItemModels/NodeBasedItemModel.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <score/model/tree/TreeNode.hpp>

#include <QChar>
#include <qnamespace.h>

class QObject;

namespace Device
{
DeviceCompleter::DeviceCompleter(Device::NodeBasedItemModel& treemodel, QObject* parent)
    : QCompleter{parent}
{
  setModel(&treemodel);

  setCompletionColumn(0);
  setCompletionRole(Qt::DisplayRole);
  setCaseSensitivity(Qt::CaseInsensitive);
}

QString DeviceCompleter::pathFromIndex(const QModelIndex& index) const
{
  QString path;

  QModelIndex iter = index;

  while (iter.isValid())
  {
    auto node = static_cast<Device::Node*>(iter.internalPointer());
    if (node && node->is<Device::DeviceSettings>())
    {
      path = QString{"%1:/"}.arg(iter.data(0).toString()) + path;
    }
    else
    {
      path = QString{"%1/"}.arg(iter.data(0).toString()) + path;
    }

    iter = iter.parent();
  }

  return path.remove(path.length() - 1, 1);
}

QStringList DeviceCompleter::splitPath(const QString& path) const
{
  QString p2 = path;

  if (p2.at(0) == QChar('/'))
  {
    p2.remove(0, 1);
  }

  QStringList split = p2.split("/");
  split.first().remove(":");
  return split;
}
}
