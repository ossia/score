#include <ossia/detail/config.hpp>

#if defined(OSSIA_PROTOCOL_ARTNET)
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Protocols/Artnet/ArtnetProtocolFactory.hpp>
#include <Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>
#include <Protocols/Artnet/DMXFixtureInstantiation.hpp>
#include <Protocols/Artnet/FixtureDialog.hpp>
#include <Protocols/Artnet/LEDDialog.hpp>

#include <score/tools/ListNetworkAddresses.hpp>
#include <score/widgets/HelpInteraction.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QColor>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QTableWidget>

#include <wobjectimpl.h>

#include <span>

W_OBJECT_IMPL(Protocols::ArtnetProtocolSettingsWidget)

namespace Protocols
{
using Artnet::usedChannels;

namespace
{
ArtnetSpecificSettings::Transport transportForIndex(int idx) noexcept
{
  switch(idx)
  {
    case 1:
      return ArtnetSpecificSettings::E131;
    case 2:
      return ArtnetSpecificSettings::DMXUSBPRO;
    case 3:
      return ArtnetSpecificSettings::DMXUSBPRO_Mk2;
    case 4:
      return ArtnetSpecificSettings::OpenDMX_USB;
    case 0:
    default:
      return ArtnetSpecificSettings::ArtNetV2;
  }
}
}

ArtnetProtocolSettingsWidget::ArtnetProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("DMX");
  checkForChanges(m_deviceNameEdit);

  m_host = new QComboBox{this};
  m_host->setEditable(true);
  checkForChanges(m_host);

  m_rate = new QSpinBox{this};
  m_rate->setRange(0, 240);
  m_rate->setValue(44);

  m_universe = new QSpinBox{this};
  m_universe->setObjectName("universe");

  m_universe_count = new QSpinBox{this};
  m_universe_count->setObjectName("universeCount");
  m_universe_count->setRange(1, 1024);

  m_channels_per_universe = new QSpinBox{this};
  m_channels_per_universe->setRange(1, 512);
  m_channels_per_universe->setValue(512);

  m_transport = new QComboBox{this};
  m_transport->setObjectName("transport");
  m_transport->addItems(
      {"ArtNet", "E1.31 / sACN", "DMX USB PRO", "DMX USB PRO Mk2", "OpenDMX USB"});
  checkForChanges(m_transport);

  m_multicast = new QCheckBox{this};
  m_multicast->setChecked(false);

  m_source = new QRadioButton{tr("Send DMX"), this};
  m_sink = new QRadioButton{tr("Receive DMX"), this};
  m_source->setChecked(true);

  connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &ArtnetProtocolSettingsWidget::updateHosts);
  connect(
      m_universe, &QSpinBox::valueChanged, this,
      &ArtnetProtocolSettingsWidget::updateUniverseCountRange);
  updateHosts(m_transport->currentIndex());

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate (Hz)"), m_rate);
  layout->addRow(tr("Universe"), m_universe);
  score::setHelp(m_universe, tr("Universe starts at 0 for ArtNet, 1 for E1.31 / sACN"));
  layout->addRow(tr("Universe count"), m_universe_count);
  layout->addRow(tr("Channels in universe"), m_channels_per_universe);
  layout->addRow(tr("Transport"), m_transport);
  layout->addRow(tr("Interface / Host"), m_host);
  layout->addRow(tr("Multicast (E1.31)"), m_multicast);

  auto radiolay = new QHBoxLayout{};
  radiolay->addWidget(m_source);
  radiolay->addWidget(m_sink);
  layout->addRow(tr("Mode"), radiolay);

  auto fixtures_layout = new QHBoxLayout;
  m_fixturesWidget = new QTableWidget;

  m_fixturesWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_fixturesWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_fixturesWidget->insertColumn(0);
  m_fixturesWidget->insertColumn(1);
  m_fixturesWidget->insertColumn(2);
  m_fixturesWidget->insertColumn(3);
  m_fixturesWidget->insertColumn(4);
  m_fixturesWidget->setHorizontalHeaderLabels(
      {tr("Name"), tr("Mode"), tr("Address"), tr("Universe"), tr("Channels used")});
  fixtures_layout->addWidget(m_fixturesWidget);

  struct AddressEditDelegate : public QStyledItemDelegate
  {
    ArtnetProtocolSettingsWidget& m_self;
    explicit AddressEditDelegate(ArtnetProtocolSettingsWidget& self)
        : QStyledItemDelegate{&self}
        , m_self{self}
    {
    }
    QWidget* createEditor(
        QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
      auto edit = static_cast<QSpinBox*>(
          QStyledItemDelegate::createEditor(parent, option, index));
      // FIXME technically should be minus the current fixture dmx address

      edit->setRange(1, m_self.m_channels_per_universe->value());
      return edit;
    }
    void setModelData(
        QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override
    {
      QStyledItemDelegate::setModelData(editor, model, index);
      m_self.addressChanged(index.row());
    }
  };
  struct UniverseEditDelegate : public QStyledItemDelegate
  {
    ArtnetProtocolSettingsWidget& m_self;
    explicit UniverseEditDelegate(ArtnetProtocolSettingsWidget& self)
        : QStyledItemDelegate{&self}
        , m_self{self}
    {
    }
    QWidget* createEditor(
        QWidget* parent, const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
      auto edit = static_cast<QSpinBox*>(
          QStyledItemDelegate::createEditor(parent, option, index));

      auto [min, max] = m_self.universeRange();
      edit->setRange(min, max);
      return edit;
    }
    void setModelData(
        QWidget* editor, QAbstractItemModel* model,
        const QModelIndex& index) const override
    {
      QStyledItemDelegate::setModelData(editor, model, index);
      m_self.universeChanged(index.row());
    }
  };
  m_fixturesWidget->setItemDelegateForColumn(2, new AddressEditDelegate{*this});
  m_fixturesWidget->setItemDelegateForColumn(3, new UniverseEditDelegate{*this});

  auto btns = new QVBoxLayout;
  m_addFixture = new QPushButton{"Add fixture"};
  m_addLEDStrip = new QPushButton{"Add LED strip"};
  m_addGenericDimmer = new QPushButton{"Add Dimmer"};
  m_addGenericRGB = new QPushButton{"Add RGB par"};
  m_addLEDPane = new QPushButton{"Add LED pane"};
  m_addLEDBox = new QPushButton{"Add LED box"};
  m_rmFixture = new QPushButton{"Remove"};
  btns->addWidget(m_addFixture);
  btns->addWidget(m_addGenericDimmer);
  btns->addWidget(m_addGenericRGB);
  btns->addWidget(m_addLEDStrip);
  btns->addWidget(m_addLEDPane);
  btns->addWidget(m_addLEDBox);
  btns->addWidget(m_rmFixture);
  btns->addStretch(2);
  fixtures_layout->addLayout(btns);
  layout->addRow(fixtures_layout);

  m_universeWarning = new QLabel{this};
  m_universeWarning->setObjectName("universeWarning");
  m_universeWarning->setWordWrap(true);
  m_universeWarning->setVisible(false);
  layout->addRow(m_universeWarning);

  for(auto* spin : {m_universe, m_universe_count, m_channels_per_universe})
    connect(
        spin, &QSpinBox::valueChanged, this,
        &ArtnetProtocolSettingsWidget::updateUniverseWarning);
  connect(
      m_transport, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &ArtnetProtocolSettingsWidget::updateUniverseWarning);

  // FIXME a cute widget for displaying used addresses in each universe
  connect(m_addGenericDimmer, &QPushButton::clicked, this, [this] {
    addFixture("Generic", "4-Channel Dimmer Pack");
  });
  connect(m_addGenericRGB, &QPushButton::clicked, this, [this] {
    addFixture("Generic", "RGB Fader");
  });

  connect(m_addLEDStrip, &QPushButton::clicked, this, [this] {
    addLEDs(AddLEDStripDialog::Strip);
  });

  connect(m_addLEDPane, &QPushButton::clicked, this, [this] {
    addLEDs(AddLEDStripDialog::Pane);
  });

  connect(m_addLEDBox, &QPushButton::clicked, this, [this] {
    addLEDs(AddLEDStripDialog::Volume);
  });

  connect(m_addFixture, &QPushButton::clicked, this, [this] { addFixture({}, {}); });

  connect(m_rmFixture, &QPushButton::clicked, this, [this] {
    ossia::flat_set<int> rows_to_remove;
    for(auto item : m_fixturesWidget->selectedItems())
    {
      rows_to_remove.insert(item->row());
    }

    for(auto it = rows_to_remove.rbegin(); it != rows_to_remove.rend(); ++it)
    {
      m_fixtures.erase(m_fixtures.begin() + *it);
    }

    updateTable();
  });

  setLayout(layout);
}

void ArtnetProtocolSettingsWidget::addFixture(QString manufacturer, QString name)
{
  const int channels_per_universe = m_channels_per_universe->value();
  auto [universe, address] = Artnet::nextAvailableAddress(m_fixtures, channels_per_universe);
  AddFixtureDialog dial{universe, address, *this};
  if(!manufacturer.isEmpty())
    dial.setCurrentFixture(manufacturer, name);
  dial.setName(newFixtureName(dial.name()));
  if(dial.exec() == QDialog::Accepted)
  {
    for(auto& fixt : dial.fixtures())
    {
      if(!fixt.fixtureName.isEmpty())
        addNewFixture(std::move(fixt), channels_per_universe);
    }
    updateTable();
  }
}

void ArtnetProtocolSettingsWidget::addLEDs(int mode)
{
  const int channels_per_universe = m_channels_per_universe->value();
  auto [universe, address] = Artnet::nextAvailableAddress(m_fixtures, channels_per_universe);
  AddLEDStripDialog dial{universe, address, (AddLEDStripDialog::Mode)mode, *this};
  dial.setName(newFixtureName(dial.name()));
  if(dial.exec() == QDialog::Accepted)
  {
    for(auto& fixt : dial.fixtures())
    {
      if(!fixt.fixtureName.isEmpty() && fixt.led)
        addNewFixture(std::move(fixt), channels_per_universe);
    }
    updateTable();
  }
}

void ArtnetProtocolSettingsWidget::addNewFixture(
    Artnet::Fixture fixt, int channels_per_universe)
{
  fixt.fixtureName = newFixtureName(std::move(fixt.fixtureName));
  Artnet::normalizeAddress(fixt, channels_per_universe);
  m_fixtures.push_back(std::move(fixt));
}

QString ArtnetProtocolSettingsWidget::newFixtureName(QString name)
{
  std::vector<QString> brethren;
  brethren.reserve(m_fixtures.size());

  for(auto& strip : this->m_fixtures)
    brethren.push_back(strip.fixtureName);
  return ossia::net::sanitize_name(std::move(name), brethren);
}

static QList<QSerialPortInfo> serialPorts()
{
#if defined(__APPLE__) || defined(_WIN32)
  return QSerialPortInfo::availablePorts();
#else
  auto ports = QSerialPortInfo::availablePorts();
  std::sort(
      ports.begin(), ports.end(),
      [](const QSerialPortInfo& a, const QSerialPortInfo& b) {
    const auto& lhs = a.portName();
    const auto& rhs = b.portName();
    bool lhs_usb = lhs.contains("USB") || lhs.contains("ACM");
    bool rhs_usb = rhs.contains("USB") || rhs.contains("ACM");
    if(lhs_usb == rhs_usb)
      return lhs < rhs;
    else
      return (lhs_usb && !rhs_usb);
  });
  return ports;
#endif
}

void ArtnetProtocolSettingsWidget::updateHosts(int idx)
{
  m_host->clear();
  switch(idx)
  {
    case 0: {
      auto ips = score::list_ipv4_for_listening();
      ips.removeAll("0.0.0.0");
      m_host->addItems(ips);
      m_host->setCurrentIndex(0);
      m_multicast->setEnabled(false);
      break;
    }
    case 1:
      m_host->addItems(score::list_ipv4_for_listening());
      m_host->setCurrentIndex(0);
      m_universe_count->setValue(1);
      m_multicast->setEnabled(true);
      break;
    case 2:
    case 3:
    case 4: {
      for(const auto& port : serialPorts())
        m_host->addItem(port.portName());
      m_multicast->setEnabled(false);
      break;
    }
  }

  const auto [first, last] = ArtnetSpecificSettings::universeLimits(transportForIndex(idx));
  m_universe->setRange(first, last);
  m_universe->setValue(first);
  m_universe->setEnabled(first != last);
  updateUniverseCountRange();
  m_universe_count->setEnabled(m_universe_count->maximum() > 1);

  if(m_host->currentText().isEmpty())
    m_host->setCurrentIndex(0);
}

void ArtnetProtocolSettingsWidget::updateUniverseCountRange()
{
  const auto [first, last]
      = ArtnetSpecificSettings::universeLimits(transportForIndex(m_transport->currentIndex()));
  m_universe_count->setRange(1, std::clamp(last - m_universe->value() + 1, 1, 1024));
}

void ArtnetProtocolSettingsWidget::updateTable()
{
  while(m_fixturesWidget->rowCount() > 0)
    m_fixturesWidget->removeRow(int(m_fixturesWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_fixtures)
  {
    int num_controls = usedChannels(fixt);

    auto name_item = new QTableWidgetItem{fixt.fixtureName};
    auto mode_item = new QTableWidgetItem{fixt.modeName};
    auto address = new QTableWidgetItem{};
    address->setData(Qt::DisplayRole, QVariant::fromValue(fixt.address + 1));
    address->setData(Qt::EditRole, QVariant::fromValue(fixt.address + 1));
    auto universe = new QTableWidgetItem{};
    universe->setData(Qt::DisplayRole, QVariant::fromValue(fixt.universe));
    universe->setData(Qt::EditRole, QVariant::fromValue(fixt.universe));
    auto controls = new QTableWidgetItem{QString::number(num_controls)};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    universe->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_fixturesWidget->insertRow(row);
    m_fixturesWidget->setItem(row, 0, name_item);
    m_fixturesWidget->setItem(row, 1, mode_item);
    m_fixturesWidget->setItem(row, 2, address);
    m_fixturesWidget->setItem(row, 3, universe);
    m_fixturesWidget->setItem(row, 4, controls);
    row++;
  }
  updateUniverseWarning();
}

void ArtnetProtocolSettingsWidget::updateUniverseWarning()
{
  if(!m_fixturesWidget || !m_universeWarning)
    return;

  const auto [first, last]
      = ArtnetSpecificSettings::universeLimits(transportForIndex(m_transport->currentIndex()));
  const int start = std::clamp(m_universe->value(), first, last);
  const int64_t channels_per_universe = std::max(1, m_channels_per_universe->value());

  QStringList outside;
  for(int row = 0; row < std::ssize(m_fixtures) && row < m_fixturesWidget->rowCount(); row++)
  {
    const auto& fixt = m_fixtures[row];
    const int64_t end = int64_t(fixt.universe) * channels_per_universe + fixt.address
                        + std::max(usedChannels(fixt), 1) - 1;
    const bool fits = fixt.universe >= start && end / channels_per_universe <= last;
    if(auto item = m_fixturesWidget->item(row, 3))
    {
      item->setData(Qt::ForegroundRole, fits ? QVariant{} : QVariant{QColor(Qt::red)});
      item->setToolTip(
          fits ? QString{}
               : tr("This device outputs universes %1 to %2 only").arg(start).arg(last));
    }
    if(!fits)
      outside.push_back(fixt.fixtureName);
  }

  m_universeWarning->setVisible(!outside.isEmpty());
  if(!outside.isEmpty())
    m_universeWarning->setText(
        tr("This device outputs universes %1 to %2 only. Outside of them: %3")
            .arg(start)
            .arg(last)
            .arg(outside.join(", ")));
}

void ArtnetProtocolSettingsWidget::addressChanged(int row)
{
  auto new_value = m_fixturesWidget->item(row, 2)->data(Qt::DisplayRole).toInt();
  m_fixtures[row].address = new_value - 1;
}
void ArtnetProtocolSettingsWidget::universeChanged(int row)
{
  auto new_value = m_fixturesWidget->item(row, 3)->data(Qt::DisplayRole).toInt();
  m_fixtures[row].universe = new_value;
}

ArtnetProtocolSettingsWidget::~ArtnetProtocolSettingsWidget() { }

int ArtnetProtocolSettingsWidget::channelsPerUniverse() const noexcept
{
  return m_channels_per_universe->value();
}

std::pair<int, int> ArtnetProtocolSettingsWidget::universeRange() const noexcept
{
  const auto [first, last]
      = ArtnetSpecificSettings::universeLimits(transportForIndex(m_transport->currentIndex()));
  const int start = std::clamp(m_universe->value(), first, last);
  return {start, std::clamp(start + m_universe_count->value() - 1, start, last)};
}

Device::DeviceSettings ArtnetProtocolSettingsWidget::getSettings() const
{
  // TODO should be = m_settings to follow the other patterns.
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = ArtnetProtocolFactory::static_concreteKey();

  ArtnetSpecificSettings settings{};
  settings.fixtures = this->m_fixtures;
  settings.host = this->m_host->currentText();
  settings.transport = transportForIndex(this->m_transport->currentIndex());

  settings.rate = this->m_rate->value();
  settings.start_universe = this->m_universe->value();
  settings.universe_count = this->m_universe_count->value();
  settings.channels_per_universe = this->m_channels_per_universe->value();
  settings.multicast = this->m_multicast->isChecked();
  settings.mode = this->m_source->isChecked() ? ArtnetSpecificSettings::Source
                                              : ArtnetSpecificSettings::Sink;
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void ArtnetProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& specif = settings.deviceSpecificSettings.value<ArtnetSpecificSettings>();
  m_fixtures = specif.fixtures;

  switch(specif.transport)
  {
    case ArtnetSpecificSettings::ArtNet:
    case ArtnetSpecificSettings::ArtNetV2:
      m_transport->setCurrentIndex(0);
      break;
    case ArtnetSpecificSettings::E131:
      m_transport->setCurrentIndex(1);
      break;
    case ArtnetSpecificSettings::DMXUSBPRO:
      m_transport->setCurrentIndex(2);
      break;
    case ArtnetSpecificSettings::DMXUSBPRO_Mk2:
      m_transport->setCurrentIndex(3);
      break;
    case ArtnetSpecificSettings::OpenDMX_USB:
      m_transport->setCurrentIndex(4);
      break;
  }

  updateHosts(m_transport->currentIndex());

  m_rate->setValue(specif.rate);
  m_universe->setValue(specif.start_universe);
  m_universe_count->setValue(specif.universe_count);
  m_channels_per_universe->setValue(specif.channels_per_universe);
  if(!specif.host.isEmpty())
    m_host->setCurrentText(specif.host);
  else
    m_host->setCurrentIndex(0);
  m_multicast->setChecked(specif.multicast);

  if(specif.mode == ArtnetSpecificSettings::Source)
    m_source->setChecked(true);
  else
    m_sink->setChecked(true);
  updateTable();
}
}
#endif
