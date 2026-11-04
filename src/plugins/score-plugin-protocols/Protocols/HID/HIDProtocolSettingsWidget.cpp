#include "HIDProtocolSettingsWidget.hpp"

#include "HIDProtocolFactory.hpp"
#include "HIDSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <ossia/protocols/hid/hid_device.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::HIDProtocolSettingsWidget)

namespace Protocols
{
HIDProtocolSettingsWidget::HIDProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);
  m_deviceNameEdit->setText("HID");

  m_devices = new QComboBox{this};
  auto refresh = new QPushButton{tr("Rescan"), this};
  auto row = new QHBoxLayout;
  row->addWidget(m_devices, 1);
  row->addWidget(refresh);

  m_details = new QLabel{this};
  m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_details->setWordWrap(true);

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Device"), row);
  layout->addRow(m_details);
  setLayout(layout);

  connect(refresh, &QPushButton::clicked, this, &HIDProtocolSettingsWidget::rescan);
  connect(m_devices, &QComboBox::currentIndexChanged, this, [this](int idx) {
    if(idx < 0)
      return;
    auto set = m_devices->itemData(idx).value<Device::DeviceSettings>();
    m_settings.deviceSpecificSettings = set.deviceSpecificSettings;
    m_deviceNameEdit->setText(set.name);
    updateDetails();
  });

  rescan();
}

HIDProtocolSettingsWidget::~HIDProtocolSettingsWidget() { }

void HIDProtocolSettingsWidget::rescan()
{
  const auto current
      = m_settings.deviceSpecificSettings.value<HIDSpecificSettings>().path;

  QSignalBlocker block{m_devices};
  m_devices->clear();
  int selected = -1;
  for(const auto& d : ossia::net::hid_enumerate())
  {
    auto set = hidDeviceSettings(d);
    if(QString::fromStdString(d.path) == current)
      selected = m_devices->count();
    m_devices->addItem(QString::fromStdString(d.display_name()), QVariant::fromValue(set));
  }
  if(selected >= 0)
    m_devices->setCurrentIndex(selected);
  else
    m_devices->setCurrentIndex(-1);
  updateDetails();
}

void HIDProtocolSettingsWidget::updateDetails()
{
  const auto s = m_settings.deviceSpecificSettings.value<HIDSpecificSettings>();
  if(s.vendorId == 0 && s.productId == 0 && s.path.isEmpty())
  {
    m_details->setText(tr("No device selected"));
    return;
  }

  QString txt = QStringLiteral("%1:%2")
                    .arg(s.vendorId, 4, 16, QChar('0'))
                    .arg(s.productId, 4, 16, QChar('0'));
  if(!s.serial.isEmpty())
    txt += tr(" — serial %1").arg(s.serial);
  if(!s.path.isEmpty())
    txt += QStringLiteral("\n") + s.path;
  if(ossia::net::hid_read_report_descriptor(s.path.toStdString()).empty())
    txt += QStringLiteral("\n")
           + tr("The report descriptor is not readable here: only the raw reports "
                "will be available.");
  m_details->setText(txt);
}

Device::DeviceSettings HIDProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = HIDProtocolFactory::static_concreteKey();
  return s;
}

void HIDProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);
  rescan();
}
}
