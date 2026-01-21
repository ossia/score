#include "WindowDevice.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/graphics/BackgroundRenderer.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/DocumentView.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>

#include <QFormLayout>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMenu>
#include <QScreen>
#include <QUrl>
#include <qcheckbox.h>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Gfx::WindowDevice)

namespace Gfx
{
static score::gfx::ScreenNode* createScreenNode()
{
  const auto& settings = score::AppContext().applicationSettings;
  const auto& gfx_settings = score::AppContext().settings<Gfx::Settings::Model>();

  auto make_configuration = [&] {
    score::gfx::OutputNode::Configuration conf;
    double rate = gfx_settings.getRate();
    if(rate > 0)
      conf = {.manualRenderingRate = 1000. / rate, .supportsVSync = true};
    else
      conf = {.manualRenderingRate = {}, .supportsVSync = true};
    return conf;
  };

  auto node = new score::gfx::ScreenNode{
      make_configuration(), false, (settings.autoplay || !settings.gui)};

  QObject::connect(
      &gfx_settings, &Gfx::Settings::Model::RateChanged, node,
      [node, make_configuration] { node->setConfiguration(make_configuration()); });

  return node;
}

class window_device : public ossia::net::device_base
{
  score::gfx::ScreenNode* m_screen{};
  gfx_node_base m_root;
  QObject m_qtContext;

public:
  ~window_device()
  {
    if(auto w = m_screen->window())
      w->close();

    m_screen->onWindowMove = [](QPointF) {};
    m_screen->onMouseMove = [](QPointF, QPointF) {};
    m_screen->onTabletMove = [](QTabletEvent*) {};
    m_screen->onKey = [](int, const QString&) {};
    m_screen->onKeyRelease = [](int, const QString&) { };
    m_screen->onFps = [](float) { };
    m_protocol->stop();

    m_root.clear_children();

    m_protocol.reset();
  }

  window_device(std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_screen{createScreenNode()}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_screen, name}
  {
    this->m_capabilities.change_tree = true;
    m_screen->setTitle(QString::fromStdString(name));

    {
      auto screen_node
          = std::make_unique<ossia::net::generic_node>("screen", *this, m_root);
      auto screen_param = screen_node->create_parameter(ossia::val_type::STRING);
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
        else if(auto val = v.target<std::string>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, scr = *val] {
            const auto& cur_screens = qApp->screens();
            for(auto s : cur_screens)
            {
              if(s->name() == scr.c_str())
              {
                screen->setScreen(s);
                break;
              }
            }
          });
        }
      });
      m_root.add_child(std::move(screen_node));
    }

    {
      struct move_window_lock
      {
        bool locked{};
      };
      auto lock = std::make_shared<move_window_lock>();

      auto pos_node
          = std::make_unique<ossia::net::generic_node>("position", *this, m_root);
      auto pos_param = pos_node->create_parameter(ossia::val_type::VEC2F);
      pos_param->add_callback([this, lock](const ossia::value& v) {
        if(lock->locked)
          return;
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val, lock] {
            screen->setPosition({(int)v[0], (int)v[1]});
          });
        }
      });

      m_screen->onWindowMove = [this, pos_param, lock](QPointF pos) {
        if(lock->locked)
          return;
        if(const auto& w = m_screen->window())
        {
          lock->locked = true;
          pos_param->set_value(ossia::vec2f{float(pos.x()), float(pos.y())});
          lock->locked = false;
        }
      };
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
      {
        auto visible
            = std::make_unique<ossia::net::generic_node>("visible", *this, *node);
        auto param = visible->create_parameter(ossia::val_type::BOOL);
        param->add_callback([this](const ossia::value& v) {
          if(auto val = v.target<bool>())
          {
            ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
              screen->setCursor(v);
            });
          }
        });
        node->add_child(std::move(visible));
      }

      m_screen->onMouseMove = [this, scaled_win, abs_win](QPointF screen, QPointF win) {
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

      m_screen->onTabletMove = [=, this](QTabletEvent* ev) {
        if(const auto& w = m_screen->window())
        {
          const auto sz = w->size();
          const auto win = ev->position();
          scaled_tablet_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_tablet_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
          tablet_pressure->push_value(ev->pressure());
          tablet_tan->push_value(ev->tangentialPressure());
          tablet_rot->push_value(ev->rotation());
          tablet_z->push_value(ev->z());
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
      {
        auto press_node
            = std::make_unique<ossia::net::generic_node>("press", *this, *node);
        ossia::net::parameter_base* press_param{};
        ossia::net::parameter_base* text_param{};
        {
          auto code_node
              = std::make_unique<ossia::net::generic_node>("code", *this, *press_node);
          press_param = code_node->create_parameter(ossia::val_type::INT);
          press_param->push_value(ossia::vec2f{0.f, 0.f});
          press_node->add_child(std::move(code_node));
        }
        {
          auto text_node
              = std::make_unique<ossia::net::generic_node>("text", *this, *press_node);
          text_param = text_node->create_parameter(ossia::val_type::STRING);
          press_node->add_child(std::move(text_node));
        }

        m_screen->onKey = [press_param, text_param](int key, const QString& text) {
          press_param->push_value(key);
          text_param->push_value(text.toStdString());
        };
        node->add_child(std::move(press_node));
      }
      {
        auto release_node
            = std::make_unique<ossia::net::generic_node>("release", *this, *node);
        ossia::net::parameter_base* press_param{};
        ossia::net::parameter_base* text_param{};
        {
          auto code_node
              = std::make_unique<ossia::net::generic_node>("code", *this, *release_node);
          press_param = code_node->create_parameter(ossia::val_type::INT);
          press_param->push_value(ossia::vec2f{0.f, 0.f});
          release_node->add_child(std::move(code_node));
        }
        {
          auto text_node
              = std::make_unique<ossia::net::generic_node>("text", *this, *release_node);
          text_param = text_node->create_parameter(ossia::val_type::STRING);
          release_node->add_child(std::move(text_node));
        }

        m_screen->onKeyRelease
            = [press_param, text_param](int key, const QString& text) {
          press_param->push_value(key);
          text_param->push_value(text.toStdString());
        };
        node->add_child(std::move(release_node));
      }

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

    {
      auto fps_node = std::make_unique<ossia::net::generic_node>("fps", *this, m_root);
      auto fps_param = fps_node->create_parameter(ossia::val_type::FLOAT);
      m_screen->onFps = [fps_param](float fps) { fps_param->push_value(fps); };
      m_root.add_child(std::move(fps_node));
    }

    {
      auto background_node
          = std::make_unique<ossia::net::generic_node>("background", *this, m_root);
      auto fs_param = background_node->create_parameter(ossia::val_type::BOOL);
      fs_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<bool>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setBackground(v);
          });
        }
      });
      m_root.add_child(std::move(background_node));
    }
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

}
namespace Gfx
{
class DeviceBackgroundRenderer : public score::BackgroundRenderer
{
public:
  explicit DeviceBackgroundRenderer(score::gfx::BackgroundNode2& node)
      : score::BackgroundRenderer{}
  {
    this->shared_readback = std::make_shared<QRhiReadbackResult>();
    node.shared_readback = this->shared_readback;
  }

  ~DeviceBackgroundRenderer() override { }

  bool render(QPainter* painter, const QRectF& rect) override
  {
    auto& m_readback = *shared_readback;
    const auto w = m_readback.pixelSize.width();
    const auto h = m_readback.pixelSize.height();
    int sz = w * h * 4;
    int bytes = m_readback.data.size();
    if(bytes > 0 && bytes >= sz)
    {
      QImage img{
          (const unsigned char*)m_readback.data.data(), w, h, w * 4,
          QImage::Format_RGBA8888};
      painter->drawImage(rect, img);
      return true;
    }
    return false;
  }

private:
  QPointer<Gfx::DocumentPlugin> plug;
  std::shared_ptr<QRhiReadbackResult> shared_readback;
};

class background_device : public ossia::net::device_base
{
  score::gfx::BackgroundNode2* m_screen{};
  gfx_node_base m_root;
  QObject m_qtContext;
  QPointer<Scenario::ScenarioDocumentView> m_view;
  DeviceBackgroundRenderer* m_renderer{};

public:
  background_device(
      Scenario::ScenarioDocumentView& view, std::unique_ptr<gfx_protocol_base> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_screen{new score::gfx::BackgroundNode2}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_screen, name}
      , m_view{&view}
  {
    this->m_capabilities.change_tree = true;
    m_renderer = new DeviceBackgroundRenderer{*m_screen};
    view.addBackgroundRenderer(m_renderer);
  }

  ~background_device()
  {
    if(m_view)
    {
      m_view->removeBackgroundRenderer(m_renderer);
    }
    delete m_renderer;
    m_protocol->stop();

    m_root.clear_children();

    m_protocol.reset();
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};
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
      auto main_view = qobject_cast<Scenario::ScenarioDocumentView*>(
          &m_ctx.document.view()->viewDelegate());
      if(set.background && main_view)
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

WindowSettingsWidget::WindowSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);
  layout->addRow(tr("Use as UI background"), m_background = new QCheckBox);
  m_deviceNameEdit->setText("window");

  setLayout(layout);
}

Device::DeviceSettings WindowSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings set;
  set.background = m_background->isChecked();
  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void WindowSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  const auto& set = settings.deviceSpecificSettings.value<WindowSettings>();
  m_background->setChecked(set.background);
}

}

template <>
void DataStreamReader::read(const Gfx::WindowSettings& n)
{
  m_stream << n.background;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::WindowSettings& n)
{
  m_stream >> n.background;
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::WindowSettings& n)
{
  obj["Background"] = n.background;
}

template <>
void JSONWriter::write(Gfx::WindowSettings& n)
{
  n.background = obj["Background"].toBool();
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::WindowSettings);
