#pragma once
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>
namespace Gfx
{
static score::gfx::MultiWindowNode*
createMultiWindowNode(const std::vector<OutputMapping>& mappings)
{
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

      auto win_node = std::make_unique<ossia::net::generic_node>(idx_str, *this, m_root);

      // /i/cursor
      {
        auto cursor_node
            = std::make_unique<ossia::net::generic_node>("cursor", *this, *win_node);
        {
          auto scale_node = std::make_unique<ossia::net::generic_node>(
              "scaled", *this, *cursor_node);
          pw.scaled_cursor = scale_node->create_parameter(ossia::val_type::VEC2F);
          pw.scaled_cursor->set_domain(ossia::make_domain(0.f, 1.f));
          pw.scaled_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(scale_node));
        }
        {
          auto abs_node = std::make_unique<ossia::net::generic_node>(
              "absolute", *this, *cursor_node);
          pw.abs_cursor = abs_node->create_parameter(ossia::val_type::VEC2F);
          pw.abs_cursor->set_domain(
              ossia::make_domain(
                  ossia::vec2f{0.f, 0.f}, ossia::vec2f{
                                              (float)mapping.windowSize.width(),
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
        pw.size_param->push_value(
            ossia::vec2f{
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
        pw.pos_param->push_value(
            ossia::vec2f{
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

      // /i/screen (accepts an integer index or a screen name string)
      {
        auto screen_node
            = std::make_unique<ossia::net::generic_node>("screen", *this, *win_node);
        auto screen_param = screen_node->create_parameter(ossia::val_type::STRING);
        screen_param->push_value(
            mapping.screenIndex >= 0 ? std::to_string(mapping.screenIndex)
                                     : std::string{});
        screen_param->add_callback([this, i](const ossia::value& v) {
          auto apply = [this, i](const std::string& scr) {
            const auto& outputs = m_node->windowOutputs();
            if(i >= (int)outputs.size())
              return;
            auto& w = outputs[i].window;
            if(!w)
              return;

            const auto& cur_screens = qApp->screens();

            // Try parsing as integer index first
            bool ok = false;
            int idx = QString::fromStdString(scr).toInt(&ok);
            if(ok)
            {
              if(ossia::valid_index(idx, cur_screens))
                w->setScreen(cur_screens[idx]);
              return;
            }

            // Otherwise match by screen name
            for(auto s : cur_screens)
            {
              if(s->name().toStdString() == scr)
              {
                w->setScreen(s);
                return;
              }
            }
          };

          if(auto val = v.target<std::string>())
          {
            ossia::qt::run_async(
                &m_qtContext, [apply, scr = *val] { apply(scr); });
          }
          else if(auto val = v.target<int>())
          {
            ossia::qt::run_async(
                &m_qtContext, [apply, scr = std::to_string(*val)] { apply(scr); });
          }
        });
        win_node->add_child(std::move(screen_node));
      }

      // /i/source (UV mapping rectangle)
      {
        auto source_node
            = std::make_unique<ossia::net::generic_node>("source", *this, *win_node);
        {
          auto pos_node = std::make_unique<ossia::net::generic_node>(
              "position", *this, *source_node);
          ossia::net::set_description(*pos_node, "UV position of source rect (0-1)");
          pw.source_pos = pos_node->create_parameter(ossia::val_type::VEC2F);
          pw.source_pos->set_domain(ossia::make_domain(0.f, 1.f));
          pw.source_pos->push_value(
              ossia::vec2f{
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
          pw.source_size->push_value(
              ossia::vec2f{
                  (float)mapping.sourceRect.width(),
                  (float)mapping.sourceRect.height()});
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

      // /i/blend (soft-edge blending per side)
      {
        auto blend_node
            = std::make_unique<ossia::net::generic_node>("blend", *this, *win_node);

        struct BlendSide
        {
          const char* name;
          int side;
          float initW;
          float initG;
        };
        BlendSide sides[]
            = {{"left", 0, mapping.blendLeft.width, mapping.blendLeft.gamma},
               {"right", 1, mapping.blendRight.width, mapping.blendRight.gamma},
               {"top", 2, mapping.blendTop.width, mapping.blendTop.gamma},
               {"bottom", 3, mapping.blendBottom.width, mapping.blendBottom.gamma}};

        for(auto& [sname, side, initW, initG] : sides)
        {
          auto side_node
              = std::make_unique<ossia::net::generic_node>(sname, *this, *blend_node);
          {
            auto w_node
                = std::make_unique<ossia::net::generic_node>("width", *this, *side_node);
            ossia::net::set_description(*w_node, "Blend width in UV space (0-0.5)");
            auto w_param = w_node->create_parameter(ossia::val_type::FLOAT);
            w_param->set_domain(ossia::make_domain(0.f, 0.5f));
            w_param->push_value(initW);
            w_param->add_callback([this, i, side](const ossia::value& v) {
              if(auto val = v.target<float>())
              {
                const auto& outputs = m_node->windowOutputs();
                if(i < (int)outputs.size())
                {
                  float gamma = 2.2f;
                  switch(side)
                  {
                    case 0:
                      gamma = outputs[i].blendLeft.gamma;
                      break;
                    case 1:
                      gamma = outputs[i].blendRight.gamma;
                      break;
                    case 2:
                      gamma = outputs[i].blendTop.gamma;
                      break;
                    case 3:
                      gamma = outputs[i].blendBottom.gamma;
                      break;
                  }
                  m_node->setEdgeBlend(i, side, *val, gamma);
                }
              }
            });
            side_node->add_child(std::move(w_node));
          }
          {
            auto g_node
                = std::make_unique<ossia::net::generic_node>("gamma", *this, *side_node);
            ossia::net::set_description(*g_node, "Blend curve exponent (0.1-4.0)");
            auto g_param = g_node->create_parameter(ossia::val_type::FLOAT);
            g_param->set_domain(ossia::make_domain(0.1f, 4.f));
            g_param->push_value(initG);
            g_param->add_callback([this, i, side](const ossia::value& v) {
              if(auto val = v.target<float>())
              {
                const auto& outputs = m_node->windowOutputs();
                if(i < (int)outputs.size())
                {
                  float width = 0.f;
                  switch(side)
                  {
                    case 0:
                      width = outputs[i].blendLeft.width;
                      break;
                    case 1:
                      width = outputs[i].blendRight.width;
                      break;
                    case 2:
                      width = outputs[i].blendTop.width;
                      break;
                    case 3:
                      width = outputs[i].blendBottom.width;
                      break;
                  }
                  m_node->setEdgeBlend(i, side, width, *val);
                }
              }
            });
            side_node->add_child(std::move(g_node));
          }
          blend_node->add_child(std::move(side_node));
        }
        win_node->add_child(std::move(blend_node));
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
            auto code_node = std::make_unique<ossia::net::generic_node>(
                "code", *this, *release_node);
            pw.key_release_code = code_node->create_parameter(ossia::val_type::INT);
            release_node->add_child(std::move(code_node));
          }
          {
            auto text_node = std::make_unique<ossia::net::generic_node>(
                "text", *this, *release_node);
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
          QObject::connect(
              w, &score::gfx::Window::mouseMove, [&pw, w](QPointF screen, QPointF win) {
        auto sz = w->size();
        if(sz.width() > 0 && sz.height() > 0)
        {
          pw.scaled_cursor->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          pw.abs_cursor->push_value(ossia::vec2f{float(win.x()), float(win.y())});
        }
      }));

      // // Window position feedback
      // pw.connections.push_back(
      //     QObject::connect(w, &QWindow::xChanged, [&pw, w](int x) {
      //       pw.pos_param->set_value(ossia::vec2f{float(x), float(w->y())});
      //     }));
      // pw.connections.push_back(
      //     QObject::connect(w, &QWindow::yChanged, [&pw, w](int y) {
      //       pw.pos_param->set_value(ossia::vec2f{float(w->x()), float(y)});
      //     }));

      // Keyboard
      pw.connections.push_back(
          QObject::connect(w, &score::gfx::Window::key, [&pw](int k, const QString& t) {
        pw.key_press_code->push_value(k);
        pw.key_press_text->push_value(t.toStdString());
      }));
      pw.connections.push_back(
          QObject::connect(
              w, &score::gfx::Window::keyRelease, [&pw](int k, const QString& t) {
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

    m_node->onWindowsCreated = [] { };
    m_node->onFps = [](float) { };
    m_protocol->stop();
    m_root.clear_children();
    m_protocol.reset();
  }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

}
