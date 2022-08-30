#include "Kinect2Device.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>

#include <score/serialization/MimeVisitor.hpp>

#include <ossia-qt/name_utils.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMenu>
#include <QMimeData>

#include <libfreenect2/frame_listener.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/logger.h>
#include <libfreenect2/registration.h>

#include <wobjectimpl.h>

#include <iostream>

namespace Gfx::Kinect2
{
struct kinect2_settings
{
  std::string device;
  bool rgb{};
  bool ir{};
  bool depth{};
};

struct kinect2_camera
{
  kinect2_camera();
  ~kinect2_camera();

  void load(kinect2_settings);
  void start();
  void stop();

  ::Video::FrameQueue rgbFrames;
  ::Video::FrameQueue irFrames;
  ::Video::FrameQueue depthFrames;

  void processColor(libfreenect2::Frame* frame);
  void processDepth(libfreenect2::Frame* frame);
  void processIR(libfreenect2::Frame* frame);

private:
  void loop();
  void process(
      libfreenect2::Frame* colorFrame, libfreenect2::Frame* irFrame,
      libfreenect2::Frame* depthFrame);

  libfreenect2::Freenect2Device* m_dev{};
  libfreenect2::PacketPipeline* m_pipeline{};
  libfreenect2::FrameListener* m_listener{};
  libfreenect2::FrameMap m_frames;
  //std::thread m_thread;
  std::atomic_bool m_running{};
  int types{};
};

class kinect2_decoder : public ::Video::ExternalInput
{
  ::Video::FrameQueue& queue;

public:
  const QString filter;
  kinect2_decoder(
      ::Video::FrameQueue& queue, int width, int height, AVPixelFormat format,
      QString filter)
      : queue{queue}
      , filter{filter}
  {
    this->width = width;
    this->height = height;
    this->pixel_format = format;
    this->fps = 30;
    this->realTime = true;
    this->dts_per_flicks = 0;
    this->flicks_per_dts = 0;
  }

  ~kinect2_decoder() { queue.drain(); }

  bool start() noexcept override;
  void stop() noexcept override;
  AVFrame* dequeue_frame() noexcept override { return queue.dequeue(); }

  void release_frame(AVFrame* frame) noexcept override { return queue.release(frame); }
};

class kinect2_protocol : public ossia::net::protocol_base
{
public:
  kinect2_camera kinect2;
  kinect2_protocol(const kinect2_settings& stgs);

  bool pull(ossia::net::parameter_base&) override;
  bool push(const ossia::net::parameter_base&, const ossia::value& v) override;
  bool push_raw(const ossia::net::full_parameter_data&) override;
  bool observe(ossia::net::parameter_base&, bool) override;
  bool update(ossia::net::node_base& node_base) override;

  void start_execution() override;
  void stop_execution() override;
};
}

namespace Gfx::Kinect2
{
struct Kinect2Context
{
  libfreenect2::Freenect2 freenect2;

  static Kinect2Context& instance()
  {
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

kinect2_camera::kinect2_camera() { }

struct Listener : libfreenect2::FrameListener
{

  // FrameListener interface
  kinect2_camera& self;

public:
  Listener(kinect2_camera& self, int types)
      : self{self}
  {
  }
  bool onNewFrame(libfreenect2::Frame::Type type, libfreenect2::Frame* frame) override
  {
    switch(type)
    {
      case libfreenect2::Frame::Type::Color:
        self.processColor(frame);
        break;
      case libfreenect2::Frame::Type::Depth:
        self.processDepth(frame);
        break;
      case libfreenect2::Frame::Type::Ir:
        self.processIR(frame);
        break;
    }

    return false;
  }
};

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
    return;
  }

  int foundSerial = -1;
  for(int i = 0; i < numDevices; i++)
  {
    if(freenect2.getDeviceSerialNumber(i) == set.device)
    {
      foundSerial = i;
      break;
    }
  }
  if(foundSerial == -1)
    set.device = freenect2.getDefaultDeviceSerialNumber();

  m_dev = freenect2.openDevice(set.device, m_pipeline);
  if(!m_dev)
  {
    qDebug("Could not create Kinect device");
    return;
  }
  types = 0;
  if(set.rgb)
    types |= libfreenect2::Frame::Color;
  if(set.ir)
    types |= libfreenect2::Frame::Ir;
  if(set.depth)
    types |= libfreenect2::Frame::Depth;

  m_listener = new Listener(*this, types);

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
  if(enable_rgb && enable_depth)
  {
    if(!m_dev->start())
      return;
  }
  else
  {
    if(!m_dev->startStreams(enable_rgb, enable_depth))
      return;
  }

  m_running = true;
}

void kinect2_camera::stop()
{
  bool was_running = m_running;
  m_running = false;
  if(m_dev && was_running)
  {
    m_dev->stop();
    qDebug("stopped");
  }
}

void kinect2_camera::processColor(libfreenect2::Frame* colorFrame)
{
  if(rgbFrames.size() < 16)
  {
    if(libfreenect2::Frame* frame = colorFrame)
    {
      auto f = rgbFrames.newFrame();
      const auto bytes = frame->width * frame->height * frame->bytes_per_pixel;

      const auto storage = Video::initFrameBuffer(*f, bytes);
      memcpy(storage, frame->data, bytes);
      f->linesize[0] = frame->width * frame->bytes_per_pixel;
      f->format = AVPixelFormat::AV_PIX_FMT_BGR0;
      f->best_effort_timestamp = 0;
      f->width = frame->width;
      f->height = frame->height;

      rgbFrames.enqueue(f.release());
    }
  }
}
void kinect2_camera::processIR(libfreenect2::Frame* irFrame)
{
  if(irFrames.size() < 16)
  {
    if(libfreenect2::Frame* frame = irFrame)
    {
      auto f = irFrames.newFrame();
      const auto bytes = frame->width * frame->height * frame->bytes_per_pixel;

      const auto storage = Video::initFrameBuffer(*f, bytes);
      memcpy(storage, frame->data, bytes);
      f->linesize[0] = frame->width * frame->bytes_per_pixel;
      f->format = AVPixelFormat::AV_PIX_FMT_GRAYF32LE;
      f->best_effort_timestamp = 0;
      f->width = frame->width;
      f->height = frame->height;

      irFrames.enqueue(f.release());
    }
  }
}

void kinect2_camera::processDepth(libfreenect2::Frame* depthFrame)
{
  if(depthFrames.size() < 16)
  {
    if(libfreenect2::Frame* frame = depthFrame)
    {
      auto f = depthFrames.newFrame();
      const auto bytes = frame->width * frame->height * frame->bytes_per_pixel;

      const auto storage = Video::initFrameBuffer(*f, bytes);
      memcpy(storage, frame->data, bytes);
      f->linesize[0] = frame->width * frame->bytes_per_pixel;
      f->format = AVPixelFormat::AV_PIX_FMT_GRAYF32LE;
      f->best_effort_timestamp = 0;
      f->width = frame->width;
      f->height = frame->height;

      depthFrames.enqueue(f.release());
    }
  }
}

class kinect2_parameter : public ossia::gfx::texture_input_parameter
{
  GfxExecutionAction* context{};

public:
  std::shared_ptr<kinect2_decoder> decoder;
  int32_t node_id{};
  score::gfx::CameraNode* node{};

  kinect2_parameter(
      const std::shared_ptr<kinect2_decoder>& dec, ossia::net::node_base& n,
      GfxExecutionAction& ctx)
      : ossia::gfx::texture_input_parameter{n}
      , context{&ctx}
      , decoder{dec}
      , node{new score::gfx::CameraNode(decoder, dec->filter)}
  {
    node_id = context->ui->register_node(std::unique_ptr<score::gfx::CameraNode>(node));
  }

  void pull_texture(port_index idx) override
  {
    context->setEdge(port_index{this->node_id, 0}, idx);

    score::gfx::Message m;
    m.node_id = node_id;
    context->ui->send_message(std::move(m));
  }

  virtual ~kinect2_parameter() { context->ui->unregister_node(node_id); }
};

class kinect2_node : public ossia::net::node_base
{
  ossia::net::device_base& m_device;
  node_base* m_parent{};
  std::unique_ptr<kinect2_parameter> m_parameter;

public:
  kinect2_node(
      const std::shared_ptr<kinect2_decoder>& settings, GfxExecutionAction& ctx,
      ossia::net::device_base& dev, std::string name)
      : m_device{dev}
      , m_parameter{std::make_unique<kinect2_parameter>(settings, *this, ctx)}
  {
    m_name = std::move(name);
  }

  kinect2_parameter* get_parameter() const override { return m_parameter.get(); }

private:
  ossia::net::device_base& get_device() const override { return m_device; }
  ossia::net::node_base* get_parent() const override { return m_parent; }
  ossia::net::node_base& set_name(std::string) override { return *this; }
  ossia::net::parameter_base* create_parameter(ossia::val_type) override
  {
    return m_parameter.get();
  }
  bool remove_parameter() override { return false; }

  std::unique_ptr<ossia::net::node_base> make_child(const std::string& name) override
  {
    return {};
  }
  void removing_child(ossia::net::node_base& node_base) override { }
};

class kinect2_device : public ossia::net::generic_device
{
public:
  kinect2_device(
      const kinect2_settings& settings, GfxExecutionAction& ctx,
      std::unique_ptr<kinect2_protocol> proto, std::string name);
};

kinect2_device::kinect2_device(
    const kinect2_settings& settings, GfxExecutionAction& ctx,
    std::unique_ptr<kinect2_protocol> proto, std::string name)
    : ossia::net::generic_device{std::move(proto), name}
{
  auto& k = ((kinect2_protocol*)m_protocol.get())->kinect2;
  if(settings.rgb)
  {
    auto decoder = std::shared_ptr<kinect2_decoder>(
        new kinect2_decoder{k.rgbFrames, 1920, 1080, AV_PIX_FMT_BGR0, QString{}});
    this->add_child(
        std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "rgb"));
  }
  if(settings.ir)
  {
    auto decoder = std::shared_ptr<kinect2_decoder>(new kinect2_decoder{
        k.irFrames, 512, 424, AV_PIX_FMT_GRAYF32LE,
        "processed.rgb = tex.rrr / 4000; processed.a = 1;"});
    this->add_child(
        std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "ir"));
  }
  if(settings.depth)
  {
    auto decoder = std::shared_ptr<kinect2_decoder>(new kinect2_decoder{
        k.depthFrames, 512, 424, AV_PIX_FMT_GRAYF32LE,
        "processed.rgb = tex.rrr / 4000; processed.a = 1;"});
    this->add_child(
        std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "depth"));
  }
}

kinect2_protocol::kinect2_protocol(const kinect2_settings& stgs)
    : ossia::net::protocol_base{flags{}}
{
  kinect2.load(stgs);
}

bool kinect2_protocol::pull(ossia::net::parameter_base&)
{
  return false;
}

bool kinect2_protocol::push(const ossia::net::parameter_base&, const ossia::value& v)
{
  return false;
}

bool kinect2_protocol::push_raw(const ossia::net::full_parameter_data&)
{
  return false;
}

bool kinect2_protocol::observe(ossia::net::parameter_base&, bool)
{
  return false;
}

bool kinect2_protocol::update(ossia::net::node_base& node_base)
{
  return false;
}

void kinect2_protocol::start_execution()
{
  kinect2.start();
}

void kinect2_protocol::stop_execution()
{
  kinect2.stop();
}

bool kinect2_decoder::start() noexcept
{
  return true;
}

void kinect2_decoder::stop() noexcept { }

}

namespace Gfx::Kinect2
{

class Kinect2Enumerator : public Device::DeviceEnumerator
{
public:
  void enumerate(std::function<void(const Device::DeviceSettings&)> f) const override
  {
    auto& freenect2 = Kinect2::Kinect2Context::instance().freenect2;
    const int numDevices = freenect2.enumerateDevices();

    for(int i = 0; i < numDevices; i++)
    {
      Device::DeviceSettings s;
      Kinect2Settings sset;
      sset.input = QString::fromStdString(freenect2.getDeviceSerialNumber(i));
      s.name = "Kinect2";

      if(numDevices > 1)
      {
        s.name += QStringLiteral("_%1").arg(i);
      }

      s.protocol = ProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(sset);
      f(s);
    }
  }
};

class Kinect2SettingsWidget final : public SharedInputSettingsWidget
{
public:
  Kinect2SettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;
};

Kinect2SettingsWidget::Kinect2SettingsWidget(QWidget* parent)
    : SharedInputSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  setLayout(layout);

  m_deviceNameEdit->setText("Kinect2");
}

Device::DeviceSettings Kinect2SettingsWidget::getSettings() const
{
  Device::DeviceSettings s = m_settings;
  s.name = m_deviceNameEdit->text();
  s.protocol = ProtocolFactory::static_concreteKey();
  return s;
}

void Kinect2SettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_settings = settings;
  m_deviceNameEdit->setText(settings.name);
}

class InputDevice final : public Gfx::GfxInputDevice
{
public:
  using GfxInputDevice::GfxInputDevice;
  ~InputDevice();

private:
  bool reconnect() override;
  ossia::net::device_base* getDevice() const override { return m_dev.get(); }

  kinect2_protocol* m_protocol{};
  mutable std::unique_ptr<kinect2_device> m_dev;
};

InputDevice::~InputDevice() { }

bool InputDevice::reconnect()
{
  disconnect();

  try
  {
    auto set = this->settings().deviceSpecificSettings.value<Kinect2Settings>();

    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if(plug)
    {
      kinect2_settings s{
          .device = set.input.toStdString(),
          .rgb = set.rgb,
          .ir = set.ir,
          .depth = set.depth};
      s.rgb = true;
      s.ir = true;
      s.depth = true;

      m_protocol = new kinect2_protocol{s};
      m_dev = std::make_unique<kinect2_device>(
          s, plug->exec, std::unique_ptr<kinect2_protocol>(m_protocol),
          this->settings().name.toStdString());
    }
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

QString ProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Kinect Input");
}

Device::DeviceEnumerator*
ProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* ProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new InputDevice(settings, ctx);
}

const Device::DeviceSettings& ProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Kinect";
    Kinect2Settings specif;
    s.deviceSpecificSettings = QVariant::fromValue(specif);
    return s;
  }();
  return settings;
}

Device::ProtocolSettingsWidget* ProtocolFactory::makeSettingsWidget()
{
  return new Kinect2SettingsWidget;
}
}

template <>
void DataStreamReader::read(const Gfx::Kinect2::Kinect2Settings& n)
{
  m_stream << n.input << n.rgb << n.ir << n.depth;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::Kinect2::Kinect2Settings& n)
{
  m_stream >> n.input >> n.rgb >> n.ir >> n.depth;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::Kinect2::Kinect2Settings& n)
{
  obj["Input"] = n.input;
  obj["RGB"] = n.rgb;
  obj["IR"] = n.ir;
  obj["Depth"] = n.depth;
}

template <>
void JSONWriter::write(Gfx::Kinect2::Kinect2Settings& n)
{
  n.input = obj["Input"].toString();
  n.rgb = obj["RGB"].toBool();
  n.ir = obj["IR"].toBool();
  n.depth = obj["Depth"].toBool();
}
