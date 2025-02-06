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
namespace PM
{
struct Package
{
  static std::optional<Package> fromJson(const QJsonObject& obj) noexcept;

  UuidKey<score::Addon> key; // Can be the same as plug-in's

  QString raw_name;
  QString name;
  int version{};  // version of the add-on
  QString target; // version of score targeted by this version of the add-on
  QString
      kind; // what kind of package it is (for now: "addon", "sdk", "library", "media", "presets")
  std::vector<QUrl> files; // URL to a file containing the current version.
  QMap<QString, std::vector<QUrl>> arch_files; // if there are per-architecture files
  QString url;    // Link to the homepage of the package if any
  QString category;
  QString shortDescription;
  QString longDescription;
  QString smallImagePath;
  QString largeImagePath;
  QImage smallImage;
  QImage largeImage;
  QString size;
  bool enabled = true;
  bool corePlugin = false; // For plug-ins shipped with score
};

class PackagesModel : public QAbstractItemModel
{
public:
  std::vector<Package> m_vec;

  void addAddon(Package e);
  void removeAddon(Package e);

  auto& addons() { return m_vec; }
  auto& addons() const { return m_vec; }

  void clear();

private:
  enum class Column
  {
    Name,
    Version,
    Size,
    ShortDesc
  };
  static constexpr const int ColumnCount = 4;

  QModelIndex index(int row, int column, const QModelIndex& parent) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
};

struct LocalPackagesModel : public PackagesModel
{
  explicit LocalPackagesModel(const score::ApplicationContext& ctx);
  void registerAddon(const QString& p);
  QFileSystemWatcher m_addonsWatch;
};

class RemotePackagesModel : public PackagesModel
{
public:
  template <typename Fun>
  void updateAddon(UuidKey<score::Addon> k, Fun f)
  {
    if(auto add = addon(k))
    {
      beginResetModel();
      f(*add);
      endResetModel();
    }
  }

private:
  Package* addon(UuidKey<score::Addon> k)
  {
    auto it = ossia::find_if(m_vec, [&](auto& add) { return add.key == k; });

    if(it != m_vec.end())
      return &*it;

    return nullptr;
  }
};
}

Q_DECLARE_METATYPE(PM::Package)
W_REGISTER_ARGTYPE(PM::Package)
