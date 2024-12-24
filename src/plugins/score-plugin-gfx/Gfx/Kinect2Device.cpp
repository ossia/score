#include "Kinect2Device.hpp"

#include <State/MessageListSerialization.hpp>
#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/Graph/VideoNode.hpp>

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
  ossia::net::parameter_base* pcl{};

  void processColor(libfreenect2::Frame* frame);
  void processDepth(libfreenect2::Frame* frame);
  void processIR(libfreenect2::Frame* frame);
  bool process(libfreenect2::Frame::Type, libfreenect2::Frame*);
  void processRegistration();

private:
  void loop();

  libfreenect2::Freenect2Device* m_dev{};
  libfreenect2::PacketPipeline* m_pipeline{};
  libfreenect2::FrameListener* m_listener{};
  libfreenect2::FrameMap m_frames;
  libfreenect2::Registration* m_reg{};
  libfreenect2::Frame* m_lastRGB{};
  libfreenect2::Frame* m_lastD{};
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
    return self.process(type, frame);
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


  // https://github.com/hanseuljun/kinect-to-hololens/blob/b1f012b07519c842f9ad2e97cb7b9583c9b0ff3e/cpp/examples/kinect_sender_demo.cpp
  // Necessary for registration to work - factory params mentioned in freenect2 docs are nowhere to be found
  libfreenect2::Freenect2Device::ColorCameraParams color_params;
  color_params.fx = 1081.37f;
  color_params.fy = 1081.37f;
  color_params.cx = 959.5f;
  color_params.cy = 539.5f;
  color_params.shift_d = 863.0f;
  color_params.shift_m = 52.0f;
  color_params.mx_x3y0 = 0.000433573f;
  color_params.mx_x0y3 = 3.11985e-06f;
  color_params.mx_x2y1 = 4.89289e-05f;
  color_params.mx_x1y2 = 0.000329721f;
  color_params.mx_x2y0 = 0.000753273f;
  color_params.mx_x0y2 = 3.57279e-05f;
  color_params.mx_x1y1 = -0.000761282f;
  color_params.mx_x1y0 = 0.633183f;
  color_params.mx_x0y1 = 0.00562461f;
  color_params.mx_x0y0 = 0.17028f;
  color_params.my_x3y0 = 3.31803e-06f;
  color_params.my_x0y3 = 0.000587018f;
  color_params.my_x2y1 = 0.000425902f;
  color_params.my_x1y2 = 1.76095e-05f;
  color_params.my_x2y0 = -0.0002469f;
  color_params.my_x0y2 = -0.000945311f;
  color_params.my_x1y1 = 0.000648708f;
  color_params.my_x1y0 = -0.00578545f;
  color_params.my_x0y1 = 0.632964f;
  color_params.my_x0y0 = -0.000370404f;

  libfreenect2::Freenect2Device::IrCameraParams ir_params;
  ir_params.fx = 368.147f;
  ir_params.fy = 368.147f;
  ir_params.cx = 264.317f;
  ir_params.cy = 208.964f;
  ir_params.k1 = 0.0807732f;
  ir_params.k2 = -0.27181f;
  ir_params.k3 = 0.103199f;
  ir_params.p1 = 0.0f;
  ir_params.p2 = 0.0f;

  m_dev->setIrCameraParams(ir_params);
  m_dev->setColorCameraParams(color_params);

  types = 0;
  if(set.rgb)
    types |= libfreenect2::Frame::Color;
  if(set.ir)
    types |= libfreenect2::Frame::Ir;
  if(set.depth)
    types |= libfreenect2::Frame::Depth;

  if(set.rgb && set.depth)
  {
    m_reg = new libfreenect2::Registration{m_dev->getIrCameraParams(), m_dev->getColorCameraParams()};
  }

  m_listener = new Listener(*this, types);

  m_dev->setColorFrameListener(m_listener);
  m_dev->setIrAndDepthFrameListener(m_listener);

}

kinect2_camera::~kinect2_camera()
{
  if(m_dev)
  {
    m_dev->close();
  }

  delete m_lastD;
  delete m_lastRGB;
  delete m_listener;
  delete m_dev;
  delete m_reg;
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

bool kinect2_camera::process(libfreenect2::Frame::Type type, libfreenect2::Frame* frame)
{
  switch(type)
  {
    case libfreenect2::Frame::Type::Color:
      processColor(frame);

      if(m_lastRGB)
        delete m_lastRGB;
      m_lastRGB = frame;

      processRegistration();
      return true;

    case libfreenect2::Frame::Type::Depth:
      processDepth(frame);

      if(m_lastD)
        delete m_lastD;
      m_lastD = frame;

      processRegistration();
      return true;

    case libfreenect2::Frame::Type::Ir:
      processIR(frame);
      return false;
  }
  return false;
}

void kinect2_camera::processRegistration()
{
  if(m_lastD && m_lastRGB)
  {
    static auto ud_buffer = new (std::align_val_t(32)) unsigned char[512*424*4];
    static auto reg_buffer = new (std::align_val_t(32)) unsigned char[512*424*4];
    libfreenect2::Frame ud(512, 424, 4, ud_buffer);
    libfreenect2::Frame reg(512, 424, 4, reg_buffer);
    m_reg->apply(m_lastRGB, m_lastD, &ud, &reg, true);

    // FIXME instead of this, use a proper geometry port

    std::vector<ossia::value> points;
    points.reserve(424 * 512 * 6);
    for(int r = 0; r < 424; ++r)
    {
      for(int c = 0; c < 512; ++c)
      {
        float rx, ry, rz, rgb;
        m_reg->getPointXYZRGB(&ud,  &reg, r, c, rx, ry, rz, rgb);

        if(!ossia::safe_isnan(rx)) {
          points.push_back(rx);
          points.push_back(ry);
          points.push_back(rz);

          const uint8_t *p = reinterpret_cast<uint8_t*>(&rgb);
          uint8_t b = p[0];
          uint8_t g = p[1];
          uint8_t r = p[2];
          points.push_back(r / 255.f);
          points.push_back(g / 255.f);
          points.push_back(b / 255.f);
        }
      }
    }

    pcl->push_value(std::move(points));
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

class kinect2_parameter : public ossia::gfx::texture_parameter
{
  GfxExecutionAction* context{};

public:
  std::shared_ptr<kinect2_decoder> decoder;
  int32_t node_id{};
  score::gfx::CameraNode* node{};

  kinect2_parameter(
      const std::shared_ptr<kinect2_decoder>& dec, ossia::net::node_base& n,
      GfxExecutionAction& ctx)
      : ossia::gfx::texture_parameter{n}
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
        "float v = sqrt(tex.r / 4500.); processed.rgb = vec3(v,v,v); processed.a = 1;"});
    this->add_child(
        std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "ir"));
  }
  if(settings.depth)
  {
    auto decoder = std::shared_ptr<kinect2_decoder>(new kinect2_decoder{
        k.depthFrames, 512, 424, AV_PIX_FMT_GRAYF32LE,
        "float v = (tex.r / 4500.); processed.rgb = vec3(v,v,v); processed.a = 1;"});
    this->add_child(
        std::make_unique<kinect2_node>(std::move(decoder), ctx, *this, "depth"));
  }
  if(settings.rgb  && settings.depth)
  {
    // FIXME pcl
    auto points = std::make_unique<ossia::net::generic_node>("points", *this, this->get_root_node());
    k.pcl = points->create_parameter(ossia::val_type::LIST);
    this->add_child(std::move(points));
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
  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
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
      f(s.name, s);
    }
  }
};

class Kinect2SettingsWidget final : public SharedInputSettingsWidget
{
public:
  explicit Kinect2SettingsWidget(QWidget* parent = nullptr);

  Device::DeviceSettings getSettings() const override;
  void setSettings(const Device::DeviceSettings& settings) override;
};

Kinect2SettingsWidget::Kinect2SettingsWidget(QWidget* parent)
    : SharedInputSettingsWidget(parent)
{
  m_deviceNameEdit->setText("Kinect2");
  ((QLabel*)m_layout->labelForField(m_shmPath))->setText("Identifier");
  setSettings(ProtocolFactory{}.defaultSettings());
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

QUrl ProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/kinect-device.html");
}

Device::DeviceEnumerators ProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  return {{"Kinect 2", new Kinect2Enumerator}};
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
