#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "ArtnetProtocolSettingsWidget.hpp"
#include "ArtnetSpecificSettings.hpp"
#include "ArtnetProtocolFactory.hpp"

#include <Library/LibrarySettings.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <score/tools/FindStringInFile.hpp>

#include <ossia/detail/flat_map.hpp>

#include <QFormLayout>
#include <QVariant>
#include <QTableWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <QComboBox>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QDirIterator>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::ArtnetProtocolSettingsWidget)

namespace Protocols
{
struct FixtureMode
{
  QString name;
  std::vector<Artnet::Channel> channels;

  QString content() const noexcept
  {
    QString str;
    str.reserve(500);
    int k = 0;
    for(auto& chan : channels)
    {
      str += QString::number(k);
      str += ": \t";
      str += chan.name;
      str += "\n";
      k++;
    }
    return str;
  }
};

class FixtureData
{
public:
  QString name;
  QStringList tags;
  QIcon icon;

  std::vector<FixtureMode> modes;

  void loadModes(const rapidjson::Document& doc)
  {
    modes.clear();
    std::unordered_map<QString, Artnet::Channel> channels;
    {
      auto it = doc.FindMember("availableChannels");
      if(it == doc.MemberEnd())
        return;

      if(!it->value.IsObject())
        return;

      for(auto chan_it = it->value.MemberBegin(); chan_it != it->value.MemberEnd(); ++chan_it)
      {
        Artnet::Channel chan;
        chan.name = chan_it->name.GetString();

        if(chan_it->value.IsObject())
        {
          if(auto default_it = chan_it->value.FindMember("defaultValue");
             default_it != chan_it->value.MemberEnd())
          {
            if(default_it->value.IsNumber())
            {
              chan.defaultValue = default_it->value.GetDouble();
            }
            else if(default_it->value.IsString())
            {
              // TODO parse strings...
              // From a quick grep in the library the only used string so far is "50%" so we optimize on that.
              // PRs accepted :D
              std::string_view str = default_it->value.GetString();
              if(str == "50%")
                chan.defaultValue = 127;
            }
          }

          if(auto capability_it = chan_it->value.FindMember("capability");
             capability_it != chan_it->value.MemberEnd())
          {
            Artnet::SingleCapability cap;
            if(auto effectname_it = capability_it->value.FindMember("effectName");
               effectname_it != capability_it->value.MemberEnd())
              cap.effectName = effectname_it->value.GetString();

            if(auto comment_it = capability_it->value.FindMember("comment");
               comment_it != capability_it->value.MemberEnd())
              cap.comment = comment_it->value.GetString();

            cap.type = capability_it->value["type"].GetString();
            chan.capabilities = std::move(cap);
          }
          else if(auto capabilities_it = chan_it->value.FindMember("capabilities");
                  capabilities_it != chan_it->value.MemberEnd())
          {
            std::vector<Artnet::RangeCapability> caps;
            for(const auto& capa : capabilities_it->value.GetArray())
            {
              QString type = capa["type"].GetString();
              if(type != "NoFunction")
              {
                Artnet::RangeCapability cap;
                if(auto effectname_it = capa.FindMember("effectName");
                   effectname_it != capa.MemberEnd())
                  cap.effectName = effectname_it->value.GetString();

                if(auto comment_it = capa.FindMember("comment");
                   comment_it != capa.MemberEnd())
                  cap.comment = comment_it->value.GetString();

                cap.type = std::move(type);
                {
                  const auto& range_arr = capa["dmxRange"].GetArray();
                  cap.range = {range_arr[0].GetInt(), range_arr[1].GetInt()};
                }
                caps.push_back(std::move(cap));
              }
            }

            chan.capabilities = std::move(caps);
          }
        }

        channels[chan.name] = std::move(chan);
      }
    }

    {
      auto it = doc.FindMember("modes");
      if(it == doc.MemberEnd())
        return;

      if(!it->value.IsArray())
        return;

      for(auto& mode : it->value.GetArray())
      {
        auto name_it = mode.FindMember("name");
        auto channels_it = mode.FindMember("channels");
        if(name_it != mode.MemberEnd() && channels_it != mode.MemberEnd())
        {
          FixtureMode m;
          if(name_it->value.IsString())
            m.name = name_it->value.GetString();

          if(channels_it->value.IsArray())
          {
            for(auto& channel : channels_it->value.GetArray())
            {
              if(channel.IsString())
              {
                auto matched_channel_it = channels.find(channel.GetString());
                if(matched_channel_it != channels.end())
                {
                  m.channels.push_back(matched_channel_it->second);
                }
              }
            }
          }
          modes.push_back(std::move(m));
        }
      }
    }
  }
};
using FixtureNode = TreeNode<FixtureData>;

std::vector<QString> fixturesLibraryPaths()
{
  auto libPath = score::AppContext().settings<Library::Settings::Model>().getPath();
  QDirIterator it{libPath, {"fixtures"}, QDir::Dirs, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks};

  std::vector<QString> fixtures;
  while(it.hasNext()) {
    QDir dirpath = it.next();
    if(!dirpath.entryList({"manufacturers.json"}, QDir::Filter::Files).isEmpty())
    {
      fixtures.push_back(dirpath.absolutePath());
    }
  }
  return fixtures;
}

ossia::flat_map<QString, QString> readManufacturers(const rapidjson::Document& doc)
{
  ossia::flat_map<QString, QString> map;
  map.container.reserve(100);
  if(!doc.IsObject())
    return map;

  // TODO coroutines
  for(auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it)
  {
    if(it->value.IsObject())
    {
      if(auto name_it = it->value.FindMember("name"); name_it != it->value.MemberEnd())
      {
        map.insert({QString::fromUtf8(it->name.GetString()), QString::fromUtf8(name_it->value.GetString())});
      }
    }
  }
  return map;
}

class FixtureDatabase: public TreeNodeBasedItemModel<FixtureNode>
{
public:
  std::vector<QString> m_paths;
  struct Scan
  {
    explicit Scan(QString dir, FixtureNode& manufacturer) noexcept
      : iterator{std::move(dir), QDirIterator::Subdirectories | QDirIterator::FollowSymlinks}
      , manufacturer{manufacturer}
    {

    }
    QDirIterator iterator;
    FixtureNode& manufacturer;
  };

  FixtureDatabase()
       : m_paths{fixturesLibraryPaths()}
  {
    if(!m_paths.empty())
    {
      for(auto& fixtures_dir : m_paths)
      {
        // First read the manufacturers
        {
          QFile f{fixtures_dir + "/manufacturers.json"};
          if(!f.open(QIODevice::ReadOnly))
            continue;
          {
            auto data = f.map(0, f.size());
            rapidjson::Document doc;
            doc.Parse(reinterpret_cast<const char*>(data), f.size());
            if (doc.HasParseError())
            {
              qDebug() << "Invalid manufacturers.json !";
              continue;
            }

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

                QModelIndex manufacturerIndex = createIndex(k, 0, &child);
                next(std::make_shared<Scan>(fixtures_dir + "/" + it->first, child), manufacturerIndex);
              }
            }
            else
            {
              for(auto it = manufacturers.begin(); it != manufacturers.end(); ++it)
              {
                auto manufacturer_node_it = ossia::find_if(m_root, [&] (const FixtureNode& n) { return n.name == it->second; });
                if(manufacturer_node_it == m_root.end())
                {
                  // We add it sorted to the model
                  int newRowPosition = 0;
                  auto other_manufacturer_it = m_root.begin();
                  while(other_manufacturer_it != m_root.end() && QString::compare(it->second, other_manufacturer_it->name, Qt::CaseInsensitive) >= 0)
                  {
                    other_manufacturer_it++;
                    newRowPosition++;
                  }

                  beginInsertRows(rootIndex, newRowPosition, newRowPosition);
                  auto& child = m_root.emplace(other_manufacturer_it, FixtureData{it->second}, &m_root);
                  endInsertRows();

                  QModelIndex manufacturerIndex = createIndex(newRowPosition, 0, &child);
                  next(std::make_shared<Scan>(fixtures_dir + "/" + it->first, child), manufacturerIndex);
                }
                else
                {
                  int distance = std::abs(std::distance(manufacturer_node_it, m_root.begin()));
                  QModelIndex manufacturerIndex = createIndex(distance, 0, &*manufacturer_node_it);
                  next(std::make_shared<Scan>(fixtures_dir + "/" + it->first, *manufacturer_node_it), manufacturerIndex);
                }
              }
            }

            f.unmap(data);
          }
        }
      }
    }
  }

  void loadFixture(std::string_view fixture_data, FixtureNode& manufacturer, const QModelIndex& manufacturerIndex)
  {
    rapidjson::Document doc;
    doc.Parse(fixture_data.data(), fixture_data.size());
    if (doc.HasParseError())
    {
      qDebug() << "Invalid JSON document !";
      return;
    }
    if(auto it = doc.FindMember("name"); it != doc.MemberEnd())
    {
      QString name = it->value.GetString();

      int newRowPosition = 0;
      auto other_fixture_it = manufacturer.begin();
      while(other_fixture_it != manufacturer.end() && QString::compare(name, other_fixture_it->name, Qt::CaseInsensitive) >= 0)
      {
        other_fixture_it++;
        newRowPosition++;
      }

      beginInsertRows(manufacturerIndex, newRowPosition, newRowPosition);
      auto& data = manufacturer.emplace(other_fixture_it, FixtureData{name}, &manufacturer);
      endInsertRows();

      data.loadModes(doc);

      if(auto it = doc.FindMember("categories"); it != doc.MemberEnd())
      {
        for(auto& category : it->value.GetArray()) {
          data.tags.push_back(category.GetString());
        }
      }
    }
  }

  // Note: we could use make_unique here but on old Ubuntus stdlibc++-7 does not seem to support it (or Qt 5.9)
  // -> QTimer::singleShot calls copy ctor
  void next(std::shared_ptr<Scan> scan, QModelIndex manufacturerIndex)
  {
    auto& iterator = scan->iterator;
    if(iterator.hasNext())
    {
      const auto filepath = iterator.next();
      if(QFileInfo fi{filepath}; fi.suffix() == "json")
      {
        const std::string_view req{"https://raw.githubusercontent.com/OpenLightingProject/open-fixture-library/master/schemas/fixture.json"};
        score::findStringInFile(filepath, req , [&] (QFile& f) {

          unsigned char* data = f.map(0, f.size());

          const char* cbegin = reinterpret_cast<char*>(data);
          const char* cend = cbegin + f.size();

          loadFixture(std::string_view(cbegin, f.size()), scan->manufacturer, manufacturerIndex);
        });
      }

      QTimer::singleShot(1, this, [this, scan=std::move(scan), idx=std::move(manufacturerIndex)] () mutable { next(std::move(scan), std::move(idx)); });
    }
  }

  FixtureNode& rootNode() override
  {
    return m_root;
  }

  const FixtureNode& rootNode() const override
  {
    return m_root;
  }

  int columnCount(const QModelIndex& parent) const override
  {
    return 2;
  }

  QVariant data(const QModelIndex& index, int role) const override
  {
    const auto& node = nodeFromModelIndex(index);
    if(index.column() == 0)
    {
      switch (role)
      {
        case Qt::DisplayRole:
          return node.name;
//        case Qt::DecorationRole:
//          return node.icon;
      }
    }
    else if(index.column() == 1)
    {
      switch (role)
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

  static FixtureDatabase& instance()
  {
    static FixtureDatabase db;
    return db;
  }

  FixtureNode m_root;
};

class FixtureTreeView : public QTreeView
{
public:
  FixtureTreeView(QWidget* parent = nullptr)
    : QTreeView{parent}
  {
    setAllColumnsShowFocus(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropMode(QAbstractItemView::NoDragDrop);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
  }

  std::function<void(const FixtureNode&)> onSelectionChanged;
  void selectionChanged(
      const QItemSelection& selected,
      const QItemSelection& deselected)
  {
    auto sel = this->selectedIndexes();
    if (!sel.empty())
    {
      auto obj = (FixtureNode*)sel.at(0).internalPointer();
      if(obj)
      {
        onSelectionChanged(*obj);
      }
    }
  }
};

class AddFixtureDialog : public QDialog
{
public:
  AddFixtureDialog(ArtnetProtocolSettingsWidget& parent)
    : QDialog{&parent}
    , m_buttons{QDialogButtonBox::StandardButton::Ok | QDialogButtonBox::StandardButton::Cancel, this}
    , m_name{this}
  {
    this->setLayout(&m_layout);
    m_layout.addWidget(&m_availableFixtures);
    m_layout.addLayout(&m_setupLayoutContainer);
    m_layout.setStretch(0, 3);
    m_layout.setStretch(1, 5);

    m_availableFixtures.setModel(&FixtureDatabase::instance());
    m_availableFixtures.header()->resizeSection(0, 180);
    m_availableFixtures.onSelectionChanged = [&] (const FixtureNode& newFixt) {
      // Manufacturer, do nothing
      if(newFixt.childCount() > 0)
        return;

      updateParameters(newFixt);
    };

    m_setupLayoutContainer.addLayout(&m_setupLayout);
    m_setupLayout.addRow(tr("Name"), &m_name);
    m_setupLayout.addRow(tr("Address"), &m_address);
    m_setupLayout.addRow(tr("Mode"), &m_mode);
    m_setupLayout.addRow(tr("Channels"), &m_content);
    m_setupLayoutContainer.addStretch(0);
    m_setupLayoutContainer.addWidget(&m_buttons);
    connect(&m_buttons, &QDialogButtonBox::accepted, this, &AddFixtureDialog::accept);
    connect(&m_buttons, &QDialogButtonBox::rejected, this, &AddFixtureDialog::reject);

    connect(&m_mode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AddFixtureDialog::setMode);
  }

  void updateParameters(const FixtureNode& fixt)
  {
    m_name.setText(fixt.name);

    m_mode.clear();
    for(auto& mode : fixt.modes)
      m_mode.addItem(mode.name);
    m_mode.setCurrentIndex(0);

    m_currentFixture = &fixt;

    setMode(0);
  }

  void setMode(int mode_index)
  {
    if(!m_currentFixture)
      return;
    if(mode_index < 0 || mode_index >= m_currentFixture->modes.size())
      return;

    const FixtureMode& mode = m_currentFixture->modes[mode_index];
    int numChannels = mode.channels.size();
    m_address.setRange(0, 512 - numChannels);

    m_content.setText(mode.content());
  }

  QSize sizeHint() const override
  {
    return QSize{800, 600};
  }

  Artnet::Fixture fixture() const noexcept
  {
    Artnet::Fixture f;
    if(!m_currentFixture)
      return f;

    int mode_index = m_mode.currentIndex();
    if(mode_index < 0 || mode_index >= m_currentFixture->modes.size())
      return f;

    f.fixtureName = m_currentFixture->name;
    f.modeName = m_mode.currentText();
    f.address = m_address.value();
    f.controls = m_currentFixture->modes[mode_index].channels;

    return f;
  }

private:
  QHBoxLayout m_layout;
  FixtureTreeView m_availableFixtures;

  QVBoxLayout m_setupLayoutContainer;
  QFormLayout m_setupLayout;
  State::AddressFragmentLineEdit m_name;
  QSpinBox m_address;
  QComboBox m_mode;
  QLabel m_content;
  QDialogButtonBox m_buttons;

  const FixtureData* m_currentFixture{};
};

ArtnetProtocolSettingsWidget::ArtnetProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Artnet");

  m_rate = new QSpinBox{this};
  m_rate->setRange(0, 44);
  m_rate->setValue(20);

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate (Hz)"), m_rate);

  m_fixturesWidget = new QTableWidget;
  layout->addRow(m_fixturesWidget);

  m_fixturesWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_fixturesWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_fixturesWidget->insertColumn(0);
  m_fixturesWidget->insertColumn(1);
  m_fixturesWidget->insertColumn(2);
  m_fixturesWidget->insertColumn(3);
  m_fixturesWidget->setHorizontalHeaderLabels({tr("Name"), tr("Mode"), tr("Address"), tr("Channels used")});

  auto btns = new QHBoxLayout;
  m_addFixture = new QPushButton{"Add a fixture"};
  m_rmFixture = new QPushButton{"Remove selected fixture"};
  btns->addWidget(m_addFixture);
  btns->addWidget(m_rmFixture);
  layout->addRow(btns);

  connect(m_addFixture, &QPushButton::clicked,
          this, [=] {
    auto dial = new AddFixtureDialog{*this};
    if(dial->exec() == QDialog::Accepted)
    {
      auto fixt = dial->fixture();
      if(!fixt.fixtureName.isEmpty() && !fixt.controls.empty())
      {
        m_fixtures.push_back(fixt);
        updateTable();
      }
    }
  });
  connect(m_rmFixture, &QPushButton::clicked,
          this, [=] {
    ossia::flat_set<int> rows_to_remove;
    for(auto item : m_fixturesWidget->selectedItems())
    {
      rows_to_remove.insert(item->row());
    }

    for(auto it = rows_to_remove.container.rbegin(); it != rows_to_remove.container.rend(); ++it)
    {
      m_fixtures.erase(m_fixtures.begin() + *it);
    }

    updateTable();
  });

  setLayout(layout);
}

void ArtnetProtocolSettingsWidget::updateTable()
{
  while(m_fixturesWidget->rowCount() > 0)
    m_fixturesWidget->removeRow(int(m_fixturesWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_fixtures)
  {
    auto name_item = new QTableWidgetItem{fixt.fixtureName};
    auto mode_item = new QTableWidgetItem{fixt.modeName};
    auto address = new QTableWidgetItem{QString::number(fixt.address)};
    auto controls = new QTableWidgetItem{QString::number(fixt.controls.size())};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_fixturesWidget->insertRow(row);
    m_fixturesWidget->setItem(row, 0, name_item);
    m_fixturesWidget->setItem(row, 1, mode_item);
    m_fixturesWidget->setItem(row, 2, address);
    m_fixturesWidget->setItem(row, 3, controls);
    row++;
  }
}
ArtnetProtocolSettingsWidget::~ArtnetProtocolSettingsWidget() { }

Device::DeviceSettings ArtnetProtocolSettingsWidget::getSettings() const
{
  // TODO should be = m_settings to follow the other patterns.
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = ArtnetProtocolFactory::static_concreteKey();

  ArtnetSpecificSettings settings{};
  settings.fixtures = this->m_fixtures;
  settings.rate = this->m_rate->value();
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void ArtnetProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& specif = settings.deviceSpecificSettings.value<ArtnetSpecificSettings>();
  m_fixtures = specif.fixtures;
  m_rate->setValue(specif.rate);
  updateTable();
}
}
#endif
