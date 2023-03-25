#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

#include <Gfx/Qt5CompatPush> // clang-format: keep
template <typename Node_T>
  requires(
      avnd::texture_input_introspection<Node_T>::size > 0
      && avnd::texture_output_introspection<Node_T>::size == 0)
struct GfxRenderer<Node_T> final : score::gfx::OutputNodeRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  const GfxNode<Node_T>& parent;
  Node_T state;
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  std::vector<QRhiReadbackResult> m_readbacks;
  ossia::time_value m_last_time{-1};

  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::OutputNodeRenderer{}
      , parent{p}
      , m_readbacks(texture_inputs::size)
  {
    prepareNewState(state, parent);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    auto it = m_rts.find(&p);
    SCORE_ASSERT(it != m_rts.end());
    return it->second;
  }

  template <typename Tex>
  void createInput(
      score::gfx::RenderList& renderer, int k, const Tex& texture_spec, QSize size)
  {
    auto port = parent.input[k];
    constexpr auto flags = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;
    auto texture = renderer.state.rhi->newTexture(
        gpp::qrhi::textureFormat<Tex>(), size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port]
        = score::gfx::createRenderTarget(renderer.state, texture, renderer.samples());
  }

  void loadInputTexture(avnd::cpu_texture auto& cpu_tex, int k)
  {
    auto& buf = m_readbacks[k].data;
    if(buf.size() != 4 * cpu_tex.width * cpu_tex.height)
    {
      cpu_tex.bytes = nullptr;
    }
    else
    {
      cpu_tex.bytes = reinterpret_cast<unsigned char*>(buf.data());
      cpu_tex.changed = true;
    }
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    // Init input render targets
    int k = 0;
    avnd::cpu_texture_input_introspection<Node_T>::for_all(
        avnd::get_inputs<Node_T>(state), [&]<typename F>(F& t) {
          auto sz = renderer.state.renderSize;
          createInput(renderer, k, t.texture, sz);
          t.texture.width = sz.width();
          t.texture.height = sz.height();
          k++;
        });
  }

  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override {
  }

  void release(score::gfx::RenderList& r) override
  {
    // Free inputs
    // TODO investigate why reference does not work here:
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    res = renderer.state.rhi->nextResourceUpdateBatch();
    const auto& inputs = this->parent.input;
    auto index_of_port = ossia::find(inputs, &p) - inputs.begin();
    SCORE_ASSERT(index_of_port == 0);
    {
      auto tex = m_rts[&p].texture;
      auto& readback = m_readbacks[index_of_port];
      readback = {};
      res->readBackTexture(QRhiReadbackDescription{tex}, &readback);
    }
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    auto& rhi = *renderer.state.rhi;

    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
    {
      return;
    }
    m_last_time = parent.last_message.token.date;

    // Fetch input textures (if any)
    {
      // Insert a synchronisation point to allow readbacks to complete
      rhi.finish();

      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      int k = 0;
      avnd::cpu_texture_input_introspection<Node_T>::for_all(
          avnd::get_inputs<Node_T>(state), [&](auto& t) {
            loadInputTexture(t.texture, k);
            k++;
          });
    }

    // Apply the controls
    avnd::parameter_input_introspection<Node_T>::for_all_n2(
        avnd::get_inputs<Node_T>(state),
        [&](avnd::parameter auto& t, auto pred_index, auto field_index) {
      auto& mess = this->parent.last_message;

      if(mess.input.size() > field_index)
      {
        if(auto val = ossia::get_if<ossia::value>(&mess.input[field_index]))
        {
          oscr::from_ossia_value(t, *val, t.value);
        }
      }
        });

    // Run the processor
    state();

    // Copy the data to the model node
    parent.processControlOut(this->state);
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep

template <typename Node_T>
  requires(
      avnd::texture_input_introspection<Node_T>::size > 0
      && avnd::texture_output_introspection<Node_T>::size == 0)
struct GfxNode<Node_T> final : CustomGpuOutputNodeBase
{
  GfxNode(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls, int id)
      : CustomGpuOutputNodeBase{std::move(q), std::move(ctls)}
  {
    this->instance = id;

    using texture_inputs = avnd::texture_input_introspection<Node_T>;

    // FIXME incorrect if we have other ports before, e.g. a float part followed by an image port
    for(std::size_t i = 0; i < texture_inputs::size; i++)
    {
      this->input.push_back(
          new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new GfxRenderer<Node_T>{*this};
  }
};

}
#endif
