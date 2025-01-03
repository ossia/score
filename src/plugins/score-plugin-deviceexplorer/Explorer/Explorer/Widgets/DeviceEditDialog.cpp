// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceEditDialog.hpp"

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/widgets/MarginLess.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/TextLabel.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
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

  m_splitter = new QSplitter{this};
  base_layout->addWidget(m_splitter);

  auto column1 = new QWidget;
  auto column1_layout = new score::MarginLess<QVBoxLayout>{column1};
  m_protocolsLabel = new QLabel{tr("Protocols"), this};
  setHeaderTextFormat(m_protocolsLabel);
  column1_layout->addWidget(m_protocolsLabel);
  m_protocolsLabel->setAlignment(Qt::AlignTop);
  m_protocolsLabel->setAlignment(Qt::AlignHCenter);
  m_protocols = new QTreeWidget{this};
  m_protocols->header()->hide();
  m_protocols->setSelectionMode(QAbstractItemView::SingleSelection);
  column1_layout->addWidget(m_protocols);
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
    auto selected_item = m_protocols->selectedItems().first();
    auto key
        = selected_item->data(0, Qt::UserRole).value<UuidKey<Device::ProtocolFactory>>();
    if(key == UuidKey<Device::ProtocolFactory>{})
      return;

    if(auto* proto = m_protocolList.get(key))
      if(auto manual = proto->manual(); !manual.isEmpty())
        QDesktopServices::openUrl(manual);
  });

  initAvailableProtocols();

  connect(
      m_protocols, &QTreeView::activated, this, [this] { selectedProtocolChanged(); });
  connect(m_devices, &QTreeView::activated, this, [this] { selectedDeviceChanged(); });

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
DeviceEditDialog::~DeviceEditDialog() { }

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
      categoryItem = new QTreeWidgetItem;
      categoryItem->setText(0, prot.category());
      categoryItem->setFlags(Qt::ItemIsEnabled);
      m_protocols->addTopLevelItem(categoryItem);
    }
    else
    {
      categoryItem = cat_list.first();
    }

    auto item = new QTreeWidgetItem{categoryItem};
    item->setText(0, prot.prettyName());
    item->setData(0, Qt::UserRole, QVariant::fromValue(prot.concreteKey()));
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    m_previousSettings.append(prot.defaultSettings());
  }

  for(int i = 0; i < m_protocols->topLevelItemCount(); i++)
  {
    setCategoryStyle(m_protocols->topLevelItem(i));
  }

  m_protocols->setRootIsDecorated(false);
  m_protocols->setExpandsOnDoubleClick(false);
  m_index = 0;
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

  auto name = item->text(0);
  auto data = item->data(0, Qt::UserRole).value<Device::DeviceSettings>();

  if(m_protocolWidget)
    m_protocolWidget->setSettings(data);

  updateValidity();
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

  // Clear listener
  m_enumerators.clear();

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
  for(auto [name, e] : protocol->getEnumerators(*doc))
    m_enumerators.emplace_back(name, e);
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

    for(auto& [name, e] : m_enumerators)
    {
      auto cat = new QTreeWidgetItem{};
      setCategoryStyle(cat);
      cat->setText(0, name);
      cat->setFlags(Qt::ItemIsEnabled);
      m_devices->addTopLevelItem(cat);

      auto addItem
          = [&, cat](const QString& name, const Device::DeviceSettings& settings) {
        auto item = new QTreeWidgetItem;
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, QVariant::fromValue(settings));
        cat->addChild(item);
        cat->setExpanded(true);
      };
      auto rmItem = [&, cat](const QString& name) {
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

      connect(e.get(), &Device::DeviceEnumerator::deviceAdded, this, addItem);
      connect(e.get(), &Device::DeviceEnumerator::deviceRemoved, this, rmItem);
      connect(e.get(), &Device::DeviceEnumerator::sort, this, [cat] {
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
  if(m_protocolWidget)
    return m_protocolWidget->getDevice();

  return {};
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
    m_enumerators.clear();

    delete m_protocols;
    m_protocols = nullptr;
    delete m_protocolsLabel;
    m_protocolsLabel = nullptr;
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
