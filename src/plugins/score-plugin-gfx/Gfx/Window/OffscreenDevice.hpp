#pragma once

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/BackgroundNode.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/protocol.hpp>
#include <ossia/network/generic/generic_node.hpp>

#include <ossia-qt/invoke.hpp>

namespace Gfx
{

// Headless device used when SCORE_FORCE_OFFSCREEN_WINDOW selects this
// window device by name. Wraps a BackgroundNode — which already drives
// beginOffscreenFrame/endOffscreenFrame — without the ScenarioDocumentView
// dependency of background_device. Exposes only the parameters required by
// offscreen tests (size, rendersize) and holds the shared_readback used by
// WindowDevice::grabTo to write frames to disk.
class offscreen_device : public ossia::net::device_base
{
  score::gfx::BackgroundNode* m_node{};
  gfx_node_base m_root;
  QObject m_qtContext;

  ossia::net::parameter_base* size_param{};
  ossia::net::parameter_base* rendersize_param{};

public:
  offscreen_device(std::unique_ptr<gfx_protocol_base> proto, std::string name)
      : ossia::net::device_base{std::move(proto)}
      , m_node{new score::gfx::BackgroundNode}
      , m_root{*this, *static_cast<gfx_protocol_base*>(m_protocol.get()), m_node, name}
  {
    this->m_capabilities.change_tree = true;
    m_node->shared_readback = std::make_shared<QRhiReadbackResult>();

    {
      auto size_node = std::make_unique<ossia::net::generic_node>("size", *this, m_root);
      size_param = size_node->create_parameter(ossia::val_type::VEC2F);
      size_param->push_value(ossia::vec2f{1280.f, 720.f});
      m_node->setSize(QSize{1280, 720});
      size_param->add_callback([this](const ossia::value& v) {
        if(auto val = v.target<ossia::vec2f>())
        {
          ossia::qt::run_async(&m_qtContext, [node = this->m_node, v = *val] {
            node->setSize({(int)v[0], (int)v[1]});
          });
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
          ossia::qt::run_async(&m_qtContext, [node = this->m_node, v = *val] {
            node->setRenderSize({(int)v[0], (int)v[1]});
          });
        }
      });
      m_root.add_child(std::move(size_node));
    }
  }

  ~offscreen_device()
  {
    // Remove our output from the render graph synchronously, while both the
    // BackgroundNode and its QRhi are still alive. This output is owned by the
    // device (not the graph), so on shutdown it is torn down here before
    // Graph::~Graph runs — and the graph's async REMOVE_NODE queue is never
    // drained again. Without this, the graph is left with a dangling output
    // pointer + a RenderList over a freed QRhi (SIGSEGV in ~Graph teardown).
    if(auto* proto = static_cast<gfx_protocol_base*>(m_protocol.get());
       proto && proto->context && proto->context->ui)
      proto->context->ui->destroyOutput(m_node.get());

    m_protocol->stop();
    m_root.clear_children();
    m_protocol.reset();
  }

  score::gfx::BackgroundNode* node() const noexcept { return m_node; }

  const gfx_node_base& get_root_node() const override { return m_root; }
  gfx_node_base& get_root_node() override { return m_root; }
};

}
