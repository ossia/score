#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Protocols/Artnet/ArtnetProtocolFactory.hpp>
#include <Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp>
#include <Protocols/Artnet/ArtnetSpecificSettings.hpp>
#include <Protocols/Artnet/FixtureDialog.hpp>
#include <Protocols/Artnet/LEDDialog.hpp>

#include <score/tools/ListNetworkAddresses.hpp>

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

  m_transport = new QComboBox{this};
  m_transport->addItems(
      {"ArtNet", "ArtNet (16 universes)", "E1.31 / sACN", "E1.31 / sACN (16 universes)",
       "DMX USB PRO", "DMX USB PRO Mk2", "OpenDMX USB"});
  checkForChanges(m_transport);

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
  layout->addRow(tr("Transport"), m_transport);
  layout->addRow(tr("Interface"), m_host);

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
  m_fixturesWidget->setHorizontalHeaderLabels(
      {tr("Name"), tr("Mode"), tr("Address"), tr("Channels used")});
  fixtures_layout->addWidget(m_fixturesWidget);

  auto btns = new QVBoxLayout;
  m_addFixture = new QPushButton{"Add fixture"};
  m_addLEDStrip = new QPushButton{"Add LED strip"};
  m_addLEDPane = new QPushButton{"Add LED pane"};
  m_rmFixture = new QPushButton{"Remove"};
  btns->addWidget(m_addFixture);
  btns->addWidget(m_addLEDStrip);
  btns->addWidget(m_addLEDPane);
  btns->addWidget(m_rmFixture);
  btns->addStretch(2);
  fixtures_layout->addLayout(btns);
  layout->addRow(fixtures_layout);

  connect(m_addLEDStrip, &QPushButton::clicked, this, [this] {
    auto dial = new AddLEDStripDialog{*this};
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

  connect(m_addFixture, &QPushButton::clicked, this, [this] {
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

void ArtnetProtocolSettingsWidget::updateHosts(int idx)
{
  m_host->clear();
  switch(idx)
  {
    case 0:
    case 1: {
      auto ips = score::list_ipv4();
      ips.removeAll("0.0.0.0");
      m_host->addItems(ips);
      m_host->setCurrentIndex(0);
      m_universe->setRange(0, 16);
      break;
    }
    case 2:
    case 3:
      m_host->addItems(score::list_ipv4());
      m_host->setCurrentIndex(0);
      m_universe->setRange(1, 65539);
      break;
    case 4: {
      m_universe->setRange(0, 0);
      for(const auto& port : QSerialPortInfo::availablePorts())
        m_host->addItem(port.portName());
      break;
    }
    case 5: {
      m_universe->setRange(0, 1);
      for(const auto& port : QSerialPortInfo::availablePorts())
        m_host->addItem(port.portName());
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
          return led.channels();
        else
          return 0;
      }, fixt.led);
    }
    auto name_item = new QTableWidgetItem{fixt.fixtureName};
    auto mode_item = new QTableWidgetItem{fixt.modeName};
    auto address = new QTableWidgetItem{QString::number(fixt.address + 1)};
    auto controls = new QTableWidgetItem{QString::number(num_controls)};
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
  settings.host = this->m_host->currentText();
  switch(this->m_transport->currentIndex())
  {
    case 0:
      settings.transport = ArtnetSpecificSettings::ArtNetV2;
      break;
    case 1:
      settings.transport = ArtnetSpecificSettings::ArtNet_MultiUniverse;
      break;
    case 2:
      settings.transport = ArtnetSpecificSettings::E131;
      break;
    case 3:
      settings.transport = ArtnetSpecificSettings::E131_MultiUniverse;
      break;
    case 4:
      settings.transport = ArtnetSpecificSettings::DMXUSBPRO;
      break;
    case 5:
      settings.transport = ArtnetSpecificSettings::DMXUSBPRO_Mk2;
      break;
    case 6:
      settings.transport = ArtnetSpecificSettings::OpenDMX_USB;
      break;
  }

  settings.rate = this->m_rate->value();
  settings.universe = this->m_universe->value();
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
    case ArtnetSpecificSettings::ArtNet_MultiUniverse:
      m_transport->setCurrentIndex(1);
      break;
    case ArtnetSpecificSettings::E131:
      m_transport->setCurrentIndex(2);
      break;
    case ArtnetSpecificSettings::E131_MultiUniverse:
      m_transport->setCurrentIndex(3);
      break;
    case ArtnetSpecificSettings::DMXUSBPRO:
      m_transport->setCurrentIndex(4);
      break;
    case ArtnetSpecificSettings::DMXUSBPRO_Mk2:
      m_transport->setCurrentIndex(5);
      break;
    case ArtnetSpecificSettings::OpenDMX_USB:
      m_transport->setCurrentIndex(6);
      break;
  }

  m_rate->setValue(specif.rate);
  m_universe->setValue(specif.universe);
  m_host->setCurrentText(specif.host);
  if(m_host->currentText().isEmpty())
    updateHosts(m_transport->currentIndex());

  if(specif.mode == ArtnetSpecificSettings::Source)
    m_source->setChecked(true);
  else
    m_sink->setChecked(true);
  updateTable();
}
}
#endif
