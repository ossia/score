#include "CameraDevice.hpp"

#include <Gfx/Graph/VideoNode.hpp>
#include <Video/CameraInput.hpp>

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/CameraDeviceEnumerator.hpp>
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Media/LibavIntrospection.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <wobjectimpl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixdesc.h>
}

namespace Gfx
{
class CameraDevice final : public GfxInputDevice
{
  W_OBJECT(CameraDevice)
public:
  using GfxInputDevice::GfxInputDevice;
  ~CameraDevice();

private:
  void disconnect() override
  {
    Gfx::GfxInputDevice::disconnect();
    auto prev = std::move(m_dev);
    m_dev = {};
    deviceChanged(prev.get(), nullptr);
  }
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  Gfx::video_texture_input_protocol* m_protocol{};
  mutable std::unique_ptr<Gfx::video_texture_input_device> m_dev;
};
}

W_OBJECT_IMPL(Gfx::CameraDevice)

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::CameraSettings);
namespace Gfx
{
void enumerateCameraDevices(std::function<void(CameraSettings, QString)> func);

CameraSettings findBestCameraMode()
{
  std::vector<CameraSettings> candidates;
  candidates.reserve(200);

  // 1. Collect all modes
  enumerateCameraDevices(
      [&](CameraSettings s, const auto&) { candidates.push_back(std::move(s)); });

  if(candidates.empty())
    return {};

  // 2. Define the scoring logic
  auto compute_score = [](const CameraSettings& s) {
    bool viable = true;

    // Not a very good default webcam
    if(s.device.contains("NDI Webcam", Qt::CaseInsensitive))
      viable = false;

    if(s.device.contains("leapmotion", Qt::CaseInsensitive))
      viable = false;

#if defined(_WIN32)
    // Win32 does not support depth well
    if(s.device.contains("depth", Qt::CaseInsensitive))
      viable = false;
#endif

    bool has_color = true;

    // If it's a raw format, check pixel format descriptors
    if(s.codec == AV_CODEC_ID_NONE || s.codec == AV_CODEC_ID_RAWVIDEO)
    {
      if(const AVPixFmtDescriptor* desc
         = av_pix_fmt_desc_get((AVPixelFormat)s.pixelformat))
      {
        // Reject if it explicitly looks like a bayer format,
        // has < 3 components, or is bitstream/paletted.
        if(desc->nb_components < 3 || (desc->flags & AV_PIX_FMT_FLAG_PAL))
          has_color = false;

        // Specific check for 1-bit/Monoblack
        if(desc->comp[0].depth <= 1)
          has_color = false;
      }
    }
    else
    {
      // compressed codec: likely to support color.
      has_color = true;
    }

    bool good_enough_fps = s.fps >= 30.0;
    int area = s.size.width() * s.size.height();
    double rawFps = s.fps;

    return std::make_tuple(
        viable,
        has_color,       // Priority 1: Must be color
        good_enough_fps, // Priority 2: Must be >= 30Hz
        area,            // Priority 3: Maximize Resolution
        rawFps           // Priority 4: Maximize Framerate
    );
  };

  // 3. Find the element with the highest score
  auto bestIt = std::max_element(
      candidates.begin(), candidates.end(),
      [&](const CameraSettings& a, const CameraSettings& b) {
    return compute_score(a) < compute_score(b);
  });

  return *bestIt;
}

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

      if(set.input == "default" && set.device == "default")
      {
        set = findBestCameraMode();
      }

      if(set.input.isEmpty() && set.device.isEmpty())
        return false;

      cam->load(
          set.input.toStdString(), set.device.toStdString(), set.size.width(),
          set.size.height(), set.fps, set.codec, set.pixelformat);

      m_protocol = new video_texture_input_protocol{std::move(cam), plug->exec};
      m_dev = std::make_unique<video_texture_input_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          this->settings().name.toStdString());
      deviceChanged(nullptr, m_dev.get());
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

class DefaultCameraEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    Device::DeviceSettings s;
    s.name = "Camera";
    s.protocol = CameraProtocolFactory::static_concreteKey();
    CameraSettings set;
    set.input = "default";
    set.device = "default";

    s.deviceSpecificSettings = QVariant::fromValue(set);
    f("Default Camera", s);
  }
};

class CameraEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    enumerateCameraDevices([&](const CameraSettings& set, QString name) {
      Device::DeviceSettings s;
      s.name = name;
      s.protocol = CameraProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(set);
      f(name, s);
    });
  }
};

class CustomCameraEnumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    CameraSettings set;
    set.input = "";
    set.device = "";
    set.size = {};
    set.fps = {};

    set.codec = 0;
    set.pixelformat = -1;
    set.colorRange = 0;
    set.custom = true;

    Device::DeviceSettings s;
    s.name = "Custom";
    s.protocol = CameraProtocolFactory::static_concreteKey();
    s.deviceSpecificSettings = QVariant::fromValue(set);
    f(s.name, s);
  }
};

QString CameraProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Camera input");
}

QUrl CameraProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/camera-device.html");
}

QString CameraProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerators
CameraProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  Device::DeviceEnumerators enums;
  enums.push_back({"Default", new DefaultCameraEnumerator});
#if !defined(__linux__) && !defined(__APPLE__) && !defined(_WIN32)
  enums.push_back({"Cameras", new CameraEnumerator});
#else
  auto devices = Gfx::make_camera_enumerator();
  devices->registerAllEnumerators(enums);
#endif
  enums.push_back({"Custom", new CustomCameraEnumerator});
  return enums;
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
  return true;
}

class CameraSettingsWidget final : public Device::ProtocolSettingsWidget
{
public:
  explicit CameraSettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;

private:
  void setDefaults();
  QLineEdit* m_deviceNameEdit{};
  QLineEdit* m_device{};
  QComboBox* m_input{};

  QFormLayout* m_layout{};

  Device::DeviceSettings m_settings;
};

Device::ProtocolSettingsWidget* CameraProtocolFactory::makeSettingsWidget()
{
  return new CameraSettingsWidget;
}

CameraSettingsWidget::CameraSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  m_device = new QLineEdit{this};
  m_input = new QComboBox{this};

  m_layout = new QFormLayout;
  m_layout->addRow(tr("Device Name"), m_deviceNameEdit);
  m_layout->addRow(tr("Device"), m_device);
  m_layout->addRow(tr("Input"), m_input);

#if QT_VERSION > QT_VERSION_CHECK(6, 4, 0)
  m_layout->setRowVisible(1, false);
  m_layout->setRowVisible(2, false);
#endif

  const auto& info = LibavIntrospection::instance();
  for(auto& demux : info.demuxers)
  {
    QString name = demux.format->name;
    if(demux.format->long_name && strlen(demux.format->long_name) > 0)
    {
      name += " (";
      name += demux.format->long_name;
      name += ")";
    }
    m_input->addItem(name, QVariant::fromValue((void*)demux.format));
  }

  setLayout(m_layout);

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
  CameraSettings specif = s.deviceSpecificSettings.value<CameraSettings>();
  if(specif.custom)
  {
    specif.device = m_device->text();
    specif.input = m_input->currentText();
  }
  s.deviceSpecificSettings = QVariant::fromValue(std::move(specif));
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

  const CameraSettings& set = settings.deviceSpecificSettings.value<CameraSettings>();
  m_device->setText(set.device);
  m_input->setCurrentText(set.input);

#if QT_VERSION > QT_VERSION_CHECK(6, 4, 0)
  m_layout->setRowVisible(1, set.custom);
  m_layout->setRowVisible(2, set.custom);
#endif
}

}

template <>
void DataStreamReader::read(const Gfx::CameraSettings& n)
{
  m_stream << n.input << n.device << n.size.width() << n.size.height() << n.fps
           << n.codec << n.pixelformat << n.colorRange << n.custom;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::CameraSettings& n)
{
  m_stream >> n.input >> n.device >> n.size.rwidth() >> n.size.rheight() >> n.fps
      >> n.codec >> n.pixelformat >> n.colorRange >> n.custom;
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
  obj["ColorRange"] = n.colorRange;
  obj["Custom"] = n.custom;
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
  if(auto range = obj.tryGet("ColorRange"))
    n.colorRange = range->toInt();
  if(auto custom = obj.tryGet("Custom"))
    n.custom = custom->toBool();
}
