#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/Video/Process.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/port.hpp>

namespace Gfx::Video
{
class video_node final : public gfx_exec_node
{
public:
  video_node(
      const std::shared_ptr<video_decoder>& dec, std::optional<double> tempo,
      GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
      , m_decoder{dec}
  {
    auto n = std::make_unique<score::gfx::VideoNode>(m_decoder, tempo);
    impl = n.get();
    id = exec_context->ui->register_node(std::move(n));
    //    m_decoder->seek(0);
  }

  ~video_node()
  {
    //    m_decoder->seek(0);
    if(id >= 0)
      exec_context->ui->unregister_node(id);
  }

  void reload(const std::shared_ptr<video_decoder>& dec, std::optional<double> tempo)
  {
    impl = nullptr;

    if(id >= 0)
      exec_context->ui->unregister_node(id);

    m_decoder = dec;
    m_decoder->seek(m_last_flicks.impl);

    auto n = std::make_unique<score::gfx::VideoNode>(m_decoder, tempo);
    impl = n.get();
    id = exec_context->ui->register_node(std::move(n));
  }

  std::string label() const noexcept override
  {
    if(this->m_decoder)
    {
      const auto& name = this->m_decoder->file();
      return "Gfx::video_node: " + name;
    }
    else
    {
      return "Gfx::video_node (null)";
    }
  }

  video_decoder& decoder() const noexcept { return *m_decoder; }

  score::gfx::VideoNode* impl{};

private:
  std::shared_ptr<video_decoder> m_decoder;
};

class video_process final : public ossia::node_process
{
public:
  using node_process::node_process;

  void offset_impl(ossia::time_value date) override
  {
    auto& vnode = static_cast<video_node&>(*node);
    // TODO should be a "seek" info in what goes from decoder to renderer instead...

    if(!this->m_loops)
    {
      vnode.decoder().seek(this->m_start_offset.impl + date.impl);
    }
    else
    {
      vnode.decoder().seek(
          this->m_start_offset.impl
          + ((date.impl - this->m_start_offset.impl) % this->m_loop_duration.impl));
    }

    //vnode.impl->m_seeked = true;
  }

  void transport_impl(ossia::time_value date) override { offset_impl(date); }

  void start() override
  {
    static_cast<video_node&>(*node).decoder().seek(this->m_start_offset.impl);
  }
  void stop() override
  {
    static_cast<video_node&>(*node).decoder().seek(this->m_start_offset.impl);
  }
  void pause() override
  {
    auto& vnode = static_cast<video_node&>(*node);
    vnode.impl->pause(true);
  }
  void resume() override
  {
    auto& vnode = static_cast<video_node&>(*node);
    vnode.impl->pause(false);
  }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Video::Model& element, const Execution::Context& ctx, QObject* parent)
    : ProcessComponent_T{element, ctx, "VideoExecutorComponent", parent}
{
  auto dec = element.makeDecoder();
  if(!dec)
    dec = std::make_shared<Video::video_decoder>(::Video::DecoderConfiguration{});

  std::optional<double> tempo;
  if(!element.ignoreTempo())
    tempo = element.nativeTempo();
  auto n = ossia::make_node<video_node>(
      *ctx.execState, std::move(dec), tempo, ctx.doc.plugin<DocumentPlugin>().exec);

  for(auto* outlet : element.outlets())
  {
    if(auto out = qobject_cast<Gfx::TextureOutlet*>(outlet))
    {
      out->nodeId = n->id;
    }
  }

  n->root_outputs().push_back(new ossia::texture_outlet);

  this->node = n;
  m_ossia_process = std::make_shared<video_process>(n);

  ::bind(
      element, Gfx::Video::Model::p_scaleMode{}, this, [this](score::gfx::ScaleMode m) {
        if(auto vn = static_cast<video_node*>(this->node.get()); vn && vn->impl)
        {
          vn->impl->setScaleMode(m);
        }
      });

  con(element, &Gfx::Video::Model::pathChanged, this, [this](const QString& new_path) {
    std::optional<double> tempo;
    if(!this->process().ignoreTempo())
      tempo = this->process().nativeTempo();

    in_exec([node = std::weak_ptr{node}, dec = this->process().makeDecoder(),
             tempo]() mutable {
      if(auto vn = static_cast<video_node*>(node.lock().get()); vn && vn->impl)
      {
        vn->reload(std::move(dec), tempo);
      }
    });
  });
}

void ProcessExecutorComponent::cleanup()
{
  for(auto* outlet : this->process().outlets())
  {
    if(auto out = qobject_cast<TextureOutlet*>(outlet))
    {
      out->nodeId = -1;
    }
  }
  ProcessComponent_T::cleanup();
}
}
