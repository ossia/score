#include "libsimpleio/libgpio.h"
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_GPS)
#include "GPSProtocolFactory.hpp"
#include "GPSProtocolSettingsWidget.hpp"
#include "GPSSpecificSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/detail/string_algorithms.hpp>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QTreeWidget>
#include <QVariant>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Protocols::GPSProtocolSettingsWidget)

namespace Protocols
{
GPSProtocolSettingsWidget::GPSProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("GPS");

  m_host = new QLineEdit{"127.0.0.1", this};
  m_port = new QSpinBox{this};
  m_port->setRange(0, 65535);
  m_port->setValue(2947);

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);
  layout->addRow(tr("Host"), m_host);
  layout->addRow(tr("Port"), m_port);

  setLayout(layout);
}

GPSProtocolSettingsWidget::~GPSProtocolSettingsWidget() { }

Device::DeviceSettings GPSProtocolSettingsWidget::getSettings() const
{
  // TODO should be = m_settings to follow the other patterns.
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = GPSProtocolFactory::static_concreteKey();

  GPSSpecificSettings settings{};
  settings.host = this->m_host->text();
  settings.port = this->m_port->value();
  s.deviceSpecificSettings = QVariant::fromValue(settings);

  return s;
}

void GPSProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& specif = settings.deviceSpecificSettings.value<GPSSpecificSettings>();
  m_host->setText(specif.host);
  m_port->setValue(specif.port);
}
}
#endif
