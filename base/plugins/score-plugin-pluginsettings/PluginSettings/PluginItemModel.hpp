#pragma once
#include <score/plugins/Addon.hpp>

#include <ossia/detail/algorithms.hpp>

#include <boost/iterator/filter_iterator.hpp>

#include <QAbstractItemModel>
#include <QImage>
#include <QUrl>

#include <wobjectdefs.h>
namespace PluginSettings
{

struct RemoteAddon
{
  std::map<QString, QUrl> architectures;
  UuidKey<score::Addon> key; // Can be the same as plug-in's

  QString name;
  QString version;
  QString
      latestVersionAddress; // URL to a file containing the current version.

  QString shortDescription;
  QString longDescription;
  QImage smallImage;
  QImage largeImage;
  bool enabled = true;
  bool corePlugin = false; // For plug-ins shipped with score
};

std::map<QString, QUrl> addonArchitectures();

struct AddonVectorWrapper
{
  struct not_core_addon
  {
    bool operator()(const score::Addon& e)
    {
      return !e.corePlugin;
    }
  };

  const std::vector<score::Addon>& vec;

  using iterator = boost::filter_iterator<
      not_core_addon, std::vector<score::Addon>::iterator>;
  using const_iterator = boost::filter_iterator<
      not_core_addon, std::vector<score::Addon>::const_iterator>;

  auto size() const
  {
    return ossia::count_if(vec, [](auto& e) { return !e.corePlugin; });
  }

  auto begin() const
  {
    return const_iterator{vec.begin()};
  }
  auto end() const
  {
    return const_iterator{vec.end()};
  }
  auto cbegin() const
  {
    return const_iterator{vec.cbegin()};
  }
  auto cend() const
  {
    return const_iterator{vec.cend()};
  }

  auto& operator[](int i)
  {
    auto it = begin();
    std::advance(it, i);
    return *it;
  }
  auto& operator[](int i) const
  {
    auto it = begin();
    std::advance(it, i);
    return *it;
  }
};

class LocalPluginItemModel : public QAbstractItemModel
{
  AddonVectorWrapper m_vec;

public:
  LocalPluginItemModel(const std::vector<score::Addon>& vec);

private:
  enum class Column
  {
    Name,
    ShortDesc,
    Path
  };
  static constexpr const int ColumnCount = 3;

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override;

  QModelIndex parent(const QModelIndex& child) const override;

  int rowCount(const QModelIndex& parent) const override;

  int columnCount(const QModelIndex& parent) const override;

  QVariant data(const QModelIndex& index, int role) const override;

  Qt::ItemFlags flags(const QModelIndex& index) const override;
};

class RemotePluginItemModel : public QAbstractItemModel
{
  std::vector<RemoteAddon> m_vec;

public:
  auto& addons()
  {
    return m_vec;
  }
  auto& addons() const
  {
    return m_vec;
  }

  void addAddon(RemoteAddon e);

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

  RemoteAddon* addon(UuidKey<score::Addon> k)
  {
    auto it = ossia::find_if(m_vec, [&](auto& add) { return add.key == k; });

    if (it != m_vec.end())
      return &*it;

    return nullptr;
  }

  static constexpr const int ColumnCount = 2;

  QModelIndex
  index(int row, int column, const QModelIndex& parent) const override;

  QModelIndex parent(const QModelIndex& child) const override;

  int rowCount(const QModelIndex& parent) const override;

  int columnCount(const QModelIndex& parent) const override;

  QVariant data(const QModelIndex& index, int role) const override;
};
}

Q_DECLARE_METATYPE(PluginSettings::RemoteAddon)
W_REGISTER_ARGTYPE(PluginSettings::RemoteAddon)
