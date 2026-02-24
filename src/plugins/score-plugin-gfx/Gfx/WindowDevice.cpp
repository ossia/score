#include "WindowDevice.hpp"

#include <State/Widgets/AddressFragmentLineEdit.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/graphics/BackgroundRenderer.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsSceneMouseEvent>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
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

  ossia::net::parameter_base* scaled_win{};
  ossia::net::parameter_base* abs_win{};
  ossia::net::parameter_base* abs_tablet_win{};
  ossia::net::parameter_base* size_param{};
  ossia::net::parameter_base* rendersize_param{};

  void update_viewport()
  {
    auto v = rendersize_param->value();
    if(auto val = v.target<ossia::vec2f>())
    {
      auto dom = abs_win->get_domain();
      if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
      {
        ossia::set_max(dom, *val);
        abs_win->set_domain(std::move(dom));
        abs_tablet_win->set_domain(std::move(dom));
      }
      else
      {
        // Use the normal size
        v = size_param->value();
        if(auto val = v.target<ossia::vec2f>())
        {
          auto dom = abs_win->get_domain();
          if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
          {
            ossia::set_max(dom, *val);
            abs_win->set_domain(std::move(dom));
            abs_tablet_win->set_domain(std::move(dom));
          }
        }
      }
    }
    else
    {
      v = size_param->value();
      if(auto val = v.target<ossia::vec2f>())
      {
        auto dom = abs_win->get_domain();
        if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
        {
          ossia::set_max(dom, *val);
          abs_win->set_domain(std::move(dom));
          abs_tablet_win->set_domain(std::move(dom));
        }
      }
    }
  }

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
      pos_param->push_value(
          ossia::vec2f{100.f, 100.f}); // FIXME Try to detect center of screen ?
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

      m_screen->onMouseMove = [this](QPointF screen, QPointF win) {
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
      size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      size_param->push_value(ossia::vec2f{1280.f, 720.f});
      size_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      m_root.add_child(std::move(size_node));
    }

    {
      auto size_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);
      ossia::net::set_description(
          *size_node, "Set to [0, 0] to use the viewport's size");

      rendersize_param = size_node->create_parameter(ossia::val_type::VEC2F);
      rendersize_param->push_value(ossia::vec2f{0.f, 0.f});
      rendersize_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setRenderSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
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
  explicit DeviceBackgroundRenderer(score::gfx::BackgroundNode& node)
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
    painter->setRenderHint(
        QPainter::RenderHint::SmoothPixmapTransform,
        w > rect.width() && h > rect.height());
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
  score::gfx::BackgroundNode* m_screen{};
  gfx_node_base m_root;
  QObject m_qtContext;
  QPointer<Scenario::ScenarioDocumentView> m_view;
  DeviceBackgroundRenderer* m_renderer{};

  ossia::net::parameter_base* scaled_win{};
  ossia::net::parameter_base* abs_win{};
  ossia::net::parameter_base* abs_tablet_win{};
  ossia::net::parameter_base* size_param{};
  ossia::net::parameter_base* rendersize_param{};
  ossia::net::parameter_base* press_param{};
  ossia::net::parameter_base* text_param{};

  boost::container::static_vector<QMetaObject::Connection, 8> con;

  void update_viewport()
  {
    auto v = rendersize_param->value();
    if(auto val = v.target<ossia::vec2f>())
    {
      auto dom = abs_win->get_domain();
      if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
      {
        ossia::set_max(dom, *val);
        abs_win->set_domain(std::move(dom));
        abs_tablet_win->set_domain(std::move(dom));
      }
      else
      {
        // Use the normal size
        v = size_param->value();
        if(auto val = v.target<ossia::vec2f>())
        {
          auto dom = abs_win->get_domain();
          if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
          {
            ossia::set_max(dom, *val);
            abs_win->set_domain(std::move(dom));
            abs_tablet_win->set_domain(std::move(dom));
          }
        }
      }
    }
    else
    {
      v = size_param->value();
      if(auto val = v.target<ossia::vec2f>())
      {
        auto dom = abs_win->get_domain();
        if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
        {
          ossia::set_max(dom, *val);
          abs_win->set_domain(std::move(dom));
          abs_tablet_win->set_domain(std::move(dom));
        }
      }
    }
  }

public:
  background_device(
      Scenario::ScenarioDocumentView& view, std::unique_ptr<gfx_protocol_base> proto,
      std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_screen{new score::gfx::BackgroundNode}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_screen, name}
      , m_view{&view}
  {
    this->m_capabilities.change_tree = true;
    m_renderer = new DeviceBackgroundRenderer{*m_screen};
    view.addBackgroundRenderer(m_renderer);

    auto& v = view.view();

    {
      auto size_node = std::make_unique<ossia::net::generic_node>("size", *this, m_root);
      size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      const auto sz = view.view().size();
      m_screen->setSize(sz);
      size_param->push_value(ossia::vec2f{(float)sz.width(), (float)sz.height()});
      size_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = this->m_screen, v = *val] {
            screen->setSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::sizeChanged, &m_qtContext,
              [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QSize e) {
        if(ptr && v)
        {
          size_param->push_value(ossia::vec2f{(float)e.width(), (float)e.height()});
        }
      }, Qt::DirectConnection));

      m_root.add_child(std::move(size_node));
    }
    {
      auto size_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);

      rendersize_param = size_node->create_parameter(ossia::val_type::VEC2F);
      rendersize_param->push_value(ossia::vec2f{0.f, 0.f});
      rendersize_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [screen = m_screen, v = *val] {
            screen->setRenderSize({(int)v[0], (int)v[1]});
          });

          update_viewport();
        }
      });

      m_root.add_child(std::move(size_node));
    }

    // Mouse input
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

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::hoverMove, &m_qtContext,
              [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QHoverEvent* e) {
        if(ptr && v)
        {
          auto win = e->position();
          auto sz = v->view().size();
          scaled_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          abs_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
        }
      }, Qt::DirectConnection));

      m_root.add_child(std::move(node));
    }

    // Tablet input
    ossia::net::parameter_base* scaled_tablet_win{};
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

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::tabletMove, m_screen,
              [=, this, v = QPointer{&view},
               ptr = QPointer{m_screen}](QTabletEvent* ev) {
        if(ptr && v)
        {
          auto win = ev->position();
          auto sz = v->view().size();
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
      },
              Qt::DirectConnection));
      m_root.add_child(std::move(node));
    }

    // Keyboard input
    {
      auto node = std::make_unique<ossia::net::generic_node>("key", *this, m_root);
      {
        auto press_node
            = std::make_unique<ossia::net::generic_node>("press", *this, *node);
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

        con.push_back(
            QObject::connect(
                &v, &Scenario::ProcessGraphicsView::keyPress, m_screen,
                [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QKeyEvent* ev) {
          if(ptr && v)
          {
            if(!ev->isAutoRepeat())
            {
              press_param->push_value(ev->key());
              text_param->push_value(ev->text().toStdString());
            }
          }
        }, Qt::DirectConnection));
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

        con.push_back(
            QObject::connect(
                &v, &Scenario::ProcessGraphicsView::keyRelease, m_screen,
                [=, v = QPointer{&view}, ptr = QPointer{m_screen}](QKeyEvent* ev) {
          if(ptr && v)
          {
            if(!ev->isAutoRepeat())
            {
              press_param->push_value(ev->key());
              text_param->push_value(ev->text().toStdString());
            }
          }
        }, Qt::DirectConnection));
        node->add_child(std::move(release_node));
      }

      m_root.add_child(std::move(node));
    }
  }

  ~background_device()
  {
    if(m_view)
    {
      m_view->removeBackgroundRenderer(m_renderer);
    }
    for(auto& c : con)
      QObject::disconnect(c);
    delete m_renderer;
    m_protocol->stop();

    m_root.clear_children();

    m_protocol.reset();
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

static score::gfx::MultiWindowNode* createMultiWindowNode(
    const std::vector<OutputMapping>& mappings)
{
  const auto& settings = score::AppContext().applicationSettings;
  const auto& gfx_settings = score::AppContext().settings<Gfx::Settings::Model>();

  score::gfx::OutputNode::Configuration conf;
  double rate = gfx_settings.getRate();
  if(rate > 0)
    conf = {.manualRenderingRate = 1000. / rate, .supportsVSync = false};
  else
    conf = {.manualRenderingRate = 1000. / 60., .supportsVSync = false};

  return new score::gfx::MultiWindowNode{conf, mappings};
}

class multiwindow_device : public ossia::net::device_base
{
  score::gfx::MultiWindowNode* m_node{};
  gfx_node_base m_root;
  QObject m_qtContext;

  ossia::net::parameter_base* fps_param{};
  ossia::net::parameter_base* rendersize_param{};

  struct PerWindowParams
  {
    ossia::net::parameter_base* scaled_cursor{};
    ossia::net::parameter_base* abs_cursor{};
    ossia::net::parameter_base* size_param{};
    ossia::net::parameter_base* pos_param{};
    ossia::net::parameter_base* source_pos{};
    ossia::net::parameter_base* source_size{};
    ossia::net::parameter_base* key_press_code{};
    ossia::net::parameter_base* key_press_text{};
    ossia::net::parameter_base* key_release_code{};
    ossia::net::parameter_base* key_release_text{};

    std::vector<QMetaObject::Connection> connections;
  };
  std::vector<PerWindowParams> m_perWindow;

  void update_viewport(PerWindowParams& pw)
  {
    if(!rendersize_param || !pw.abs_cursor)
      return;

    auto v = rendersize_param->value();
    if(auto val = v.target<ossia::vec2f>())
    {
      if((*val)[0] >= 1.f && (*val)[1] >= 1.f)
      {
        auto dom = pw.abs_cursor->get_domain();
        ossia::set_max(dom, *val);
        pw.abs_cursor->set_domain(std::move(dom));
      }
      else if(pw.size_param)
      {
        v = pw.size_param->value();
        if(auto sz = v.target<ossia::vec2f>())
        {
          if((*sz)[0] >= 1.f && (*sz)[1] >= 1.f)
          {
            auto dom = pw.abs_cursor->get_domain();
            ossia::set_max(dom, *sz);
            pw.abs_cursor->set_domain(std::move(dom));
          }
        }
      }
    }
  }

public:
  multiwindow_device(
      const std::vector<OutputMapping>& mappings,
      std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_node{createMultiWindowNode(mappings)}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_node, name}
  {
    this->m_capabilities.change_tree = true;

    // Connect window signals once windows are created by createOutput()
    m_node->onWindowsCreated = [this] { connectWindowSignals(); };

    // Global FPS output parameter
    {
      auto fps_node = std::make_unique<ossia::net::generic_node>("fps", *this, m_root);
      fps_param = fps_node->create_parameter(ossia::val_type::FLOAT);
      m_node->onFps = [this](float fps) { fps_param->push_value(fps); };
      m_root.add_child(std::move(fps_node));
    }

    // Global rendersize parameter
    {
      auto rs_node
          = std::make_unique<ossia::net::generic_node>("rendersize", *this, m_root);
      ossia::net::set_description(*rs_node, "Set to [0, 0] to use the viewport's size");
      rendersize_param = rs_node->create_parameter(ossia::val_type::VEC2F);
      rendersize_param->push_value(ossia::vec2f{0.f, 0.f});
      rendersize_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          m_node->setRenderSize({(int)(*val)[0], (int)(*val)[1]});
          for(auto& pw : m_perWindow)
            update_viewport(pw);
        }
      });
      m_root.add_child(std::move(rs_node));
    }

    // Per-window subtrees: /0, /1, /2, ...
    m_perWindow.resize(mappings.size());

    for(int i = 0; i < (int)mappings.size(); ++i)
    {
      auto& pw = m_perWindow[i];
      const auto& mapping = mappings[i];
      const auto idx_str = std::to_string(i);

      auto win_node
          = std::make_unique<ossia::net::generic_node>(idx_str, *this, m_root);

      // /i/cursor
      {
        auto cursor_node
            = std::make_unique<ossia::net::generic_node>("cursor", *this, *win_node);
        {
          auto scale_node
              = std::make_unique<ossia::net::generic_node>("scaled", *this, *cursor_node);
          pw.scaled_cursor = scale_node->create_parameter(ossia::val_type::VEC2F);
          pw.scaled_cursor->set_domain(ossia::make_domain(0.f, 1.f));
          pw.scaled_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(scale_node));
        }
        {
          auto abs_node
              = std::make_unique<ossia::net::generic_node>("absolute", *this, *cursor_node);
          pw.abs_cursor = abs_node->create_parameter(ossia::val_type::VEC2F);
          pw.abs_cursor->set_domain(ossia::make_domain(
              ossia::vec2f{0.f, 0.f},
              ossia::vec2f{(float)mapping.windowSize.width(),
                           (float)mapping.windowSize.height()}));
          pw.abs_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(abs_node));
        }
        win_node->add_child(std::move(cursor_node));
      }

      // /i/size
      {
        auto size_node
            = std::make_unique<ossia::net::generic_node>("size", *this, *win_node);
        pw.size_param = size_node->create_parameter(ossia::val_type::VEC2F);
        pw.size_param->push_value(ossia::vec2f{
            (float)mapping.windowSize.width(), (float)mapping.windowSize.height()});
        pw.size_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<ossia::vec2f>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, v = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                  w->resize(QSize{(int)v[0], (int)v[1]});
              }
            });
            if(i < (int)m_perWindow.size())
              update_viewport(m_perWindow[i]);
          }
        });
        win_node->add_child(std::move(size_node));
      }

      // /i/position
      {
        auto pos_node
            = std::make_unique<ossia::net::generic_node>("position", *this, *win_node);
        pw.pos_param = pos_node->create_parameter(ossia::val_type::VEC2F);
        pw.pos_param->push_value(ossia::vec2f{
            (float)mapping.windowPosition.x(), (float)mapping.windowPosition.y()});
        pw.pos_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<ossia::vec2f>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, v = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                  w->setPosition(QPoint{(int)v[0], (int)v[1]});
              }
            });
          }
        });
        win_node->add_child(std::move(pos_node));
      }

      // /i/fullscreen
      {
        auto fs_node
            = std::make_unique<ossia::net::generic_node>("fullscreen", *this, *win_node);
        auto fs_param = fs_node->create_parameter(ossia::val_type::BOOL);
        fs_param->push_value(mapping.fullscreen);
        fs_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<bool>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, v = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                {
                  if(v)
                    w->showFullScreen();
                  else
                    w->showNormal();
                }
              }
            });
          }
        });
        win_node->add_child(std::move(fs_node));
      }

      // /i/screen
      {
        auto screen_node
            = std::make_unique<ossia::net::generic_node>("screen", *this, *win_node);
        auto screen_param = screen_node->create_parameter(ossia::val_type::STRING);
        screen_param->set_domain(ossia::make_domain(int(0), int(100)));
        screen_param->add_callback([this, i](const ossia::value& v) {
          if(auto val = v.target<int>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, scr = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                {
                  const auto& cur_screens = qApp->screens();
                  if(ossia::valid_index(scr, cur_screens))
                    w->setScreen(cur_screens[scr]);
                }
              }
            });
          }
          else if(auto val = v.target<std::string>())
          {
            ossia::qt::run_async(&m_qtContext, [this, i, scr = *val] {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                if(auto& w = outputs[i].window)
                {
                  const auto& cur_screens = qApp->screens();
                  for(auto s : cur_screens)
                    if(s->name() == scr.c_str())
                    {
                      w->setScreen(s);
                      break;
                    }
                }
              }
            });
          }
        });
        win_node->add_child(std::move(screen_node));
      }

      // /i/source (UV mapping rectangle)
      {
        auto source_node
            = std::make_unique<ossia::net::generic_node>("source", *this, *win_node);
        {
          auto pos_node
              = std::make_unique<ossia::net::generic_node>("position", *this, *source_node);
          ossia::net::set_description(*pos_node, "UV position of source rect (0-1)");
          pw.source_pos = pos_node->create_parameter(ossia::val_type::VEC2F);
          pw.source_pos->set_domain(ossia::make_domain(0.f, 1.f));
          pw.source_pos->push_value(ossia::vec2f{
              (float)mapping.sourceRect.x(), (float)mapping.sourceRect.y()});
          pw.source_pos->add_callback([this, i](const ossia::value& v) {
            if(auto val = v.target<ossia::vec2f>())
            {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                auto r = outputs[i].sourceRect;
                r.moveTopLeft(QPointF{(*val)[0], (*val)[1]});
                m_node->setSourceRect(i, r);
              }
            }
          });
          source_node->add_child(std::move(pos_node));
        }
        {
          auto sz_node
              = std::make_unique<ossia::net::generic_node>("size", *this, *source_node);
          ossia::net::set_description(*sz_node, "UV size of source rect (0-1)");
          pw.source_size = sz_node->create_parameter(ossia::val_type::VEC2F);
          pw.source_size->set_domain(ossia::make_domain(0.f, 1.f));
          pw.source_size->push_value(ossia::vec2f{
              (float)mapping.sourceRect.width(), (float)mapping.sourceRect.height()});
          pw.source_size->add_callback([this, i](const ossia::value& v) {
            if(auto val = v.target<ossia::vec2f>())
            {
              const auto& outputs = m_node->windowOutputs();
              if(i < (int)outputs.size())
              {
                auto r = outputs[i].sourceRect;
                r.setSize(QSizeF{(*val)[0], (*val)[1]});
                m_node->setSourceRect(i, r);
              }
            }
          });
          source_node->add_child(std::move(sz_node));
        }
        win_node->add_child(std::move(source_node));
      }

      // /i/key
      {
        auto key_node
            = std::make_unique<ossia::net::generic_node>("key", *this, *win_node);

        // /i/key/press
        {
          auto press_node
              = std::make_unique<ossia::net::generic_node>("press", *this, *key_node);
          {
            auto code_node
                = std::make_unique<ossia::net::generic_node>("code", *this, *press_node);
            pw.key_press_code = code_node->create_parameter(ossia::val_type::INT);
            press_node->add_child(std::move(code_node));
          }
          {
            auto text_node
                = std::make_unique<ossia::net::generic_node>("text", *this, *press_node);
            pw.key_press_text = text_node->create_parameter(ossia::val_type::STRING);
            press_node->add_child(std::move(text_node));
          }
          key_node->add_child(std::move(press_node));
        }

        // /i/key/release
        {
          auto release_node
              = std::make_unique<ossia::net::generic_node>("release", *this, *key_node);
          {
            auto code_node
                = std::make_unique<ossia::net::generic_node>("code", *this, *release_node);
            pw.key_release_code = code_node->create_parameter(ossia::val_type::INT);
            release_node->add_child(std::move(code_node));
          }
          {
            auto text_node
                = std::make_unique<ossia::net::generic_node>("text", *this, *release_node);
            pw.key_release_text = text_node->create_parameter(ossia::val_type::STRING);
            release_node->add_child(std::move(text_node));
          }
          key_node->add_child(std::move(release_node));
        }

        win_node->add_child(std::move(key_node));
      }

      m_root.add_child(std::move(win_node));
    }
  }

  // Called after windows are created by createOutput() to connect Qt signals
  void connectWindowSignals()
  {
    const auto& outputs = m_node->windowOutputs();
    for(int i = 0; i < (int)outputs.size() && i < (int)m_perWindow.size(); ++i)
    {
      auto& pw = m_perWindow[i];
      const auto& wo = outputs[i];
      if(!wo.window)
        continue;

      auto* w = wo.window.get();

      // Mouse cursor
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::mouseMove,
              [&pw, w](QPointF screen, QPointF win) {
                auto sz = w->size();
                if(sz.width() > 0 && sz.height() > 0)
                {
                  pw.scaled_cursor->push_value(ossia::vec2f{
                      float(win.x() / sz.width()), float(win.y() / sz.height())});
                  pw.abs_cursor->push_value(
                      ossia::vec2f{float(win.x()), float(win.y())});
                }
              }));

      // Window position feedback
      pw.connections.push_back(
          QObject::connect(w, &QWindow::xChanged, [&pw, w](int x) {
            pw.pos_param->set_value(ossia::vec2f{float(x), float(w->y())});
          }));
      pw.connections.push_back(
          QObject::connect(w, &QWindow::yChanged, [&pw, w](int y) {
            pw.pos_param->set_value(ossia::vec2f{float(w->x()), float(y)});
          }));

      // Keyboard
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::key,
              [&pw](int k, const QString& t) {
                pw.key_press_code->push_value(k);
                pw.key_press_text->push_value(t.toStdString());
              }));
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::keyRelease,
              [&pw](int k, const QString& t) {
                pw.key_release_code->push_value(k);
                pw.key_release_text->push_value(t.toStdString());
              }));
    }
  }

  ~multiwindow_device()
  {
    // Disconnect all window signals
    for(auto& pw : m_perWindow)
      for(auto& c : pw.connections)
        QObject::disconnect(c);
    m_perWindow.clear();

    m_node->onWindowsCreated = [] {};
    m_node->onFps = [](float) {};
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

// --- OutputMappingItem implementation ---

OutputMappingItem::OutputMappingItem(int index, const QRectF& rect, QGraphicsItem* parent)
    : QGraphicsRectItem(rect, parent)
    , m_index{index}
{
  setFlags(
      QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable
      | QGraphicsItem::ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);
  setPen(QPen(Qt::white, 1));
  setBrush(QBrush(QColor(100, 150, 255, 80)));
}

void OutputMappingItem::setOutputIndex(int idx)
{
  m_index = idx;
  update();
}

void OutputMappingItem::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  QGraphicsRectItem::paint(painter, option, widget);

  // Draw index label
  painter->setPen(Qt::white);
  auto font = painter->font();
  font.setPixelSize(14);
  painter->setFont(font);
  painter->drawText(rect(), Qt::AlignCenter, QString::number(m_index));

  // Draw selection highlight
  if(isSelected())
  {
    QPen selPen(Qt::yellow, 2, Qt::DashLine);
    painter->setPen(selPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect());
  }
}

int OutputMappingItem::hitTestEdges(const QPointF& pos) const
{
  constexpr double margin = 6.0;
  int edges = None;
  auto r = rect();
  if(pos.x() - r.left() < margin)
    edges |= Left;
  if(r.right() - pos.x() < margin)
    edges |= Right;
  if(pos.y() - r.top() < margin)
    edges |= Top;
  if(r.bottom() - pos.y() < margin)
    edges |= Bottom;
  return edges;
}

QVariant OutputMappingItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemPositionChange && scene())
  {
    // Visual scene rect = pos + rect, so clamp pos such that
    // pos + rect.topLeft >= sceneRect.topLeft and pos + rect.bottomRight <= sceneRect.bottomRight
    QPointF newPos = value.toPointF();
    auto r = rect();
    auto sr = scene()->sceneRect();

    newPos.setX(qBound(sr.left() - r.left(), newPos.x(), sr.right() - r.right()));
    newPos.setY(qBound(sr.top() - r.top(), newPos.y(), sr.bottom() - r.bottom()));
    return newPos;
  }
  if(change == ItemPositionHasChanged)
  {
    if(onChanged)
      onChanged();
  }
  return QGraphicsRectItem::itemChange(change, value);
}

void OutputMappingItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_resizeEdges = hitTestEdges(event->pos());
  if(m_resizeEdges != None)
  {
    m_dragStart = event->scenePos();
    m_rectStart = rect();
    event->accept();
  }
  else
  {
    QGraphicsRectItem::mousePressEvent(event);
  }
}

void OutputMappingItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_resizeEdges != None)
  {
    auto delta = event->scenePos() - m_dragStart;
    QRectF r = m_rectStart;
    constexpr double minSize = 10.0;

    // Scene bounds in item-local coordinates (account for pos() offset)
    auto sr = scene()->sceneRect();
    auto p = pos();
    double sLeft = sr.left() - p.x();
    double sRight = sr.right() - p.x();
    double sTop = sr.top() - p.y();
    double sBottom = sr.bottom() - p.y();

    if(m_resizeEdges & Left)
    {
      double newLeft = qMin(r.left() + delta.x(), r.right() - minSize);
      r.setLeft(qMax(newLeft, sLeft));
    }
    if(m_resizeEdges & Right)
    {
      double newRight = qMax(r.right() + delta.x(), r.left() + minSize);
      r.setRight(qMin(newRight, sRight));
    }
    if(m_resizeEdges & Top)
    {
      double newTop = qMin(r.top() + delta.y(), r.bottom() - minSize);
      r.setTop(qMax(newTop, sTop));
    }
    if(m_resizeEdges & Bottom)
    {
      double newBottom = qMax(r.bottom() + delta.y(), r.top() + minSize);
      r.setBottom(qMin(newBottom, sBottom));
    }

    setRect(r);
    if(onChanged)
      onChanged();
    event->accept();
  }
  else
  {
    QGraphicsRectItem::mouseMoveEvent(event);
  }
}

void OutputMappingItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_resizeEdges = None;
  QGraphicsRectItem::mouseReleaseEvent(event);
}

void OutputMappingItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  int edges = hitTestEdges(event->pos());
  if((edges & Left) && (edges & Top))
    setCursor(Qt::SizeFDiagCursor);
  else if((edges & Right) && (edges & Bottom))
    setCursor(Qt::SizeFDiagCursor);
  else if((edges & Left) && (edges & Bottom))
    setCursor(Qt::SizeBDiagCursor);
  else if((edges & Right) && (edges & Top))
    setCursor(Qt::SizeBDiagCursor);
  else if(edges & (Left | Right))
    setCursor(Qt::SizeHorCursor);
  else if(edges & (Top | Bottom))
    setCursor(Qt::SizeVerCursor);
  else
    setCursor(Qt::ArrowCursor);

  QGraphicsRectItem::hoverMoveEvent(event);
}

// --- OutputMappingCanvas implementation ---

OutputMappingCanvas::OutputMappingCanvas(QWidget* parent)
    : QGraphicsView(parent)
{
  setScene(&m_scene);
  m_scene.setSceneRect(0, 0, kCanvasWidth, kCanvasHeight);
  setMinimumSize(200, 150);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setBackgroundBrush(QBrush(QColor(40, 40, 40)));
  setRenderHint(QPainter::Antialiasing);

  // Draw border around the full texture area
  auto border = m_scene.addRect(0, 0, kCanvasWidth, kCanvasHeight, QPen(Qt::gray, 1));
  border->setZValue(-1);

  connect(&m_scene, &QGraphicsScene::selectionChanged, this, [this] {
    if(onSelectionChanged)
    {
      auto items = m_scene.selectedItems();
      if(!items.isEmpty())
      {
        if(auto* item = dynamic_cast<OutputMappingItem*>(items.first()))
          onSelectionChanged(item->outputIndex());
      }
      else
      {
        onSelectionChanged(-1);
      }
    }
  });
}

void OutputMappingCanvas::resizeEvent(QResizeEvent* event)
{
  QGraphicsView::resizeEvent(event);
  fitInView(m_scene.sceneRect(), Qt::KeepAspectRatio);
}

void OutputMappingCanvas::setupItemCallbacks(OutputMappingItem* item)
{
  item->onChanged = [this, item] {
    if(onItemGeometryChanged)
      onItemGeometryChanged(item->outputIndex());
  };
}

void OutputMappingCanvas::setMappings(const std::vector<OutputMapping>& mappings)
{
  // Remove existing mapping items
  QList<QGraphicsItem*> toRemove;
  for(auto* item : m_scene.items())
  {
    if(dynamic_cast<OutputMappingItem*>(item))
      toRemove.append(item);
  }
  for(auto* item : toRemove)
  {
    m_scene.removeItem(item);
    delete item;
  }

  for(int i = 0; i < (int)mappings.size(); ++i)
  {
    const auto& m = mappings[i];
    QRectF sceneRect(
        m.sourceRect.x() * kCanvasWidth, m.sourceRect.y() * kCanvasHeight,
        m.sourceRect.width() * kCanvasWidth, m.sourceRect.height() * kCanvasHeight);
    auto* item = new OutputMappingItem(i, sceneRect);
    item->screenIndex = m.screenIndex;
    item->windowPosition = m.windowPosition;
    item->windowSize = m.windowSize;
    item->fullscreen = m.fullscreen;
    setupItemCallbacks(item);
    m_scene.addItem(item);
  }
}

std::vector<OutputMapping> OutputMappingCanvas::getMappings() const
{
  std::vector<OutputMapping> result;
  QList<OutputMappingItem*> items;

  for(auto* item : m_scene.items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      items.append(mi);
  }

  // Sort by index
  std::sort(items.begin(), items.end(), [](auto* a, auto* b) {
    return a->outputIndex() < b->outputIndex();
  });

  for(auto* item : items)
  {
    OutputMapping m;
    auto r = item->mapRectToScene(item->rect());
    m.sourceRect = QRectF(
        r.x() / kCanvasWidth, r.y() / kCanvasHeight, r.width() / kCanvasWidth,
        r.height() / kCanvasHeight);
    m.screenIndex = item->screenIndex;
    m.windowPosition = item->windowPosition;
    m.windowSize = item->windowSize;
    m.fullscreen = item->fullscreen;
    result.push_back(m);
  }
  return result;
}

void OutputMappingCanvas::addOutput()
{
  int count = 0;
  for(auto* item : m_scene.items())
    if(dynamic_cast<OutputMappingItem*>(item))
      count++;

  // Place new output at a default position
  double x = (count * 30) % (int)(kCanvasWidth - 100);
  double y = (count * 30) % (int)(kCanvasHeight - 75);
  auto* item = new OutputMappingItem(count, QRectF(x, y, 100, 75));
  setupItemCallbacks(item);
  m_scene.addItem(item);
}

void OutputMappingCanvas::removeSelectedOutput()
{
  auto items = m_scene.selectedItems();
  for(auto* item : items)
  {
    if(dynamic_cast<OutputMappingItem*>(item))
    {
      m_scene.removeItem(item);
      delete item;
    }
  }

  // Re-index remaining items
  QList<OutputMappingItem*> remaining;
  for(auto* item : m_scene.items())
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
      remaining.append(mi);

  std::sort(remaining.begin(), remaining.end(), [](auto* a, auto* b) {
    return a->outputIndex() < b->outputIndex();
  });

  for(int i = 0; i < remaining.size(); ++i)
    remaining[i]->setOutputIndex(i);
}

// --- WindowSettingsWidget implementation ---

WindowSettingsWidget::WindowSettingsWidget(QWidget* parent)
    : ProtocolSettingsWidget(parent)
{
  m_deviceNameEdit = new State::AddressFragmentLineEdit{this};
  checkForChanges(m_deviceNameEdit);

  auto layout = new QFormLayout;
  layout->addRow(tr("Device Name"), m_deviceNameEdit);

  m_modeCombo = new QComboBox;
  m_modeCombo->addItem(tr("Single Window"));
  m_modeCombo->addItem(tr("Background"));
  m_modeCombo->addItem(tr("Multi-Window Mapping"));
  layout->addRow(tr("Mode"), m_modeCombo);

  m_stack = new QStackedWidget;

  // Page 0: Single (empty)
  m_stack->addWidget(new QWidget);

  // Page 1: Background (empty)
  m_stack->addWidget(new QWidget);

  // Page 2: Multi-window
  {
    auto* multiWidget = new QWidget;
    auto* multiLayout = new QVBoxLayout(multiWidget);

    // Input texture resolution (for pixel label display)
    {
      auto* resLayout = new QHBoxLayout;
      resLayout->addWidget(new QLabel(tr("Input Resolution")));
      m_inputWidth = new QSpinBox;
      m_inputWidth->setRange(1, 16384);
      m_inputWidth->setValue(1920);
      m_inputHeight = new QSpinBox;
      m_inputHeight->setRange(1, 16384);
      m_inputHeight->setValue(1080);
      resLayout->addWidget(m_inputWidth);
      resLayout->addWidget(new QLabel(QStringLiteral("x")));
      resLayout->addWidget(m_inputHeight);
      multiLayout->addLayout(resLayout);
    }

    m_canvas = new OutputMappingCanvas;
    multiLayout->addWidget(m_canvas);

    // Add/Remove buttons
    auto* btnLayout = new QHBoxLayout;
    auto* addBtn = new QPushButton(tr("Add Output"));
    auto* removeBtn = new QPushButton(tr("Remove Selected"));
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(removeBtn);
    multiLayout->addLayout(btnLayout);

    connect(addBtn, &QPushButton::clicked, this, [this] {
      m_canvas->addOutput();
    });
    connect(removeBtn, &QPushButton::clicked, this, [this] {
      m_canvas->removeSelectedOutput();
      m_selectedOutput = -1;
    });

    // Properties for selected output
    auto* propGroup = new QGroupBox(tr("Selected Output Properties"));
    auto* propLayout = new QFormLayout(propGroup);

    // Source rect (UV coordinates in the input texture)
    m_srcX = new QDoubleSpinBox;
    m_srcX->setRange(0.0, 1.0);
    m_srcX->setSingleStep(0.01);
    m_srcX->setDecimals(3);
    m_srcY = new QDoubleSpinBox;
    m_srcY->setRange(0.0, 1.0);
    m_srcY->setSingleStep(0.01);
    m_srcY->setDecimals(3);
    auto* srcPosLayout = new QHBoxLayout;
    srcPosLayout->addWidget(m_srcX);
    srcPosLayout->addWidget(m_srcY);
    m_srcPosPixelLabel = new QLabel;
    srcPosLayout->addWidget(m_srcPosPixelLabel);
    propLayout->addRow(tr("Source UV Pos"), srcPosLayout);

    m_srcW = new QDoubleSpinBox;
    m_srcW->setRange(0.01, 1.0);
    m_srcW->setSingleStep(0.01);
    m_srcW->setDecimals(3);
    m_srcW->setValue(1.0);
    m_srcH = new QDoubleSpinBox;
    m_srcH->setRange(0.01, 1.0);
    m_srcH->setSingleStep(0.01);
    m_srcH->setDecimals(3);
    m_srcH->setValue(1.0);
    auto* srcSizeLayout = new QHBoxLayout;
    srcSizeLayout->addWidget(m_srcW);
    srcSizeLayout->addWidget(m_srcH);
    m_srcSizePixelLabel = new QLabel;
    srcSizeLayout->addWidget(m_srcSizePixelLabel);
    propLayout->addRow(tr("Source UV Size"), srcSizeLayout);

    // Output window properties
    m_screenCombo = new QComboBox;
    m_screenCombo->addItem(tr("Default"));
    for(auto* screen : qApp->screens())
      m_screenCombo->addItem(screen->name());
    propLayout->addRow(tr("Screen"), m_screenCombo);

    m_winPosX = new QSpinBox;
    m_winPosX->setRange(0, 16384);
    m_winPosY = new QSpinBox;
    m_winPosY->setRange(0, 16384);
    auto* posLayout = new QHBoxLayout;
    posLayout->addWidget(m_winPosX);
    posLayout->addWidget(m_winPosY);
    propLayout->addRow(tr("Window Pos"), posLayout);

    m_winWidth = new QSpinBox;
    m_winWidth->setRange(1, 16384);
    m_winWidth->setValue(1280);
    m_winHeight = new QSpinBox;
    m_winHeight->setRange(1, 16384);
    m_winHeight->setValue(720);
    auto* sizeLayout = new QHBoxLayout;
    sizeLayout->addWidget(m_winWidth);
    sizeLayout->addWidget(m_winHeight);
    propLayout->addRow(tr("Window Size"), sizeLayout);

    m_fullscreenCheck = new QCheckBox;
    propLayout->addRow(tr("Fullscreen"), m_fullscreenCheck);

    propGroup->setEnabled(false);
    multiLayout->addWidget(propGroup);

    m_canvas->onSelectionChanged = [this, propGroup](int idx) {
      m_selectedOutput = idx;
      propGroup->setEnabled(idx >= 0);
      if(idx >= 0)
        updatePropertiesFromSelection();
    };

    m_canvas->onItemGeometryChanged = [this](int idx) {
      if(idx == m_selectedOutput)
        updatePropertiesFromSelection();
    };

    // Wire window property changes back to the selected canvas item
    auto applyProps = [this] { applyPropertiesToSelection(); };
    connect(m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, applyProps);
    connect(m_winPosX, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winPosY, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winWidth, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_winHeight, qOverload<int>(&QSpinBox::valueChanged), this, applyProps);
    connect(m_fullscreenCheck, &QCheckBox::toggled, this, applyProps);

    // Wire source rect spinboxes to update canvas item geometry
    auto applySrc = [this] { applySourceRectToSelection(); };
    connect(m_srcX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);
    connect(m_srcH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, applySrc);

    // Update pixel labels when UV or input resolution changes
    auto updatePx = [this] { updatePixelLabels(); };
    connect(m_srcX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcW, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_srcH, qOverload<double>(&QDoubleSpinBox::valueChanged), this, updatePx);
    connect(m_inputWidth, qOverload<int>(&QSpinBox::valueChanged), this, updatePx);
    connect(m_inputHeight, qOverload<int>(&QSpinBox::valueChanged), this, updatePx);
    connect(m_screenCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, updatePx);

    m_stack->addWidget(multiWidget);
  }

  layout->addRow(m_stack);
  m_deviceNameEdit->setText("window");

  connect(
      m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
      &WindowSettingsWidget::onModeChanged);

  setLayout(layout);
}

void WindowSettingsWidget::onModeChanged(int index)
{
  m_stack->setCurrentIndex(index);
}

void WindowSettingsWidget::updatePropertiesFromSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        // Block signals to avoid feedback loops when setting multiple spinboxes
        const QSignalBlocker b1(m_screenCombo);
        const QSignalBlocker b2(m_winPosX);
        const QSignalBlocker b3(m_winPosY);
        const QSignalBlocker b4(m_winWidth);
        const QSignalBlocker b5(m_winHeight);
        const QSignalBlocker b6(m_fullscreenCheck);
        const QSignalBlocker b7(m_srcX);
        const QSignalBlocker b8(m_srcY);
        const QSignalBlocker b9(m_srcW);
        const QSignalBlocker b10(m_srcH);

        // Update source rect from canvas item geometry
        auto sceneRect = mi->mapRectToScene(mi->rect());
        m_srcX->setValue(sceneRect.x() / OutputMappingCanvas::kCanvasWidth);
        m_srcY->setValue(sceneRect.y() / OutputMappingCanvas::kCanvasHeight);
        m_srcW->setValue(sceneRect.width() / OutputMappingCanvas::kCanvasWidth);
        m_srcH->setValue(sceneRect.height() / OutputMappingCanvas::kCanvasHeight);

        // Update window properties
        m_screenCombo->setCurrentIndex(mi->screenIndex + 1); // +1 because index 0 is "Default"
        m_winPosX->setValue(mi->windowPosition.x());
        m_winPosY->setValue(mi->windowPosition.y());
        m_winWidth->setValue(mi->windowSize.width());
        m_winHeight->setValue(mi->windowSize.height());
        m_fullscreenCheck->setChecked(mi->fullscreen);

        updatePixelLabels();
        return;
      }
    }
  }
}

void WindowSettingsWidget::applyPropertiesToSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        mi->screenIndex = m_screenCombo->currentIndex() - 1; // -1 because index 0 is "Default"
        mi->windowPosition = QPoint(m_winPosX->value(), m_winPosY->value());
        mi->windowSize = QSize(m_winWidth->value(), m_winHeight->value());
        mi->fullscreen = m_fullscreenCheck->isChecked();
        return;
      }
    }
  }
}

void WindowSettingsWidget::applySourceRectToSelection()
{
  if(!m_canvas || m_selectedOutput < 0)
    return;

  for(auto* item : m_canvas->scene()->items())
  {
    if(auto* mi = dynamic_cast<OutputMappingItem*>(item))
    {
      if(mi->outputIndex() == m_selectedOutput)
      {
        QRectF newSceneRect(
            m_srcX->value() * OutputMappingCanvas::kCanvasWidth,
            m_srcY->value() * OutputMappingCanvas::kCanvasHeight,
            m_srcW->value() * OutputMappingCanvas::kCanvasWidth,
            m_srcH->value() * OutputMappingCanvas::kCanvasHeight);

        // Temporarily disable onChanged to avoid feedback loop
        auto savedCallback = std::move(mi->onChanged);
        mi->setPos(0, 0);
        mi->setRect(newSceneRect);
        mi->onChanged = std::move(savedCallback);
        return;
      }
    }
  }
}

void WindowSettingsWidget::updatePixelLabels()
{
  if(!m_inputWidth || !m_inputHeight)
    return;

  int w = m_inputWidth->value();
  int h = m_inputHeight->value();

  int pxX = int(m_srcX->value() * w);
  int pxY = int(m_srcY->value() * h);
  int pxW = qMax(1, int(m_srcW->value() * w));
  int pxH = qMax(1, int(m_srcH->value() * h));

  if(m_srcPosPixelLabel)
    m_srcPosPixelLabel->setText(QStringLiteral("(%1, %2 px)").arg(pxX).arg(pxY));

  if(m_srcSizePixelLabel)
    m_srcSizePixelLabel->setText(QStringLiteral("%1 x %2 px").arg(pxW).arg(pxH));

  // Auto-match window pos/size when screen is "Default" (combo index 0 = screenIndex -1)
  if(m_screenCombo && m_screenCombo->currentIndex() == 0 && m_selectedOutput >= 0)
  {
    const QSignalBlocker bx(m_winPosX);
    const QSignalBlocker by(m_winPosY);
    const QSignalBlocker bw(m_winWidth);
    const QSignalBlocker bh(m_winHeight);

    m_winPosX->setValue(pxX);
    m_winPosY->setValue(pxY);
    m_winWidth->setValue(pxW);
    m_winHeight->setValue(pxH);

    applyPropertiesToSelection();
  }
}

Device::DeviceSettings WindowSettingsWidget::getSettings() const
{
  Device::DeviceSettings s;
  s.name = m_deviceNameEdit->text();
  s.protocol = WindowProtocolFactory::static_concreteKey();
  WindowSettings set;
  set.mode = (WindowMode)m_modeCombo->currentIndex();

  if(set.mode == WindowMode::MultiWindow && m_canvas)
  {
    // Apply any pending property changes before reading
    const_cast<WindowSettingsWidget*>(this)->applyPropertiesToSelection();
    set.outputs = m_canvas->getMappings();
  }

  s.deviceSpecificSettings = QVariant::fromValue(set);
  return s;
}

void WindowSettingsWidget::setSettings(const Device::DeviceSettings& settings)
{
  m_deviceNameEdit->setText(settings.name);
  if(settings.deviceSpecificSettings.canConvert<WindowSettings>())
  {
    const auto set = settings.deviceSpecificSettings.value<WindowSettings>();
    m_modeCombo->setCurrentIndex((int)set.mode);
    m_stack->setCurrentIndex((int)set.mode);

    if(set.mode == WindowMode::MultiWindow && m_canvas)
    {
      m_canvas->setMappings(set.outputs);
    }
  }
}

}

template <>
void DataStreamReader::read(const Gfx::OutputMapping& n)
{
  m_stream << n.sourceRect << n.screenIndex << n.windowPosition << n.windowSize
           << n.fullscreen;
}

template <>
void DataStreamWriter::write(Gfx::OutputMapping& n)
{
  m_stream >> n.sourceRect >> n.screenIndex >> n.windowPosition >> n.windowSize
      >> n.fullscreen;
}

template <>
void DataStreamReader::read(const Gfx::WindowSettings& n)
{
  m_stream << (int32_t)2; // version tag
  m_stream << (int32_t)n.mode;
  m_stream << (int32_t)n.outputs.size();
  for(const auto& o : n.outputs)
    read(o);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Gfx::WindowSettings& n)
{
  int32_t version{};
  m_stream >> version;
  if(version == 0 || version == 1)
  {
    // Old format: version is actually the bool background value
    n.mode = version ? Gfx::WindowMode::Background : Gfx::WindowMode::Single;
  }
  else if(version == 2)
  {
    int32_t mode{};
    m_stream >> mode;
    n.mode = (Gfx::WindowMode)mode;
    int32_t count{};
    m_stream >> count;
    n.outputs.resize(count);
    for(auto& o : n.outputs)
      write(o);
  }
  checkDelimiter();
}

template <>
void JSONReader::read(const Gfx::OutputMapping& n)
{
  stream.StartObject();
  stream.Key("SourceRect");
  stream.StartArray();
  stream.Double(n.sourceRect.x());
  stream.Double(n.sourceRect.y());
  stream.Double(n.sourceRect.width());
  stream.Double(n.sourceRect.height());
  stream.EndArray();
  stream.Key("ScreenIndex");
  stream.Int(n.screenIndex);
  stream.Key("WindowPosition");
  stream.StartArray();
  stream.Int(n.windowPosition.x());
  stream.Int(n.windowPosition.y());
  stream.EndArray();
  stream.Key("WindowSize");
  stream.StartArray();
  stream.Int(n.windowSize.width());
  stream.Int(n.windowSize.height());
  stream.EndArray();
  stream.Key("Fullscreen");
  stream.Bool(n.fullscreen);
  stream.EndObject();
}

template <>
void JSONWriter::write(Gfx::OutputMapping& n)
{
  if(auto sr = obj.tryGet("SourceRect"))
  {
    auto arr = sr->toArray();
    if(arr.Size() == 4)
      n.sourceRect = QRectF(
          arr[0].GetDouble(), arr[1].GetDouble(), arr[2].GetDouble(),
          arr[3].GetDouble());
  }
  if(auto v = obj.tryGet("ScreenIndex"))
    n.screenIndex = v->toInt();
  if(auto wp = obj.tryGet("WindowPosition"))
  {
    auto arr = wp->toArray();
    if(arr.Size() == 2)
      n.windowPosition = QPoint(arr[0].GetInt(), arr[1].GetInt());
  }
  if(auto ws = obj.tryGet("WindowSize"))
  {
    auto arr = ws->toArray();
    if(arr.Size() == 2)
      n.windowSize = QSize(arr[0].GetInt(), arr[1].GetInt());
  }
  if(auto v = obj.tryGet("Fullscreen"))
    n.fullscreen = v->toBool();
}

template <>
void JSONReader::read(const Gfx::WindowSettings& n)
{
  obj["Mode"] = (int)n.mode;
  obj["Outputs"] = n.outputs;
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
}

SCORE_SERALIZE_DATASTREAM_DEFINE(Gfx::WindowSettings);
