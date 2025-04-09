#pragma once

#if SCORE_PLUGIN_GFX
#include <Process/ExecutionContext.hpp>

#include <Crousti/Concepts.hpp>
#include <Crousti/GpuUtils.hpp>
#include <Crousti/Metadatas.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Uniforms.hpp>

namespace oscr
{
// Compute nodes that do not have any texture output so that they can get pulled
// must drive things internally instead
template <typename Node_T>
using ComputeNodeBaseType = std::conditional_t<
    avnd::gpu_image_output_introspection<Node_T>::size != 0, CustomGpuNodeBase,
    CustomGpuOutputNodeBase>;
template <typename Node_T>
using ComputeRendererBaseType = std::conditional_t<
    avnd::gpu_image_output_introspection<Node_T>::size != 0, score::gfx::NodeRenderer,
    score::gfx::OutputNodeRenderer>;

template <typename Node_T>
struct GpuComputeNode final : ComputeNodeBaseType<Node_T>
{
  GpuComputeNode(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls, int id,
      const score::DocumentContext& ctx)
      : ComputeNodeBaseType<Node_T>{std::move(q), std::move(ctls), ctx}
  {
    this->instance = id;

    initGfxPorts<Node_T>(this, this->input, this->output);
    using layout = typename Node_T::layout;
    static constexpr auto lay = layout{};

    gpp::qrhi::generate_shaders gen;
    this->compute
        = QString::fromStdString(gen.compute_shader(lay) + Node_T{}.compute().data());
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;
};

template <typename Node_T>
struct GpuComputeRenderer final : ComputeRendererBaseType<Node_T>
{
  using texture_inputs = avnd::gpu_image_input_introspection<Node_T>;
  using texture_outputs = avnd::gpu_image_output_introspection<Node_T>;
  Node_T state;
  score::gfx::Message m_last_message{};
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  ossia::time_value m_last_time{-1};

  QRhiShaderResourceBindings* m_srb{};
  QRhiComputePipeline* m_pipeline{};

  bool m_createdPipeline{};

  int sampler_k = 0;
  int ubo_k = 0;
  ossia::flat_map<int, QRhiBuffer*> createdUbos;
  ossia::flat_map<int, QRhiTexture*> createdTexs;

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
  std::vector<QRhiBufferReadbackResult*> bufReadbacks;
  void addReadback(QRhiBufferReadbackResult* r) { bufReadbacks.push_back(r); }
#endif

  std::vector<QRhiReadbackResult*> texReadbacks;
  void addReadback(QRhiReadbackResult* r) { texReadbacks.push_back(r); }

  const GpuComputeNode<Node_T>& node() const noexcept
  {
    return static_cast<const GpuComputeNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }

  GpuComputeRenderer(const GpuComputeNode<Node_T>& p)
      : ComputeRendererBaseType<Node_T>{p}
  {
    prepareNewState(state, p);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    auto it = m_rts.find(&p);
    SCORE_ASSERT(it != m_rts.end());
    return it->second;
  }

  QRhiTexture* createInput(
      score::gfx::RenderList& renderer, int k, QRhiTexture::Format fmt, QSize size)
  {
    auto& parent = node();
    auto port = parent.input[k];
    static constexpr auto flags
        = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore;
    auto texture = renderer.state.rhi->newTexture(fmt, size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port]
        = score::gfx::createRenderTarget(renderer.state, texture, renderer.samples());
    return texture;
  }

  template <typename F>
  QRhiShaderResourceBinding initBinding(score::gfx::RenderList& renderer, F field)
  {
    static constexpr auto bindingStages = QRhiShaderResourceBinding::ComputeStage;
    if constexpr(requires { F::ubo; })
    {
      auto it = createdUbos.find(F::binding());
      QRhiBuffer* buffer = it != createdUbos.end() ? it->second : nullptr;
      return QRhiShaderResourceBinding::uniformBuffer(
          F::binding(), bindingStages, buffer);
    }
    else if constexpr(requires { F::image2D; })
    {
      auto tex_it = createdTexs.find(F::binding());
      QRhiTexture* tex
          = tex_it != createdTexs.end() ? tex_it->second : &renderer.emptyTexture();

      if constexpr(
          requires { F::load; } && requires { F::store; })
        return QRhiShaderResourceBinding::imageLoadStore(
            F::binding(), bindingStages, tex, 0);
      else if constexpr(requires { F::readonly; })
        return QRhiShaderResourceBinding::imageLoad(F::binding(), bindingStages, tex, 0);
      else if constexpr(requires { F::writeonly; })
        return QRhiShaderResourceBinding::imageStore(
            F::binding(), bindingStages, tex, 0);
      else
        static_assert(F::load || F::store);
    }
    else if constexpr(requires { F::buffer; })
    {
      auto it = createdUbos.find(F::binding());
      QRhiBuffer* buf = it != createdUbos.end() ? it->second : nullptr;

      if constexpr(
          requires { F::load; } && requires { F::store; })
        return QRhiShaderResourceBinding::bufferLoadStore(
            F::binding(), bindingStages, buf);
      else if constexpr(requires { F::load; })
        return QRhiShaderResourceBinding::bufferLoad(F::binding(), bindingStages, buf);
      else if constexpr(requires { F::store; })
        return QRhiShaderResourceBinding::bufferStore(F::binding(), bindingStages, buf);
      else
        static_assert(F::load || F::store);
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

    using bindings_type = decltype(Node_T::layout::bindings);
    boost::pfr::for_each_field(
        bindings_type{}, [&](auto f) { bindings.push_back(initBinding(renderer, f)); });

    srb->setBindings(bindings.begin(), bindings.end());
    return srb;
  }

  QRhiComputePipeline* createComputePipeline(score::gfx::RenderList& renderer)
  {
    auto& parent = node();
    auto& rhi = *renderer.state.rhi;
    auto compute = rhi.newComputePipeline();
    auto cs = score::gfx::makeCompute(renderer.state, parent.compute);
    compute->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, cs));

    return compute;
  }

  void init_input(score::gfx::RenderList& renderer, auto field)
  {
    //using input_type = std::decay_t<F>;
  }

  template <std::size_t Idx, typename F>
    requires avnd::image_port<F>
  void init_input(score::gfx::RenderList& renderer, avnd::field_reflection<Idx, F> field)
  {
    using bindings_type = decltype(Node_T::layout::bindings);
    using image_type = std::decay_t<decltype(bindings_type{}.*F::image())>;
    auto tex = createInput(
        renderer, sampler_k++, gpp::qrhi::textureFormat<image_type>(),
        renderer.state.renderSize);

    using sampler_type = typename avnd::member_reflection<F::image()>::member_type;
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
    auto& parent = node();
    if constexpr(requires { state.prepare(); })
    {
      parent.processControlIn(
          *this, state, m_last_message, parent.last_message, parent.m_ctx);
      state.prepare();
    }

    // Create the global shared inputs
    avnd::input_introspection<Node_T>::for_all(
        [this, &renderer](auto f) { init_input(renderer, f); });

    m_srb = initBindings(renderer);
    m_pipeline = createComputePipeline(renderer);
    m_pipeline->setShaderResourceBindings(m_srb);

    // No update step: we can directly create the pipeline here
    if constexpr(!requires { &Node_T::update; })
    {
      SCORE_ASSERT(m_srb->create());
      SCORE_ASSERT(m_pipeline->create());
      m_createdPipeline = true;
    }
  }

  std::vector<QRhiShaderResourceBinding> tmp;
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    // First copy all the "public" uniforms to their space in memory
    avnd::gpu_uniform_introspection<Node_T>::for_all(
        avnd::get_inputs<Node_T>(state), [&]<avnd::uniform_port F>(const F& t) {
          using uniform_type =
              typename avnd::member_reflection<F::uniform()>::member_type;
          using ubo_type = typename avnd::member_reflection<F::uniform()>::class_type;

          auto ubo = this->createdUbos.at(ubo_type::binding());

          static constexpr int offset = gpp::std140_offset<F::uniform()>();
          static constexpr int size = sizeof(uniform_type::value);
          res.updateDynamicBuffer(ubo, offset, size, &t.value);
        });

    // Then run the update loop, which is a bit complicated
    // as we have to take into account that buffers could be allocated, freed, etc.
    // and thus updated in the shader resource bindings

    if constexpr(requires { &Node_T::update; })
    {
      bool srb_touched{false};
      tmp.assign(m_srb->cbeginBindings(), m_srb->cendBindings());
      for(auto& promise : state.update())
      {
        using ret_type = decltype(promise.feedback_value);
        gpp::qrhi::handle_update<GpuComputeRenderer, ret_type> handler{
            *this, *renderer.state.rhi, res, tmp, srb_touched};
        promise.feedback_value = visit(handler, promise.current_command);
      }

      if(srb_touched)
      {
        if(m_createdPipeline)
          m_srb->destroy();
        m_srb->setBindings(tmp.begin(), tmp.end());
      }

      /*
      qDebug() << srb_touched << m_createdPipeline;
      for(auto& b : tmp) {
        const QRhiShaderResourceBinding::Data& dat = *b.data();
        switch(dat.type) {
          case QRhiShaderResourceBinding::UniformBuffer:
            qDebug() << "ubo: " << dat.binding << (void*) dat.u.ubuf.buf;
            break;
          case QRhiShaderResourceBinding::ImageLoad:
          case QRhiShaderResourceBinding::ImageStore:
          case QRhiShaderResourceBinding::ImageLoadStore:
            qDebug() << "image: " << dat.binding << (void*) dat.u.simage.tex;
            break;
          case QRhiShaderResourceBinding::BufferLoad:
          case QRhiShaderResourceBinding::BufferStore:
          case QRhiShaderResourceBinding::BufferLoadStore:
            qDebug() << "buffer: " << dat.binding << (void*) dat.u.sbuf.buf;
            break;
          default:
            qDebug() << "WRTF: " << dat.binding;
        }
      }
      */

      if(!m_createdPipeline)
      {
        SCORE_ASSERT(m_srb->create());
        SCORE_ASSERT(m_pipeline->create());
        m_createdPipeline = true;
      }
      tmp.clear();
    }
  }

  void release(score::gfx::RenderList& r) override
  {
    m_createdPipeline = false;

    // Release the object's internal states
    if constexpr(requires { &Node_T::release; })
    {
      for(auto& promise : state.release())
      {
        gpp::qrhi::handle_release handler{*r.state.rhi};
        visit(handler, promise.current_command);
      }
      state.release();
    }

    // Release the allocated textures
    for(auto& [id, tex] : this->createdTexs)
      tex->deleteLater();
    this->createdTexs.clear();

    // Release the allocated ubos
    for(auto& [id, ubo] : this->createdUbos)
      ubo->deleteLater();
    this->createdUbos.clear();

    // Release the allocated rts
    // TODO investigate why reference does not work here:
    for(auto& e : m_rts)
    {
      e.second.release();
    }
    m_rts.clear();

    // Release the allocated pipelines
    if(m_srb)
      m_srb->deleteLater();
    if(m_pipeline)
      m_pipeline->deleteLater();
    m_srb = nullptr;
    m_pipeline = nullptr;

    m_createdPipeline = false;

    sampler_k = 0;
    ubo_k = 0;
  }

  void runCompute(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res)
  {
    auto& parent = node();
    // If we are paused, we don't run the processor implementation.
    // if(parent.last_message.token.date == m_last_time) {
    //   return;
    // }
    // m_last_time = parent.last_message.token.date;

    // Apply the controls
    parent.processControlIn(
        *this, this->state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the compute shader
    {
      SCORE_ASSERT(this->m_pipeline);
      SCORE_ASSERT(this->m_pipeline->shaderResourceBindings());
      for(auto& promise : this->state.dispatch())
      {
        using ret_type = decltype(promise.feedback_value);
        gpp::qrhi::handle_dispatch<GpuComputeRenderer, ret_type> handler{
            *this, *renderer.state.rhi, cb, res, *this->m_pipeline};
        promise.feedback_value = visit(handler, promise.current_command);
      }
    }

    // Clear the readbacks
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
    for(auto rb : this->bufReadbacks)
      delete rb;
    this->bufReadbacks.clear();
#endif
    for(auto rb : this->texReadbacks)
      delete rb;
    this->texReadbacks.clear();

    // Copy the data to the model node
    parent.processControlOut(this->state);
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    runCompute(renderer, cb, res);
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      score::gfx::Edge& edge) override
  {
  }
};

template <typename Node_T>
inline score::gfx::OutputNodeRenderer*
GpuComputeNode<Node_T>::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new GpuComputeRenderer<Node_T>{*this};
};

}
#endif
