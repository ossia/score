#include "libsimpleio/libgpio.h"
#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_EVDEV)
#include "EvdevProtocolFactory.hpp"
#include "EvdevProtocolSettingsWidget.hpp"
#include "EvdevSpecificSettings.hpp"

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

W_OBJECT_IMPL(Protocols::EvdevProtocolSettingsWidget)

namespace Protocols
{
EvdevProtocolSettingsWidget::EvdevProtocolSettingsWidget(QWidget* parent)
    : Device::ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  m_deviceNameEdit->setText("Evdev");

  auto layout = new QFormLayout;
  layout->addRow(tr("Name"), m_deviceNameEdit);

  setLayout(layout);
}

EvdevProtocolSettingsWidget::~EvdevProtocolSettingsWidget() { }

Device::DeviceSettings EvdevProtocolSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = EvdevProtocolFactory::static_concreteKey();

  return s;
}

void EvdevProtocolSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  QString n = settings.name;
  ossia::net::sanitize_device_name(n);
  m_deviceNameEdit->setText(n);
  const auto& specif = settings.deviceSpecificSettings.value<EvdevSpecificSettings>();
}
}
#endif
