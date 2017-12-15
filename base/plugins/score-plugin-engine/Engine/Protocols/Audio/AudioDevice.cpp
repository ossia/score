#include "AudioDevice.hpp"
#include <ossia/network/generic/generic_parameter.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>
#include <QLineEdit>
#include <QFormLayout>
#include <Engine/Executor/DocumentPlugin.hpp>
namespace Dataflow
{
AudioDevice::AudioDevice(
    const Device::DeviceSettings& settings,
    ossia::net::device_base& dev)
  : OSSIADevice{settings}
  , m_dev{dev}
{
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canRenameNode = false;
  m_capas.canSetProperties = false;
  m_capas.canRefreshTree = true;
  m_capas.canRefreshValue = false;
  m_capas.hasCallbacks = false;
  m_capas.canListen = false;
  m_capas.canSerialize = false;

  reconnect();
}


void AudioDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(false);
}

bool AudioDevice::reconnect()
{
  disconnect();

  try
  {
    AudioSpecificSettings stgs
        = settings().deviceSpecificSettings.value<AudioSpecificSettings>();

    auto& proto = static_cast<ossia::audio_protocol&>(m_dev.get_protocol());
    proto.rate = stgs.rate;
    proto.bufferSize = 256;//stgs.bufferSize;
    proto.reload();

    setLogging_impl(isLogging());
  }
  catch (std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch (...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

void AudioDevice::recreate(const Device::Node& n)
{
  for(auto& child : n)
  {
    addNode(child);
  }
}

Device::Node AudioDevice::refresh()
{
  return simple_refresh();
}

QString AudioProtocolFactory::prettyName() const
{
  return QObject::tr("Audio");
}

Device::DeviceInterface* AudioProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const score::DocumentContext& ctx)
{
  qDebug() << "updating audio" << settings.name ;
  auto doc = ctx.findPlugin<Engine::Execution::DocumentPlugin>();
  if (doc)
  {
    doc->audio_device->updateSettings(settings);
    return doc->audio_device;
  }
  else
    return nullptr;
}

const Device::DeviceSettings& AudioProtocolFactory::defaultSettings() const
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Audio";
    AudioSpecificSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* AudioProtocolFactory::makeSettingsWidget()
{
  return new AudioSettingsWidget;
}

QVariant AudioProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<AudioSpecificSettings>(visitor);
}

void AudioProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<AudioSpecificSettings>(data, visitor);
}

bool AudioProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const
{
  return a.name != b.name;
}


AudioSettingsWidget::AudioSettingsWidget(QWidget* parent)
  : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void AudioSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("audio");
}

Device::DeviceSettings AudioSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();

  AudioSpecificSettings audio;
  audio.card = "default";
  audio.rate = 44100;
  audio.bufferSize = 64;

  s.deviceSpecificSettings = QVariant::fromValue(audio);
  return s;
}

void AudioSettingsWidget::setSettings(
    const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  AudioSpecificSettings audio;
  if (settings.deviceSpecificSettings
      .canConvert<AudioSpecificSettings>())
  {
    audio = settings.deviceSpecificSettings
            .value<AudioSpecificSettings>();
  }
}

}


template <>
void DataStreamReader::read(
    const Dataflow::AudioSpecificSettings& n)
{
  m_stream << n.card << n.bufferSize << n.rate;
  insertDelimiter();
}


template <>
void DataStreamWriter::write(
    Dataflow::AudioSpecificSettings& n)
{
  m_stream >> n.card >> n.bufferSize >> n.rate;
  checkDelimiter();
}


template <>
void JSONObjectReader::read(
    const Dataflow::AudioSpecificSettings& n)
{
  obj["Card"] = n.card;
  obj["BufferSize"] = n.bufferSize;
  obj["Rate"] = n.rate;
}


template <>
void JSONObjectWriter::write(
    Dataflow::AudioSpecificSettings& n)
{
  n.card = obj["Card"].toString();
  n.bufferSize = obj["BufferSize"].toInt();
  n.rate = obj["Rate"].toInt();
}

