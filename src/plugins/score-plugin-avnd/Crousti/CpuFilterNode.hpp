#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
      avnd::texture_input_introspection<Node_T>::size > 0
      && avnd::texture_output_introspection<Node_T>::size > 0)
struct GfxRenderer<Node_T> final : score::gfx::GenericNodeRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  Node_T state;
  score::gfx::Message m_last_message{};
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  std::vector<QRhiReadbackResult> m_readbacks;
  ossia::time_value m_last_time{-1};

  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }

  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::GenericNodeRenderer{p}
      , m_readbacks(texture_inputs::size)
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

  template <typename Tex>
  void createInput(
      score::gfx::RenderList& renderer, int k, const Tex& texture_spec, QSize size)
  {
    auto& parent = node();
    auto port = parent.input[k];
    static constexpr auto flags
        = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;
    auto texture = renderer.state.rhi->newTexture(
        gpp::qrhi::textureFormat<Tex>(), size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port]
        = score::gfx::createRenderTarget(renderer.state, texture, renderer.samples());
  }

  template <typename Tex>
  void
  createOutput(score::gfx::RenderList& renderer, const Tex& texture_spec, QSize size)
  {
    auto& rhi = *renderer.state.rhi;
    QRhiTexture* texture = &renderer.emptyTexture();
    if(size.width() > 0 && size.height() > 0)
    {
      texture = rhi.newTexture(
          gpp::qrhi::textureFormat<Tex>(), size, 1, QRhiTexture::Flag{});

      texture->create();
    }

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

    sampler->create();
    this->m_samplers.push_back({sampler, texture});
  }

  template <avnd::cpu_texture Tex>
  QRhiTexture* updateTexture(score::gfx::RenderList& renderer, int k, const Tex& cpu_tex)
  {
    auto& [sampler, texture] = this->m_samplers[k];
    if(texture)
    {
      auto sz = texture->pixelSize();
      if(cpu_tex.width == sz.width() && cpu_tex.height == sz.height())
        return texture;
    }

    // Check the texture size
    if(cpu_tex.width > 0 && cpu_tex.height > 0)
    {
      QRhiTexture* oldtex = texture;
      QRhiTexture* newtex = renderer.state.rhi->newTexture(
          gpp::qrhi::textureFormat<Tex>(), QSize{cpu_tex.width, cpu_tex.height}, 1,
          QRhiTexture::Flag{});
      newtex->create();
      for(auto& [edge, pass] : this->m_p)
        if(pass.srb)
          score::gfx::replaceTexture(*pass.srb, sampler, newtex);
      texture = newtex;

      if(oldtex && oldtex != &renderer.emptyTexture())
      {
        oldtex->deleteLater();
      }

      return newtex;
    }
    else
    {
      for(auto& [edge, pass] : this->m_p)
        if(pass.srb)
          score::gfx::replaceTexture(*pass.srb, sampler, &renderer.emptyTexture());

      return &renderer.emptyTexture();
    }
  }

  void uploadOutputTexture(
      score::gfx::RenderList& renderer, int k, avnd::cpu_texture auto& cpu_tex,
      QRhiResourceUpdateBatch* res)
  {
    if(cpu_tex.changed)
    {
      if(auto texture = updateTexture(renderer, k, cpu_tex))
      {
        // Upload it (mirroring is done in shader generic_texgen_fs if necessary)
        {
          QRhiTextureSubresourceUploadDescription sd(
              cpu_tex.bytes, cpu_tex.width * cpu_tex.height * 4);
          QRhiTextureUploadDescription desc{QRhiTextureUploadEntry{0, 0, sd}};
          res->uploadTexture(texture, desc);
        }

        cpu_tex.changed = false;
        k++;
      }
    }
  }

  void loadInputTexture(QRhi& rhi, avnd::cpu_texture auto& cpu_tex, int k)
  {
    auto& buf = m_readbacks[k].data;
    if(buf.size() != 4 * cpu_tex.width * cpu_tex.height)
    {
      cpu_tex.bytes = nullptr;
    }
    else
    {
      cpu_tex.bytes = reinterpret_cast<unsigned char*>(buf.data());

      if(rhi.isYUpInFramebuffer())
        if(cpu_tex.width * cpu_tex.height > 0)
          inplaceMirror(cpu_tex.bytes, cpu_tex.width, cpu_tex.height);

      cpu_tex.changed = true;
    }
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

    const auto& mesh = renderer.defaultTriangle();
    this->defaultMeshInit(renderer, mesh, res);
    this->processUBOInit(renderer);
    // Not needed here as we do not have a GPU pass:
    // this->m_material.init(renderer, this->node.input, this->m_samplers);

    std::tie(this->m_vertexS, this->m_fragmentS)
        = score::gfx::makeShaders(renderer.state, generic_texgen_vs, generic_texgen_fs);

    {
      // Init input render targets
      int k = 0;
      avnd::cpu_texture_input_introspection<Node_T>::for_all(
          avnd::get_inputs<Node_T>(state), [&]<typename F>(F& t) {
            auto sz = renderer.state.renderSize;
            if constexpr(requires {
                           t.request_width;
                           t.request_height;
                         })
            {
              sz.rwidth() = t.request_width;
              sz.rheight() = t.request_height;
            }
            createInput(renderer, k, t.texture, sz);
            t.texture.width = sz.width();
            t.texture.height = sz.height();
            k++;
          });
    }

    {
      // Init textures for the outputs
      avnd::cpu_texture_output_introspection<Node_T>::for_all(
          avnd::get_outputs<Node_T>(state), [&](auto& t) {
            createOutput(renderer, t.texture, QSize{t.texture.width, t.texture.height});
          });
    }

    this->defaultPassesInit(renderer, mesh);
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    this->defaultUBOUpdate(renderer, res);
  }

  void release(score::gfx::RenderList& r) override
  {
    // Free outputs
    for(auto& [sampl, texture] : this->m_samplers)
    {
      if(texture != &r.emptyTexture())
        texture->deleteLater();
      texture = nullptr;
    }

    // Free inputs
    // TODO investigate why reference does not work here:
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();

    this->defaultRelease(r);
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    auto& parent = node();
    res = renderer.state.rhi->nextResourceUpdateBatch();
    const auto& inputs = parent.input;
    auto index_of_port = ossia::find(inputs, &p) - inputs.begin();
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
    auto& parent = node();
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
        loadInputTexture(rhi, t.texture, k);
        k++;
      });
    }

    parent.processControlIn(
        *this, state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the processor
    state();

    // Upload output textures
    {
      int k = 0;
      avnd::cpu_texture_output_introspection<Node_T>::for_all(
          avnd::get_outputs<Node_T>(state), [&](auto& t) {
            uploadOutputTexture(renderer, k, t.texture, res);
            k++;
          });

      commands.resourceUpdate(res);
      res = renderer.state.rhi->nextResourceUpdateBatch();
    }

    // Copy the data to the model node
    parent.processControlOut(this->state);
  }
};

template <typename Node_T>
  requires(avnd::texture_input_introspection<Node_T>::size > 0
           && avnd::texture_output_introspection<Node_T>::size > 0)
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
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls, int id,
      const score::DocumentContext& ctx)
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
