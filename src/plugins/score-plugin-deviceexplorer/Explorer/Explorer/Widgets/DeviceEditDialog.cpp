// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceEditDialog.hpp"

#include <Device/Loading/ScoreDeviceLoader.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/RecursiveWatch.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SearchLineEdit.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVariant>
#include <QWidget>
#include <qnamespace.h>

#include <rapidfuzz/fuzz.hpp>

#include <wobjectimpl.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
W_OBJECT_IMPL(Explorer::DeviceEditDialog)
namespace Explorer
{
static void setCategoryStyle(QTreeWidgetItem* catItem)
{
  catItem->setFont(0, score::Skin::instance().SectionTitleFont);
  catItem->setExpanded(true);
}

namespace
{
//! Lowercased UTF-8 of the item text, so that a keystroke does not re-case
//! hundreds of strings.
constexpr int SearchKeyRole = Qt::UserRole + 1;
constexpr int SearchScoreRole = Qt::UserRole + 2;

constexpr double FuzzyCutoff = 70.;
//! Under that length partial_ratio matches nearly anything: substrings only.
constexpr std::size_t FuzzyMinLength = 3;

void initSearchKey(QTreeWidgetItem& item)
{
  item.setData(0, SearchKeyRole, item.text(0).toLower().toUtf8());
}

bool isLeaf(const QTreeWidgetItem& item) noexcept
{
  return item.flags() & Qt::ItemIsSelectable;
}
}

//! Sorts on the search score first so that the best matches come up on top;
//! every score is zero when nothing is typed, which leaves the usual
//! alphabetical order.
class SearchableItem final : public QTreeWidgetItem
{
public:
  using QTreeWidgetItem::QTreeWidgetItem;

  bool operator<(const QTreeWidgetItem& other) const override
  {
    const double self = data(0, SearchScoreRole).toDouble();
    const double rhs = other.data(0, SearchScoreRole).toDouble();
    if(self != rhs)
      return self > rhs;
    return text(0) < other.text(0);
  }
};

class TreeSearchLineEdit final : public score::SearchLineEdit
{
public:
  TreeSearchLineEdit(QTreeWidget& tree, QWidget* parent)
      : score::SearchLineEdit{parent}
      , m_tree{tree}
  {
    setPlaceholderText(tr("Filter"));
    setClearButtonEnabled(true);
    connect(this, &QLineEdit::textChanged, this, [this] { search(); });
  }

  void search() override
  {
    const auto utf8 = text().trimmed().toLower().toUtf8();
    std::string needle(utf8.constData(), utf8.size());
    if(needle == m_needle)
      return;

    m_needle = std::move(needle);
    m_scorer.reset();
    if(m_needle.size() >= FuzzyMinLength)
      m_scorer.emplace(m_needle);

    refilter();
  }

  //! Both the enumerators and the preset scan keep filling the tree long after
  //! the user typed: new items are ranked as they land.
  void itemAdded(QTreeWidgetItem& item)
  {
    if(!isLeaf(item))
    {
      item.setData(0, SearchScoreRole, 0.);
      item.setHidden(!m_needle.empty());
      return;
    }

    const bool matches = rank(item);
    item.setHidden(!matches);
    if(!matches)
      return;

    if(auto* cat = item.parent())
    {
      cat->setHidden(false);
      const double s = item.data(0, SearchScoreRole).toDouble();
      if(s > cat->data(0, SearchScoreRole).toDouble())
        cat->setData(0, SearchScoreRole, s);
    }
  }

  void refilter()
  {
    for(int i = 0; i < m_tree.topLevelItemCount(); i++)
    {
      auto& top = *m_tree.topLevelItem(i);
      if(isLeaf(top))
      {
        top.setHidden(!rank(top));
        continue;
      }

      double best = 0.;
      bool any = false;
      for(int j = 0; j < top.childCount(); j++)
      {
        auto& child = *top.child(j);
        const bool matches = rank(child);
        child.setHidden(!matches);
        if(matches)
        {
          any = true;
          best = std::max(best, child.data(0, SearchScoreRole).toDouble());
        }
      }
      top.setData(0, SearchScoreRole, best);
      top.setHidden(!any);
    }

    m_tree.sortItems(0, Qt::AscendingOrder);
    if(!m_needle.empty())
      m_tree.expandAll();
  }

private:
  //! Stores the ranking score on the item and tells whether it is a match.
  bool rank(QTreeWidgetItem& item) const
  {
    if(m_needle.empty())
    {
      item.setData(0, SearchScoreRole, 0.);
      return true;
    }

    const auto key = item.data(0, SearchKeyRole).toByteArray();
    const std::string_view text{key.constData(), std::size_t(key.size())};

    // Substrings always win over any fuzzy near-miss, and the earlier the
    // better: this is what keeps one- or two-letter queries usable.
    if(const auto pos = text.find(m_needle); pos != std::string_view::npos)
    {
      item.setData(0, SearchScoreRole, 100. + 100. / (1. + pos));
      return true;
    }

    const double fuzzy = m_scorer ? m_scorer->similarity(text, FuzzyCutoff) : 0.;
    item.setData(0, SearchScoreRole, fuzzy);
    return fuzzy >= FuzzyCutoff;
  }

  QTreeWidget& m_tree;
  std::string m_needle;
  std::optional<rapidfuzz::fuzz::CachedPartialRatio<char>> m_scorer;
};

DeviceEditDialog::DeviceEditDialog(
    const DeviceExplorerModel& model, const Device::ProtocolFactoryList& pl, Mode mode,
    QWidget* parent)
    : QDialog{parent}
    , m_model{model}
    , m_protocolList{pl}
    , m_mode{mode}
    , m_protocolWidget{nullptr}
    , m_index{-1}
{
  setObjectName("DeviceEditDialog");

  const auto& skin = score::Skin::instance();
  const QColor textHeaderColor = QColor("#D5D5D5");
  auto setHeaderTextFormat = [&](QLabel* label) {
    label->setFont(skin.TitleFont);
    auto p = label->palette();
    p.setColor(QPalette::WindowText, textHeaderColor);
    label->setPalette(p);
  };

  setWindowTitle(tr("Add device"));
  auto base_layout = new QHBoxLayout{this};
  setLayout(base_layout);
  setModal(true);
  setWindowModality(Qt::WindowModal);

  m_splitter = new QSplitter{this};
  base_layout->addWidget(m_splitter);

  auto column1 = new QWidget;
  auto column1_layout = new score::MarginLess<QVBoxLayout>{column1};

  // Tab buttons for Protocols / Presets
  {
    auto tabBar = new QWidget{this};
    auto tabLayout = new score::MarginLess<QHBoxLayout>{tabBar};

    m_protocolsTabButton = new QPushButton{tr("Protocols"), this};
    m_presetsTabButton = new QPushButton{tr("Presets"), this};

    m_protocolsTabButton->setCheckable(true);
    m_presetsTabButton->setCheckable(true);
    m_protocolsTabButton->setChecked(true);
    m_protocolsTabButton->setFlat(true);
    m_presetsTabButton->setFlat(true);

    tabLayout->addWidget(m_protocolsTabButton);
    tabLayout->addWidget(m_presetsTabButton);
    tabBar->setLayout(tabLayout);
    column1_layout->addWidget(tabBar);
  }

  // Stacked widget: page 0 = protocols tree, page 1 = presets tree
  m_column1Stack = new QStackedWidget{this};

  auto makeListPage = [this](QTreeWidget*& tree, TreeSearchLineEdit*& search) {
    auto page = new QWidget{m_column1Stack};
    auto layout = new score::MarginLess<QVBoxLayout>{page};
    tree = new QTreeWidget{page};
    tree->header()->hide();
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    search = new TreeSearchLineEdit{*tree, page};
    layout->addWidget(tree);
    layout->addWidget(search);
    m_column1Stack->addWidget(page);
  };
  makeListPage(m_protocols, m_protocolsSearch);
  makeListPage(m_presets, m_presetsSearch);

  m_column1Stack->setCurrentIndex(0);
  column1_layout->addWidget(m_column1Stack);

  connect(m_protocolsTabButton, &QPushButton::clicked, this, [this] {
    m_protocolsTabButton->setChecked(true);
    m_presetsTabButton->setChecked(false);
    m_column1Stack->setCurrentIndex(0);
  });
  connect(m_presetsTabButton, &QPushButton::clicked, this, [this] {
    m_presetsTabButton->setChecked(true);
    m_protocolsTabButton->setChecked(false);
    m_column1Stack->setCurrentIndex(1);
  });

  column1->setLayout(column1_layout);
  column1->setFixedWidth(200);
  base_layout->addWidget(column1);

  if(m_mode == Mode::Editing)
  {
    column1->setVisible(false);
  }

  base_layout->addWidget(m_splitter);

  // Column 2: Devices
  auto column2 = new QWidget;
  auto column2_layout = new score::MarginLess<QVBoxLayout>{column2};
  m_devicesLabel = new QLabel{tr("Devices"), this};
  setHeaderTextFormat(m_devicesLabel);
  column2_layout->addWidget(m_devicesLabel);
  m_devicesLabel->setAlignment(Qt::AlignTop);
  m_devicesLabel->setAlignment(Qt::AlignHCenter);
  m_devices = new QTreeWidget{this};
  m_devices->header()->hide();
  m_devices->setSelectionMode(QAbstractItemView::SingleSelection);
  column2_layout->addWidget(m_devices);
  m_devicesSearch = new TreeSearchLineEdit{*m_devices, column2};
  column2_layout->addWidget(m_devicesSearch);
  column2->setLayout(column2_layout);
  m_splitter->addWidget(column2);

  // Column 3: Settings
  auto column3 = new QWidget;
  auto column3_layout = m_column3Layout = new score::MarginLess<QVBoxLayout>{column3};
  m_protocolNameLabel = new QLabel{tr("Settings"), this};
  setHeaderTextFormat(m_protocolNameLabel);
  column3_layout->addWidget(m_protocolNameLabel);
  m_protocolNameLabel->setAlignment(Qt::AlignTop);
  m_protocolNameLabel->setAlignment(Qt::AlignHCenter);
  // m_main = new QWidget{this};
  // m_settingsFormLayout = new QFormLayout;
  // m_settingsFormLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  // m_main->setLayout(m_settingsFormLayout);
  // column3_layout->addWidget(m_main, 255, Qt::AlignTop);

  m_invalidLabel = new QLabel{
      tr("Cannot add device.\n Try changing the name to make it unique, \nor "
         "check that the ports aren't already used")};
  m_invalidLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
  m_invalidLabel->setTextFormat(Qt::PlainText);

  m_buttonBox = new QDialogButtonBox(Qt::Horizontal, this);
  m_helpButton = m_buttonBox->addButton(tr("Help"), QDialogButtonBox::HelpRole);
  m_okButton = m_buttonBox->addButton(tr("Add"), QDialogButtonBox::AcceptRole);
  m_buttonBox->addButton(QDialogButtonBox::Cancel);
  //column3_layout->addStretch(1);
  column3_layout->addWidget(m_invalidLabel, 1, Qt::AlignBottom);
  column3_layout->addWidget(m_buttonBox, 1, Qt::AlignBottom);
  column3->setLayout(column3_layout);
  m_splitter->addWidget(column3);

  m_devices->setMinimumWidth(40);
  // m_main->setMinimumWidth(100);

  m_splitter->setCollapsible(0, false);
  m_splitter->setCollapsible(1, false);

  m_splitter->setStretchFactor(0, 1);
  m_splitter->setStretchFactor(1, 2);

  connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_buttonBox, &QDialogButtonBox::helpRequested, [this] {
    if(!this->m_protocols)
      return;
    auto items = m_protocols->selectedItems();
    if(items.empty())
    {
      QDesktopServices::openUrl(QUrl("https://ossia.io/score-docs/devices.html"));
      return;
    }
    auto selected_item = items.first();
    auto key
        = selected_item->data(0, Qt::UserRole).value<UuidKey<Device::ProtocolFactory>>();
    if(key == UuidKey<Device::ProtocolFactory>{})
      return;

    if(auto* proto = m_protocolList.get(key))
      if(auto manual = proto->manual(); !manual.isEmpty())
        QDesktopServices::openUrl(manual);
  });

  initAvailableProtocols();
  initPresets();

  connect(
      m_protocols, &QTreeView::activated, this, [this] { selectedProtocolChanged(); });
  connect(m_devices, &QTreeView::activated, this, [this] { selectedDeviceChanged(); });
  connect(m_presets, &QTreeView::activated, this, [this] { selectedPresetChanged(); });

  if(m_protocols->topLevelItemCount() > 0)
  {
    selectedProtocolChanged();
  }

  setMinimumWidth(850);
  setMinimumHeight(550);

  setAcceptEnabled(false);
}

DeviceEditDialog::~DeviceEditDialog()
{
  clearEnumerators();
}

void DeviceEditDialog::clearEnumerators()
{
  // Order is load-bearing. The enumerator callbacks capture QTreeWidgetItem*s
  // that the following m_devices->clear() deletes. An enumerator emitting from
  // a worker thread makes the connection queued, and Qt retracts posted events
  // only when their *receiver* dies - not the sender - so with `this` as the
  // context they still ran, on freed items. Each selection therefore gets its
  // own context object, destroyed here, before those items.
  delete m_enumeratorContext;
  m_enumeratorContext = nullptr;

  m_enumerators.clear();
}

void DeviceEditDialog::initAvailableProtocols()
{
  // initialize previous settings
  m_previousSettings.clear();

  std::vector<Device::ProtocolFactory*> sorted;
  for(auto& elt : m_protocolList)
  {
    sorted.push_back(&elt);
  }

  ossia::sort(sorted, [](Device::ProtocolFactory* lhs, Device::ProtocolFactory* rhs) {
    return lhs->visualPriority() > rhs->visualPriority()
           || (lhs->visualPriority() == rhs->visualPriority()
               && lhs->prettyName() < rhs->prettyName());
  });
  for(const auto& prot_pair : sorted)
  {
    auto& prot = *prot_pair;
    auto cat_list = m_protocols->findItems(prot.category(), Qt::MatchFixedString);
    QTreeWidgetItem* categoryItem{};
    if(cat_list.size() == 0)
    {
      categoryItem = new SearchableItem;
      categoryItem->setText(0, prot.category());
      categoryItem->setFlags(Qt::ItemIsEnabled);
      m_protocols->addTopLevelItem(categoryItem);
    }
    else
    {
      categoryItem = cat_list.first();
    }

    auto item = new SearchableItem{categoryItem};
    item->setText(0, prot.prettyName());
    item->setData(0, Qt::UserRole, QVariant::fromValue(prot.concreteKey()));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    initSearchKey(*item);
    m_previousSettings.append(prot.defaultSettings());
  }

  m_protocols->sortItems(0, Qt::AscendingOrder);

  // A QTreeWidgetItem is no QObject and cannot own a subscription, so the
  // dialog re-styles both trees itself.
  score::onSkinChange(this, [this] {
    for(QTreeWidget* tree : {m_protocols, m_devices})
    {
      if(!tree)
        continue;
      for(int i = 0; i < tree->topLevelItemCount(); i++)
        setCategoryStyle(tree->topLevelItem(i));
    }
  });

  m_protocols->setRootIsDecorated(false);
  m_protocols->setExpandsOnDoubleClick(false);
  m_index = 0;
}

void DeviceEditDialog::initPresets()
{
  m_presets->clear();

  // Read the library root path directly from QSettings
  // to avoid a dependency on score-plugin-library
  QSettings s;
  QString rootPath = s.value("Library/RootPath").toString();
  if(rootPath.isEmpty())
  {
    auto paths = QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
    if(!paths.isEmpty())
    {
      rootPath = QString("%1/%2/%3")
                     .arg(
                         paths[0], QCoreApplication::organizationName(),
                         QCoreApplication::applicationName());
    }
  }

  if(rootPath.isEmpty())
    return;

  static score::RecursiveWatch r;
  r.reset();
  r.registerWatch(
      "device", score::RecursiveWatch::AsyncCallbacks{
                    .filter = [&](std::string_view path) -> std::function<void()> {
    const auto path_info = score::PathInfo{path};
    auto basename = QString::fromUtf8(
        path_info.completeBaseName.data(), path_info.completeBaseName.size());
    auto absolutePath = QString::fromUtf8(
        path_info.absoluteFilePath.data(), path_info.absoluteFilePath.size());

    return
        [this, basename = std::move(basename), absolutePath = std::move(absolutePath)] {
      // The scan outlives setBrowserEnabled(false), which drops the tree.
      if(!m_presets)
        return;
      auto item = new SearchableItem;
      item->setText(0, basename);
      item->setData(0, Qt::UserRole, absolutePath);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      initSearchKey(*item);
      m_presets->addTopLevelItem(item);
      m_presetsSearch->itemAdded(*item);
      queuePresetSort();
    };
  }});
  r.setWatchedFolder(rootPath.toStdString() + "/packages");
  r.scanAsync(this);
}

void DeviceEditDialog::queuePresetSort()
{
  if(m_presetSortQueued)
    return;
  m_presetSortQueued = true;
  QMetaObject::invokeMethod(this, [this] {
    m_presetSortQueued = false;
    if(m_presets)
      m_presets->sortItems(0, Qt::AscendingOrder);
  }, Qt::QueuedConnection);
}

void DeviceEditDialog::selectedPresetChanged()
{
  if(m_presets->selectedItems().isEmpty())
    return;

  auto item = m_presets->currentItem();
  if(!item)
    return;

  auto filePath = item->data(0, Qt::UserRole).toString();
  if(filePath.isEmpty())
    return;

  // Load the full device node from the .device file
  Device::Node n;
  if(!Device::loadDeviceFromScoreJSON(filePath, n))
    return;

  if(!n.is<Device::DeviceSettings>())
    return;

  auto& deviceSettings = n.get<Device::DeviceSettings>();

  // Find the protocol factory for this device
  auto protocol = m_protocolList.get(deviceSettings.protocol);
  if(!protocol)
    return;

  // Clear previous state
  clearEnumerators();
  m_devices->clear();
  if(m_protocolWidget)
  {
    if(m_index >= 0 && m_index < m_previousSettings.count())
      m_previousSettings[m_index] = m_protocolWidget->getSettings();
    m_column3Layout->removeWidget(m_protocolWidget);
    delete m_protocolWidget;
    m_protocolWidget = nullptr;
  }

  // Hide devices column — presets don't use enumerators
  m_devices->setVisible(false);
  m_devicesSearch->setVisible(false);
  m_devicesLabel->setVisible(false);
  if(m_splitter->count() > 0)
    m_splitter->widget(0)->hide();

  // Create the correct settings widget for this protocol
  m_protocolNameLabel->setText(tr("Settings (%1)").arg(protocol->prettyName()));
  m_protocolWidget = protocol->makeSettingsWidget();

  if(m_protocolWidget)
  {
    m_protocolWidget->setSettings(deviceSettings);
    connect(
        m_protocolWidget, &Device::ProtocolSettingsWidget::changed, this,
        &DeviceEditDialog::updateValidity);

    m_column3Layout->insertWidget(1, m_protocolWidget);

    QSizePolicy pol{QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding};
    pol.setVerticalStretch(255);
    m_protocolWidget->setSizePolicy(pol);
    m_protocolWidget->setMinimumHeight(200);
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    updateGeometry();
  }

  // Save the full node so getDevice() returns it with the address tree
  m_presetNode = std::move(n);

  updateValidity();
}

void DeviceEditDialog::selectedDeviceChanged()
{
  if(!m_devices->isVisible())
    return;
  if(m_devices->selectedItems().isEmpty())
    return;

  auto item = m_devices->currentItem();
  if(!item)
    return;

  auto data = item->data(0, Qt::UserRole).value<Device::DeviceSettings>();

  if(m_protocolWidget)
  {
    if(m_mode == Mode::Editing)
    {
      // The score refers to the device by its name: picking another physical
      // device (camera, joystick, MIDI port...) for an existing device must
      // only change the device-specific settings, not rename it.
      data.name = editedDeviceName();
    }
    m_protocolWidget->setSettings(data);
  }

  updateValidity();
}

QString DeviceEditDialog::editedDeviceName() const
{
  if(m_protocolWidget)
    if(auto name = m_protocolWidget->getSettings().name; !name.isEmpty())
      return name;
  return m_originalName;
}

void DeviceEditDialog::selectedProtocolChanged()
{
  auto doc = score::GUIAppContext().currentDocument();
  if(!doc)
    return;

  // Recreate
  if(m_protocols->selectedItems().isEmpty())
  {
    return;
  }
  auto selected_item = m_protocols->selectedItems().first();
  auto key
      = selected_item->data(0, Qt::UserRole).value<UuidKey<Device::ProtocolFactory>>();
  if(key == UuidKey<Device::ProtocolFactory>{})
    return;

  // Clear preset state
  m_presetNode = Device::Node{};

  // Clear listener (must happen before the tree items the callbacks captured)
  clearEnumerators();

  // Clear devices
  m_devices->clear();
  m_devicesSearch->clear();

  // Clear protocol widget
  if(m_protocolWidget)
  {
    SCORE_ASSERT(m_index < m_previousSettings.count());
    m_previousSettings[m_index] = m_protocolWidget->getSettings();
    m_column3Layout->removeWidget(m_protocolWidget);
    delete m_protocolWidget;
    m_protocolWidget = nullptr;
  }

  auto protocol = m_protocolList.get(key);
  for(auto [name, e] : protocol->getEnumerators(*doc))
    m_enumerators.emplace_back(name, e);
  std::sort(m_enumerators.begin(), m_enumerators.end(),
      [](const auto& a, const auto& b) { return a.first < b.first; });
  if(!m_enumerators.empty())
  {
    m_devices->setVisible(true);
    m_devicesSearch->setVisible(true);
    m_devicesLabel->setVisible(true);
    m_devices->setRootIsDecorated(false);
    m_devices->setExpandsOnDoubleClick(false);
    if(m_splitter->count() > 0)
    {
      m_splitter->widget(0)->show();
      m_splitter->widget(0)->setMinimumWidth(200);
    }

    // Context object for every connection made below. It is destroyed by
    // clearEnumerators() before the QTreeWidgetItems these lambdas capture, so
    // that Qt drops any queued deviceAdded/deviceRemoved/sort still in flight.
    // See the comment in clearEnumerators().
    SCORE_ASSERT(!m_enumeratorContext);
    m_enumeratorContext = new QObject{this};
    auto* ctx = m_enumeratorContext;

    for(auto& [name, e] : m_enumerators)
    {
      auto cat = new SearchableItem{};
      setCategoryStyle(cat);
      cat->setText(0, name);
      cat->setFlags(Qt::ItemIsEnabled);
      m_devices->addTopLevelItem(cat);
      m_devicesSearch->itemAdded(*cat);

      auto addItem
          = [this, cat](const QString& name, const Device::DeviceSettings& settings) {
        auto item = new SearchableItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, QVariant::fromValue(settings));
        initSearchKey(*item);
        cat->addChild(item);
        cat->setExpanded(true);
        m_devicesSearch->itemAdded(*item);
      };
      auto rmItem = [this, cat](const QString& name) {
        for(int i = 0; i < cat->childCount();)
        {
          auto cld = cat->child(i);
          if(cld->text(0) == name)
          {
            cat->removeChild(cld);
            continue;
          }
          else
          {
            i++;
          }
        }
        m_devicesSearch->refilter();
      };

      connect(e.get(), &Device::DeviceEnumerator::deviceAdded, ctx, addItem);
      connect(e.get(), &Device::DeviceEnumerator::deviceRemoved, ctx, rmItem);
      connect(e.get(), &Device::DeviceEnumerator::sort, ctx, [cat] {
        cat->sortChildren(0, Qt::SortOrder::AscendingOrder);
      });
      e->enumerate(addItem);
    }
  }
  else
  {
    m_devices->setVisible(false);
    m_devicesSearch->setVisible(false);
    m_devicesLabel->setVisible(false);
    m_splitter->widget(0)->hide();
  }
  m_protocolNameLabel->setText(tr("Settings (%1)").arg(protocol->prettyName()));
  m_protocolWidget = protocol->makeSettingsWidget();

  if(m_protocolWidget)
  {
    m_protocolWidget->setSettings(protocol->defaultSettings());
    connect(
        m_protocolWidget, &Device::ProtocolSettingsWidget::changed, this,
        &DeviceEditDialog::updateValidity);

    m_column3Layout->insertWidget(1, m_protocolWidget);

    QSizePolicy pol{QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding};
    pol.setVerticalStretch(255);
    m_protocolWidget->setSizePolicy(pol);
    m_protocolWidget->setMinimumHeight(200);
    this->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    updateGeometry();
  }
  updateValidity();
}

Device::DeviceSettings DeviceEditDialog::getSettings() const
{
  if(m_protocolWidget)
    return m_protocolWidget->getSettings();

  return {};
}

Device::Node DeviceEditDialog::getDevice() const
{
  if(!m_protocolWidget)
    return {};

  // If a preset was loaded, return the full node (with address tree)
  // but re-apply the current widget settings (user may have edited name, ports, etc.)
  if(m_presetNode.is<Device::DeviceSettings>())
  {
    Device::Node n = m_presetNode;
    if(auto dev = n.target<Device::DeviceSettings>())
      *dev = m_protocolWidget->getSettings();
    return n;
  }

  return m_protocolWidget->getDevice();
}

void DeviceEditDialog::setSettings(const Device::DeviceSettings& settings)
{
  m_originalName = settings.name;

  for(int i = 0; i < m_protocols->topLevelItemCount(); i++)
  {
    auto catItem = m_protocols->topLevelItem(i);
    for(int j = 0; j < catItem->childCount(); j++)
    {
      auto item = catItem->child(j);
      if(item->data(0, Qt::UserRole).value<UuidKey<Device::ProtocolFactory>>()
         == settings.protocol)
      {
        m_protocols->setCurrentItem(item);
        selectedProtocolChanged();
        if(m_protocolWidget)
        {
          m_protocolWidget->setSettings(settings);
        }
        updateValidity();
        return;
      }
    }
  }
}

void DeviceEditDialog::setAcceptEnabled(bool st)
{
  m_okButton->setEnabled(st);
  m_invalidLabel->setVisible(!st && this->m_protocolWidget);
}

void DeviceEditDialog::setBrowserEnabled(bool st)
{
  if(!st)
  {
    clearEnumerators();

    delete m_column1Stack;
    m_column1Stack = nullptr;
    m_protocols = nullptr;
    m_presets = nullptr;
    m_protocolsSearch = nullptr;
    m_presetsSearch = nullptr;
    delete m_protocolsTabButton;
    m_protocolsTabButton = nullptr;
    delete m_presetsTabButton;
    m_presetsTabButton = nullptr;
    delete m_devicesSearch;
    m_devicesSearch = nullptr;
    delete m_devices;
    m_devices = nullptr;
    delete m_devicesLabel;
    m_devicesLabel = nullptr;
  }
}

void DeviceEditDialog::updateValidity()
{
  switch(m_mode)
  {
    case Mode::Creating:
      setAcceptEnabled(m_model.checkDeviceInstantiatable(getSettings()));
      break;
    case Mode::Editing:
      setAcceptEnabled(m_model.checkDeviceEditable(m_originalName, getSettings()));
      break;
  }
}
}
