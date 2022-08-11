#include "WindowDevice.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>

#include <QFormLayout>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMenu>
#include <QScreen>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::WindowDevice)

namespace Gfx
{
static score::gfx::ScreenNode* createScreenNode()
{
  const auto& settings = score::AppContext().applicationSettings;
  if(settings.autoplay || !settings.gui)
  //if(QGuiApplication::platformName().toLower().contains("gl"))
  {
    return new score::gfx::ScreenNode{false, true};

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
    return new score::gfx::ScreenNode;
  }
}

class window_device : public ossia::net::device_base
{
  score::gfx::ScreenNode* m_screen{};
  gfx_node_base m_root;
  QObject m_qtContext;

public:
  ~window_device()
  {
    m_protocol->stop();

    {
      m_root.clear_children();
    }

    m_protocol.reset();
  }
  window_device(std::unique_ptr<ossia::net::protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_screen{createScreenNode()}
      , m_root{*this, m_screen, name}
  {
    this->m_capabilities.change_tree = true;

    {
      auto screen_node
          = std::make_unique<ossia::net::generic_node>("screen", *this, m_root);
      auto screen_param = screen_node->create_parameter(ossia::val_type::INT);
      screen_param->set_domain(ossia::make_domain(int(0), int(100)));
      screen_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<int>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, scr = *val] {
            const auto& cur_screens = qApp->screens();
            if(ossia::valid_index(scr, cur_screens))
            {
              screen->setScreen(cur_screens[scr]);
            }
          });
        }
      });
      m_root.add_child(std::move(screen_node));
    }

    {
      auto pos_node
          = std::make_unique<ossia::net::generic_node>("position", *this, m_root);
      auto pos_param = pos_node->create_parameter(ossia::val_type::VEC2F);
      pos_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setPosition({(int)v[0], (int)v[1]});
          });
        }
      });
      m_root.add_child(std::move(pos_node));
    }

    // Mouse input
    ossia::net::parameter_base* scaled_win{};
    ossia::net::parameter_base* abs_win{};
    {
      auto node = std::make_unique<ossia::net::generic_node>("cursor", *this, m_root);
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("scaled", *this, *node);
        scaled_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        scaled_win->set_domain(ossia::make_domain(0.f, 1.f));
        scaled_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(scale_node));
      }
      {
        auto abs_node
            = std::make_unique<ossia::net::generic_node>("absolute", *this, *node);
        abs_win = abs_node->create_parameter(ossia::val_type::VEC2F);
        abs_win->set_domain(
            ossia::make_domain(ossia::vec2f{0.f, 0.f}, ossia::vec2f{1280, 270.f}));
        abs_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(abs_node));
      }

      m_screen->onMouseMove = [=](QPointF screen, QPointF win) {
        if(const auto& w = m_screen->window())
        {
          auto sz = w->size();
          scaled_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
        }
      };

      m_root.add_child(std::move(node));
    }

    // Tablet input
    ossia::net::parameter_base* scaled_tablet_win{};
    ossia::net::parameter_base* abs_tablet_win{};
    {
      auto node = std::make_unique<ossia::net::generic_node>("tablet", *this, m_root);
      ossia::net::parameter_base* tablet_pressure{};
      ossia::net::parameter_base* tablet_z{};
      ossia::net::parameter_base* tablet_tan{};
      ossia::net::parameter_base* tablet_rot{};
      ossia::net::parameter_base* tablet_tilt_x{};
      ossia::net::parameter_base* tablet_tilt_y{};
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("scaled", *this, *node);
        scaled_tablet_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        scaled_tablet_win->set_domain(ossia::make_domain(0.f, 1.f));
        scaled_tablet_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(scale_node));
      }
      {
        auto abs_node
            = std::make_unique<ossia::net::generic_node>("absolute", *this, *node);
        abs_tablet_win = abs_node->create_parameter(ossia::val_type::VEC2F);
        abs_tablet_win->set_domain(
            ossia::make_domain(ossia::vec2f{0.f, 0.f}, ossia::vec2f{1280, 270.f}));
        abs_tablet_win->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(abs_node));
      }
      {
        auto scale_node = std::make_unique<ossia::net::generic_node>("z", *this, *node);
        tablet_z = scale_node->create_parameter(ossia::val_type::INT);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("pressure", *this, *node);
        tablet_pressure = scale_node->create_parameter(ossia::val_type::FLOAT);
        //tablet_pressure->set_domain(ossia::make_domain(0.f, 1.f));
        //tablet_pressure->push_value(0.f);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tangential", *this, *node);
        tablet_tan = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tan->set_domain(ossia::make_domain(-1.f, 1.f));
        //tablet_tan->push_value(0.f);
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("rotation", *this, *node);
        tablet_rot = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_rot->set_unit(ossia::degree_u{});
        tablet_rot->set_domain(ossia::make_domain(-180.f, 180.f));
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tilt_x", *this, *node);
        tablet_tilt_x = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tilt_x->set_domain(ossia::make_domain(-60.f, 60.f));
        tablet_tilt_x->set_unit(ossia::degree_u{});
        node->add_child(std::move(scale_node));
      }
      {
        auto scale_node
            = std::make_unique<ossia::net::generic_node>("tilt_y", *this, *node);
        tablet_tilt_y = scale_node->create_parameter(ossia::val_type::FLOAT);
        tablet_tilt_y->set_domain(ossia::make_domain(-60.f, 60.f));
        tablet_tilt_y->set_unit(ossia::degree_u{});
        node->add_child(std::move(scale_node));
      }

      m_screen->onTabletMove = [=](QTabletEvent* ev) {
        if(const auto& w = m_screen->window())
        {
          const auto sz = w->size();
          const auto win = ev->posF();
          scaled_tablet_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_tablet_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
          tablet_pressure->push_value(ev->pressure());
          tablet_tan->push_value(ev->tangentialPressure());
          tablet_rot->push_value(ev->rotation());
          tablet_tilt_x->push_value(ev->xTilt());
          tablet_tilt_y->push_value(ev->yTilt());
        }
      };

      m_root.add_child(std::move(node));
    }

    {
      auto size_node = std::make_unique<ossia::net::generic_node>("size", *this, m_root);
      auto size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      size_param->push_value(ossia::vec2f{1280.f, 720.f});
      size_param->add_callback([this, abs_win, abs_tablet_win](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setSize({(int)v[0], (int)v[1]});
          });

          auto dom = abs_win->get_domain();
          ossia::set_max(dom, *val);
          {
            abs_win->set_domain(std::move(dom));
            abs_tablet_win->set_domain(std::move(dom));
          }
        }
      });

      m_root.add_child(std::move(size_node));
    }

    {
      auto size_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);
      ossia::net::set_description(
          *size_node, "Set to [0, 0] to use the viewport's size");

      auto size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      size_param->push_value(ossia::vec2f{0.f, 0.f});
      size_param->add_callback([this, abs_win](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setRenderSize({(int)v[0], (int)v[1]});
          });

          auto dom = abs_win->get_domain();
          ossia::set_max(dom, *val);
          abs_win->set_domain(std::move(dom));
        }
      });

      m_root.add_child(std::move(size_node));
    }

    // Keyboard input
    {
      auto node = std::make_unique<ossia::net::generic_node>("key", *this, m_root);
      ossia::net::parameter_base* press_param{};
      ossia::net::parameter_base* text_param{};
      {
        auto press_node
            = std::make_unique<ossia::net::generic_node>("code", *this, *node);
        press_param = press_node->create_parameter(ossia::val_type::INT);
        press_param->push_value(ossia::vec2f{0.f, 0.f});
        node->add_child(std::move(press_node));
      }
      {
        auto text_node
            = std::make_unique<ossia::net::generic_node>("text", *this, *node);
        text_param = text_node->create_parameter(ossia::val_type::STRING);
        node->add_child(std::move(text_node));
      }

      m_screen->onKey = [=](int key, const QString& text) {
        press_param->push_value(key);
        text_param->push_value(text.toStdString());
      };

      m_root.add_child(std::move(node));
    }

    {
      auto fs_node
          = std::make_unique<ossia::net::generic_node>("fullscreen", *this, m_root);
      auto fs_param = fs_node->create_parameter(ossia::val_type::BOOL);
      fs_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<bool>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setFullScreen(v);
          });
        }
      });
      m_root.add_child(std::move(fs_node));
    }
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

WindowDevice::~WindowDevice() { }

void WindowDevice::addAddress(const Device::FullAddressSettings& settings)
{
  if(!m_dev)
    return;

  updateAddress(settings.address, settings);
}

void WindowDevice::setupContextMenu(QMenu& menu) const
{
  if(m_dev)
  {
    auto p = m_dev.get()->get_root_node().get_parameter();
    if(auto param = safe_cast<gfx_parameter_base*>(p))
    {
      if(auto s = safe_cast<score::gfx::ScreenNode*>(param->node))
      {
        if(const auto& w = s->window())
        {
          auto showhide = new QAction;
          if(!w->isVisible())
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
    if(plug)
    {
      m_protocol = new gfx_protocol_base{plug->exec};
      m_dev = std::make_unique<window_device>(
          std::unique_ptr<ossia::net::protocol_base>(m_protocol),
          m_settings.name.toStdString());

      enableCallbacks();
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

Device::DeviceEnumerator*
WindowProtocolFactory::getEnumerator(const score::DocumentContext& ctx) const
{
  return nullptr;
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
  return {};
}

void WindowProtocolFactory::serializeProtocolSpecificSettings(
    const QVariant& data, const VisitorVariant& visitor) const
{
}

bool WindowProtocolFactory::checkCompatibility(
    const Device::DeviceSettings& a, const Device::DeviceSettings& b) const noexcept
{
  return a.name != b.name;
}

WindowSettingsWidget::WindowSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  m_deviceNameEdit->setText("window");

  setLayout(layout);
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
