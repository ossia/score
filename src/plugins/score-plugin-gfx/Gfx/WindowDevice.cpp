#include "WindowDevice.hpp"

#include <Gfx/Window/BackgroundDevice.hpp>
#include <Gfx/Window/MultiWindowDevice.hpp>
#include <Gfx/Window/WindowDevice.hpp>
#include <Gfx/Window/WindowSettingsWidget.hpp>

#include <core/document/DocumentView.hpp>

#include <ossia-qt/invoke.hpp>

#include <QMenu>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::WindowDevice)

namespace Gfx
{

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
                m_settings.name.toStdString());
          }
          break;
        }
        case WindowMode::MultiWindow: {
          m_dev = std::make_unique<multiwindow_device>(
              set.outputs, std::unique_ptr<gfx_protocol_base>(m_protocol),
              m_settings.name.toStdString());
          break;
        }
        case WindowMode::Single:
        default: {
          m_dev = std::make_unique<window_device>(
              std::unique_ptr<gfx_protocol_base>(m_protocol),
              m_settings.name.toStdString());
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
  return StandardCategories::video;
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
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::WindowSettings& n)
{
  obj["Mode"] = (int)n.mode;
  obj["Outputs"] = n.outputs;
  obj["InputWidth"] = n.inputWidth;
  obj["InputHeight"] = n.inputHeight;
}

template <>
void JSONWriter::write(Gfx::WindowSettings& n)
{
  // Backward compatibility with old format
  if(auto v = obj.tryGet("Background"))
  {
    n.mode = v->toBool() ? Gfx::WindowMode::Background : Gfx::WindowMode::Single;
    return;
  }

  if(auto v = obj.tryGet("Mode"))
    n.mode = (Gfx::WindowMode)v->toInt();
  if(auto v = obj.tryGet("Outputs"))
  {
    const auto arr = v->toArray();
    n.outputs.resize(arr.Size());
    for(int i = 0; i < arr.Size(); i++)
    {
      JSONWriter w{arr[i]};
      w.write(n.outputs[i]);
    }
  }
  if(auto v = obj.tryGet("InputWidth"))
    n.inputWidth = v->toInt();
  if(auto v = obj.tryGet("InputHeight"))
    n.inputHeight = v->toInt();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::WindowSettings);
