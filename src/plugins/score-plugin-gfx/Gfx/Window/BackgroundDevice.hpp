#pragma once

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Window/PointerAddresses.hpp>

#include <score/graphics/BackgroundRenderer.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>

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
  ossia::net::parameter_base* gl_win{};
  ossia::net::parameter_base* abs_win{};
  ossia::net::parameter_base* abs_tablet_win{};
  ossia::net::parameter_base* size_param{};
  ossia::net::parameter_base* rendersize_param{};
  pointer_addresses m_pointers;
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
        auto scale_node = std::make_unique<ossia::net::generic_node>("gl", *this, *node);
        gl_win = scale_node->create_parameter(ossia::val_type::VEC2F);
        gl_win->set_domain(ossia::make_domain(0.f, 1.f));
        gl_win->push_value(ossia::vec2f{0.f, 0.f});
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
      m_pointers.add_clip(*this, *node);

      con.push_back(
          QObject::connect(
              &v, &Scenario::ProcessGraphicsView::hoverMove, &m_qtContext,
              [this, v = QPointer{&view}, ptr = QPointer{m_screen}](QHoverEvent* e) {
        if(ptr && v)
        {
          const auto win = e->position();
          const auto sz = v->view().size();
          scaled_win->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          gl_win->push_value(
              ossia::vec2f{
                  float(win.x() / sz.width()), 1.f - float(win.y() / sz.height())});
          const auto clip = score::gfx::PointerState::toClip(win, QSizeF(sz));
          m_pointers.set_clip(float(clip.x()), float(clip.y()));
          // The view sends hovers only: slot 0 moves, never pressed.
          m_pointers.move_slot(0, float(clip.x()), float(clip.y()));
          abs_win->push_value(ossia::vec2f{float(win.x()), float(win.y())});
        }
      }, Qt::DirectConnection));

      m_root.add_child(std::move(node));
    }
    m_pointers.add_touch(
        *this, m_root, static_cast<gfx_protocol_base*>(m_protocol.get())->context);

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

}
