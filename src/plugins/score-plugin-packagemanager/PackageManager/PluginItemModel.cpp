// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PluginItemModel.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/tools/File.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/ssize.hpp>

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
void PackagesModel::clear()
{
  beginResetModel();
  m_vec.clear();
  endResetModel();
}

QModelIndex PackagesModel::index(int row, int column, const QModelIndex& parent) const
{
  if(row >= std::ssize(m_vec) || row < 0)
    return {};

  if(column >= ColumnCount || column < 0)
    return {};

  return createIndex(row, column, nullptr);
}

QModelIndex PackagesModel::parent(const QModelIndex& child) const
{
  return {};
}

int PackagesModel::rowCount(const QModelIndex& parent) const
{
  return m_vec.size();
}

int PackagesModel::columnCount(const QModelIndex& parent) const
{
  return ColumnCount;
}

QVariant
PackagesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  switch(role)
  {
    case Qt::DisplayRole: {
      switch((Column)section)
      {
        case Column::Name:
          return tr("Name");
          break;
        case Column::ShortDesc:
          return tr("Description");
          break;
        case Column::Version:
          return tr("Version");
          break;
        case Column::Size:
          return tr("Size");
          break;
        default:
          break;
      }

      return {};
      break;
    }
  }
  return {};
}

QVariant PackagesModel::data(const QModelIndex& index, int role) const
{
  auto row = index.row();
  auto column = (Column)index.column();
  if(row >= int(m_vec.size()) || row < 0)
    return {};

  if(index.column() >= ColumnCount || index.column() < 0)
    return {};

  const auto& addon = m_vec[row];

  switch(role)
  {
    case Qt::DisplayRole: {
      switch(column)
      {
        case Column::Name:
          return addon.name;
          break;
        case Column::ShortDesc:
          return addon.shortDescription;
          break;
        case Column::Version:
          return addon.version;
          break;
        case Column::Size:
          return addon.size;
          break;
        default:
          break;
      }

      return {};
      break;
    }

    case Qt::FontRole: {
      QFont f;
      if(column == Column::Name)
      {
        f.setBold(true);
      }
      return f;
    }

    case Qt::DecorationRole: {
      switch(column)
      {
        case Column::Name: {
          if(!addon.smallImage.isNull())
          {
            return QIcon{QPixmap::fromImage(addon.smallImage)};
          }
          return {};
        }
        default:
          return {};
      }

      break;
    } /*
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
    }*/
  }

  return {};
}

Qt::ItemFlags PackagesModel::flags(const QModelIndex& index) const
{
  return QAbstractItemModel::flags(index);

  // Qt::ItemFlags flags = Qt::ItemIsEnabled;
  // if (index.column() == 0)
  //   flags |= Qt::ItemIsUserCheckable;
  //
  // return flags;
}

LocalPackagesModel::LocalPackagesModel(const score::ApplicationContext& ctx)
{
  const auto& addons_path = ctx.settings<Library::Settings::Model>();

  QDirIterator addons{addons_path.getPackagesPath()};
  while(addons.hasNext())
  {
    registerAddon(addons.next());
  }
}

void LocalPackagesModel::registerAddon(const QString& p)
{
  QFileInfo path{p};
  if(!path.exists() || !path.isDir())
  {
    // Check for removal of an addon
  }
  else
  {
    QFile addon{p + "/package.json"};
    if(addon.open(QIODevice::ReadOnly))
    {
      auto add = Package::fromJson(
          QJsonDocument::fromJson(score::mapAsByteArray(addon)).object());
      if(add)
      {
        addAddon(*add);
      }
    }
  }
}

void PackagesModel::addAddon(Package e)
{
  auto it = ossia::find_if(m_vec, [&](auto& pkg) { return pkg.key == e.key; });
  beginResetModel();
  if(it != m_vec.end())
    *it = e;
  else
    m_vec.push_back(std::move(e));
  endResetModel();
}

void PackagesModel::removeAddon(Package e)
{
  beginResetModel();
  ossia::remove_erase_if(m_vec, [&e](auto p) { return e.name == p.name; });
  endResetModel();
}

std::optional<Package> Package::fromJson(const QJsonObject& obj) noexcept
{
  Package add;

  using Funmap = ossia::flat_map<QString, std::function<void(QJsonValue)>>;
  const Funmap funmap{
      {score::addonArchitecture(),
       [&](QJsonValue v) { add.files.push_back(v.toString()); }},
      {"file", [&](QJsonValue v) { add.files.push_back(v.toString()); }},
      {"src", [&](QJsonValue v) { add.files.push_back(v.toString()); }},
      {"files", [&](QJsonValue v) { add.files.push_back(v.toString()); }},
      {"name", [&](QJsonValue v) { add.name = v.toString(); }},
      {"raw_name", [&](QJsonValue v) { add.raw_name = v.toString(); }},
      {"version", [&](QJsonValue v) { add.version = v.toInt(); }},
      {"kind", [&](QJsonValue v) { add.kind = v.toString(); }},
      {"url", [&](QJsonValue v) { add.url = v.toString(); }},
      {"short", [&](QJsonValue v) { add.shortDescription = v.toString(); }},
      {"long", [&](QJsonValue v) { add.longDescription = v.toString(); }},
      {"small", [&](QJsonValue v) { add.smallImagePath = v.toString(); }},
      {"large", [&](QJsonValue v) { add.largeImagePath = v.toString(); }},
      {"category", [&](QJsonValue v) { add.category = v.toString(); }},
      {"size", [&](QJsonValue v) { add.size = v.toString(); }},
      {"key", [&](QJsonValue v) {
    add.key = UuidKey<score::Addon>::fromString(v.toString());
  }}};

  // Add metadata keys
  for(const auto& k : obj.keys())
  {
    auto it = funmap.find(k);
    if(it != funmap.end())
    {
      it->second(obj[k]);
    }
  }
  auto it = std::remove_if(add.raw_name.begin(), add.raw_name.end(), [](const QChar& c) {
    return !(
        c.isLetterOrNumber() || c == QChar('_') || c == QChar(' ') || c == QChar('-'));
  });
  add.raw_name.chop(std::distance(it, add.raw_name.end()));

  if(add.key.impl().is_nil() || add.name.isEmpty() || add.raw_name.isEmpty())
  {
    return std::nullopt;
  }

  return add;
}

}
