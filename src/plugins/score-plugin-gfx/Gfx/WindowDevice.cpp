#include "WindowDevice.hpp"
#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Gfx/Graph/window.hpp>

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>

#include <QLineEdit>
#include <QFormLayout>
#include <QMenu>
#include <QGuiApplication>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::WindowDevice)

namespace Gfx
{
static
ScreenNode* createScreenNode()
{
  const auto& settings = score::AppContext().applicationSettings;
  if(settings.autoplay || !settings.gui)
  //if(QGuiApplication::platformName().toLower().contains("gl"))
  {
    return new ScreenNode{false, true};

    /*
    auto window = std::make_shared<Window>(defaultGraphicsAPI());
    window->showFullScreen();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    return new ScreenNode{window};
    */
  }
  else

  {
    return new ScreenNode;
  }
}

class gfx_device : public ossia::net::device_base
{
  gfx_node_base root;

public:
  gfx_device(std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)},
        root{*this, createScreenNode(), name}
  {
  }

  const gfx_node_base& get_root_node() const override { return root; }
  gfx_node_base& get_root_node() override { return root; }
};

WindowDevice::~WindowDevice() { }

void WindowDevice::setupContextMenu(QMenu& menu) const
{
  if (m_dev)
  {
    auto p = m_dev.get()->get_root_node().get_parameter();
    if (auto param = safe_cast<gfx_parameter_base*>(p))
    {
      if (auto s = safe_cast<ScreenNode*>(param->node))
      {
        if (auto w = s->window)
        {
          auto showhide = new QAction;
          if (!w->isVisible())
          {
            showhide->setText(tr("Show"));
            connect(showhide, &QAction::triggered, w.get(), [w] { w->show(); });
          }
          else
          {
            showhide->setText(tr("Hide"));
            connect(showhide, &QAction::triggered, w.get(), [w] { w->hide(); });
          }
          menu.addAction(showhide);
        }
      }
    }
  }
}

bool WindowDevice::reconnect()
{
  disconnect();

  try
  {
    auto plug = m_ctx.findPlugin<Gfx::DocumentPlugin>();
    if (plug)
    {
      m_protocol = new gfx_protocol_base{plug->exec};
      m_dev = std::make_unique<gfx_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          m_settings.name.toStdString());
    }
    // TODOengine->reload(&proto);

    // setLogging_impl(Device::get_cur_logging(isLogging()));
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

QString WindowProtocolFactory::prettyName() const noexcept
{
  return QObject::tr("Window");
}

QString WindowProtocolFactory::category() const noexcept
{
  return StandardCategories::video;
}

Device::DeviceEnumerator* WindowProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
}

Device::DeviceInterface* WindowProtocolFactory::makeDevice(
    const Device::DeviceSettings& settings,
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
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::AddressDialog* WindowProtocolFactory::makeEditAddressDialog(
    const Device::AddressSettings& set,
    const Device::DeviceInterface& dev,
    const score::DocumentContext& ctx,
    QWidget* parent)
{
  return nullptr;
}

Device::ProtocolSettingsWidget* WindowProtocolFactory::makeSettingsWidget()
{
  return new WindowSettingsWidget;
}

QVariant WindowProtocolFactory::makeProtocolSpecificSettings(const VisitorVariant& visitor) const
{
  return {};
}

void WindowProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data,
    const VisitorVariant& visitor) const
{
}

bool WindowProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a,
    const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

WindowSettingsWidget::WindowSettingsWidget(QWidget* parent) : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  setLayout(layout);

  setDefaults();
}

void WindowSettingsWidget::setDefaults()
{
  m_deviceNameEdit->setText("gfx");
}

Device::DeviceSettings WindowSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = WindowProtocolFactory::static_concreteKey();
  return s;
}

void WindowSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
}

}
