#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
    (avnd::texture_output_introspection<Node_T>::size + avnd::buffer_output_introspection<Node_T>::size + avnd::geometry_output_introspection<Node_T>::size) >= 1
  )
struct GfxRenderer<Node_T> final : score::gfx::GenericNodeRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  using buffer_inputs = avnd::buffer_input_introspection<Node_T>;
  using buffer_outputs = avnd::buffer_output_introspection<Node_T>;
  std::shared_ptr<Node_T> state;
  score::gfx::Message m_last_message{};
  ossia::time_value m_last_time{-1};

  AVND_NO_UNIQUE_ADDRESS texture_inputs_storage<Node_T> texture_ins;
  AVND_NO_UNIQUE_ADDRESS texture_outputs_storage<Node_T> texture_outs;

  AVND_NO_UNIQUE_ADDRESS buffer_inputs_storage<Node_T> buffer_ins;
  AVND_NO_UNIQUE_ADDRESS buffer_outputs_storage<Node_T> buffer_outs;

  AVND_NO_UNIQUE_ADDRESS geometry_outputs_storage<Node_T> geometry_outs;

  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }

  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::GenericNodeRenderer{p}
      , state{std::make_shared<Node_T>()}
  {
    prepareNewState<Node_T>(state, p);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
    {
      auto it = texture_ins.m_rts.find(&p);
      SCORE_ASSERT(it != texture_ins.m_rts.end());
      return it->second;
    }
    return {};
  }

  QRhiBuffer* bufferForOutput(const score::gfx::Port& output) override
  {
    if constexpr(avnd::buffer_output_introspection<Node_T>::size > 0)
    {
      for(auto& [p, b] : buffer_outs.m_buffers)
        if(p == &output)
          return b;
    }
    return nullptr;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& parent = node();
    if constexpr(requires { state->prepare(); })
    {
      parent.processControlIn(
          *this, *state, m_last_message, parent.last_message, parent.m_ctx);
      state->prepare();
    }

    texture_ins.init(*this, renderer);

    texture_outs.init(*this, renderer, res);

    buffer_outs.init(renderer, *state, parent);
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
    {
      this->defaultUBOUpdate(renderer, res);
    }
  }

  void release(score::gfx::RenderList& r) override
  {
    texture_ins.release();
    texture_outs.release(*this, r);
    buffer_outs.release(r);

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0 || avnd::texture_output_introspection<Node_T>::size > 0)
    {
      this->defaultRelease(r);
    }
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0 || avnd::buffer_input_introspection<Node_T>::size > 0)
    {
      res = renderer.state.rhi->nextResourceUpdateBatch();

      texture_ins.inputAboutToFinish(this->node(), p, res);
      buffer_ins.inputAboutToFinish(renderer, res, *state, this->node());
    }
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    auto& parent = node();
    auto& rhi = *renderer.state.rhi;

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0 || avnd::buffer_input_introspection<Node_T>::size > 0)
    {
      // Insert a synchronisation point to allow readbacks to complete
      rhi.finish();
    }

    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
      return;
    m_last_time = parent.last_message.token.date;

    texture_ins.runInitialPasses(*this, rhi);

    buffer_ins.readInputBuffers(renderer, parent, *state);

    parent.processControlIn(
        *this, *state, m_last_message, parent.last_message, parent.m_ctx);

    buffer_outs.prepareUpload(*res);

    // Run the processor
    if_possible((*state)());

    // Upload output buffers
    buffer_outs.upload(renderer, *state, *res);

    // Upload output textures
    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
    {
      texture_outs.runInitialPasses(*this, renderer, res);

      commands.resourceUpdate(res);
      res = renderer.state.rhi->nextResourceUpdateBatch();
    }

    // Copy the data to the model node
    parent.processControlOut(*this->state);

    // Copy the geometry
    geometry_outs.upload(renderer, *this->state, edge);
  }
};

template <typename Node_T>
  requires(
    (avnd::texture_output_introspection<Node_T>::size + avnd::buffer_output_introspection<Node_T>::size + avnd::geometry_output_introspection<Node_T>::size) >= 1
  )
struct GfxNode<Node_T> final
    : CustomGfxNodeBase
    , GpuWorker
    , GpuControlIns
    , GpuControlOuts
    , GpuNodeElements<Node_T>
{
  oscr::ProcessModel<Node_T>& processModel;
  GfxNode(
      oscr::ProcessModel<Node_T>& element,
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls,
      int64_t id, const score::DocumentContext& ctx)
      : CustomGfxNodeBase{ctx}
      , GpuControlOuts{std::move(q), std::move(ctls)}
      , processModel{element}
  {
    this->instance = id;

    initGfxPorts<Node_T>(this, this->input, this->output);
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new GfxRenderer<Node_T>{*this};
  }
};
}
#endif
