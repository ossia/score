#include "Kinect2Device.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QMenu>
#include <QMimeData>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/registration.h>
#include <libfreenect2/logger.h>
#include <wobjectimpl.h>

namespace Gfx
{
struct Kinect2Context
{
  libfreenect2::Freenect2 freenect2;

  static Kinect2Context& instance() {
    static Kinect2Context inst;
    return inst;
  }

private:
  Kinect2Context()
  {
    // libfreenect2::setGlobalLogger(libfreenect2::createConsoleLogger(libfreenect2::Logger::Debug));
  }

  Kinect2Context(const Kinect2Context&) = delete;
  Kinect2Context(Kinect2Context&&) = delete;
  Kinect2Context& operator=(const Kinect2Context&) = delete;
  Kinect2Context& operator=(Kinect2Context&&) = delete;
};


kinect2_camera::kinect2_camera()
{

}

void kinect2_camera::load(kinect2_settings set)
{
  auto& freenect2 = Kinect2Context::instance().freenect2;

#if defined(LIBFREENECT2_WITH_CUDA_SUPPORT)
  m_pipeline = new libfreenect2::CudaPacketPipeline();
#elif defined(LIBFREENECT2_WITH_OPENCL_SUPPORT)
  m_pipeline = new libfreenect2::OpenCLPacketPipeline();
#elif defined(LIBFREENECT2_WITH_OPENGL_SUPPORT)
  m_pipeline = new libfreenect2::OpenGLPacketPipeline();
#else
  m_pipeline = new libfreenect2::CpuPacketPipeline();
#endif

  const int numDevices = freenect2.enumerateDevices();
  if(numDevices == 0)
  {
    std::cerr << "no device connected!" << std::endl;
    return ;
  }

  int foundSerial = -1;
  for(int i = 0; i < numDevices; i++) {
    if(freenect2.getDeviceSerialNumber(i) == set.device) {
      foundSerial = i;
      break;
    }
  }
  if(foundSerial == -1)
    set.device = freenect2.getDefaultDeviceSerialNumber();

  m_dev = freenect2.openDevice(set.device, m_pipeline);

  types = 0;
  if(set.rgb)
    types |= libfreenect2::Frame::Color;
  if(set.ir)
    types |= libfreenect2::Frame::Ir;
  if(set.depth)
    types |= libfreenect2::Frame::Depth;

  m_listener = new libfreenect2::SyncMultiFrameListener(types);

  m_dev->setColorFrameListener(m_listener);
  m_dev->setIrAndDepthFrameListener(m_listener);
}

kinect2_camera::~kinect2_camera()
{
  if(m_dev)
  {
    qDebug() << "Close:";
    m_dev->close();
  }

  qDebug() << "A:";
  delete m_listener;
  qDebug() << "B:";
  delete m_dev;
  qDebug() << "D:";
  //delete registration;
}

void kinect2_camera::start()
{
  if(!m_dev)
    return;

  const bool enable_rgb = types & libfreenect2::Frame::Color;
  const bool enable_depth = types & libfreenect2::Frame::Depth;
  if (enable_rgb && enable_depth)
  {
    if (!m_dev->start())
      return;
  }
  else
  {
    if (!m_dev->startStreams(enable_rgb, enable_depth))
      return ;
  }

  m_running = true;
  m_thread = std::thread{[this] { this->loop(); }};
}

void kinect2_camera::stop()
{
  m_running = false;
  if(m_dev)
  {
    if(m_thread.joinable())
      m_thread.join();
    m_dev->stop();
  }
}

void kinect2_camera::loop()
{
  while(m_running) {
    libfreenect2::FrameMap frames;
    if (!m_listener->waitForNewFrame(frames, 2000))
    {
      return;
    }

    process(
        frames[libfreenect2::Frame::Color],
        frames[libfreenect2::Frame::Ir],
        frames[libfreenect2::Frame::Depth]);

    m_listener->release(frames);
  }
}

void kinect2_camera::process(libfreenect2::Frame* colorFrame, libfreenect2::Frame* irFrame, libfreenect2::Frame* depthFrame)
{
  if (rgbFrames.size() < 16)
  {
    if(libfreenect2::Frame* frame = colorFrame)
    {
      auto f = rgbFrames.newFrame();
      const auto bytes = frame->width * frame->height * frame->bytes_per_pixel;
      if(!f->data[0])
        f->data[0] = (uint8_t*)malloc(bytes);
      memcpy(f->data[0], frame->data, bytes);

      f->linesize[0] = frame->width * frame->bytes_per_pixel;
      f->format = AVPixelFormat::AV_PIX_FMT_BGR0;
      f->best_effort_timestamp = 0;

      rgbFrames.enqueue(f);
    }
  }

  if (irFrames.size() < 16)
  {
    if(libfreenect2::Frame* frame = irFrame)
    {
      auto f = irFrames.newFrame();
      const auto bytes = frame->width * frame->height * frame->bytes_per_pixel;
      if(!f->data[0])
        f->data[0] = (uint8_t*)malloc(bytes);
      memcpy(f->data[0], frame->data, bytes);

      f->linesize[0] = frame->width * frame->bytes_per_pixel;
      f->format = AVPixelFormat::AV_PIX_FMT_GRAYF32LE;
      f->best_effort_timestamp = 0;

      irFrames.enqueue(f);
    }
  }

  if (depthFrames.size() < 16)
  {
    if(libfreenect2::Frame* frame = depthFrame)
    {
      auto f = depthFrames.newFrame();
      const auto bytes = frame->width * frame->height * frame->bytes_per_pixel;
      if(!f->data[0])
        f->data[0] = (uint8_t*)malloc(bytes);
      memcpy(f->data[0], frame->data, bytes);

      f->linesize[0] = frame->width * frame->bytes_per_pixel;
      f->format = AVPixelFormat::AV_PIX_FMT_GRAYF32LE;
      f->best_effort_timestamp = 0;

      depthFrames.enqueue(f);
    }
  }
}

kinect2_device::kinect2_device(const kinect2_settings& settings, GfxExecutionAction& ctx, std::unique_ptr<kinect2_protocol> proto, std::string name)
  : ossia::net::generic_device{std::move(proto), name}
{
  auto& k = ((kinect2_protocol*)m_protocol.get())->kinect2;
  if(settings.rgb)
  {
    auto decoder = std::make_shared<kinect2_decoder>(k.rgbFrames, 1920, 1080, AV_PIX_FMT_BGR0, QString{});
    this->add_child(std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "rgb"));
  }
  if(settings.ir)
  {
    auto decoder = std::make_shared<kinect2_decoder>(k.irFrames, 512, 424, AV_PIX_FMT_GRAYF32LE, "processed.rgb = tex.rgb / 4000; processed.a = 1;");
    this->add_child(std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "ir"));
  }
  if(settings.depth)
  {
    auto decoder = std::make_shared<kinect2_decoder>(k.depthFrames, 512, 424, AV_PIX_FMT_GRAYF32LE, "processed.rgb = tex.rgb / 4000; processed.a = 1;");
    this->add_child(std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "depth"));
  }
}

kinect2_protocol::kinect2_protocol(const kinect2_settings& stgs)
  : ossia::net::protocol_base{flags{}}
{
  kinect2.load(stgs);
}

bool kinect2_protocol::pull(ossia::net::parameter_base&) { return false; }

bool kinect2_protocol::push(const ossia::net::parameter_base&, const ossia::value& v) { return false; }

bool kinect2_protocol::push_raw(const ossia::net::full_parameter_data&) { return false; }

bool kinect2_protocol::observe(ossia::net::parameter_base&, bool) { return false; }

bool kinect2_protocol::update(ossia::net::node_base& node_base) { return false; }

void kinect2_protocol::start_execution()
{
  kinect2.start();
}

void kinect2_protocol::stop_execution()
{
  kinect2.stop();
}

}

W_OBJECT_IMPL(Gfx::Kinect2Device)

namespace Gfx
{
  Kinect2Device::~Kinect2Device() { }

  bool Kinect2Device::reconnect()
  {
    disconnect();

    try
    {
      auto set = this->settings().deviceSpecificSettings.value<Kinect2Settings>();
      kinect2_settings ossia_stgs{set.input.toStdString(), set.rgb, set.ir, set.depth};

      auto plug = m_ctx.findPlugin<DocumentPlugin>();
      if (plug)
      {
        m_protocol = new kinect2_protocol{ossia_stgs};
        m_dev = std::make_unique<kinect2_device>(
              ossia_stgs,
              plug->exec,
              std::unique_ptr<kinect2_protocol>(m_protocol),
              this->settings().name.toStdString());
      }
      // TODOengine->reload(&proto);
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



  class Kinect2Enumerator : public Device::DeviceEnumerator
  {
  public:
    void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
    {
    auto& freenect2 = Kinect2Context::instance().freenect2;
    const int numDevices = freenect2.enumerateDevices();

    for(int i = 0; i < numDevices; i++) {
      Device::DeviceSettings s;
      Kinect2Settings sset;
      sset.input = QString::fromStdString(freenect2.getDeviceSerialNumber(i));
      s.name = "Kinect2";

      if(numDevices > 1)
      {
        s.name += QStringLiteral("_%1").arg(i);
      }

      s.protocol = Kinect2ProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(sset);
      f(s);
    }
  }
};

QString Kinect2ProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Kinect 2");
}

QString Kinect2ProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* Kinect2ProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return new Kinect2Enumerator;
}

Device::DeviceInterface* Kinect2ProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
    const score::DocumentContext& ctx)
{
  return new Kinect2Device(settings, ctx);
}

const Device::DeviceSettings& Kinect2ProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Kinect2";
    Kinect2Settings specif;
    specif.rgb = true;
    specif.ir = true;
    specif.depth = true;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::AddressDialog* Kinect2ProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* Kinect2ProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* Kinect2ProtocolFactory::makeSettingsWidget()
{
  return new Kinect2SettingsWidget;
}

QVariant Kinect2ProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<Kinect2Settings>(visitor);
}

void Kinect2ProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<Kinect2Settings>(data, visitor);
}

bool Kinect2ProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

Kinect2SettingsWidget::Kinect2SettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  setLayout(layout);

  setDefaults();
}

void Kinect2SettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("Kinect2");
}

Device::DeviceSettings Kinect2SettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = Kinect2ProtocolFactory::static_concreteKey();
  return s;
}

void Kinect2SettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);
}

}

template <>
void DataStreamReader::read(const Gfx::Kinect2Settings& n)
{
  m_stream << n.input << n.rgb << n.ir << n.depth;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Kinect2Settings& n)
{
  m_stream >> n.input >> n.rgb >> n.ir >> n.depth;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Kinect2Settings& n)
{
  obj["Input"] = n.input;
  obj["RGB"] = n.rgb;
  obj["IR"] = n.ir;
  obj["Depth"] = n.depth;
}

template <>
void JSONWriter::write(Gfx::Kinect2Settings& n)
{
  n.input = obj["Input"].toString();
  n.rgb = obj["RGB"].toBool();
  n.ir = obj["IR"].toBool();
  n.depth = obj["Depth"].toBool();
}
