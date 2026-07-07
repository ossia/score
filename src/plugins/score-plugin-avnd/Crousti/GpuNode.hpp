#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/Concepts.hpp>
#include <Crousti/GpuUtils.hpp>
#include <Crousti/Metadatas.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <ossia/detail/algorithms.hpp>
#include <Gfx/Graph/Uniforms.hpp>

// #include <gpp/ports.hpp>

namespace oscr
{

template <typename Node_T>
struct CustomGpuRenderer final : score::gfx::NodeRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  std::vector<std::shared_ptr<Node_T>> states;
  score::gfx::Message m_last_message{};
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  ossia::time_value m_last_time{-1};

  score::gfx::PassMap m_p;

  // Per-pass "pipeline + SRB created" flags, kept index-parallel with m_p
  // and `states` (same push_back in addOutputPass / same erase in
  // removeOutputPass). A single global m_createdPipeline could not handle
  // a pass added live onto an update()-driven node: the first frame would
  // (re)create already-live passes, or skip the new one entirely. Each
  // pass now gates its own srb->create()/pipeline->create().
  ossia::small_vector<bool, 2> m_passCreated;

  score::gfx::MeshBuffers m_meshBuffer{};

  QRhiShaderResourceBindings* m_srb{};

  int sampler_k = 0;
  ossia::flat_map<int, QRhiBuffer*> createdUbos;
  ossia::flat_map<int, QRhiSampler*> createdSamplers;
  ossia::flat_map<int, QRhiTexture*> createdTexs;

  const CustomGpuNodeBase& node() const noexcept
  {
    return static_cast<const CustomGpuNodeBase&>(score::gfx::NodeRenderer::node);
  }

  CustomGpuRenderer(const CustomGpuNodeBase& p)
      : NodeRenderer{p}
  {
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    auto it = m_rts.find(&p);
    SCORE_ASSERT(it != m_rts.end());
    return it->second;
  }

  QRhiTexture* createInput(score::gfx::RenderList& renderer, int k, QSize size)
  {
    auto& parent = node();
    auto port = parent.input[k];
    static constexpr auto flags = QRhiTexture::RenderTarget;
    auto texture = renderer.state.rhi->newTexture(QRhiTexture::RGBA8, size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port] = score::gfx::createRenderTarget(
        renderer.state, texture, renderer.samples(), renderer.requiresDepth(*port));
    return texture;
  }

  template <typename F>
  QRhiShaderResourceBinding initBinding(score::gfx::RenderList& renderer, F field)
  {
    static constexpr auto bindingStages = QRhiShaderResourceBinding::VertexStage
                                          | QRhiShaderResourceBinding::FragmentStage;
    if constexpr(requires { F::ubo; })
    {
      auto it = createdUbos.find(F::binding());
      QRhiBuffer* buffer = it != createdUbos.end() ? it->second : nullptr;
      return QRhiShaderResourceBinding::uniformBuffer(
          F::binding(), bindingStages, buffer);
    }
    else if constexpr(requires { F::sampler2D; })
    {
      auto tex_it = createdTexs.find(F::binding());
      QRhiTexture* tex
          = tex_it != createdTexs.end() ? tex_it->second : &renderer.emptyTexture();

      // Samplers are always created by us
      QRhiSampler* sampler = renderer.state.rhi->newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->create();
      createdSamplers[F::binding()] = sampler;

      return QRhiShaderResourceBinding::sampledTexture(
          F::binding(), bindingStages, tex, sampler);
    }
    else
    {
      static_assert(F::nope);
      throw;
    }
  }

  auto initBindings(score::gfx::RenderList& renderer)
  {
    auto& rhi = *renderer.state.rhi;
    // Shader resource bindings
    auto srb = rhi.newShaderResourceBindings();
    SCORE_ASSERT(srb);

    QVarLengthArray<QRhiShaderResourceBinding, 8> bindings;

    if constexpr(requires { decltype(Node_T::layout::bindings){}; })
    {
      using bindings_type = decltype(Node_T::layout::bindings);
      avnd::pfr::for_each_field(bindings_type{}, [&](auto f) {
        bindings.push_back(initBinding(renderer, f));
      });
    }
    else if constexpr(requires { sizeof(typename Node_T::layout::bindings); })
    {
      using bindings_type = typename Node_T::layout::bindings;
      avnd::pfr::for_each_field(bindings_type{}, [&](auto f) {
        bindings.push_back(initBinding(renderer, f));
      });
    }

    srb->setBindings(bindings.begin(), bindings.end());
    return srb;
  }

  QRhiGraphicsPipeline* createRenderPipeline(
      score::gfx::RenderList& renderer, score::gfx::TextureRenderTarget& rt)
  {
    auto& parent = node();
    auto& rhi = *renderer.state.rhi;
    auto& mesh = renderer.defaultTriangle();
    auto ps = rhi.newGraphicsPipeline();
    ps->setName("createRenderPipeline");
    SCORE_ASSERT(ps);
    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    premulAlphaBlend.srcColor = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
    premulAlphaBlend.dstColor = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;
    premulAlphaBlend.srcAlpha = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
    premulAlphaBlend.dstAlpha = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;
    ps->setTargetBlends({premulAlphaBlend});

    // Match the destination render target's actual sample count, with the
    // standard -1 fallback to renderer.samples() for placeholder RTs that
    // only carry a renderPass descriptor. Without this, MSAA renderlists
    // create RTs with samples=N but this pipeline would default to 1, and
    // Vulkan rejects the render pass for sample-count mismatch.
    {
      const int rtS = rt.sampleCount();
      ps->setSampleCount(rtS > 0 ? rtS : renderer.samples());
    }

    mesh.preparePipeline(*ps);

    auto [v, f]
        = score::gfx::makeShaders(renderer.state, parent.vertex, parent.fragment);
    ps->setShaderStages({{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, f}});

    SCORE_ASSERT(rt.renderPass);
    ps->setRenderPassDescriptor(rt.renderPass);

    return ps;
  }

  void init_input(score::gfx::RenderList& renderer, auto field)
  {
    //using input_type = std::decay_t<F>;
  }

  template <std::size_t Idx, typename F>
  requires avnd::sampler_port<F>
  void init_input(score::gfx::RenderList& renderer, avnd::field_reflection<Idx, F> field)
  {
    auto tex = createInput(renderer, sampler_k++, renderer.state.renderSize);

    using sampler_type = typename avnd::member_reflection<F::sampler()>::member_type;
    createdTexs[sampler_type::binding()] = tex;
  }

  template <std::size_t Idx, typename F>
  requires avnd::uniform_port<F>
  void init_input(score::gfx::RenderList& renderer, avnd::field_reflection<Idx, F> field)
  {
    using ubo_type = typename avnd::member_reflection<F::uniform()>::class_type;

    // We must mark the UBO to construct.
    if(createdUbos.find(ubo_type::binding()) != createdUbos.end())
      return;

    auto ubo = renderer.state.rhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, gpp::std140_size<ubo_type>());
    ubo->setName(oscr::getUtf8Name<F>());
    ubo->create();

    createdUbos[ubo_type::binding()] = ubo;
  }

  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if(m_initialized)
      return;

    // NB: prepare()/processControlIn for graphics nodes is NOT invoked
    // here — `states` is empty at initState time (states are constructed
    // per-edge in addOutputPass), so there is nothing to prepare. The old
    // `states[0].prepare()` detection was also doubly-wrong: `states[0]`
    // is a shared_ptr, so the requires-expression never matched, and even
    // if it had, indexing an empty vector is UB. The prepare/control-in
    // call now happens in addOutputPass right after each state is built.

    if(m_meshBuffer.buffers.empty())
    {
      auto& mesh = renderer.defaultTriangle();
      m_meshBuffer = renderer.initMeshBuffer(mesh, res);
    }

    // Create the global shared inputs
    avnd::input_introspection<Node_T>::for_all(
        [this, &renderer](auto f) { init_input(renderer, f); });

    // Create the shared shader resource bindings
    m_srb = initBindings(renderer);

    m_initialized = true;
  }

  void addOutputPass(
      score::gfx::RenderList& renderer, score::gfx::Edge& edge,
      QRhiResourceUpdateBatch& res) override
  {
    auto& parent = node();
    auto rt = renderer.renderTargetForOutput(edge);
    if(rt.renderTarget)
    {
      states.push_back(std::make_shared<Node_T>());
      prepareNewState(states.back(), parent);

      // Graphics nodes that declare prepare(): apply any pending control
      // input and run prepare() on the freshly-constructed state, here —
      // not in initState, where `states` is still empty. Detection uses
      // operator-> because states.back() is a shared_ptr<Node_T>.
      if constexpr(requires { states.back()->prepare(); })
      {
        parent.processControlIn(
            *this, *states.back(), m_last_message, parent.last_message, parent.m_ctx);
        states.back()->prepare();
      }

      auto ps = createRenderPipeline(renderer, rt);
      ps->setShaderResourceBindings(m_srb);

      m_p.emplace_back(&edge, score::gfx::Pass{rt, score::gfx::Pipeline{ps, m_srb}, nullptr});
      m_passCreated.push_back(false);

      // No update step: we can directly create this pass's pipeline here.
      // The SRB is shared across all passes (m_srb); creating it is
      // idempotent for our purposes, and the per-pass flag tracks the
      // pipeline that is genuinely per-edge.
      if constexpr(!requires { &Node_T::update; })
      {
        SCORE_ASSERT(m_srb->create());
        SCORE_ASSERT(ps->create());
        m_passCreated.back() = true;
      }
    }
  }

  bool hasOutputPassForEdge(score::gfx::Edge& edge) const override
  {
    return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
           != m_p.end();
  }

  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge& edge) override
  {
    // Mirror addOutputPass: each edge owns one entry in m_p (pipeline +
    // SRB) and one parallel entry in `states`. Release both. The shared
    // m_srb pointer is owned by initState; Pass::p.srb refers to the
    // SAME pointer (see addOutputPass), so null it out before
    // Pipeline::release() to avoid double-deleteLater of the shared SRB.
    auto it
        = ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; });
    if(it == m_p.end())
      return;
    const auto idx = std::distance(m_p.begin(), it);
    it->second.p.srb = nullptr; // shared with siblings — owned by initState
    it->second.release();
    m_p.erase(it);
    if((std::size_t)idx < states.size())
      states.erase(states.begin() + idx);
    if((std::size_t)idx < m_passCreated.size())
      m_passCreated.erase(m_passCreated.begin() + idx);
  }

  void releaseState(score::gfx::RenderList& r) override
  {
    if(!m_initialized)
      return;

    m_passCreated.clear();

    // Release the object's internal states
    if constexpr(requires { &Node_T::release; })
    {
      for(auto& state : states)
      {
        for(auto& promise : state->release())
        {
          gpp::qrhi::handle_release handler{*r.state.rhi};
          visit(handler, promise.current_command);
        }
      }
    }
    states.clear();

    // Release the allocated mesh buffers
    m_meshBuffer = {};

    // Release the allocated textures
    for(auto& [id, tex] : this->createdTexs)
      tex->deleteLater();
    this->createdTexs.clear();

    // Release the allocated samplers
    for(auto& [id, sampl] : this->createdSamplers)
      sampl->deleteLater();
    this->createdSamplers.clear();

    // Release the allocated ubos
    for(auto& [id, ubo] : this->createdUbos)
      ubo->deleteLater();
    this->createdUbos.clear();

    // Release the allocated rts
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();

    // Release the allocated pipelines. Each Pass::p.srb refers to the
    // SAME shared m_srb (see addOutputPass); null it out per-pass before
    // Pipeline::release() so the shared SRB isn't deleteLater'd once per
    // pass (it survived previously only via QRhi's QSet dedup), then
    // delete it exactly once below — covering the m_p-empty case too,
    // which formerly leaked m_srb. Mirrors removeOutputPass.
    for(auto& pass : m_p)
    {
      pass.second.p.srb = nullptr; // shared — owned by initState
      pass.second.release();
    }
    m_p.clear();
    if(m_srb)
      m_srb->deleteLater();
    m_srb = nullptr;

    m_meshBuffer = {};

    sampler_k = 0;

    m_initialized = false;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);

    auto& parent = node();
    for(score::gfx::Edge* edge : parent.output[0]->edges)
      addOutputPass(renderer, *edge, res);
  }

  std::vector<QRhiShaderResourceBinding> tmp;
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    // First copy all the "public" uniforms to their space in memory
    if(states.size() > 0)
    {
      auto& state = *states[0];

      avnd::gpu_uniform_introspection<Node_T>::for_all(
          avnd::get_inputs<Node_T>(state), [&]<avnd::uniform_port F>(const F& t) {
            //using input_type = std::decay_t<F>;
            using uniform_type =
                typename avnd::member_reflection<F::uniform()>::member_type;
            using ubo_type = typename avnd::member_reflection<F::uniform()>::class_type;

            auto ubo = this->createdUbos.at(ubo_type::binding());
#if defined(_MSC_VER)
#define MSVC_BUGGY_STATIC_CONSTEXPR
#else
#define MSVC_BUGGY_STATIC_CONSTEXPR static constexpr
#endif
            static constexpr int offset = gpp::std140_offset<F::uniform()>();
            static constexpr int size = sizeof(uniform_type::value);
            res.updateDynamicBuffer(ubo, offset, size, &t.value);
          });
    }

    if constexpr(requires { &Node_T::update; })
    {
      // Then run the update loop, which is a bit complicated
      // as we have to take into account that buffers could be allocated, freed, etc.
      // and thus updated in the shader resource bindings
      SCORE_ASSERT(states.size() == m_p.size());
      SCORE_ASSERT(states.size() == m_passCreated.size());
      //SCORE_SOFT_ASSERT(state.size() == edges);
      for(int k = 0; k < states.size(); k++)
      {
        auto& state = *states[k];
        auto& pass = m_p[k].second;

        // Per-pass creation flag: a pass added live (e.g. a new output
        // edge onto an update()-driven node) starts at false and gets its
        // srb/pipeline created on the next update; passes already live
        // keep their pipeline. A single global flag would skip the new
        // pass entirely (or needlessly destroy the live ones).
        const bool created = m_passCreated[k];

        bool srb_touched{false};
        tmp.assign(pass.p.srb->cbeginBindings(), pass.p.srb->cendBindings());
        for(auto& promise : state.update())
        {
          using ret_type = decltype(promise.feedback_value);
          gpp::qrhi::handle_update<CustomGpuRenderer, ret_type> handler{
              *this, *renderer.state.rhi, res, tmp, srb_touched};
          promise.feedback_value = visit(handler, promise.current_command);
        }

        if(srb_touched)
        {
          if(created)
            pass.p.srb->destroy();

          pass.p.srb->setBindings(tmp.begin(), tmp.end());
          if(created && !pass.p.srb->create())
            qWarning("GpuNode: SRB recreation failed");
        }

        if(!created)
        {
          SCORE_ASSERT(pass.p.srb->create());
          SCORE_ASSERT(pass.p.pipeline->create());
          m_passCreated[k] = true;
        }
      }
      tmp.clear();
    }
  }

  void release(score::gfx::RenderList& r) override { releaseState(r); }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    auto& parent = node();
    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
    {
      return;
    }
    m_last_time = parent.last_message.token.date;

    // Apply the controls
    for(auto& state : states)
    {
      parent.processControlIn(
          *this, *state, m_last_message, parent.last_message, parent.m_ctx);
    }
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      score::gfx::Edge& edge) override
  {
    auto& parent = node();
    auto& mesh = renderer.defaultTriangle();
    score::gfx::defaultRenderPass(renderer, mesh, m_meshBuffer, commands, edge, m_p);

    // Copy the data to the model node
    if(!this->states.empty())
      parent.processControlOut(*this->states[0]);
  }
};

template <typename Node_T>
struct CustomGpuNode final
    : CustomGpuNodeBase
    , GpuNodeElements<Node_T>
{
  CustomGpuNode(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls,
      int64_t id, const score::DocumentContext& ctx)
      : CustomGpuNodeBase{std::move(q), std::move(ctls), ctx}
  {
    this->instance = id;

    initGfxPorts<Node_T>(this, this->input, this->output);

    using layout = typename Node_T::layout;
    static constexpr auto lay = layout{};

    gpp::qrhi::generate_shaders gen;
    if constexpr(requires { &Node_T::vertex; })
    {
      vertex = QString::fromStdString(gen.vertex_shader(lay) + Node_T{}.vertex().data());
    }
    else
    {
      vertex = gpp::qrhi::DefaultPipeline::vertex();
    }

    fragment
        = QString::fromStdString(gen.fragment_shader(lay) + Node_T{}.fragment().data());
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new CustomGpuRenderer<Node_T>{*this};
  }
};

}
#endif
