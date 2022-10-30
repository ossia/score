#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/Concepts.hpp>
#include <Crousti/GpuUtils.hpp>
#include <Crousti/Metadatas.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Uniforms.hpp>
#include <Gfx/Qt5CompatPush> // clang-format: keep

#include <gpp/ports.hpp>

namespace oscr
{

template <typename Node_T>
struct CustomGpuRenderer final : score::gfx::NodeRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  const CustomGpuNodeBase& parent;
  std::vector<Node_T> states;
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  ossia::time_value m_last_time{-1};

  score::gfx::PassMap m_p;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  bool m_createdPipeline{};

  int sampler_k = 0;
  ossia::flat_map<int, QRhiBuffer*> createdUbos;
  ossia::flat_map<int, QRhiSampler*> createdSamplers;
  ossia::flat_map<int, QRhiTexture*> createdTexs;

  CustomGpuRenderer(const CustomGpuNodeBase& p)
      : NodeRenderer{}
      , parent{p}
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
    auto port = parent.input[k];
    constexpr auto flags = QRhiTexture::RenderTarget;
    auto texture = renderer.state.rhi->newTexture(QRhiTexture::RGBA8, size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port]
        = score::gfx::createRenderTarget(renderer.state, texture, renderer.samples());
    return texture;
  }

  template <typename F>
  QRhiShaderResourceBinding initBinding(score::gfx::RenderList& renderer, F field)
  {
    constexpr auto bindingStages = QRhiShaderResourceBinding::VertexStage
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
      boost::pfr::for_each_field(bindings_type{}, [&](auto f) {
        bindings.push_back(initBinding(renderer, f));
      });
    }
    else if constexpr(requires { sizeof(typename Node_T::layout::bindings); })
    {
      using bindings_type = typename Node_T::layout::bindings;
      boost::pfr::for_each_field(bindings_type{}, [&](auto f) {
        bindings.push_back(initBinding(renderer, f));
      });
    }

    srb->setBindings(bindings.begin(), bindings.end());
    return srb;
  }

  QRhiGraphicsPipeline* createRenderPipeline(
      score::gfx::RenderList& renderer, score::gfx::TextureRenderTarget& rt)
  {
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

    ps->setSampleCount(1);

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
    ubo->create();

    createdUbos[ubo_type::binding()] = ubo;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if(!m_meshBuffer)
    {
      auto& mesh = renderer.defaultTriangle();
      auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh, res);
      m_meshBuffer = mbuffer;
      m_idxBuffer = ibuffer;
    }

    // Create the global shared inputs
    avnd::input_introspection<Node_T>::for_all(
        [this, &renderer](auto f) { init_input(renderer, f); });

    // Create the initial srbs
    // TODO when implementing multi-pass, we may have to
    // move this back inside the loop below as they may depend on the pipelines...
    auto srb = initBindings(renderer);

    // Create the states and pipelines
    for(score::gfx::Edge* edge : this->parent.output[0]->edges)
    {
      auto rt = renderer.renderTargetForOutput(*edge);
      if(rt.renderTarget)
      {
        states.push_back({});
        auto ps = createRenderPipeline(renderer, rt);
        ps->setShaderResourceBindings(srb);

        m_p.emplace_back(edge, score::gfx::Pipeline{ps, srb});

        // No update step: we can directly create the pipeline here
        if constexpr(!requires { &Node_T::update; })
        {
          SCORE_ASSERT(srb->create());
          SCORE_ASSERT(ps->create());
          m_createdPipeline = true;
        }
      }
    }
  }

  std::vector<QRhiShaderResourceBinding> tmp;
  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    // First copy all the "public" uniforms to their space in memory
    if(states.size() > 0)
    {
      auto& state = states[0];

      avnd::gpu_uniform_introspection<Node_T>::for_all(
          avnd::get_inputs<Node_T>(state), [&]<avnd::uniform_port F>(const F& t) {
            //using input_type = std::decay_t<F>;
            using uniform_type =
                typename avnd::member_reflection<F::uniform()>::member_type;
            using ubo_type = typename avnd::member_reflection<F::uniform()>::class_type;

            auto ubo = this->createdUbos.at(ubo_type::binding());

            constexpr int offset = gpp::std140_offset<F::uniform()>();
            constexpr int size = sizeof(uniform_type::value);
            res.updateDynamicBuffer(ubo, offset, size, &t.value);
          });
    }

    if constexpr(requires { &Node_T::update; })
    {
      // Then run the update loop, which is a bit complicated
      // as we have to take into account that buffers could be allocated, freed, etc.
      // and thus updated in the shader resource bindings
      SCORE_ASSERT(states.size() == m_p.size());
      //SCORE_SOFT_ASSERT(state.size() == edges);
      for(int k = 0; k < states.size(); k++)
      {
        auto& state = states[k];
        auto& pass = m_p[k].second;

        bool srb_touched{false};
        tmp.assign(pass.srb->cbeginBindings(), pass.srb->cendBindings());
        for(auto& promise : state.update())
        {
          using ret_type = decltype(promise.feedback_value);
          gpp::qrhi::handle_update<CustomGpuRenderer, ret_type> handler{
              *this, *renderer.state.rhi, res, tmp, srb_touched};
          promise.feedback_value = visit(handler, promise.current_command);
        }

        if(srb_touched)
        {
          if(m_createdPipeline)
            pass.srb->destroy();

          pass.srb->setBindings(tmp.begin(), tmp.end());
        }

        if(!m_createdPipeline)
        {
          SCORE_ASSERT(pass.srb->create());
          SCORE_ASSERT(pass.pipeline->create());
        }
      }
      m_createdPipeline = true;
      tmp.clear();
    }
  }

  void release(score::gfx::RenderList& r) override
  {
    m_createdPipeline = false;

    // Release the object's internal states
    if constexpr(requires { &Node_T::release; })
    {
      for(auto& state : states)
      {
        for(auto& promise : state.release())
        {
          gpp::qrhi::handle_release handler{*r.state.rhi};
          visit(handler, promise.current_command);
        }
      }
    }
    states.clear();

    // Release the allocated mesh buffers
    m_meshBuffer = nullptr;

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
    // TODO investigate why reference does not work here:
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();

    // Release the allocated pipelines
    for(auto& pass : m_p)
      pass.second.release();
    m_p.clear();

    m_meshBuffer = nullptr;
    m_createdPipeline = false;

    sampler_k = 0;
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
    {
      return;
    }
    m_last_time = parent.last_message.token.date;

    // Apply the controls
    for(auto& state : states)
    {
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
    }
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      score::gfx::Edge& edge) override
  {
    auto& mesh = renderer.defaultTriangle();
    score::gfx::defaultRenderPass(
        renderer, mesh, {m_meshBuffer, m_idxBuffer}, commands, edge, m_p);

    // Copy the data to the model node
    if(!this->states.empty())
      parent.processControlOut(this->states[0]);
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep

template <typename Node_T>
struct CustomGpuNode final : CustomGpuNodeBase
{
  CustomGpuNode(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls)
      : CustomGpuNodeBase{std::move(q), std::move(ctls)}
  {
    using texture_inputs = avnd::gpu_sampler_introspection<Node_T>;
    using texture_outputs = avnd::gpu_attachment_introspection<Node_T>;

    for(std::size_t i = 0; i < texture_inputs::size; i++)
    {
      input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }
    for(std::size_t i = 0; i < texture_outputs::size; i++)
    {
      output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }

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
