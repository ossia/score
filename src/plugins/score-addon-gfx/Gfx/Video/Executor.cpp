#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>

#include <score/document/DocumentContext.hpp>

#include <ossia/dataflow/port.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExec.hpp>
#include <Gfx/Graph/videonode.hpp>
#include <Gfx/Video/Process.hpp>

extern "C"
{
#include <libavutil/pixdesc.h>
}
namespace Gfx::Video
{
class video_node final : public gfx_exec_node
{
public:
  video_node(const std::shared_ptr<video_decoder>& dec, GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}, m_decoder{dec}
  {
    switch (dec->pixel_format)
    {
      case AV_PIX_FMT_YUV420P:
        id = exec_context->ui->register_node(std::make_unique<YUV420Node>(dec));
        break;
      case AV_PIX_FMT_RGB0:
        id = exec_context->ui->register_node(std::make_unique<RGB0Node>(dec));
        break;
      default:
        qDebug() << "Unhandled pixel format: " << av_get_pix_fmt_name(dec->pixel_format);
        break;
    }
    dec->seek(0);
  }

  ~video_node()
  {
    m_decoder->seek(0);
    if (id >= 0)
      exec_context->ui->unregister_node(id);
  }

  std::string label() const noexcept override { return "Gfx::video_node"; }

  video_decoder& decoder() const noexcept { return *m_decoder; }

private:
  std::shared_ptr<video_decoder> m_decoder;
};

class video_process : public ossia::node_process
{
public:
  using node_process::node_process;

  void offset_impl(ossia::time_value tv) override
  {
    // TODO
    static_cast<video_node&>(*node).decoder().seek(0);
  }
  void transport_impl(ossia::time_value date) override
  {
    // TODO
    static_cast<video_node&>(*node).decoder().seek(0);
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
    : ProcessComponent_T{element, ctx, id, "gfxExecutorComponent", parent}
{
  if (element.decoder())
  {
    auto n
        = std::make_shared<video_node>(element.decoder(), ctx.doc.plugin<DocumentPlugin>().exec);

    n->root_outputs().push_back(new ossia::texture_outlet);

    this->node = n;
    m_ossia_process = std::make_shared<video_process>(n);
  }
}
}
