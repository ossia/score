// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PluginItemModel.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>

namespace PM
{
LocalPackagesModel::LocalPackagesModel(
    const score::ApplicationContext& ctx)
{
  auto registerAddon = [this](const QString& p) {
    QFileInfo path{p};
    if (!path.exists() || !path.isDir())
    {
      // Check for removal of an addon
    }
    else
    {
      QFile addon{p + "/addon.json"};
      if (addon.open(QIODevice::ReadOnly))
      {
        auto add = RemotePackage::fromJson(
            QJsonDocument::fromJson(addon.readAll()).object());
        if (add)
        {
          beginResetModel();
          m_vec.push_back(std::move(*add));
          endResetModel();
        }
      }
    }
  };

  const QString addons_path
      = ctx.settings<Library::Settings::Model>().getPath() + "/Addons";
  con(m_addonsWatch,
      &QFileSystemWatcher::directoryChanged,
      this,
      registerAddon);

  QDirIterator addons{addons_path};
  while (addons.hasNext())
  {
    registerAddon(addons.next());
  }
}

QModelIndex LocalPackagesModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
  if (row >= (int)m_vec.size() || row < 0)
    return {};

  if (column >= ColumnCount || column < 0)
    return {};

  return createIndex(row, column, nullptr);
}

QModelIndex LocalPackagesModel::parent(const QModelIndex& child) const
{
  return {};
}

int LocalPackagesModel::rowCount(const QModelIndex& parent) const
{
  return m_vec.size();
}

int LocalPackagesModel::columnCount(const QModelIndex& parent) const
{
  return ColumnCount;
}

QVariant LocalPackagesModel::data(const QModelIndex& index, int role) const
{
  auto row = index.row();
  auto column = (Column)index.column();
  if (row >= int(m_vec.size()) || row < 0)
    return {};

  if (index.column() >= ColumnCount || index.column() < 0)
    return {};

  const auto& addon = m_vec[row];

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

Qt::ItemFlags LocalPackagesModel::flags(const QModelIndex& index) const
{
  Qt::ItemFlags flags = Qt::ItemIsEnabled;
  if (index.column() == 0)
    flags |= Qt::ItemIsUserCheckable;

  return flags;
}

QModelIndex RemotePackagesModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
  if (row >= (int)m_vec.size() || row < 0)
    return {};

  if (column >= ColumnCount || column < 0)
    return {};

  return createIndex(row, column, nullptr);
}

QModelIndex RemotePackagesModel::parent(const QModelIndex& child) const
{
  return {};
}

int RemotePackagesModel::rowCount(const QModelIndex& parent) const
{
  return m_vec.size();
}

int RemotePackagesModel::columnCount(const QModelIndex& parent) const
{
  return ColumnCount;
}

QVariant RemotePackagesModel::data(const QModelIndex& index, int role) const
{
  auto row = index.row();
  auto column = (Column)index.column();
  if (row >= int(m_vec.size()) || row < 0)
    return {};

  if (index.column() >= ColumnCount || index.column() < 0)
    return {};

  const RemotePackage& addon = m_vec[row];

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

void RemotePackagesModel::addAddon(RemotePackage e)
{
  beginResetModel();
  m_vec.push_back(std::move(e));
  endResetModel();
}

void RemotePackagesModel::clear()
{
  beginResetModel();
  m_vec.clear();
  endResetModel();
}

std::optional<RemotePackage>
RemotePackage::fromJson(const QJsonObject& obj) noexcept
{
  RemotePackage add;

  using Funmap = ossia::flat_map<QString, std::function<void(QJsonValue)>>;
  const Funmap funmap{
      {"file", [&](QJsonValue v) { add.file = v.toString(); }},
      {"name", [&](QJsonValue v) { add.name = v.toString(); }},
      {"raw_name", [&](QJsonValue v) { add.raw_name = v.toString(); }},
      {"version", [&](QJsonValue v) { add.version = v.toString(); }},
      {"kind", [&](QJsonValue v) { add.kind = v.toString(); }},
      {"url", [&](QJsonValue v) { add.url = v.toString(); }},
      {"short", [&](QJsonValue v) { add.shortDescription = v.toString(); }},
      {"long", [&](QJsonValue v) { add.longDescription = v.toString(); }},
      {"small", [&](QJsonValue v) { add.smallImagePath = v.toString(); }},
      {"large", [&](QJsonValue v) { add.largeImagePath = v.toString(); }},
      {"key", [&](QJsonValue v) {
         add.key = UuidKey<score::Addon>::fromString(v.toString());
       }}};

  // Add metadata keys
  for (const auto& k : obj.keys())
  {
    auto it = funmap.find(k);
    if (it != funmap.end())
    {
      it->second(obj[k]);
    }
  }

  if (add.key.impl().is_nil() || add.name.isEmpty())
  {
    return std::nullopt;
  }

  return add;
}

}
