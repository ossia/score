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
  std::shared_ptr<Node_T> state;
  score::gfx::Message m_last_message{};
  ossia::time_value m_last_time{-1};

  AVND_NO_UNIQUE_ADDRESS texture_inputs_storage<Node_T> texture_ins;
  AVND_NO_UNIQUE_ADDRESS texture_outputs_storage<Node_T> texture_outs;

  AVND_NO_UNIQUE_ADDRESS buffer_inputs_storage<Node_T> buffer_ins;
  AVND_NO_UNIQUE_ADDRESS buffer_outputs_storage<Node_T> buffer_outs;

  AVND_NO_UNIQUE_ADDRESS geometry_inputs_storage<Node_T> geometry_ins;
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

  score::gfx::BufferView bufferForOutput(const score::gfx::Port& output) override
  {
    if constexpr(avnd::buffer_output_introspection<Node_T>::size > 0)
    {
      for(auto& [p, b] : buffer_outs.m_buffers)
        if(p == &output)
          return b;
    }
    return {};
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

    // Init input render targets
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
      texture_ins.init(*this, renderer);

    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
      texture_outs.init(*this, renderer, res);

    if constexpr(avnd::buffer_output_introspection<Node_T>::size > 0)
      buffer_outs.init(renderer, *state, parent);

    if_possible(state->init(renderer, res));
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    auto& parent = node();
    parent.processControlIn(
        *this, *state, m_last_message, parent.last_message, parent.m_ctx);

    bool updated = false;
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
    {
      updated |= texture_ins.update(*this, renderer, res);
    }

    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
    {
      this->defaultUBOUpdate(renderer, res);
    }

    if_possible(state->update(renderer, res, e));

    if(updated)
    {
      // We must notify the graph that the previous nodes have to be recomputed
    }
  }

  void release(score::gfx::RenderList& r) override
  {
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
      texture_ins.release();

    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
      texture_outs.release(*this, r);

    if constexpr(avnd::buffer_output_introspection<Node_T>::size > 0)
      buffer_outs.release(r);

    if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
      geometry_ins.release(r);

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0 || avnd::texture_output_introspection<Node_T>::size > 0)
    {
      this->defaultRelease(r);
    }

    if_possible(state->release(r));
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    if constexpr(
        avnd::texture_input_introspection<Node_T>::size > 0
        || avnd::buffer_input_introspection<Node_T>::size > 0
        || avnd::geometry_input_introspection<Node_T>::size > 0)
    {
      res = renderer.state.rhi->nextResourceUpdateBatch();

      if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
        texture_ins.inputAboutToFinish(this->node(), p, res);
      if constexpr(avnd::buffer_input_introspection<Node_T>::size > 0)
        buffer_ins.inputAboutToFinish(renderer, res, *state, this->node());
      if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
        geometry_ins.inputAboutToFinish(
            renderer, res, this->geometry, *state, this->node());
    }

    if_possible(state->inputAboutToFinish(renderer, p, res));
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    auto& parent = node();
    auto& rhi = *renderer.state.rhi;

    if constexpr(
        avnd::texture_input_introspection<Node_T>::size > 0
        || avnd::buffer_input_introspection<Node_T>::size > 0
        || avnd::geometry_input_introspection<Node_T>::size > 0)
    {
      // FIXME: for geometry, here we should optimize if we know we aren't going to need them on the CPU, OR if it is a type ?
      // Insert a synchronisation point to allow readbacks to complete
      rhi.finish();
    }

    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
      return;
    m_last_time = parent.last_message.token.date;

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
      texture_ins.runInitialPasses(*this, rhi);
    if constexpr(avnd::buffer_input_introspection<Node_T>::size > 0)
      buffer_ins.readInputBuffers(renderer, parent, *state);
    if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
      geometry_ins.readInputGeometries(renderer, this->geometry, parent, *state);

    buffer_outs.prepareUpload(*res);

    // Run the processor
    if_possible(state->runInitialPasses(renderer, commands, res, edge));
    if_possible((*state)());

    // Upload output buffers
    if constexpr(avnd::buffer_output_introspection<Node_T>::size > 0)
      buffer_outs.upload(renderer, *state, *res);

    // Upload output textures
    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
    {
      texture_outs.runInitialPasses(*this, renderer, res);

      commands.resourceUpdate(res);
      res = renderer.state.rhi->nextResourceUpdateBatch();
    }

    // Copy the geometry
    if constexpr(avnd::geometry_output_introspection<Node_T>::size > 0)
      geometry_outs.upload(renderer, *this->state, edge);

    // Copy the data to the model node
    parent.processControlOut(*this->state);
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
