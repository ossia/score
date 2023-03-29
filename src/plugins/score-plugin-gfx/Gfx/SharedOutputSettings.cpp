#include "SharedOutputSettings.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

namespace Gfx
{

SharedOutputProtocolFactory::~SharedOutputProtocolFactory() = default;

QString SharedOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::AddressDialog* SharedOutputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* SharedOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

QVariant SharedOutputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<SharedOutputSettings>(visitor);
}

void SharedOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<SharedOutputSettings>(data, visitor);
}

bool SharedOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

SharedOutputSettingsWidget::SharedOutputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  m_layout = new QFormLayout;
  m_layout->addRow(tr("Device Name"), m_deviceNameEdit);
  m_layout->addRow(tr("Shmdata path"), m_shmPath = new QLineEdit);
  m_layout->addRow(tr("Width"), m_width = new QSpinBox);
  m_layout->addRow(tr("Height"), m_height = new QSpinBox);
  m_layout->addRow(tr("Rate"), m_rate = new QSpinBox);

  m_width->setRange(1, 16384);
  m_height->setRange(1, 16384);
  m_rate->setRange(1, 1000);

  setLayout(m_layout);
}

Device::DeviceSettings SharedOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  SharedOutputSettings set;
  set.width = m_width->value();
  set.height = m_height->value();
  set.path = m_shmPath->text();
  set.rate = m_rate->value();

  s.deviceSpecificSettings = QVariant::fromValue(set);

  return s;
}

void SharedOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& set = settings.deviceSpecificSettings.value<SharedOutputSettings>();
  m_shmPath->setText(set.path);
  m_width->setValue(set.width);
  m_height->setValue(set.height);
  m_rate->setValue(set.rate);
}

}

template <>
void DataStreamReader::read(const Gfx::SharedOutputSettings& n)
{
  m_stream << n.path << n.width << n.height << n.rate;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::SharedOutputSettings& n)
{
  m_stream >> n.path >> n.width >> n.height >> n.rate;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::SharedOutputSettings& n)
{
  obj["Path"] = n.path;
  obj["Width"] = n.width;
  obj["Height"] = n.height;
  obj["Rate"] = n.rate;
}

template <>
void JSONWriter::write(Gfx::SharedOutputSettings& n)
{
  n.path = obj["Path"].toString();
  n.width = obj["Width"].toDouble();
  n.height = obj["Height"].toDouble();
  n.rate = obj["Rate"].toDouble();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::SharedOutputSettings);
