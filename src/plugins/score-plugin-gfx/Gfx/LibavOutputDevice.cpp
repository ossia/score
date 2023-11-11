#include "LibavOutputDevice.hpp"
#if SCORE_HAS_LIBAV
#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/LibavEncoder.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <wobjectimpl.h>

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::LibavOutputSettings);
namespace Gfx
{
// ffmpeg -y  -hwaccel cuda -hwaccel_output_format cuda -f video4linux2 -input_format mjpeg -framerate 30 -i  /dev/video0
// -fflags nobuffer
// -c:v hevc_nvenc
// -preset:v llhq
// -rc constqp
// -zerolatency 1
// -delay 0
// -forced-idr 1
// -g 1
// -cbr 1
// -qp 10
// -f matroska  -

struct EncoderConfiguration
{
  AVPixelFormat hardwareAcceleration{AV_PIX_FMT_NONE};
  std::string encoder;
  int threads{};
};

class libav_output_protocol : public ossia::net::protocol_base
{
public:
  explicit libav_output_protocol(GfxExecutionAction& ctx)
      : protocol_base{flags{}}
      , context{&ctx}
  {
    LibavEncoder encoder;
    encoder.test();
  }
  ~libav_output_protocol() { }

  GfxExecutionAction* context{};
  bool pull(ossia::net::parameter_base&) override { return false; }
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override
  {
    return false;
  }
  bool push_raw(const ossia::net::full_parameter_data&) override { return false; }
  bool observe(ossia::net::parameter_base&, bool) override { return false; }
  bool update(ossia::net::node_base& node_base) override { return false; }

  void start_execution() override
  {
    // Reset and start streaming
  }
  void stop_execution() override
  {
    // Stop streaming
  }
};

class LibavOutputDevice final : public GfxOutputDevice
{
  W_OBJECT(LibavOutputDevice)
public:
  using GfxOutputDevice::GfxOutputDevice;
  ~LibavOutputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  mutable std::unique_ptr<Gfx::video_texture_input_device> m_dev;
};

class LibavOutputSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  LibavOutputSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  Device::DeviceSettings m_settings;
};

LibavOutputDevice::~LibavOutputDevice() { }

bool LibavOutputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<LibavOutputSettings>();
    auto plug = m_ctx.findPlugin<DocumentPlugin>();
    if(plug)
    {
      auto cam = std::make_shared<::Video::CameraInput>();
      /*
      cam->load(
          set.input.toStdString(), set.device.toStdString(), set.size.width(),
          set.size.height(), set.fps, set.codec, set.pixelformat);
*/
      auto m_protocol = new libav_output_protocol{plug->exec};
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

QString LibavOutputProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Libav Output");
}

QString LibavOutputProtocolFactory::category() const noexcept
{
  return StandardCategories::util;
}

Device::DeviceInterface* LibavOutputProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new LibavOutputDevice(settings, ctx);
}

const Device::DeviceSettings&
LibavOutputProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Libav Output";
    LibavOutputSettings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* LibavOutputProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* LibavOutputProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* LibavOutputProtocolFactory::makeSettingsWidget()
{
  return new LibavOutputSettingsWidget;
}

QVariant LibavOutputProtocolFactory::makeProtocolSpecificSettings(
    const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<LibavOutputSettings>(visitor);
}

void LibavOutputProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<LibavOutputSettings>(data, visitor);
}

bool LibavOutputProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

LibavOutputSettingsWidget::LibavOutputSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  setLayout(layout);

  setDefaults();
}

void LibavOutputSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("Libav Output");
}

Device::DeviceSettings LibavOutputSettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = LibavOutputProtocolFactory::static_concreteKey();
  return s;
}

void LibavOutputSettingsWidget::setSettings(const Device::DeviceSettings& settings)
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
void DataStreamReader::read(const Gfx::LibavOutputSettings& n)
{
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::LibavOutputSettings& n)
{
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::LibavOutputSettings& n)
{
}

template <>
void JSONWriter::write(Gfx::LibavOutputSettings& n)
{
}

W_OBJECT_IMPL(Gfx::LibavOutputDevice)
#endif
