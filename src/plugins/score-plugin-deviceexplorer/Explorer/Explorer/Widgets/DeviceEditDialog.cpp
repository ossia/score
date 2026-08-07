// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceEditDialog.hpp"

#include <Device/Loading/ScoreDeviceLoader.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/File.hpp>
#include <score/tools/FilePath.hpp>
#include <score/tools/RecursiveWatch.hpp>
#include <score/widgets/MarginLess.hpp>
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

#include <wobjectimpl.h>

#include <utility>
W_OBJECT_IMPL(Explorer::DeviceEditDialog)
namespace Explorer
{
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

  m_protocols = new QTreeWidget{this};
  m_protocols->header()->hide();
  m_protocols->setSelectionMode(QAbstractItemView::SingleSelection);
  m_column1Stack->addWidget(m_protocols);

  m_presets = new QTreeWidget{this};
  m_presets->header()->hide();
  m_presets->setSelectionMode(QAbstractItemView::SingleSelection);
  m_column1Stack->addWidget(m_presets);

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

static void setCategoryStyle(QTreeWidgetItem* catItem)
{
  auto font = catItem->font(0);
  font.setPixelSize(13);
  font.setBold(true);
  catItem->setFont(0, font);
  catItem->setExpanded(true);
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

  // What may be added at all. A document whose score runs on another machine
  // says so through its catalog, and then it is that machine's protocols that
  // are worth offering -- ours are unreachable from there.
  struct Listed
  {
    UuidKey<Device::ProtocolFactory> key;
    QString name;
    QString category;
    Device::DeviceSettings defaults;
    int priority{};
  };
  std::vector<Listed> listed;

  if(auto* cat = catalog())
  {
    for(const auto& p : cat->protocols())
    {
      Device::DeviceSettings def;
      if(auto* fac = m_protocolList.get(p.key))
        def = fac->defaultSettings();
      else
      {
        def.protocol = p.key;
        def.name = p.name;
      }
      listed.push_back(Listed{p.key, p.name, p.category, def, 0});
    }
  }
  else
  {
    for(auto& prot : m_protocolList)
      listed.push_back(Listed{
          prot.concreteKey(), prot.prettyName(), prot.category(),
          prot.defaultSettings(), prot.visualPriority()});
  }

  ossia::sort(listed, [](const Listed& lhs, const Listed& rhs) {
    return lhs.priority > rhs.priority
           || (lhs.priority == rhs.priority && lhs.name < rhs.name);
  });

  for(const auto& prot : listed)
  {
    auto cat_list = m_protocols->findItems(prot.category, Qt::MatchFixedString);
    QTreeWidgetItem* categoryItem{};
    if(cat_list.size() == 0)
    {
      categoryItem = new QTreeWidgetItem;
      categoryItem->setText(0, prot.category);
      categoryItem->setFlags(Qt::ItemIsEnabled);
      m_protocols->addTopLevelItem(categoryItem);
    }
    else
    {
      categoryItem = cat_list.first();
    }

    auto item = new QTreeWidgetItem{categoryItem};
    item->setText(0, prot.name);
    item->setData(0, Qt::UserRole, QVariant::fromValue(prot.key));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_previousSettings.append(prot.defaults);
  }

  m_protocols->sortItems(0, Qt::AscendingOrder);

  for(int i = 0; i < m_protocols->topLevelItemCount(); i++)
  {
    setCategoryStyle(m_protocols->topLevelItem(i));
  }

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
      auto item = new QTreeWidgetItem;
      item->setText(0, basename);
      item->setData(0, Qt::UserRole, absolutePath);
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      m_presets->addTopLevelItem(item);
    };
  }});
  r.setWatchedFolder(rootPath.toStdString() + "/packages");
  r.scanAsync(this);

  m_presets->sortItems(0, Qt::AscendingOrder);
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
  m_devicesLabel->setVisible(false);
  if(m_splitter->count() > 0)
    m_splitter->widget(0)->hide();

  // Create the correct settings widget for this protocol
  // No factory, no form: the widget is C++ in a plug-in this build does not
  // have. Such a protocol can still be used through what the other machine
  // enumerates, since those come with the settings it wrote itself.
  m_protocolNameLabel->setText(
      protocol ? tr("Settings (%1)").arg(protocol->prettyName())
               : tr("Configured on the other machine"));
  m_protocolWidget = protocol ? protocol->makeSettingsWidget() : nullptr;

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

Device::DeviceCatalog* DeviceEditDialog::catalog() const noexcept
{
  return m_model.deviceModel().catalog();
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

  // The hardware to offer. Through the catalog when the document has one --
  // then it is the other machine's, listed as its answers arrive -- and
  // otherwise through the protocol's own enumerators, which look at this one.
  if(auto* cat = catalog())
  {
    auto addRemote
        = [this](const QString& name, const Device::DeviceSettings& settings) {
      auto item = new QTreeWidgetItem;
      item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setText(0, name);
      item->setData(0, Qt::UserRole, QVariant::fromValue(settings));
      m_devices->addTopLevelItem(item);
      m_devices->setVisible(true);
      m_devicesLabel->setVisible(true);
    };

    m_devices->setRootIsDecorated(false);
    m_devices->setExpandsOnDoubleClick(false);
    cat->enumerate(key, addRemote);
  }
  else if(protocol)
  {
    for(auto [name, e] : protocol->getEnumerators(*doc))
      m_enumerators.emplace_back(name, e);
  }

  std::sort(m_enumerators.begin(), m_enumerators.end(),
      [](const auto& a, const auto& b) { return a.first < b.first; });
  if(!m_enumerators.empty())
  {
    m_devices->setVisible(true);
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
      auto cat = new QTreeWidgetItem{};
      setCategoryStyle(cat);
      cat->setText(0, name);
      cat->setFlags(Qt::ItemIsEnabled);
      m_devices->addTopLevelItem(cat);

      auto addItem = [cat](const QString& name, const Device::DeviceSettings& settings) {
        auto item = new QTreeWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, QVariant::fromValue(settings));
        cat->addChild(item);
        cat->setExpanded(true);
      };
      auto rmItem = [cat](const QString& name) {
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
    m_devicesLabel->setVisible(false);
    m_splitter->widget(0)->hide();
  }
  m_protocolNameLabel->setText(
      protocol ? tr("Settings (%1)").arg(protocol->prettyName())
               : tr("Configured on the other machine"));
  m_protocolWidget = protocol ? protocol->makeSettingsWidget() : nullptr;

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
    delete m_protocolsTabButton;
    m_protocolsTabButton = nullptr;
    delete m_presetsTabButton;
    m_presetsTabButton = nullptr;
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
