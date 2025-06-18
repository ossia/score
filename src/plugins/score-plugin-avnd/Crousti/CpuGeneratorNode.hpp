#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
      avnd::texture_input_introspection<Node_T>::size == 0
      && avnd::texture_output_introspection<Node_T>::size > 0)
struct GfxRenderer<Node_T> final : score::gfx::GenericNodeRenderer
{
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  Node_T state;
  score::gfx::Message m_last_message{};
  ossia::time_value m_last_time{-1};

  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }

  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::GenericNodeRenderer{p}
  {
    prepareNewState(state, p);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
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
        // Upload it
        {
          QRhiTextureSubresourceUploadDescription sd(cpu_tex.bytes, cpu_tex.bytesize());
          QRhiTextureUploadDescription desc{QRhiTextureUploadEntry{0, 0, sd}};

          res->uploadTexture(texture, desc);
        }

        cpu_tex.changed = false;
        k++;
      }
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

    // Init textures for the outputs
    avnd::cpu_texture_output_introspection<Node_T>::for_all(
        avnd::get_outputs<Node_T>(state), [&](auto& t) {
          createOutput(renderer, t.texture, QSize{t.texture.width, t.texture.height});
        });

    this->defaultPassesInit(renderer, mesh);
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
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

    this->defaultRelease(r);
  }

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

    parent.processControlIn(
        *this, state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the processor
    state();

    // Upload output textures
    int k = 0;
    avnd::cpu_texture_output_introspection<Node_T>::for_all(
        avnd::get_outputs<Node_T>(state), [&](auto& t) {
          uploadOutputTexture(renderer, k, t.texture, res);
          k++;
        });

    commands.resourceUpdate(res);
    res = renderer.state.rhi->nextResourceUpdateBatch();

    // Copy the data to the model node
    parent.processControlOut(this->state);
  }
};

template <typename Node_T>
  requires(avnd::texture_input_introspection<Node_T>::size == 0
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
