#pragma once
#include <score/plugins/Addon.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/optional.hpp>

#include <boost/iterator/filter_iterator.hpp>

#include <QAbstractItemModel>
#include <QFileSystemWatcher>
#include <QImage>
#include <QUrl>

#include <verdigris>

namespace score
{
struct ApplicationContext;
}
namespace PluginSettings
{

struct RemotePackage
{
  static std::optional<RemotePackage>
  fromJson(const QJsonObject& obj) noexcept;

  UuidKey<score::Addon> key; // Can be the same as plug-in's

  QString raw_name;
  QString name;
  QString version; // version of the add-on
  QString target; // version of score targeted by this version of the add-on
  QString kind; // what kind of package it is (for now: "addon", "sdk", "library")
  QUrl file; // URL to a file containing the current version.
  QString url; // Link to the homepage of the package if any

  QString shortDescription;
  QString longDescription;
  QString smallImagePath;
  QString largeImagePath;
  QImage smallImage;
  QImage largeImage;
  bool enabled = true;
  bool corePlugin = false; // For plug-ins shipped with score
};

class LocalPackagesModel : public QAbstractItemModel
{
  std::vector<RemotePackage> m_vec;

public:
  LocalPackagesModel(const score::ApplicationContext& ctx);

private:
  enum class Column
  {
    Name,
    ShortDesc
  };
  static constexpr const int ColumnCount = 3;

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  QFileSystemWatcher m_addonsWatch;
};

class RemotePackagesModel : public QAbstractItemModel
{
  std::vector<RemotePackage> m_vec;

public:
  auto& addons() { return m_vec; }
  auto& addons() const { return m_vec; }

  void addAddon(RemotePackage e);

  template <typename Fun>
  void updateAddon(UuidKey<score::Addon> k, Fun f)
  {
    if (auto add = addon(k))
    {
      beginResetModel();
      f(*add);
      endResetModel();
    }
  }

  void clear();

private:
  enum class Column
  {
    Name,
    ShortDesc
  };

  RemotePackage* addon(UuidKey<score::Addon> k)
  {
    auto it = ossia::find_if(m_vec, [&](auto& add) { return add.key == k; });

    if (it != m_vec.end())
      return &*it;

    return nullptr;
  }

  static constexpr const int ColumnCount = 2;

  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
};
}

Q_DECLARE_METATYPE(PluginSettings::RemotePackage)
W_REGISTER_ARGTYPE(PluginSettings::RemotePackage)
