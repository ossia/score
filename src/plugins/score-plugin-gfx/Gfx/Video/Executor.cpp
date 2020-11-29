#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/Graph/videonode.hpp>
#include <Gfx/Video/Process.hpp>

namespace Gfx::Video
{
class video_node final : public gfx_exec_node
{
public:
  video_node(const std::shared_ptr<video_decoder>& dec, std::optional<double> tempo, GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}, m_decoder{dec->clone()}
  {
    auto n = std::make_unique<VideoNode>(m_decoder, tempo);
    impl = n.get();
    id = exec_context->ui->register_node(std::move(n));
    m_decoder->seek(0);
  }

  ~video_node()
  {
    m_decoder->seek(0);
    if (id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::video_node"; }

  video_decoder& decoder() const noexcept { return *m_decoder; }

  VideoNode* impl{};
private:
  std::shared_ptr<video_decoder> m_decoder;
};

class video_process : public ossia::node_process
{
public:
  using node_process::node_process;

  void offset_impl(ossia::time_value tv) override
  {
    auto& vnode = static_cast<video_node&>(*node);
    // TODO should be a "seek" info in what goes from decoder to renderer instead...
    vnode.decoder().seek(tv.impl);
    vnode.impl->seeked = true;
  }

  void transport_impl(ossia::time_value date) override
  {
    auto& vnode = static_cast<video_node&>(*node);
    vnode.decoder().seek(date.impl);
    vnode.impl->seeked = true;
  }

  void state_impl(const ossia::token_request& req) { node->request(req); }

  void start() override { static_cast<video_node&>(*node).decoder().seek(0); }
  void stop() override { static_cast<video_node&>(*node).decoder().seek(0); }
  void pause() override { }
  void resume() override { }
};

ProcessExecutorComponent::ProcessExecutorComponent(
    Gfx::Video::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : ProcessComponent_T{element, ctx, id, "VideoExecutorComponent", parent}
{
  if (element.decoder())
  {
    std::optional<double> tempo;
    if(!element.ignoreTempo())
      tempo = element.nativeTempo();
    auto n
        = std::make_shared<video_node>(element.decoder(), tempo, ctx.doc.plugin<DocumentPlugin>().exec);

    n->root_outputs().push_back(new ossia::texture_outlet);

    this->node = n;
    m_ossia_process = std::make_shared<video_process>(n);
  }
}
}
