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
#include <Gfx/Qt5CompatPush> // clang-format: keep

namespace oscr
{
template <typename F>
constexpr QRhiTexture::Format textureFormat()
{
  constexpr std::string_view fmt = F::format();

  if(fmt == "rgba" || fmt == "rgba8")
    return QRhiTexture::RGBA8;
  else if(fmt == "bgra" || fmt == "bgra8")
    return QRhiTexture::BGRA8;
  else if(fmt == "r8")
    return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "rg8")
    return QRhiTexture::RG8;
#endif
  else if(fmt == "r16")
    return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "rg16")
    return QRhiTexture::RG16;
#endif
  else if(fmt == "red_or_alpha8")
    return QRhiTexture::RED_OR_ALPHA8;
  else if(fmt == "rgba16f")
    return QRhiTexture::RGBA16F;
  else if(fmt == "rgba32f")
    return QRhiTexture::RGBA32F;
  else if(fmt == "r16f")
    return QRhiTexture::R16F;
  else if(fmt == "r32")
    return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "rgb10a2")
    return QRhiTexture::RGB10A2;
#endif

  else if(fmt == "d16")
    return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "d24")
    return QRhiTexture::D24;
  else if(fmt == "d24s8")
    return QRhiTexture::D24S8;
#endif
  else if(fmt == "d32f")
    return QRhiTexture::D32F;

  else if(fmt == "bc1")
    return QRhiTexture::BC1;
  else if(fmt == "bc2")
    return QRhiTexture::BC2;
  else if(fmt == "bc3")
    return QRhiTexture::BC3;
  else if(fmt == "bc4")
    return QRhiTexture::BC4;
  else if(fmt == "bc5")
    return QRhiTexture::BC5;
  else if(fmt == "bc6h")
    return QRhiTexture::BC6H;
  else if(fmt == "bc7")
    return QRhiTexture::BC7;
  else if(fmt == "etc2_rgb8")
    return QRhiTexture::ETC2_RGB8;
  else if(fmt == "etc2_rgb8a1")
    return QRhiTexture::ETC2_RGB8A1;
  else if(fmt == "etc2_rgb8a8")
    return QRhiTexture::ETC2_RGBA8;
  else if(fmt == "astc_4x4")
    return QRhiTexture::ASTC_4x4;
  else if(fmt == "astc_5x4")
    return QRhiTexture::ASTC_5x4;
  else if(fmt == "astc_5x5")
    return QRhiTexture::ASTC_5x5;
  else if(fmt == "astc_6x5")
    return QRhiTexture::ASTC_6x5;
  else if(fmt == "astc_6x6")
    return QRhiTexture::ASTC_6x6;
  else if(fmt == "astc_8x5")
    return QRhiTexture::ASTC_8x5;
  else if(fmt == "astc_8x6")
    return QRhiTexture::ASTC_8x6;
  else if(fmt == "astc_8x8")
    return QRhiTexture::ASTC_8x8;
  else if(fmt == "astc_10x5")
    return QRhiTexture::ASTC_10x5;
  else if(fmt == "astc_10x6")
    return QRhiTexture::ASTC_10x6;
  else if(fmt == "astc_10x8")
    return QRhiTexture::ASTC_10x8;
  else if(fmt == "astc_10x10")
    return QRhiTexture::ASTC_10x10;
  else if(fmt == "astc_12x10")
    return QRhiTexture::ASTC_12x10;
  else if(fmt == "astc_12x12")
    return QRhiTexture::ASTC_12x12;
  else
    return QRhiTexture::RGBA8;
}
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
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls, int id)
      : ComputeNodeBaseType<Node_T>{std::move(q), std::move(ctls)}
  {
    this->instance = id;

    using texture_inputs = avnd::gpu_image_input_introspection<Node_T>;
    using texture_outputs = avnd::gpu_image_output_introspection<Node_T>;

    for(std::size_t i = 0; i < texture_inputs::size; i++)
    {
      this->input.push_back(
          new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }
    for(std::size_t i = 0; i < texture_outputs::size; i++)
    {
      this->output.push_back(
          new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }

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
  const GpuComputeNode<Node_T>& parent;
  Node_T state;
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

  std::vector<QRhiBufferReadbackResult*> bufReadbacks;
  std::vector<QRhiReadbackResult*> texReadbacks;

  void addReadback(QRhiBufferReadbackResult* r) { bufReadbacks.push_back(r); }
  void addReadback(QRhiReadbackResult* r) { texReadbacks.push_back(r); }

  GpuComputeRenderer(const GpuComputeNode<Node_T>& p)
      : ComputeRendererBaseType<Node_T>{}
      , parent{p}
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

  QRhiTexture* createInput(
      score::gfx::RenderList& renderer, int k, QRhiTexture::Format fmt, QSize size)
  {
    auto port = parent.input[k];
    constexpr auto flags = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore;
    auto texture = renderer.state.rhi->newTexture(fmt, size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port]
        = score::gfx::createRenderTarget(renderer.state, texture, renderer.samples());
    return texture;
  }

  template <typename F>
  QRhiShaderResourceBinding initBinding(score::gfx::RenderList& renderer, F field)
  {
    constexpr auto bindingStages = QRhiShaderResourceBinding::ComputeStage;
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
        renderer, sampler_k++, oscr::textureFormat<image_type>(),
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
  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    // First copy all the "public" uniforms to their space in memory
    avnd::gpu_uniform_introspection<Node_T>::for_all(
        avnd::get_inputs<Node_T>(state), [&]<avnd::uniform_port F>(const F& t) {
          using uniform_type =
              typename avnd::member_reflection<F::uniform()>::member_type;
          using ubo_type = typename avnd::member_reflection<F::uniform()>::class_type;

          auto ubo = this->createdUbos.at(ubo_type::binding());

          constexpr int offset = gpp::std140_offset<F::uniform()>();
          constexpr int size = sizeof(uniform_type::value);
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
    // for(auto [port, rt] : m_rts)
    //   rt.release();
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
    // If we are paused, we don't run the processor implementation.
    // if(parent.last_message.token.date == m_last_time) {
    //   return;
    // }
    // m_last_time = parent.last_message.token.date;

    // Apply the controls
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
    for(auto rb : this->bufReadbacks)
      delete rb;
    this->bufReadbacks.clear();
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

#include <Gfx/Qt5CompatPop> // clang-format: keep

template <typename Node_T>
inline score::gfx::OutputNodeRenderer*
GpuComputeNode<Node_T>::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new GpuComputeRenderer<Node_T>{*this};
};

}
#endif
