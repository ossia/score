#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

#include <halp/texture.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
    (avnd::texture_output_introspection<Node_T>::size + avnd::buffer_output_introspection<Node_T>::size + avnd::geometry_output_introspection<Node_T>::size + scene_output_introspection<Node_T>::size + avnd::gpu_render_target_output_port_output_introspection<Node_T>::size) >= 1
  )
struct GfxRenderer<Node_T> final : score::gfx::GenericNodeRenderer
{
  std::shared_ptr<Node_T> state;
  score::gfx::Message m_last_message{};
  // RenderList::frame id of the last frame on which we ran the expensive
  // once-per-frame body of runInitialPasses (input readbacks, operator()(),
  // output uploads). runInitialPasses is invoked once PER OUTGOING EDGE, so
  // without this guard that whole body re-ran for every downstream edge,
  // every frame. -1 = never run yet.
  int64_t m_last_frame{-1};

  AVND_NO_UNIQUE_ADDRESS texture_inputs_storage<Node_T> texture_ins;
  AVND_NO_UNIQUE_ADDRESS texture_outputs_storage<Node_T> texture_outs;

  AVND_NO_UNIQUE_ADDRESS buffer_inputs_storage<Node_T> buffer_ins;
  AVND_NO_UNIQUE_ADDRESS buffer_outputs_storage<Node_T> buffer_outs;

  AVND_NO_UNIQUE_ADDRESS geometry_inputs_storage<Node_T> geometry_ins;
  AVND_NO_UNIQUE_ADDRESS geometry_outputs_storage<Node_T> geometry_outs;
  AVND_NO_UNIQUE_ADDRESS scene_inputs_storage<Node_T> scene_ins;
  AVND_NO_UNIQUE_ADDRESS scene_outputs_storage<Node_T> scene_outs;

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
      // Only texture-RT inputs live in m_rts. Geometry / buffer / scene
      // inputs on the same node (e.g. PBRMesh: 4 gpu_texture_inputs + a
      // dynamic_gpu_geometry mesh in) land here through the generic
      // renderTargetForOutput path — return empty so the upstream's
      // addOutputPass skips creating a graphics render pass for them.
      auto it = texture_ins.m_rts.find(&p);
      if(it == texture_ins.m_rts.end())
        return {};
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

  // For non-2D gpu_texture_input fields (cubemap / array / 3D): the port
  // is flagged GrabsFromSource (see initGfxPorts +
  // port_flags_for_field), so Graph::updateSinkSampler calls us here
  // with the upstream's QRhiTexture. Write it into the matching halp
  // field so the node's operator()() / runInitialPasses see the handle.
  // 2D (classic RT-rendered) inputs ignore this path — their handle is
  // set up at init() time by texture_inputs_storage::init.
  //
  // depthTex: when the port also opts in via halp_meta(samplable_depth,
  // true), Graph passes the upstream's depth attachment here too. Stored
  // on `texture.depth_handle` for the consumer to sample alongside color.
  void updateInputTexture(
      const score::gfx::Port& input, QRhiTexture* tex,
      QRhiTexture* depthTex = nullptr) override
  {
    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
    {
      const auto& inputs = this->node().input;
      int port_idx = -1;
      for(int i = 0, n = (int)inputs.size(); i < n; ++i)
      {
        if(inputs[i] == &input)
        {
          port_idx = i;
          break;
        }
      }
      if(port_idx < 0)
        return;

      avnd::texture_input_introspection<Node_T>::for_all_n2(
          avnd::get_inputs<Node_T>(*state),
          [&]<typename F, std::size_t K, std::size_t N>(
              F& t, avnd::predicate_index<K>, avnd::field_index<N>) {
        if constexpr(avnd::gpu_texture_port<F>
                     && halp::texture_kind_of<F>() != halp::texture_kind::texture_2d)
        {
          if((int)N == port_idx)
          {
            t.texture.handle = tex;
            if(tex)
            {
              const auto sz = tex->pixelSize();
              t.texture.width = sz.width();
              t.texture.height = sz.height();
            }
            else
            {
              t.texture.width = 0;
              t.texture.height = 0;
            }
            t.texture.kind = halp::texture_kind_of<F>();
            if constexpr(halp::samplable_depth_of<F>())
            {
              t.texture.depth_handle = depthTex;
              if(depthTex)
                t.texture.depth_format
                    = qrhiToHalpDepthFormat(depthTex->format());
            }
          }
        }
      });
    }
  }

  QRhiTexture* textureForOutput(const score::gfx::Port& output) override
  {
    if constexpr(avnd::gpu_texture_output_introspection<Node_T>::size > 0)
    {
      // Find which output port index this is
      const auto& outputs = this->node().output;
      int port_idx = -1;
      for(int i = 0, n = outputs.size(); i < n; i++)
      {
        if(outputs[i] == &output)
        {
          port_idx = i;
          break;
        }
      }
      if(port_idx < 0)
        return nullptr;

      // Walk gpu_texture outputs; for_all_n2 gives us both the
      // predicate index and the struct field index (== port index)
      QRhiTexture* result = nullptr;
      avnd::gpu_texture_output_introspection<Node_T>::for_all_n2(
          avnd::get_outputs<Node_T>(*state),
          [&]<std::size_t PredIdx, std::size_t FieldIdx>(
              auto& field, avnd::predicate_index<PredIdx>,
              avnd::field_index<FieldIdx>) {
        if(static_cast<int>(FieldIdx) == port_idx)
          result = static_cast<QRhiTexture*>(field.texture.handle);
      });

      return result;
    }
    return nullptr;
  }

  // All of the setup lives in initState(), not init(). The incremental
  // edge-rewire path (Graph::createPassForEdgeIfMissing) only calls
  // initState() on newly-created renderers — so a halp scene-in/scene-out
  // node inserted live would otherwise never allocate its storage, its
  // operator()() would run against uninitialised state every frame, and
  // nothing would flow downstream until a stop/start cycle forced a full
  // rebuild through init().
  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if(m_initialized)
      return;

    auto& parent = node();

    // Optional renderlist backchannel for CPU halp nodes that need to
    // reach their hosting RenderList's GpuResourceRegistry / AssetTable
    // (e.g. Camera / Light / PBRMesh / MaterialOverride allocating arena
    // slots). Populated by SFINAE so nodes that don't declare the member
    // pay nothing. Lifetime: valid from initState until releaseState
    // clears it back to nullptr.
    if constexpr(requires { state->renderlist = &renderer; })
      state->renderlist = &renderer;

    // Ordering invariant: init → processControlIn → operator()()
    //
    // For nodes WITHOUT prepare(): processControlIn is NOT called here.
    // state->init() therefore runs (line below) before any control-update
    // callback can fire rebuild(). All five scene producers — Camera,
    // CameraArray, Light, Transform3D, SceneGroup — rely on this: they
    // populate m_*_ref arena handles in init(), and rebuild() reads those
    // handles unconditionally. The invariant is also enforced at the two
    // call-graph roots:
    //   • Graph.cpp:865-893  (incremental edge update): initState() is
    //     called before seedInitialOutputs() / operator()().
    //   • RenderList.cpp:434-470 (full graph init): init() for all
    //     renderers runs before the first render frame fires update().
    //
    // If you add prepare() to a scene producer, processControlIn becomes
    // reachable BEFORE state->init() (see branch below vs. line 202) and
    // any m_*_ref read inside rebuild() will observe an empty handle.
    // Re-audit the producer's rebuild() ref-read sites before doing so.
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

    m_initialized = true;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);
  }

  // Called by Graph::reconcileAllRenderLists right after this renderer is
  // spawned (in particular when the user live-inserts a scene-producing
  // node — Camera, EnvironmentLoader, Light — into a running
  // graph). Runs the node's operator()() once to populate its outputs and
  // then pushes the result into every downstream sink's per-port scene
  // cache immediately, rather than waiting for the first render-frame's
  // upstream-input scan to find our new edge. Without this, the Camera
  // live-insertion symptom is that the camera has no visible effect until
  // the user stops and restarts transport (triggering a full render-list
  // rebuild where every renderer's runInitialPasses runs from clean
  // state).
  void seedInitialOutputs(score::gfx::RenderList& renderer) override
  {
    if constexpr(
        scene_output_introspection<Node_T>::size > 0
        || avnd::geometry_output_introspection<Node_T>::size > 0)
    {
      auto& parent = node();
      // Apply any control values that arrived before we were created.
      // processControlIn is normally called from update() but the render
      // loop won't run update() until the first frame after reconcile
      // — the inserted Camera's slider defaults would leak through for
      // one frame otherwise.
      parent.processControlIn(
          *this, *state, m_last_message, parent.last_message, parent.m_ctx);

      if_possible((*state)());

      // Push to every existing output edge on scene/geometry ports. The
      // upload helpers look at edge.sink to find the downstream renderer
      // and call its NodeRenderer::process(port, scene_spec, source) —
      // seeding exactly the same m_portScenes slot the first runInitialPasses
      // would have filled one frame later.
      //
      // Scene and geometry ports both stamp score::gfx::Types::Geometry (per
      // port_to_type_enum in GpuUtils.hpp — Process::GeometryInlet carries
      // either a geometry or a full scene by design). Dispatching on the
      // runtime port->type can never see Types::Scene, so we branch on
      // compile-time introspection instead. Each upload helper is a no-op
      // for nodes that don't have the corresponding output kind, and both
      // branches can fire for nodes with mixed outputs.
      const auto& outs = parent.output;
      for(std::size_t i = 0; i < outs.size(); ++i)
      {
        auto* port = outs[i];
        if(!port || port->edges.empty())
          continue;
        if constexpr(scene_output_introspection<Node_T>::size > 0)
          for(auto* edge : port->edges)
            scene_outs.upload(renderer, *this->state, *edge);
        if constexpr(avnd::geometry_output_introspection<Node_T>::size > 0)
          for(auto* edge : port->edges)
            geometry_outs.upload(renderer, *this->state, *edge);
      }
    }
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

  void releaseState(score::gfx::RenderList& r) override
  {
    if(!m_initialized)
      return;

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
      texture_ins.release();

    if constexpr(avnd::texture_output_introspection<Node_T>::size > 0)
      texture_outs.release(*this, r);

    if constexpr(avnd::buffer_output_introspection<Node_T>::size > 0)
      buffer_outs.release(r);

    if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
      geometry_ins.release(r);

    if constexpr(scene_input_introspection<Node_T>::size > 0)
      scene_ins.release(r);

    // Symmetric with the other *_outs.release calls above. No-ops today
    // (scene_outputs_storage / geometry_outputs_storage own no QRhi
    // resources — scene_spec is value-semantics + a shared_ptr; geometry
    // wraps non-owning pointers + transform values). Wired so future
    // RHI handles on the storages release cleanly.
    if constexpr(avnd::geometry_output_introspection<Node_T>::size > 0)
      geometry_outs.release(r);
    if constexpr(scene_output_introspection<Node_T>::size > 0)
      scene_outs.release(r);

    if constexpr(avnd::texture_input_introspection<Node_T>::size > 0 || avnd::texture_output_introspection<Node_T>::size > 0)
    {
      this->defaultRelease(r);
    }

    if_possible(state->release(r));

    // Clear the optional renderlist backchannel. Paired with the init
    // assignment; same SFINAE guard so nodes without the member are
    // unaffected.
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

    // runInitialPasses is called once PER OUTGOING EDGE per frame. The
    // expensive work below — rhi.finish() sync point, input readbacks,
    // operator()(), and output buffer/texture uploads — only needs to run
    // ONCE per frame: its result lives in `*this->state` and the storages,
    // identical for every edge. We dedupe on RenderList::frame, which is
    // bumped exactly once at the end of each RenderList::render() (see
    // RenderList.cpp). This is NOT a transport-date gate: it does not
    // freeze scene producers when the transport is paused (token.date
    // frozen) — operator()() still re-runs every frame so live parameter
    // edits take effect immediately. The per-edge geometry/scene uploads
    // (which genuinely differ per edge — they target edge.sink) run for
    // EVERY edge, below the guard.
    const bool firstEdgeThisFrame = (renderer.frame != m_last_frame);
    if(firstEdgeThisFrame)
    {
      m_last_frame = renderer.frame;

      if constexpr(
          avnd::texture_input_introspection<Node_T>::size > 0
          || avnd::buffer_input_introspection<Node_T>::size > 0
          || avnd::geometry_input_introspection<Node_T>::size > 0)
      {
        // FIXME: for geometry, here we should optimize if we know we aren't going to need them on the CPU, OR if it is a type ?
        // Insert a synchronisation point to allow readbacks to complete
        rhi.finish();
      }

      if constexpr(avnd::texture_input_introspection<Node_T>::size > 0)
        texture_ins.runInitialPasses(*this, rhi);
      if constexpr(avnd::buffer_input_introspection<Node_T>::size > 0)
        buffer_ins.readInputBuffers(renderer, parent, *state);
      if constexpr(avnd::geometry_input_introspection<Node_T>::size > 0)
        geometry_ins.readInputGeometries(renderer, this->geometry, parent, *state);
      if constexpr(scene_input_introspection<Node_T>::size > 0)
        scene_ins.readInputScenes(this->scene, *state);

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

      // Copy the data to the model node
      parent.processControlOut(*this->state);
    }

    // Per-edge uploads: these target the specific downstream sink
    // (edge.sink) and must run for every outgoing edge, even on edges
    // after the first this frame. The producer's output is already
    // populated in *this->state by the once-per-frame body above.

    // Copy the geometry
    if constexpr(avnd::geometry_output_introspection<Node_T>::size > 0)
      geometry_outs.upload(renderer, *this->state, edge);

    // Copy the scene (travels on the same Gfx::GeometryOutlet as geometry,
    // published via NodeRenderer::process(scene_spec)).
    if constexpr(scene_output_introspection<Node_T>::size > 0)
      scene_outs.upload(renderer, *this->state, edge);
  }

  // Customization point for halp nodes that produce their output via
  // their own GPU pipeline (post-process effects, custom rasterizers).
  //
  // Default GenericNodeRenderer::runRenderPass calls defaultRenderPass,
  // which uses a pre-built fullscreen-quad pipeline that samples
  // m_samplers[0] (the upstream input texture, set up by
  // m_material.init) and writes to the consumer's per-edge RT via the
  // generic_texgen_fs shader. That hard-codes "blit upstream input →
  // downstream input RT" — which is fine for halp filter nodes whose
  // output IS just a CPU-uploaded copy of their input, but is wrong for
  // any node that did real work in runInitialPasses (writing to its own
  // m_outputTex / a private RT): the framework's input-blit overwrites
  // the result, so the consumer sees the unmodified upstream.
  //
  // When the halp class declares its own runRenderPass, we hand off to
  // it. The method runs INSIDE the consumer's beginPass/endPass cycle —
  // it is expected to record draw commands only (no beginPass/endPass
  // on its own) targeting the currently-bound (per-edge) render target.
  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      score::gfx::Edge& edge) override
  {
    if constexpr(requires { state->runRenderPass(renderer, commands, edge); })
      state->runRenderPass(renderer, commands, edge);
    else
      score::gfx::GenericNodeRenderer::runRenderPass(renderer, commands, edge);
  }
};

template <typename Node_T>
  requires(
    (avnd::texture_output_introspection<Node_T>::size + avnd::buffer_output_introspection<Node_T>::size + avnd::geometry_output_introspection<Node_T>::size + scene_output_introspection<Node_T>::size + avnd::gpu_render_target_output_port_output_introspection<Node_T>::size) >= 1
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
