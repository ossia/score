#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QSpinBox>
#include <QString>
#include <QVariant>

#include "WSProtocolSettingsWidget.hpp"
#include "WSSpecificSettings.hpp"
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <QPlainTextEdit>
#include <iscore/widgets/JS/JSEdit.hpp>
class QWidget;

namespace Engine
{
namespace Network
{
WSProtocolSettingsWidget::WSProtocolSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  auto deviceNameLabel = new QLabel(tr("Device name"), this);
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto addrLabel = new QLabel(tr("Address"), this);
  m_addressNameEdit = new QLineEdit{this};

  auto codeLabel = new QLabel(tr("Code"), this);
  m_codeEdit = new JSEdit(this);
  m_codeEdit->setSizePolicy(
      QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
  m_codeEdit->setMinimumHeight(300);

  auto layout = new QGridLayout;

  layout->addWidget(deviceNameLabel, 0, 0, 1, 1);
  layout->addWidget(m_deviceNameEdit, 0, 1, 1, 1);
  layout->addWidget(addrLabel, 1, 0, 1, 1);
  layout->addWidget(m_addressNameEdit, 1, 1, 1, 1);

  layout->addWidget(codeLabel, 3, 0, 1, 1);
  layout->addWidget(m_codeEdit, 3, 1, 1, 1);

  setLayout(layout);

  setDefaults();
}

void WSProtocolSettingsWidget::setDefaults()
{
  ISCORE_ASSERT(m_deviceNameEdit);
  ISCORE_ASSERT(m_codeEdit);

  m_deviceNameEdit->setText("newDevice");
  m_codeEdit->setPlainText("");
  m_addressNameEdit->clear();
}

Device::DeviceSettings WSProtocolSettingsWidget::getSettings() const
{
  ISCORE_ASSERT(m_deviceNameEdit);

  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  Network::WSSpecificSettings specific;
  specific.address = m_addressNameEdit->text();
  specific.text = m_codeEdit->toPlainText();

  s.deviceSpecificSettings = QVariant::fromValue(specific);
  return s;
}

void WSProtocolSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  Network::WSSpecificSettings specific;
  if (settings.deviceSpecificSettings
          .canConvert<Network::WSSpecificSettings>())
  {
    specific
        = settings.deviceSpecificSettings.value<Network::WSSpecificSettings>();

    m_addressNameEdit->setText(specific.address);
    m_codeEdit->setPlainText(specific.text);
  }
}
}
}
