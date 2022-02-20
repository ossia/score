#include "SharedInputSettings.hpp"
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

namespace Gfx
{
SharedInputProtocolFactory::~SharedInputProtocolFactory() = default;
Device::AddressDialog* SharedInputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* SharedInputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

QVariant SharedInputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<SharedInputSettings>(visitor);
}

void SharedInputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<SharedInputSettings>(data, visitor);
}

bool SharedInputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

SharedInputSettingsWidget::SharedInputSettingsWidget(QWidget* parent)
  : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Spout path"), m_shmPath = new QLineEdit);
  setLayout(layout);
}

Device::DeviceSettings SharedInputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  SharedInputSettings set;
  set.path = m_shmPath->text();
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void SharedInputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);

  const auto& set = settings.deviceSpecificSettings.value<SharedInputSettings>();
  m_shmPath->setText(set.path);
}


}
template <>
void DataStreamReader::read(const Gfx::SharedInputSettings& n)
{
  m_stream << n.path;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::SharedInputSettings& n)
{
  m_stream >> n.path;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::SharedInputSettings& n)
{
  obj["Path"] = n.path;
}

template <>
void JSONWriter::write(Gfx::SharedInputSettings& n)
{
  n.path = obj["Path"].toString();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::SharedInputSettings);
