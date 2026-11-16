#pragma once
#include <algorithm>
#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/MultiWindowNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Window/CalibrationSet.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <score/tools/ProjectFiles.hpp>

#include <ossia-qt/invoke.hpp>

#include <QGuiApplication>
namespace Gfx
{
//! `m` with every file located against `roots`, as score::locateFilePath does.
inline OutputMaps locatedMaps(OutputMaps m, const score::PathRoots& roots)
{
  for(QString* f : {&m.warp, &m.alpha, &m.beta, &m.lut, &m.response, &m.compensation,
                    &m.calibration})
    if(!f->isEmpty())
      *f = score::locateFilePath(*f, roots);
  return m;
}

static score::gfx::MultiWindowNode* createMultiWindowNode(
    std::vector<OutputMapping> mappings, SwapchainFlag swapFlag,
    SwapchainFormat swapFormat, const score::PathRoots& roots, QSize canvasSize)
{
  for(auto& m : mappings)
  {
    m.maps = locatedMaps(std::move(m.maps), roots);
    refreshCalibration(m.maps);
  }
  const auto& gfx_settings = score::AppContext().settings<Gfx::Settings::Model>();

  score::gfx::OutputNode::Configuration conf;
  double rate = gfx_settings.getRate();
  if(rate > 0)
    conf = {.manualRenderingRate = 1000. / rate, .supportsVSync = false};
  else
    conf = {.manualRenderingRate = 1000. / 60., .supportsVSync = false};

  auto node = new score::gfx::MultiWindowNode{conf, mappings, canvasSize};
  node->setSwapchainFlag(swapFlag);
  node->setSwapchainFormat(swapFormat);
  return node;
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
    ossia::net::parameter_base* gl_cursor{};
    ossia::net::parameter_base* clip_cursor{};
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
  score::PathRoots m_roots;

  struct MapParam
  {
    QString OutputMaps::* member;
    ossia::net::parameter_base* param;
  };
  std::vector<std::vector<MapParam>> m_mapParams;
  std::vector<ossia::net::parameter_base*> m_calibrationParams;
  std::vector<QString> m_mapSignatures;
  std::unique_ptr<CalibrationWatcher> m_calibrationWatcher;

  const OutputMaps& maps(int i) const { return m_node->mappings()[i].maps; }

  void syncMapParams(int i)
  {
    if(i < (int)m_mapParams.size())
      for(auto& [member, p] : m_mapParams[i])
        p->set_value_quiet((maps(i).*member).toStdString());
    if(i < (int)m_calibrationParams.size() && m_calibrationParams[i])
      m_calibrationParams[i]->set_value_quiet(maps(i).calibration.toStdString());
  }

  void watchCalibrations()
  {
    if(m_calibrationWatcher)
      m_calibrationWatcher->setFiles(watchedCalibrationFiles(outputMaps()));
  }

  void setOutputMaps(int i, const OutputMaps& m)
  {
    m_mapSignatures[i] = mapsSignature(m);
    m_node->setMaps(i, m);
    syncMapParams(i);
    watchCalibrations();
  }

  //! Runs `f` on the GUI thread, which owns the node's outputs.
  template <typename F>
  void onGui(F&& f)
  {
    ossia::qt::run_async(&m_qtContext, std::forward<F>(f));
  }

public:
  //! Reloads the maps whose files changed on disk, following their calibration.
  void reloadCalibrations()
  {
    const int n = (int)m_node->mappings().size();
    for(int i = 0; i < n; i++)
    {
      auto m = maps(i);
      const bool moved = refreshCalibration(m);
      if(moved || mapsSignature(m) != m_mapSignatures[i])
      {
        m_mapSignatures[i] = mapsSignature(m);
        m_node->setMaps(i, m);
        syncMapParams(i);
      }
    }
    watchCalibrations();
  }
  std::vector<OutputMaps> outputMaps() const
  {
    std::vector<OutputMaps> out;
    for(const auto& m : m_node->mappings())
      out.push_back(m.maps);
    return out;
  }
  QStringList watchedFiles() const
  {
    return m_calibrationWatcher ? m_calibrationWatcher->watched() : QStringList{};
  }

private:

  const EdgeBlend& blendSide(int i, int side) const
  {
    const auto& m = m_node->mappings()[i];
    const EdgeBlend* sides[4]{&m.blendLeft, &m.blendRight, &m.blendTop, &m.blendBottom};
    return *sides[side];
  }

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
      std::unique_ptr<gfx_protocol_base> proto, std::string name,
      SwapchainFlag swapFlag = SwapchainFlag::NoFlag,
      SwapchainFormat swapFormat = SwapchainFormat::SDR,
      score::PathRoots roots = score::pathRoots(), QSize canvasSize = QSize{1280, 720},
      CanvasLayout canvas = {})
      : ossia::net::device_base{std::move(proto)}
      , m_node{createMultiWindowNode(mappings, swapFlag, swapFormat, roots, canvasSize)}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_node, name}
      , m_roots{std::move(roots)}
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

    m_node->setCanvasLayout(canvas);
    {
      auto canvas_node
          = std::make_unique<ossia::net::generic_node>("canvas", *this, m_root);
      {
        auto n = std::make_unique<ossia::net::generic_node>("projection", *this, *canvas_node);
        ossia::net::set_description(
            *n, "What the canvas holds: 0 none, 1 equirectangular (enables orientation)");
        auto p = n->create_parameter(ossia::val_type::INT);
        p->set_domain(ossia::make_domain(0, 1));
        p->set_bounding(ossia::bounding_mode::CLIP);
        p->push_value(int(canvas.projection));
        p->add_callback([this](const ossia::value& v) {
          if(auto val = v.target<int>())
            onGui([this, x = *val] {
              auto c = m_node->canvasLayout();
              c.projection = x == 1 ? CanvasProjection::Equirectangular
                                    : CanvasProjection::None;
              m_node->setCanvasLayout(c);
            });
        });
        canvas_node->add_child(std::move(n));
      }
      {
        auto n = std::make_unique<ossia::net::generic_node>("orientation", *this, *canvas_node);
        ossia::net::set_description(
            *n, "Equirectangular canvas orientation, Euler degrees (pitch, yaw, roll)");
        auto p = n->create_parameter(ossia::val_type::VEC3F);
        p->set_domain(ossia::make_domain(-180.f, 180.f));
        p->push_value(
            ossia::vec3f{canvas.orientation[0], canvas.orientation[1], canvas.orientation[2]});
        p->add_callback([this](const ossia::value& v) {
          if(auto val = v.target<ossia::vec3f>())
            onGui([this, x = *val] {
              auto c = m_node->canvasLayout();
              c.orientation = {x[0], x[1], x[2]};
              m_node->setCanvasLayout(c);
            });
        });
        canvas_node->add_child(std::move(n));
      }
      m_root.add_child(std::move(canvas_node));
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
          // setRenderSize tears down GPU resources: marshal to the Qt
          // thread like the per-window callbacks below.
          ossia::qt::run_async(&m_qtContext, [this, v = *val] {
            m_node->setRenderSize({(int)v[0], (int)v[1]});
            for(auto& pw : m_perWindow)
              update_viewport(pw);
          });
        }
      });
      m_root.add_child(std::move(rs_node));
    }

    // Per-window subtrees: /0, /1, /2, ...
    m_perWindow.resize(mappings.size());
    m_mapParams.resize(mappings.size());
    m_calibrationParams.resize(mappings.size());
    m_mapSignatures.resize(mappings.size());
    for(int i = 0; i < (int)mappings.size(); i++)
      m_mapSignatures[i] = mapsSignature(maps(i));

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
          auto scale_node
              = std::make_unique<ossia::net::generic_node>("gl", *this, *cursor_node);
          pw.gl_cursor = scale_node->create_parameter(ossia::val_type::VEC2F);
          pw.gl_cursor->set_domain(ossia::make_domain(0.f, 1.f));
          pw.gl_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(scale_node));
        }
        {
          // Clip space, [-1, 1], y up: vertexshaderart's mouse.
          auto clip_node
              = std::make_unique<ossia::net::generic_node>("clip", *this, *cursor_node);
          pw.clip_cursor = clip_node->create_parameter(ossia::val_type::VEC2F);
          pw.clip_cursor->set_domain(ossia::make_domain(-1.f, 1.f));
          pw.clip_cursor->push_value(ossia::vec2f{0.f, 0.f});
          cursor_node->add_child(std::move(clip_node));
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

            const auto& cur_screens = qGuiApp->screens();

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
              onGui([this, i, v = *val] {
                auto r = m_node->mappings()[i].sourceRect;
                r.moveTopLeft(QPointF{v[0], v[1]});
                m_node->setSourceRect(i, r);
              });
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
              onGui([this, i, v = *val] {
                auto r = m_node->mappings()[i].sourceRect;
                r.setSize(QSizeF{v[0], v[1]});
                m_node->setSourceRect(i, r);
              });
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
                onGui([this, i, side, w = *val] {
                  m_node->setEdgeBlend(i, side, w, blendSide(i, side).gamma);
                });
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
                onGui([this, i, side, g = *val] {
                  m_node->setEdgeBlend(i, side, blendSide(i, side).width, g);
                });
            });
            side_node->add_child(std::move(g_node));
          }
          blend_node->add_child(std::move(side_node));
        }
        win_node->add_child(std::move(blend_node));
      }

      // /i/warp (corner pin, output UV space, y down)
      {
        auto warp_node
            = std::make_unique<ossia::net::generic_node>("warp", *this, *win_node);
        struct Corner
        {
          const char* name;
          QPointF Gfx::CornerWarp::* member;
        };
        const Corner corners[]
            = {{"tl", &Gfx::CornerWarp::topLeft},
               {"tr", &Gfx::CornerWarp::topRight},
               {"bl", &Gfx::CornerWarp::bottomLeft},
               {"br", &Gfx::CornerWarp::bottomRight}};
        for(const auto& [cname, member] : corners)
        {
          auto c_node
              = std::make_unique<ossia::net::generic_node>(cname, *this, *warp_node);
          ossia::net::set_description(
              *c_node, "Corner position in output UV space (0-1, y down)");
          auto c_param = c_node->create_parameter(ossia::val_type::VEC2F);
          const QPointF init = mapping.cornerWarp.*member;
          c_param->push_value(ossia::vec2f{(float)init.x(), (float)init.y()});
          c_param->add_callback([this, i, member](const ossia::value& v) {
            if(auto val = v.target<ossia::vec2f>())
              onGui([this, i, member, v = *val] {
                auto w = m_node->mappings()[i].cornerWarp;
                w.*member = QPointF{v[0], v[1]};
                m_node->setCornerWarp(i, w);
              });
          });
          warp_node->add_child(std::move(c_node));
        }
        win_node->add_child(std::move(warp_node));
      }

      // /i/color (applied last, after the blend)
      {
        auto color_node
            = std::make_unique<ossia::net::generic_node>("color", *this, *win_node);
        auto setColor = [this, i](auto f) {
          onGui([this, i, f = std::move(f)] {
            auto c = m_node->mappings()[i].color;
            f(c);
            m_node->setColor(i, c);
          });
        };
        {
          auto n = std::make_unique<ossia::net::generic_node>("gain", *this, *color_node);
          ossia::net::set_description(*n, "RGB gain");
          auto p = n->create_parameter(ossia::val_type::VEC3F);
          p->set_domain(ossia::make_domain(0.f, 4.f));
          p->push_value(
              ossia::vec3f{
                  mapping.color.gain[0], mapping.color.gain[1], mapping.color.gain[2]});
          p->add_callback([setColor](const ossia::value& v) {
            if(auto val = v.target<ossia::vec3f>())
              setColor([v = *val](Gfx::OutputColor& c) { c.gain = {v[0], v[1], v[2]}; });
          });
          color_node->add_child(std::move(n));
        }
        {
          auto n
              = std::make_unique<ossia::net::generic_node>("offset", *this, *color_node);
          ossia::net::set_description(*n, "RGB offset");
          auto p = n->create_parameter(ossia::val_type::VEC3F);
          p->set_domain(ossia::make_domain(-1.f, 1.f));
          p->push_value(
              ossia::vec3f{
                  mapping.color.offset[0], mapping.color.offset[1],
                  mapping.color.offset[2]});
          p->add_callback([setColor](const ossia::value& v) {
            if(auto val = v.target<ossia::vec3f>())
              setColor([v = *val](Gfx::OutputColor& c) { c.offset = {v[0], v[1], v[2]}; });
          });
          color_node->add_child(std::move(n));
        }
        {
          auto n
              = std::make_unique<ossia::net::generic_node>("gamma", *this, *color_node);
          ossia::net::set_description(*n, "Output gamma: out = pow(c, 1 / gamma)");
          auto p = n->create_parameter(ossia::val_type::FLOAT);
          p->set_domain(ossia::make_domain(0.1f, 4.f));
          p->push_value(mapping.color.gamma);
          p->add_callback([setColor](const ossia::value& v) {
            if(auto val = v.target<float>(); val && *val > 0.f)
              setColor([v = *val](Gfx::OutputColor& c) { c.gamma = v; });
          });
          color_node->add_child(std::move(n));
        }
        {
          auto n
              = std::make_unique<ossia::net::generic_node>("linear", *this, *color_node);
          ossia::net::set_description(
              *n, "Apply the blend ramps, alpha and beta maps in linear light");
          auto p = n->create_parameter(ossia::val_type::BOOL);
          p->push_value(mapping.color.linearBlend);
          p->add_callback([setColor](const ossia::value& v) {
            if(auto val = v.target<bool>())
              setColor([v = *val](Gfx::OutputColor& c) { c.linearBlend = v; });
          });
          color_node->add_child(std::move(n));
        }
        win_node->add_child(std::move(color_node));
      }

      // /i/maps (calibration files, reloaded on change)
      {
        auto maps_node
            = std::make_unique<ossia::net::generic_node>("maps", *this, *win_node);
        struct MapField
        {
          const char* name;
          const char* description;
          QString Gfx::OutputMaps::* member;
        };
        const MapField fields[] = {
            {"warp", "UV lookup map file (PFM, 16-bit PNG)", &Gfx::OutputMaps::warp},
            {"alpha", "Blend weight map file", &Gfx::OutputMaps::alpha},
            {"beta", "Black level map file", &Gfx::OutputMaps::beta},
            {"lut", "Colour lookup table file (.cube), applied last", &Gfx::OutputMaps::lut},
            {"response", "Measured projector response (.response.json, 1D .cube)",
             &Gfx::OutputMaps::response},
            {"compensation", "Radiometric compensation (.comp.json)",
             &Gfx::OutputMaps::compensation}};
        for(const auto& [fname, fdesc, member] : fields)
        {
          auto f_node
              = std::make_unique<ossia::net::generic_node>(fname, *this, *maps_node);
          ossia::net::set_description(*f_node, fdesc);
          auto f_param = f_node->create_parameter(ossia::val_type::STRING);
          f_param->push_value((maps(i).*member).toStdString());
          f_param->add_callback([this, i, member](const ossia::value& v) {
            if(auto val = v.target<std::string>())
            {
              onGui([this, i, member, path = *val] {
                auto m = maps(i);
                const QString p = QString::fromStdString(path);
                m.*member = p.isEmpty() ? p : score::locateFilePath(p, m_roots);
                setOutputMaps(i, m);
              });
            }
          });
          m_mapParams[i].push_back({member, f_param});
          maps_node->add_child(std::move(f_node));
        }
        {
          auto c_node
              = std::make_unique<ossia::net::generic_node>("calibration", *this, *maps_node);
          ossia::net::set_description(
              *c_node, "Calibration name.json: fills the maps that exist for that name and "
                       "follows them on disk");
          auto c_param = c_node->create_parameter(ossia::val_type::STRING);
          c_param->push_value(maps(i).calibration.toStdString());
          c_param->add_callback([this, i](const ossia::value& v) {
            if(auto val = v.target<std::string>())
            {
              onGui([this, i, path = *val] {
                auto m = maps(i);
                const QString p = QString::fromStdString(path);
                applyCalibration(m, p.isEmpty() ? p : score::locateFilePath(p, m_roots));
                setOutputMaps(i, m);
              });
            }
          });
          m_calibrationParams[i] = c_param;
          maps_node->add_child(std::move(c_node));
        }
        win_node->add_child(std::move(maps_node));
      }

      // /i/quality
      {
        auto quality_node
            = std::make_unique<ossia::net::generic_node>("quality", *this, *win_node);
        auto setQuality = [this, i](auto f) {
          onGui([this, i, f = std::move(f)] {
            auto q = m_node->mappings()[i].quality;
            f(q);
            m_node->setQuality(i, q);
          });
        };
        auto addInt = [&](const char* name, const char* desc, int min, int max, int init,
                          auto apply) {
          auto n = std::make_unique<ossia::net::generic_node>(name, *this, *quality_node);
          ossia::net::set_description(*n, desc);
          auto p = n->create_parameter(ossia::val_type::INT);
          p->set_domain(ossia::make_domain(min, max));
          p->set_bounding(ossia::bounding_mode::CLIP);
          p->push_value(init);
          p->add_callback([setQuality, apply, min, max](const ossia::value& v) {
            if(auto val = v.target<int>())
            {
              const int x = std::clamp(*val, min, max);
              setQuality([apply, x](Gfx::OutputQuality& q) { apply(q, x); });
            }
          });
          quality_node->add_child(std::move(n));
        };
        addInt(
            "sampling",
            "Canvas sampling: 0 auto, 1 linear, 2 mipmapped, 3 anisotropic", 0, 3,
            int(mapping.quality.sampling),
            [](Gfx::OutputQuality& q, int x) { q.sampling = Gfx::OutputSampling(x); });
        addInt(
            "warp_interpolation", "Warp map interpolation: 0 bilinear, 1 bicubic", 0, 1,
            int(mapping.quality.warpInterpolation), [](Gfx::OutputQuality& q, int x) {
          q.warpInterpolation = Gfx::WarpInterpolation(x);
        });
        addInt(
            "dither", "Dither before quantization: 0 auto, 1 off, 2 on", 0, 2,
            int(mapping.quality.dither),
            [](Gfx::OutputQuality& q, int x) { q.dither = Gfx::OutputDither(x); });
        addInt(
            "max_anisotropy", "Most taps of anisotropic sampling", 1, 16,
            mapping.quality.maxAnisotropy,
            [](Gfx::OutputQuality& q, int x) { q.maxAnisotropy = x; });
        win_node->add_child(std::move(quality_node));
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

    m_calibrationWatcher
        = std::make_unique<CalibrationWatcher>([this] { reloadCalibrations(); });
    watchCalibrations();
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
              w, &score::gfx::Window::mouseMove,
              [&pw, w](const QPointF screen, const QPointF win) {
        auto sz = w->size();
        if(sz.width() > 0 && sz.height() > 0)
        {
          pw.scaled_cursor->push_value(
              ossia::vec2f{float(win.x() / sz.width()), float(win.y() / sz.height())});
          pw.gl_cursor->push_value(
              ossia::vec2f{
                  float(win.x() / sz.width()), 1.f - float(win.y() / sz.height())});
          pw.clip_cursor->push_value(
              ossia::vec2f{
                  float(win.x() / sz.width()) * 2.f - 1.f,
                  float(win.y() / sz.height()) * -2.f + 1.f});
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
    m_calibrationWatcher.reset();
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
