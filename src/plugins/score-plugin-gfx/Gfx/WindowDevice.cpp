#include "WindowDevice.hpp"
#include <Gfx/Graph/ScreenNode.hpp>

#include <QCoreApplication>
#include <QEventLoop>

#include <Gfx/Window/BackgroundDevice.hpp>
#include <Gfx/Window/MultiWindowDevice.hpp>
#include <Gfx/Window/OffscreenDevice.hpp>
#include <Gfx/Window/WindowDevice.hpp>
#include <Gfx/Window/WindowSettingsWidget.hpp>

#include <score/serialization/JSONParse.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <ossia-qt/invoke.hpp>

#include <QMenu>
#include <QScreen>
#include <QWindow>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::WindowDevice)

namespace Gfx
{

// SCORE_FORCE_OFFSCREEN_WINDOW=Name1,Name2 forces any matching WindowDevice
// (whatever its Single/Background/MultiWindow mode) into a headless offscreen
// render path. Used by tests that need grabTo output but must not pop a
// platform window.
static bool shouldForceOffscreen(const QString& name)
{
  static const QByteArray env = qgetenv("SCORE_FORCE_OFFSCREEN_WINDOW");
  if(env.isEmpty())
    return false;
  for(const auto& part : env.split(','))
  {
    const auto trimmed = QString::fromUtf8(part).trimmed();
    if(!trimmed.isEmpty() && trimmed == name)
      return true;
  }
  return false;
}

score::gfx::ScreenNode* WindowDevice::screenNode() const noexcept
{
  if(m_dev)
  {
    auto p = m_dev.get()->get_root_node().get_parameter();
    if(auto param = safe_cast<gfx_parameter_base*>(p))
      return dynamic_cast<score::gfx::ScreenNode*>(param->node);
  }
  return nullptr;
}

score::gfx::Window* WindowDevice::window() const noexcept
{
  if(m_dev)
  {
    auto p = m_dev.get()->get_root_node().get_parameter();
    if(auto param = safe_cast<gfx_parameter_base*>(p))
    {
      if(auto s = dynamic_cast<score::gfx::ScreenNode*>(param->node))
      {
        if(const auto& w = s->window())
        {
          return w.get();
        }
      }
    }
  }
  return nullptr;
}

WindowDevice::~WindowDevice() { }

void WindowDevice::addAddress(const Device::FullAddressSettings& settings)
{
  if(!m_dev)
    return;

  updateAddress(settings.address, settings);
}

void WindowDevice::setupContextMenu(QMenu& menu) const
{
  if(auto w = this->window())
  {
    auto showhide = new QAction;
    if(!w->isVisible())
    {
      showhide->setText(tr("Show"));
      connect(showhide, &QAction::triggered, w, &score::gfx::Window::show);
    }
    else
    {
      showhide->setText(tr("Hide"));
      connect(showhide, &QAction::triggered, w, &score::gfx::Window::hide);
    }
    menu.addAction(showhide);
  }
}

void WindowDevice::disconnect()
{
  DeviceInterface::disconnect();
  auto prev = std::move(m_dev);
  m_dev = {};
  deviceChanged(prev.get(), nullptr);
}

void WindowDevice::grabTo(const QString& path) const
{
  if(auto dev = dynamic_cast<offscreen_device*>(m_dev.get()))
  {
    auto node = dev->node();
    if(!node || !node->shared_readback)
    {
      qWarning() << "grabTo: offscreen device has not rendered yet";
      return;
    }

    const auto& rb = *node->shared_readback;
    const int w = rb.pixelSize.width();
    const int h = rb.pixelSize.height();
    const int expected = w * h * 4;

    // BackgroundNode::render() clears the readback when its render list holds
    // nothing but the output itself, which leaves the default-constructed
    // QSize(-1, -1) here.
    if(w <= 0 || h <= 0)
    {
      qWarning() << "grabTo: nothing rendered into" << m_settings.name
                 << "- no process is connected to this device's input";
      return;
    }
    if(rb.data.size() < expected)
    {
      qWarning() << "grabTo: readback is" << rb.data.size() << "bytes for" << w << "x"
                 << h << "- expected" << expected;
      return;
    }

    QImage img{
        reinterpret_cast<const unsigned char*>(rb.data.constData()), w, h, w * 4,
        QImage::Format_RGBA8888};
    if(!img.save(path))
      qWarning() << "grabTo: could not write" << path;
  }
  else if(auto* screen = this->screenNode())
  {
    // Read the swapchain's own backbuffer rather than QScreen::grabWindow,
    // which reads the desktop at the window's geometry and therefore captures
    // anything drawn on top of it. Arm the readback, then drive frames until
    // the result lands: QRhi fills it when the frame it was queued in
    // completes, which is not the call that queued it.
    screen->requestReadback();
    const auto& rbp = screen->readback();

    rbp->data.clear();
    rbp->pixelSize = {};

    // The window may not be exposed yet, and until it is, canRender() is false
    // and renderFrames() draws nothing. Pump the event loop between attempts so
    // exposure can happen, and give QRhi a few frames to hand the result back --
    // it fills the result when the frame it was queued in completes, which with
    // a buffered swapchain is not the frame that queued it.
    //
    // Pumping re-enters: a queued OSC /script message calling grabTo again is
    // dispatched from inside processEvents, and a harness that retries the grab
    // sends several. Without this guard that recurses until the stack dies --
    // observed at 200+ nested grabTo frames. Re-entrant calls return quietly;
    // the outermost one is still driving frames for all of them.
    static bool s_grabbing = false;
    if(s_grabbing)
      return;
    s_grabbing = true;
    for(int i = 0; i < 60 && rbp->data.isEmpty(); ++i)
    {
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 16);
      renderFrames(1);
    }
    s_grabbing = false;

    const auto& rb = *rbp;
    const int w = rb.pixelSize.width();
    const int h = rb.pixelSize.height();
    if(w <= 0 || h <= 0 || rb.data.size() < w * h * 4)
    {
      qWarning() << "grabTo: the window produced no readback for" << m_settings.name;
      return;
    }

    // A swapchain backbuffer is commonly BGRA8; the offscreen path is always
    // RGBA8. Ask the result rather than assuming.
    const auto fmt = rb.format == QRhiTexture::BGRA8 ? QImage::Format_ARGB32
                                                     : QImage::Format_RGBA8888;
    QImage img{
        reinterpret_cast<const unsigned char*>(rb.data.constData()), w, h, w * 4, fmt};

    // OpenGL hands back a bottom-up framebuffer; the other backends do not.
    if(auto st = screen->renderState(); st && st->rhi && st->rhi->isYUpInFramebuffer())
      img = img.mirrored(false, true);

    if(!img.copy().save(path))
      qWarning() << "grabTo: could not write" << path;
  }
  else
  {
    qWarning() << "grabTo: device has no window and is not offscreen";
  }
}

void WindowDevice::renderFrames(int frames) const
{
  if(auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>())
    plug->context.renderFrames(frames);
  else
    qWarning() << "renderFrames: no gfx document plugin";
}

void WindowDevice::setStepRate(double fps) const
{
  if(auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>())
    plug->context.setStepRate(fps);
  else
    qWarning() << "setStepRate: no gfx document plugin";
}

void WindowDevice::grabFrame(int frames, const QString& path) const
{
  renderFrames(frames);
  grabTo(path);
}

bool WindowDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();

    if(plug)
    {
      m_protocol = new gfx_protocol_base{plug->exec};
      auto set = m_settings.deviceSpecificSettings.value<WindowSettings>();
      auto view = m_ctx.document.view();
      auto main_view = view ? qobject_cast<Scenario::ScenarioDocumentView*>(
          &view->viewDelegate()) : nullptr;

      if(shouldForceOffscreen(m_settings.name))
      {
        m_dev = std::make_unique<offscreen_device>(
            std::unique_ptr<gfx_protocol_base>(m_protocol),
            m_settings.name.toStdString());

        enableCallbacks();
        deviceChanged(nullptr, m_dev.get());
        return connected();
      }

      switch(set.mode)
      {
        case WindowMode::Background: {
          if(main_view)
          {
            m_dev = std::make_unique<background_device>(
                *main_view, std::unique_ptr<gfx_protocol_base>(m_protocol),
                m_settings.name.toStdString());
          }
          else
          {
            m_dev = std::make_unique<window_device>(
                std::unique_ptr<gfx_protocol_base>(m_protocol),
                m_settings.name.toStdString(), set.flag, set.format);
          }
          break;
        }
        case WindowMode::MultiWindow: {
          m_dev = std::make_unique<multiwindow_device>(
              set.outputs, std::unique_ptr<gfx_protocol_base>(m_protocol),
              m_settings.name.toStdString(), set.flag, set.format);
          break;
        }
        case WindowMode::Single:
        default: {
          m_dev = std::make_unique<window_device>(
              std::unique_ptr<gfx_protocol_base>(m_protocol),
              m_settings.name.toStdString(), set.flag, set.format);
          break;
        }
      }

      enableCallbacks();
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

QString WindowProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Window");
}

QString WindowProtocolFactory::category() const noexcept
{
  return StandardCategories::video_out;
}

QUrl WindowProtocolFactory::manual() const noexcept
{
  return QUrl("https://ossia.io/score-docs/devices/window-device.html");
}

Device::DeviceInterface* WindowProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings, const Explorer::DeviceDocumentPlugin& plugin,
    const score::DocumentContext& ctx)
{
  return new WindowDevice{settings, ctx};
}

const Device::DeviceSettings& WindowProtocolFactory::defaultSettings() const noexcept
{
  static const Device::DeviceSettings settings = [&]() {
    Device::DeviceSettings s;
    s.protocol = concreteKey();
    s.name = "Window";
    return s;
  }();
  return settings;
}

Device::AddressDialog* WindowProtocolFactory::makeAddAddressDialog(
    const Device::DeviceInterface& dev, const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* WindowProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set, const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx, QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* WindowProtocolFactory::makeSettingsWidget()
{
  return new WindowSettingsWidget;
}

QVariant
WindowProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return makeProtocolSpecificSettings_T<WindowSettings>(visitor);
}

void WindowProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
  serializeProtocolSpecificSettings_T<WindowSettings>(data, visitor);
}

bool WindowProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return true;
}

}

template <>
void DataStreamReader::read(const Gfx::WindowSettings& n)
{
  m_stream << (int32_t)n.mode;
  m_stream << (int32_t)n.outputs.size();
  for(const auto& o : n.outputs)
    read(o);
  m_stream << (int32_t)n.inputWidth << (int32_t)n.inputHeight;
  m_stream << (int32_t)n.flag << (int32_t)n.format;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::WindowSettings& n)
{
  int32_t mode{};
  m_stream >> mode;
  n.mode = (Gfx::WindowMode)mode;
  int32_t count{};
  m_stream >> count;
  n.outputs.resize(count);
  for(auto& o : n.outputs)
    write(o);
  int32_t inW{}, inH{};
  m_stream >> inW >> inH;
  n.inputWidth = inW;
  n.inputHeight = inH;
  int32_t flag{}, format{};
  m_stream >> flag >> format;
  n.flag = (Gfx::SwapchainFlag)flag;
  n.format = (Gfx::SwapchainFormat)format;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::WindowSettings& n)
{
  obj["Mode"] = (int)n.mode;
  obj["Outputs"] = n.outputs;
  obj["InputWidth"] = n.inputWidth;
  obj["InputHeight"] = n.inputHeight;
  obj["SwapchainFlag"] = (int)n.flag;
  obj["SwapchainFormat"] = (int)n.format;
}

template <>
void JSONWriter::write(Gfx::WindowSettings& n)
{
  // Backward compatibility with old format
  if(auto v = obj.tryGet("Background"); v && v->obj.IsBool())
  {
    n.mode = v->obj.GetBool() ? Gfx::WindowMode::Background : Gfx::WindowMode::Single;
    return;
  }

  score::parseJsonField(obj, "Mode", n.mode);
  if(auto v = obj.tryGet("Outputs"); v && v->obj.IsArray())
  {
    const auto arr = v->obj.GetArray();
    n.outputs.resize(arr.Size());
    for(rapidjson::SizeType i = 0; i < arr.Size(); i++)
    {
      if(!arr[i].IsObject())
        continue;
      JSONWriter w{arr[i]};
      w.write(n.outputs[i]);
    }
  }
  score::parseJsonField(obj, "InputWidth", n.inputWidth);
  score::parseJsonField(obj, "InputHeight", n.inputHeight);
  score::parseJsonField(obj, "SwapchainFlag", n.flag);
  score::parseJsonField(obj, "SwapchainFormat", n.format);
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::WindowSettings);
