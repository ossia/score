#include "CameraDevice.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::CameraDevice)

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::CameraSettings);
namespace Gfx
{
void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func);

CameraDevice::~CameraDevice() { }

bool CameraDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<CameraSettings>();
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto cam = std::make_shared<::Video::CameraInput>();

      cam->load(
          set.input.toStdString(), set.device.toStdString(), set.size.width(),
          set.size.height(), set.fps, set.codec, set.pixelformat);

      m_protocol = new video_texture_input_protocol{std::move(cam), plug->exec};
      m_dev = std::make_unique<video_texture_input_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          this->settings().name.toStdString());
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
  }
  catch(std::exception& e)
  {
    qDebug() << "Could not connect: " << e.what();
  }
  catch(...)
  {
    // TODO save the reason of the non-connection.
  }

  return connected();
}

class CameraEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
  {
    enumerateCameraDevices([&](const CameraSettings& set, QString name) {
      Device::DeviceSettings s;
      s.name = name;
      s.protocol = CameraProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(set);
      f(s);
    });
  }
};

QString CameraProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Camera input");
}

QString CameraProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator*
CameraProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new CameraEnumerator;
}

Device::DeviceInterface* CameraProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new CameraDevice(settings, ctx);
}

const Device::DeviceSettings& CameraProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Camera";
    CameraSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* CameraProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* CameraProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* CameraProtocolFactory::makeSettingsWidget()
{
  return new CameraSettingsWidget;
}

QVariant
CameraProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<CameraSettings>(visitor);
}

void CameraProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<CameraSettings>(data, visitor);
}

bool CameraProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

CameraSettingsWidget::CameraSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  setLayout(layout);

  setDefaults();
}

void CameraSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("camera");
}

Device::DeviceSettings CameraSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = CameraProtocolFactory::static_concreteKey();
  return s;
}

void CameraSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;

  // Clean up the name a bit
  auto prettyName = settings.name;
  if(!prettyName.isEmpty())
  {
    prettyName = prettyName.split(':').front();
    prettyName = prettyName.split('(').front();
    prettyName.remove("/dev/");
    prettyName = prettyName.trimmed();
    ossia::net::sanitize_device_name(prettyName);
  }
  m_deviceNameEdit->setText(prettyName);
}

}

template <>
void DataStreamReader::read(const Gfx::CameraSettings& n)
{
  m_stream << n.input << n.device << n.size.width() << n.size.height() << n.fps
           << n.codec << n.pixelformat;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::CameraSettings& n)
{
  m_stream >> n.input >> n.device >> n.size.rwidth() >> n.size.rheight() >> n.fps
      >> n.codec >> n.pixelformat;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::CameraSettings& n)
{
  obj["Input"] = n.input;
  obj["Device"] = n.device;
  obj["Size"] = n.size;
  obj["FPS"] = n.fps;
  obj["Codec"] = n.codec;
  obj["PixelFormat"] = n.pixelformat;
}

template <>
void JSONWriter::write(Gfx::CameraSettings& n)
{
  n.input = obj["Input"].toString();
  n.device = obj["Device"].toString();
  n.size <<= obj["Size"];
  n.fps = obj["FPS"].toDouble();
  if(auto codec = obj.tryGet("Codec"))
    n.codec = codec->toInt();
  if(auto format = obj.tryGet("PixelFormat"))
    n.pixelformat = format->toInt();
}
