// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PluginItemModel.hpp"

#include <QFont>
#include <QIcon>
#include <QPixmap>
namespace PluginSettings
{
LocalPluginItemModel::LocalPluginItemModel(
    const std::vector<score::Addon>& vec)
    : m_vec{vec}
{
}

QModelIndex LocalPluginItemModel::index(
    int row, int column, const QModelIndex& parent) const
{
  if (row >= (int)m_vec.size() || row < 0)
    return {};

  if (column >= ColumnCount || column < 0)
    return {};

  return createIndex(row, column, nullptr);
}

QModelIndex LocalPluginItemModel::parent(const QModelIndex& child) const
{
  return {};
}

int LocalPluginItemModel::rowCount(const QModelIndex& parent) const
{
  return m_vec.size();
}

int LocalPluginItemModel::columnCount(const QModelIndex& parent) const
{
  return ColumnCount;
}

QVariant LocalPluginItemModel::data(const QModelIndex& index, int role) const
{
  auto row = index.row();
  auto column = (Column)index.column();
  if (row >= m_vec.size() || row < 0)
    return {};

  if (index.column() >= ColumnCount || index.column() < 0)
    return {};

  const score::Addon& addon = m_vec[row];

  switch (role)
  {
    case Qt::DisplayRole:
    {
      switch (column)
      {
        case Column::Name:
          return addon.name;
          break;
        case Column::ShortDesc:
          return addon.shortDescription;
          break;
        case Column::Path:
          return addon.path;
          break;
        default:
          break;
      }

      return {};
      break;
    }

    case Qt::FontRole:
    {
      QFont f;
      if (column == Column::Name)
      {
        f.setBold(true);
      }
      return f;
    }

    case Qt::DecorationRole:
    {
      switch (column)
      {
        case Column::Name:
        {
          if (!addon.smallImage.isNull())
          {
            return QIcon{QPixmap::fromImage(addon.smallImage)};
          }
          return {};
        }
        default:
          return {};
      }

      break;
    }
    case Qt::CheckStateRole:
    {
      if (column == Column::Name)
      {
        return addon.enabled ? Qt::Checked : Qt::Unchecked;
      }
      else
      {
        return QVariant{};
      }
      break;
    }
  }

  return {};
}

Qt::ItemFlags LocalPluginItemModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags flags = Qt::ItemIsEnabled;
  if (index.column() == 0)
    flags |= Qt::ItemIsUserCheckable;

  return flags;
}

QModelIndex RemotePluginItemModel::index(
    int row, int column, const QModelIndex& parent) const
{
  if (row >= (int)m_vec.size() || row < 0)
    return {};

  if (column >= ColumnCount || column < 0)
    return {};

  return createIndex(row, column, nullptr);
}

QModelIndex RemotePluginItemModel::parent(const QModelIndex& child) const
{
  return {};
}

int RemotePluginItemModel::rowCount(const QModelIndex& parent) const
{
  return m_vec.size();
}

int RemotePluginItemModel::columnCount(const QModelIndex& parent) const
{
  return ColumnCount;
}

QVariant RemotePluginItemModel::data(const QModelIndex& index, int role) const
{
  auto row = index.row();
  auto column = (Column)index.column();
  if (row >= int(m_vec.size()) || row < 0)
    return {};

  if (index.column() >= ColumnCount || index.column() < 0)
    return {};

  const RemoteAddon& addon = m_vec[row];

  switch (role)
  {
    case Qt::DisplayRole:
    {
      switch (column)
      {
        case Column::Name:
          return addon.name;
          break;
        case Column::ShortDesc:
          return addon.shortDescription;
          break;
        default:
          break;
      }

      return {};
    }

    case Qt::FontRole:
    {
      QFont f;
      if (column == Column::Name)
      {
        f.setBold(true);
      }
      return f;
    }

    case Qt::DecorationRole:
    {
      switch (column)
      {
        case Column::Name:
        {
          if (!addon.smallImage.isNull())
          {
            return QIcon{QPixmap::fromImage(addon.smallImage)};
          }
          return {};
        }
        default:
          return {};
      }

      return {};
    }
    default:
      return {};
  }

  return {};
}

void RemotePluginItemModel::addAddon(RemoteAddon e)
{
  beginResetModel();
  m_vec.push_back(std::move(e));
  endResetModel();
}

void RemotePluginItemModel::clear()
{
  beginResetModel();
  m_vec.clear();
  endResetModel();
}

std::map<QString, QUrl> addonArchitectures()
{
  return {{"windows-x86", {}},
          {"windows-amd64", {}},
          {"darwin-amd64", {}},
          {"linux-amd64", {}},
          {"linux-arm", {}}};
}
}
