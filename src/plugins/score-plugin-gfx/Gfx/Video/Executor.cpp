#include "Executor.hpp"

#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxApplicationPlugin.hpp>
#include <Gfx/GfxContext.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/Video/Process.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/port.hpp>

namespace Gfx::Video
{
static std::unique_ptr<score::gfx::VideoNode> makeVideoNode(
    const std::shared_ptr<video_decoder>& dec, std::optional<double> tempo)
{
  auto n = std::make_unique<score::gfx::VideoNode>(dec, tempo);
  auto& set = score::AppContext().settings<Gfx::Settings::Model>();
  n->setDecodingSettings(set.getHardwareDecode(), set.getDecodingThreads());
  return n;
}

class video_node final : public gfx_exec_node
{
public:
  video_node(
      const std::shared_ptr<video_decoder>& dec, std::optional<double> tempo,
      GfxExecutionAction& ctx)
      : gfx_exec_node{ctx}
      , m_decoder{dec}
  {
    auto n = makeVideoNode(m_decoder, tempo);
    impl = n.get();
    id = exec_context->ui->register_node(std::move(n));
    //    m_decoder->seek(0);
  }

  ~video_node()
  {
    //    m_decoder->seek(0);
    exec_context->ui->unregister_node(id);
  }

  void reload(const std::shared_ptr<video_decoder>& dec, std::optional<double> tempo)
  {
    impl = nullptr;

    exec_context->ui->unregister_node(id);
    id = score::gfx::invalid_node_index;

    m_decoder = dec;
    if(m_decoder)
    {
      m_decoder->seek(m_last_flicks.impl);

      auto n = makeVideoNode(m_decoder, tempo);
      impl = n.get();
      id = exec_context->ui->register_node(std::move(n));
    }
    else
    {
      impl = {};
      id = score::gfx::invalid_node_index;
    }
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

  const std::shared_ptr<video_decoder>& decoder() const noexcept { return m_decoder; }

  score::gfx::VideoNode* impl{};

private:
  std::shared_ptr<video_decoder> m_decoder;
};

class video_process final : public ossia::node_process
{
public:
  using node_process::node_process;

  // libossia passes two kinds of time: the interval gives offset_impl and
  // transport_impl the date of the parent, and a loop wrap in state() gives
  // the time in the media where the next loop starts. The parent's date maps
  // to the media the way the tokens of state() do.
  void state(const ossia::token_request& tok) override
  {
    if(!this->m_loops)
      return node_process::state(tok);
    tok.loop(
        this->m_start_offset, this->m_loop_duration,
        [this](const ossia::token_request& tr) { state_impl(tr); },
        [this](ossia::time_value t) { seek(t); });
  }

  void offset_impl(ossia::time_value date) override { seek(mediaTime(date)); }
  void transport_impl(ossia::time_value date) override { seek(mediaTime(date)); }

  void start() override { seek(this->m_start_offset); }
  void stop() override { seek(this->m_start_offset); }
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

private:
  //! Where a date of the parent falls in the media
  ossia::time_value mediaTime(ossia::time_value date) const noexcept
  {
    const int64_t loop = this->m_loop_duration.impl;
    if(!this->m_loops || loop <= 0)
      return this->m_start_offset + date;
    const int64_t in_loop = ((date.impl % loop) + loop) % loop;
    return this->m_start_offset + ossia::time_value{in_loop};
  }

  void seek(ossia::time_value media)
  {
    if(auto dec = static_cast<video_node&>(*node).decoder())
      dec->seek(media.impl);
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

  ::bind(
      element, Gfx::Video::Model::p_playbackMode{}, this,
      [this](score::gfx::PlaybackMode m) {
        if(auto vn = static_cast<video_node*>(this->node.get()); vn && vn->impl)
        {
          vn->impl->setPlaybackMode(m);
        }
      });

  ::bind(
      element, Gfx::Video::Model::p_outputFormat{}, this,
      [this](::Video::OutputFormat m) {
    if(auto vn = static_cast<video_node*>(this->node.get()); vn && vn->impl)
    {
      vn->impl->setOutputFormat(m);
    }
  });
  ::bind(
      element, Gfx::Video::Model::p_tonemap{}, this,
      [this](::Video::Tonemap m) {
    if(auto vn = static_cast<video_node*>(this->node.get()); vn && vn->impl)
    {
      vn->impl->setTonemap(m);
    }
  });

  con(element, &Gfx::Video::Model::pathChanged, this, [this](const QString& new_path) {
    auto& p = this->process();
    auto scale = p.scaleMode();
    auto playback = p.playbackMode();
    auto fmt = p.outputFormat();
    auto tonemap = p.tonemap();

    std::optional<double> tempo;
    if(!this->process().ignoreTempo())
      tempo = this->process().nativeTempo();

    in_exec([node = std::weak_ptr{node}, dec = this->process().makeDecoder(),
             tempo, scale, playback, fmt, tonemap]() mutable {
      if(auto vn = static_cast<video_node*>(node.lock().get()); vn && vn->impl)
      {
        vn->reload(std::move(dec), tempo);
        if(vn->impl) {
          vn->impl->setScaleMode(scale);
          vn->impl->setPlaybackMode(playback);
          vn->impl->setOutputFormat(fmt);
          vn->impl->setTonemap(tonemap);
        }
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
      out->nodeId = score::gfx::invalid_node_index;
    }
  }
  ProcessComponent_T::cleanup();
}
}
