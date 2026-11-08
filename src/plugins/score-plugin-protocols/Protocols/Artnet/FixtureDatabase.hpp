#pragma once
#include <Library/LibrarySettings.hpp>
#include <Library/RecursiveFilterProxy.hpp>
#include <Protocols/Artnet/OpenFixtureLibrary.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/FindStringInFile.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/json.hpp>

#include <QDebug>
#include <QDirIterator>
#include <QIcon>
#include <QTimer>

namespace Protocols
{
using FixtureMode = OFL::FixtureMode;

static std::vector<QString> fixturesLibraryPaths()
{
  auto libPath
      = score::AppContext().settings<Library::Settings::Model>().getPackagesPath();
  QDirIterator it{
      libPath,
      {"fixtures"},
      QDir::Dirs,
      QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};

  std::vector<QString> fixtures;
  while(it.hasNext())
  {
    QDir dirpath = it.next();
    if(!dirpath.entryList({"manufacturers.json"}, QDir::Filter::Files).isEmpty())
    {
      fixtures.push_back(dirpath.absolutePath());
    }
  }
  return fixtures;
}

static ossia::flat_map<QString, QString>
readManufacturers(const rapidjson::Document& doc)
{
  ossia::flat_map<QString, QString> map;
  map.reserve(100);
  if(!doc.IsObject())
    return map;

  for(auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
  {
    if(!it->value.IsObject())
      continue;
    if(auto name_it = it->value.FindMember("name");
       name_it != it->value.MemberEnd() && name_it->value.IsString())
    {
      map.insert(
          {QString::fromUtf8(it->name.GetString()),
           QString::fromUtf8(name_it->value.GetString())});
    }
  }
  return map;
}

class FixtureData
{
public:
  QString name{};
  QStringList tags{};
  QIcon icon{};
  std::vector<FixtureMode> modes{};
};
using FixtureNode = TreeNode<FixtureData>;

class FixtureDatabase : public TreeNodeBasedItemModel<FixtureNode>
{
public:
  std::vector<QString> m_paths;
  struct Scan
  {
    explicit Scan(QString root, const QString& dir, FixtureNode& manufacturer) noexcept
        : iterator{root + "/" + dir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks}
        , root{std::move(root)}
        , manufacturer{manufacturer}
    {
    }
    QDirIterator iterator;
    QString root;
    FixtureNode& manufacturer;
  };

  FixtureDatabase()
      : m_paths{fixturesLibraryPaths()}
  {
    if(!m_paths.empty())
    {
      for(auto& fixtures_dir : m_paths)
      {
        QFile f{fixtures_dir + "/manufacturers.json"};
        if(!f.open(QIODevice::ReadOnly))
          continue;
        auto data = f.map(0, f.size());

        rapidjson::Document doc;
        doc.Parse(reinterpret_cast<const char*>(data), f.size());
        if(doc.HasParseError())
        {
          qDebug() << "Invalid manufacturers.json !";
          continue;
        }

        loadManufacturer(doc, fixtures_dir);

        f.unmap(data);
      }
    }
  }

  void loadManufacturer(rapidjson::Document& doc, const QString& fixtures_dir)
  {
    QModelIndex rootIndex;
    auto manufacturers = readManufacturers(doc);
    if(m_root.childCount() == 0)
    {
      // Fast-path since we know that everything is already sorted
      int k = 0;
      for(auto it = manufacturers.begin(); it != manufacturers.end(); ++it, ++k)
      {
        beginInsertRows(rootIndex, k, k);
        auto& child = m_root.emplace_back(FixtureData{it->second}, &m_root);
        endInsertRows();

        nextFixture(std::make_shared<Scan>(fixtures_dir, it->first, child));
      }
    }
    else
    {
      for(auto it = manufacturers.begin(); it != manufacturers.end(); ++it)
      {
        auto manufacturer_node_it = ossia::find_if(
            m_root, [&](const FixtureNode& n) { return n.name == it->second; });
        if(manufacturer_node_it == m_root.end())
        {
          // We add it sorted to the model
          int newRowPosition = 0;
          auto other_manufacturer_it = m_root.begin();
          while(other_manufacturer_it != m_root.end()
                && QString::compare(
                       it->second, other_manufacturer_it->name, Qt::CaseInsensitive)
                       >= 0)
          {
            other_manufacturer_it++;
            newRowPosition++;
          }

          beginInsertRows(rootIndex, newRowPosition, newRowPosition);
          auto& child
              = m_root.emplace(other_manufacturer_it, FixtureData{it->second}, &m_root);
          endInsertRows();

          nextFixture(std::make_shared<Scan>(fixtures_dir, it->first, child));
        }
        else
        {
          nextFixture(
              std::make_shared<Scan>(fixtures_dir, it->first, *manufacturer_node_it));
        }
      }
    }
  }

  void loadFixture(
      std::string_view fixture_data, FixtureNode& manufacturer, const QString& root)
  {
    auto fixture = OFL::parseFixture(
        fixture_data, [&root](const QString& key) -> std::optional<std::string> {
      QFile f{root + "/" + key + ".json"};
      if(!f.open(QIODevice::ReadOnly))
        return std::nullopt;
      return f.readAll().toStdString();
    });
    if(!fixture)
      return;

    const QModelIndex manufacturerIndex
        = createIndex(m_root.indexOfChild(&manufacturer), 0, &manufacturer);

    int newRowPosition = 0;
    auto other_fixture_it = manufacturer.begin();
    while(other_fixture_it != manufacturer.end()
          && QString::compare(fixture->name, other_fixture_it->name, Qt::CaseInsensitive)
                 >= 0)
    {
      other_fixture_it++;
      newRowPosition++;
    }

    beginInsertRows(manufacturerIndex, newRowPosition, newRowPosition);
    manufacturer.emplace(
        other_fixture_it,
        FixtureData{
            .name = std::move(fixture->name),
            .tags = std::move(fixture->categories),
            .icon = {},
            .modes = std::move(fixture->modes)},
        &manufacturer);
    endInsertRows();
  }

  // Note: we could use make_unique here but on old Ubuntus stdlibc++-7 does not seem to support it (or Qt 5.9)
  // -> QTimer::singleShot calls copy ctor
  void nextFixture(std::shared_ptr<Scan> scan)
  {
    auto& iterator = scan->iterator;
    if(iterator.hasNext())
    {
      const auto filepath = iterator.next();
      if(QFileInfo fi{filepath}; fi.suffix() == "json")
      {
        const std::string_view req{"schemas/fixture"};
        score::findStringInFile(filepath, req, [&](QFile& f) {
          unsigned char* data = f.map(0, f.size());

          const char* cbegin = reinterpret_cast<char*>(data);

          loadFixture(std::string_view(cbegin, f.size()), scan->manufacturer, scan->root);
        });
      }

      m_inFlight++;
      QTimer::singleShot(
          1, this, [this, scan = std::move(scan)]() mutable {
        nextFixture(std::move(scan));
        m_inFlight--;
      });
    }
  }

  FixtureNode& rootNode() override { return m_root; }

  const FixtureNode& rootNode() const override { return m_root; }

  int columnCount(const QModelIndex& parent) const override { return 2; }

  QVariant data(const QModelIndex& index, int role) const override
  {
    const auto& node = nodeFromModelIndex(index);
    if(index.column() == 0)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return node.name;
          //        case Qt::DecorationRole:
          //          return node.icon;
      }
    }
    else if(index.column() == 1)
    {
      switch(role)
      {
        case Qt::DisplayRole:
          return node.tags.join(", ");
      }
    }
    return QVariant{};
  }

  QVariant headerData(int section, Qt::Orientation orientation, int role) const override
  {
    if(role != Qt::DisplayRole)
      return TreeNodeBasedItemModel<FixtureNode>::headerData(section, orientation, role);
    switch(section)
    {
      case 0:
        return tr("Fixture");
      case 1:
        return tr("Tags");
      default:
        return {};
    }
  }
  Qt::ItemFlags flags(const QModelIndex& index) const override
  {
    Qt::ItemFlags f;

    f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    return f;
  }

  QModelIndex modelIndexFromNode(const FixtureNode& n) const
  {
    node_type* parent = n.parent();
    SCORE_ASSERT(parent);
    SCORE_ASSERT(parent != &rootNode());

    return createIndex(parent->indexOfChild(&n), 0, &n);
  }

  static FixtureDatabase& instance()
  {
    static FixtureDatabase db;
    return db;
  }

  void onPopulated(auto func)
  {
    if(m_inFlight == 0)
    {
      func();
    }
    else
    {
      QTimer::singleShot(8, this, [this, func = std::move(func)]() mutable {
        onPopulated(std::move(func));
      });
    }
  }

  FixtureNode m_root;

  int m_inFlight{};
};


class FixtureFilterProxy final : public Library::RecursiveFilterProxy
{
public:
  using RecursiveFilterProxy::RecursiveFilterProxy;

  const FixtureNode* nodeAt(const QModelIndex& proxyIndex) const noexcept
  {
    const auto src = mapToSource(proxyIndex);
    return src.isValid() ? static_cast<const FixtureNode*>(src.internalPointer())
                         : nullptr;
  }

  int columnCount(const QModelIndex& parent = {}) const override
  {
    return sourceModel() ? sourceModel()->columnCount(mapToSource(parent)) : 0;
  }

private:

  bool filterAcceptsRowItself(int srcRow, const QModelIndex& srcParent) const override
  {
    const auto idx = sourceModel()->index(srcRow, 0, srcParent);
    const auto* node = static_cast<const FixtureNode*>(idx.internalPointer());
    if(!node)
      return false;
    if(node->name.contains(m_textPattern, Qt::CaseInsensitive))
      return true;
    return ossia::any_of(node->tags, [this](const QString& tag) {
      return tag.contains(m_textPattern, Qt::CaseInsensitive);
    });
  }
};

}
