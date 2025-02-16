#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Protocols/Artnet/ArtnetProtocolFactory.hpp>
#include <Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>
#include <Protocols/Artnet/FixtureDialog.hpp>
#include <Protocols/Artnet/LEDDialog.hpp>

#include <score/tools/ListNetworkAddresses.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTableWidget>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::ArtnetProtocolSettingsWidget)

namespace Protocols
{
static void updateFixtureAddress(Artnet::Fixture& fixture, int channels_per_universe)
{
  while(fixture.address > channels_per_universe)
  {
    fixture.universe++;
    fixture.address -= channels_per_universe;
  }
}
ArtnetProtocolSettingsWidget::ArtnetProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Artnet");
  checkForChanges(m_deviceNameEdit);

  m_host = new QComboBox{this};
  m_host->setEditable(true);
  checkForChanges(m_host);

  m_rate = new QSpinBox{this};
  m_rate->setRange(0, 240);
  m_rate->setValue(44);

  m_universe = new QSpinBox{this};
  m_universe->setRange(0, 65539);

  m_universe_count = new QSpinBox{this};
  m_universe_count->setRange(0, 1024);

  m_channels_per_universe = new QSpinBox{this};
  m_channels_per_universe->setRange(1, 512);
  m_channels_per_universe->setValue(512);

  m_transport = new QComboBox{this};
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
  updateHosts(m_transport->currentIndex());

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Rate (Hz)"), m_rate);
  layout->addRow(tr("Universe"), m_universe);
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

  auto btns = new QVBoxLayout;
  m_addFixture = new QPushButton{"Add fixture"};
  m_addLEDStrip = new QPushButton{"Add LED strip"};
  //m_addLEDPane = new QPushButton{"Add LED pane"};
  m_rmFixture = new QPushButton{"Remove"};
  btns->addWidget(m_addFixture);
  btns->addWidget(m_addLEDStrip);
  //btns->addWidget(m_addLEDPane);
  btns->addWidget(m_rmFixture);
  btns->addStretch(2);
  fixtures_layout->addLayout(btns);
  layout->addRow(fixtures_layout);

  connect(m_addLEDStrip, &QPushButton::clicked, this, [this] {
    auto dial = new AddLEDStripDialog{*this};
    dial->setName(newFixtureName(dial->name()));
    if(dial->exec() == QDialog::Accepted)
    {
      const int channels_per_universe = this->m_channels_per_universe->value();
      for(auto fixt : dial->fixtures())
      {
        if(!fixt.fixtureName.isEmpty() && fixt.led)
        {
          fixt.fixtureName = newFixtureName(fixt.fixtureName);
          m_fixtures.push_back(std::move(fixt));
          updateFixtureAddress(m_fixtures.back(), channels_per_universe);
        }
      }
      updateTable();
    }
  });

  connect(m_addFixture, &QPushButton::clicked, this, [this] {
    auto dial = new AddFixtureDialog{*this};
    dial->setName(newFixtureName(dial->name()));
    if(dial->exec() == QDialog::Accepted)
    {
      const int channels_per_universe = this->m_channels_per_universe->value();
      for(auto fixt : dial->fixtures())
      {
        if(!fixt.fixtureName.isEmpty())
        {
          fixt.fixtureName = newFixtureName(fixt.fixtureName);
          m_fixtures.push_back(std::move(fixt));
          updateFixtureAddress(m_fixtures.back(), channels_per_universe);
        }
      }
      updateTable();
    }
  });

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

QString ArtnetProtocolSettingsWidget::newFixtureName(QString name)
{
  static std::vector<QString> brethren;
  brethren.clear();
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
      auto ips = score::list_ipv4();
      ips.removeAll("0.0.0.0");
      m_host->addItems(ips);      
      m_host->setCurrentIndex(0);

      m_universe->setRange(0, 256);
      m_universe->setValue(0);
      m_universe->setEnabled(true);
      m_universe_count->setValue(std::clamp(m_universe_count->value(), 0, 256));
      m_universe_count->setEnabled(true);
      m_multicast->setEnabled(false);
      break;
    }
    case 1:
      m_host->addItems(score::list_ipv4());
      m_host->setCurrentIndex(0);

      m_universe->setRange(1, 65539);
      m_universe->setValue(1);
      m_universe->setEnabled(true);
      m_universe_count->setValue(1);
      m_universe_count->setEnabled(true);
      m_multicast->setEnabled(true);
      break;
    case 2:
    case 4: {
      for(const auto& port : serialPorts())
        m_host->addItem(port.portName());

      m_universe->setRange(0, 0);
      m_universe->setValue(0);
      m_universe->setEnabled(false);
      m_universe_count->setValue(1);
      m_universe_count->setEnabled(false);
      m_multicast->setEnabled(false);
      break;
    }
    case 3: {
      for(const auto& port : serialPorts())
        m_host->addItem(port.portName());

      m_universe->setRange(0, 1);
      m_universe->setValue(0);
      m_universe->setEnabled(true);
      m_universe_count->setValue(std::clamp(m_universe_count->value(), 0, 2));
      m_universe_count->setEnabled(true);
      m_multicast->setEnabled(false);
      break;
    }
  }

  if(m_host->currentText().isEmpty())
    m_host->setCurrentIndex(0);
}

void ArtnetProtocolSettingsWidget::updateTable()
{
  while(m_fixturesWidget->rowCount() > 0)
    m_fixturesWidget->removeRow(int(m_fixturesWidget->rowCount()) - 1);

  int row = 0;
  for(auto& fixt : m_fixtures)
  {
    int num_controls = 0;
    if(!fixt.controls.empty())
    {
      num_controls
          = fixt.controls.size(); // FIXME does not handle high precision channels?
    }
    else if(fixt.led)
    {
      num_controls = ossia::apply_nonnull([](const auto& led) {
        if constexpr(requires { led.channels(); })
        {
          return led.channels();
        }
        else
          return 0;
      }, fixt.led);
    }

    auto name_item = new QTableWidgetItem{fixt.fixtureName};
    auto mode_item = new QTableWidgetItem{fixt.modeName};
    auto address = new QTableWidgetItem{QString::number(fixt.address + 1)};
    auto universe = new QTableWidgetItem{QString::number(fixt.universe)};
    auto controls = new QTableWidgetItem{QString::number(num_controls)};
    name_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    mode_item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    address->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    universe->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    controls->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    m_fixturesWidget->insertRow(row);
    m_fixturesWidget->setItem(row, 0, name_item);
    m_fixturesWidget->setItem(row, 1, mode_item);
    m_fixturesWidget->setItem(row, 2, address);
    m_fixturesWidget->setItem(row, 3, universe);
    m_fixturesWidget->setItem(row, 4, controls);
    row++;
  }
}

ArtnetProtocolSettingsWidget::~ArtnetProtocolSettingsWidget() { }

std::pair<int, int> ArtnetProtocolSettingsWidget::universeRange() const noexcept
{
  return {m_universe->value(), m_universe->value() + m_universe_count->value() - 1};
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
  switch(this->m_transport->currentIndex())
  {
    case 0:
      settings.transport = ArtnetSpecificSettings::ArtNetV2;
      break;
    case 1:
      settings.transport = ArtnetSpecificSettings::E131;
      break;
    case 2:
      settings.transport = ArtnetSpecificSettings::DMXUSBPRO;
      break;
    case 3:
      settings.transport = ArtnetSpecificSettings::DMXUSBPRO_Mk2;
      break;
    case 4:
      settings.transport = ArtnetSpecificSettings::OpenDMX_USB;
      break;
  }

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
