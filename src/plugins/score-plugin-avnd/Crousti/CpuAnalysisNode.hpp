#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{
template <typename Node_T>
  requires(
      (avnd::texture_output_introspection<Node_T>::size + avnd::buffer_output_introspection<Node_T>::size + avnd::geometry_output_introspection<Node_T>::size + scene_output_introspection<Node_T>::size) == 0
      && (avnd::gpu_render_target_output_port_output_introspection<Node_T>::size == 0)
  )
struct GfxRenderer<Node_T> final : score::gfx::OutputNodeRenderer
{
  std::shared_ptr<Node_T> state;
  score::gfx::Message m_last_message{};
  ossia::time_value m_last_time{-1};

  AVND_NO_UNIQUE_ADDRESS texture_inputs_storage<Node_T> texture_ins;
  AVND_NO_UNIQUE_ADDRESS buffer_inputs_storage<Node_T> buffer_ins;
  AVND_NO_UNIQUE_ADDRESS geometry_inputs_storage<Node_T> geometry_ins;
  AVND_NO_UNIQUE_ADDRESS scene_inputs_storage<Node_T> scene_ins;

  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }

  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::OutputNodeRenderer{p}
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

  // See CpuFilterNode.hpp for the reasoning: init must live in initState
  // so the incremental edge-rewire path also runs it.
  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if(m_initialized)
      return;

    // See CpuFilterNode for the reasoning: optional renderlist
    // backchannel populated via SFINAE so nodes can reach the
    // RenderList's GpuResourceRegistry / AssetTable without plumbing.
    if constexpr(requires { state->renderlist = &renderer; })
      state->renderlist = &renderer;

    if constexpr(requires { state->prepare(); })
    {
      this->node().processControlIn(
          *this, *state, m_last_message, this->node().last_message, this->node().m_ctx);
      state->prepare();
    }

    // Init input render targets
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
      texture_ins.init(*this, renderer);

    if_possible(state->init(renderer, res));

    m_initialized = true;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);
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
    if_possible(state->update(renderer, res, e));

    if(updated)
    {
      // We must notify the graph that the previous nodes have to be recomputed
    }
  }

  void releaseState(score::gfx::RenderList& r) override
  {
    if(!m_initialized)
      return;

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
      texture_ins.release();

    if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
      geometry_ins.release(r);

    if constexpr(scene_input_introspection<Node_T>::size > 0)
      scene_ins.release(r);

    if constexpr(
        avnd::texture_input_introspection<Node_T>::size > 0
        || avnd::texture_output_introspection<Node_T>::size > 0)
    {
      // No call-through to GenericNodeRenderer::defaultRelease here:
      // CpuAnalysisNode's GfxRenderer derives from OutputNodeRenderer,
      // not GenericNodeRenderer, and OutputNodeRenderer has no
      // defaultRelease equivalent (it owns no pipeline / passes — it
      // is a sink, not a node renderer with m_p / m_pipelineCache).
      // CpuFilterNode's mirror at line ~357 IS valid because that
      // GfxRenderer derives from GenericNodeRenderer.
      //
      // If a future CpuAnalysisNode uses textures via OutputNodeRenderer
      // surfaces, they'll need their own per-storage release path
      // (texture_ins.release above already handles texture INPUTS).
    }

    if_possible(state->release(r));

    // Clear the optional renderlist backchannel. Paired with initState;
    // same SFINAE guard.
    if constexpr(requires { state->renderlist = nullptr; })
      state->renderlist = nullptr;

    m_initialized = false;
  }

  void release(score::gfx::RenderList& r) override
  {
    releaseState(r);
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    // Outer guard includes scene_input_introspection so a node with ONLY
    // scene inputs (no texture / buffer / geometry) still allocates `res`
    // — necessary if scene_inputs_storage ever grows an inputAboutToFinish
    // method (today it's read-only via readInputScenes, but the storage's
    // lifecycle is part of the new scene_port concept and may evolve).
    // Without the include, a scene-only sink would silently skip the
    // res allocation and any future scene-side write would have nowhere
    // to land.
    if constexpr(
        avnd::texture_input_introspection<Node_T>::size > 0
        || avnd::buffer_input_introspection<Node_T>::size > 0
        || avnd::geometry_input_introspection<Node_T>::size > 0
        || scene_input_introspection<Node_T>::size > 0)
    {
      res = renderer.state.rhi->nextResourceUpdateBatch();

      if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
        texture_ins.inputAboutToFinish(this->node(), p, res);
      if constexpr(avnd::buffer_input_introspection<Node_T>::size > 0)
        buffer_ins.inputAboutToFinish(renderer, res, *state, this->node());
      if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
        geometry_ins.inputAboutToFinish(
            renderer, res, this->geometry, *state, this->node());
      // No scene_ins.inputAboutToFinish today — the guard is forward-
      // looking; add the call here when scene_inputs_storage grows one.
    }

    if_possible(state->inputAboutToFinish(renderer, p, res));
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    auto& parent = this->node();
    auto& rhi = *renderer.state.rhi;

    // Insert a synchronisation point to allow readbacks to complete
    rhi.finish();

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
    if constexpr(scene_input_introspection<Node_T>::size > 0)
      scene_ins.readInputScenes(this->scene, *state);

    parent.processControlIn(
        *this, *state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the processor
    if_possible(state->runInitialPasses(renderer, commands, res, edge));
    if_possible((*state)());

    // Copy the data to the model node
    parent.processControlOut(*this->state);
  }
};

template <typename Node_T>
  requires((avnd::texture_output_introspection<Node_T>::size
            + avnd::buffer_output_introspection<Node_T>::size
            + avnd::geometry_output_introspection<Node_T>::size
            + scene_output_introspection<Node_T>::size)
               == 0
           && (avnd::gpu_render_target_output_port_output_introspection<Node_T>::size
               == 0))
struct GfxNode<Node_T> final
    : CustomGpuOutputNodeBase
    , GpuNodeElements<Node_T>
{
  oscr::ProcessModel<Node_T>& processModel;
  GfxNode(
      oscr::ProcessModel<Node_T>& element,
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls,
      int64_t id, const score::DocumentContext& ctx)
      : CustomGpuOutputNodeBase{std::move(q), std::move(ctls), ctx}
      , processModel{element}
  {
    this->instance = id;

    initGfxPorts<Node_T>(this, this->input, this->output);
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new GfxRenderer<Node_T>{*this};
  }
};
}
#endif
