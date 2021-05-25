#include "depthnode.hpp"

DepthNode::DepthNode(const QShader& compute)
{
  m_computeS = compute;

  input.push_back(new Port{this, {}, Types::Image, {}});
  output.push_back(new Port{this, {}, Types::Image, {}});
}

struct RenderedDepthNode : score::gfx::NodeRenderer
{
  struct Pass
  {
    QRhiSampler* sampler{};
    TextureRenderTarget renderTarget;
    Pipeline p;
    QRhiBuffer* processUBO{};
  };
  std::vector<Pass> m_passes;

  DepthNode& n;

  TextureRenderTarget m_lastPassRT;

  std::vector<Sampler> m_samplers;

  // Pipeline
  Pipeline m_p;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};
  int64_t materialChangedIndex{-1};

  RenderedDepthNode(const DepthNode& node) noexcept
      : score::gfx::NodeRenderer{}
      , n{const_cast<DepthNode&>(node)}
  {
  }

  std::optional<QSize> renderTargetSize() const noexcept override
  {
    return {};
  }

  TextureRenderTarget createRenderTarget(const RenderState& state) override
  {
    auto sz = state.size;
    if (auto true_sz = renderTargetSize())
    {
      sz = *true_sz;
    }

    m_lastPassRT = score::gfx::createRenderTarget(state, sz);
    return m_lastPassRT;
  }

  QSize computeTextureSize(const isf::pass& pass)
  {
    QSize res = m_lastPassRT.renderTarget->pixelSize();

    exprtk::symbol_table<float> syms;

    syms.add_constant("var_WIDTH", res.width());
    syms.add_constant("var_HEIGHT", res.height());
    int port_k = 0;
    for (const isf::input& input : n.m_descriptor.inputs)
    {
      auto port = n.input[port_k];
      if (std::get_if<isf::float_input>(&input.data))
      {
        syms.add_constant("var_" + input.name, *(float*)port->value);
      }
      else
      {
        // TODO exprtk only handles the expression type...
      }

      port_k++;
    }

    if (auto expr = pass.width_expression; !expr.empty())
    {
      boost::algorithm::replace_all(expr, "$", "var_");
      exprtk::expression<float> e;
      e.register_symbol_table(syms);
      exprtk::parser<float> parser;
      bool ok = parser.compile(expr, e);
      if (ok)
        res.setWidth(e());
      else
        qDebug() << parser.error().c_str() << expr.c_str();
    }
    if (auto expr = pass.height_expression; !expr.empty())
    {
      boost::algorithm::replace_all(expr, "$", "var_");
      exprtk::expression<float> e;
      e.register_symbol_table(syms);
      exprtk::parser<float> parser;
      bool ok = parser.compile(expr, e);
      if (ok)
        res.setHeight(e());
      else
        qDebug() << parser.error().c_str() << expr.c_str();
    }

    return res;
  }

  int initShaderSamplers(Renderer& renderer)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& input = n.input;
    int cur_pos = 0;
    for (auto in : input)
    {
      switch (in->type)
      {
        case Types::Empty:
          break;
        case Types::Int:
        case Types::Float:
          cur_pos += 4;
          break;
        case Types::Vec2:
          cur_pos += 8;
          if (cur_pos % 8 != 0)
            cur_pos += 4;
          break;
        case Types::Vec3:
          while (cur_pos % 16 != 0)
          {
            cur_pos += 4;
          }
          cur_pos += 12;
          break;
        case Types::Vec4:
          while (cur_pos % 16 != 0)
          {
            cur_pos += 4;
          }
          cur_pos += 16;
          break;
        case Types::Image:
        {
          auto sampler = rhi.newSampler(
              QRhiSampler::Linear,
              QRhiSampler::Linear,
              QRhiSampler::None,
              QRhiSampler::ClampToEdge,
              QRhiSampler::ClampToEdge);
          SCORE_ASSERT(sampler->create());

          auto texture = renderer.textureTargetForInputPort(*in);
          m_samplers.push_back({sampler, texture});

          if (cur_pos % 8 != 0)
            cur_pos += 4;

          *(float*)(n.m_materialData.get() + cur_pos)
              = texture->pixelSize().width();
          *(float*)(n.m_materialData.get() + cur_pos + 4)
              = texture->pixelSize().height();

          cur_pos += 8;
          break;
        }
        default:
          break;
      }
    }
    return cur_pos;
  }

  void initAudioTextures(Renderer& renderer)
  {
    QRhi& rhi = *renderer.state.rhi;
    for (auto& texture : n.audio_textures)
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->create();

      m_samplers.push_back({sampler, renderer.m_emptyTexture});
      texture.samplers[&renderer] = {sampler, nullptr};
    }
  }

  void initPassSamplers(Renderer& renderer, int& cur_pos)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& model_passes = n.m_descriptor.passes;
    for (int i = 0, N = model_passes.size(); i < N - 1; i++)
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->create();

      const QSize texSize = computeTextureSize(model_passes[i]);

      const auto fmt = (model_passes[i].float_storage) ? QRhiTexture::RGBA32F
                                                       : QRhiTexture::RGBA8;

      auto tex = rhi.newTexture(
          fmt, texSize, 1, QRhiTexture::Flag{QRhiTexture::RenderTarget});
      tex->create();

      m_samplers.push_back({sampler, tex});

      if (cur_pos % 8 != 0)
        cur_pos += 4;

      *(float*)(n.m_materialData.get() + cur_pos) = texSize.width();
      *(float*)(n.m_materialData.get() + cur_pos + 4) = texSize.height();

      cur_pos += 8;
    }
  }

  Pipeline buildPassPipeline(
      Renderer& renderer,
      TextureRenderTarget tgt,
      QRhiBuffer* processUBO)
  {
    return score::gfx::buildPipeline(
        renderer,
        n.mesh(),
        n.m_vertexS,
        n.m_fragmentS,
        tgt,
        processUBO,
        m_materialUBO,
        m_samplers);
  };

  Pass createPass(Renderer& renderer, Sampler target)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto [sampler, tex] = target;

    auto rt = rhi.newTextureRenderTarget({tex});
    auto rp = rt->newCompatibleRenderPassDescriptor();
    SCORE_ASSERT(rp);
    rt->setRenderPassDescriptor(rp);
    SCORE_ASSERT(rt->create());

    QRhiBuffer* pubo{};
    pubo = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
    pubo->create();

    auto pip
        = buildPassPipeline(renderer, TextureRenderTarget{tex, rp, rt}, pubo);
    auto srb = pip.srb;

    // We have to replace the rendered-to texture by an empty one in each pass,
    // as RHI does not support both reading and writing to a texture in the same pass.
    {
      QVarLengthArray<QRhiShaderResourceBinding> bindings;
      for (auto it = srb->cbeginBindings(); it != srb->cendBindings(); ++it)
      {
        bindings.push_back(*it);

        if (it->data()->type == QRhiShaderResourceBinding::SampledTexture)
        {
          if (it->data()->u.stex.texSamplers->tex == tex)
          {
            bindings.back().data()->u.stex.texSamplers->tex
                = renderer.m_emptyTexture;
          }
        }
      }
      srb->setBindings(bindings.begin(), bindings.end());
      srb->create();
    }
    return Pass{sampler, {tex, rp, rt}, pip, pubo};
  }

  void init(Renderer& renderer) override
  {
    // init()
    {
      const auto& mesh = n.mesh();
      if (!m_meshBuffer)
      {
        auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
        m_meshBuffer = mbuffer;
        m_idxBuffer = ibuffer;
      }
    }

    QRhi& rhi = *renderer.state.rhi;

    m_materialSize = n.m_materialSize;
    if (m_materialSize > 0)
    {
      m_materialUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
      SCORE_ASSERT(m_materialUBO->create());
    }

    int cur_pos = initShaderSamplers(renderer);

    initAudioTextures(renderer);

    auto& model_passes = n.m_descriptor.passes;
    if (!model_passes.empty())
    {
      int first_pass_sampler_idx = m_samplers.size();

      // First create all the samplers / textures
      initPassSamplers(renderer, cur_pos);

      // Then create the passes
      for (int i = 0, N = model_passes.size(); i < N - 1; i++)
      {
        auto target = m_samplers[first_pass_sampler_idx + i];
        auto pass = createPass(renderer, target);
        m_passes.push_back(pass);
      }
    }

    // Last pass is the main write
    {
      QRhiBuffer* pubo{};
      pubo = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
      pubo->create();

      auto p = buildPassPipeline(renderer, m_lastPassRT, pubo);
      m_passes.push_back(Pass{nullptr, m_lastPassRT, p, pubo});
    }
  }

  void update(Renderer& renderer, QRhiResourceUpdateBatch& res) override
  {
    {
      if (m_materialUBO && m_materialSize > 0
          && materialChangedIndex != n.materialChanged)
      {
        char* data = n.m_materialData.get();
        res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
        materialChangedIndex = n.materialChanged;
      }
    }

    QRhi& rhi = *renderer.state.rhi;
    for (auto& audio : n.audio_textures)
    {
      bool textureChanged = false;
      auto& [rhiSampler, rhiTexture] = audio.samplers[&renderer];
      const auto curSz = (rhiTexture) ? rhiTexture->pixelSize() : QSize{};
      int numSamples = curSz.width() * curSz.height();
      if (numSamples != audio.data.size())
      {
        delete rhiTexture;
        rhiTexture = nullptr;
        textureChanged = true;
      }

      if (!rhiTexture)
      {
        if (audio.channels > 0)
        {
          int samples = audio.data.size() / audio.channels;
          rhiTexture = rhi.newTexture(
              QRhiTexture::D32F,
              {samples, audio.channels},
              1,
              QRhiTexture::Flag{});
          rhiTexture->create();
          textureChanged = true;
        }
        else
        {
          rhiTexture = nullptr;
          textureChanged = true;
        }
      }

      if (textureChanged)
      {
        score::gfx::replaceTexture(
            *m_p.srb,
            rhiSampler,
            rhiTexture ? rhiTexture : renderer.m_emptyTexture);
      }

      if (rhiTexture)
      {
        QRhiTextureSubresourceUploadDescription subdesc(
            audio.data.data(), audio.data.size() * 4);
        QRhiTextureUploadEntry entry{0, 0, subdesc};
        QRhiTextureUploadDescription desc{entry};
        res.uploadTexture(rhiTexture, desc);
      }
    }

    {
      // Update all the process UBOs
      for (int i = 0, N = m_passes.size(); i < N; i++)
      {
        n.standardUBO.passIndex = i;
        res.updateDynamicBuffer(
            m_passes[i].processUBO,
            0,
            sizeof(ProcessUBO),
            &this->n.standardUBO);
      }
    }
  }

  void releaseWithoutRenderTarget(Renderer& r) override
  {
    // customRelease
    {
      for (auto& texture : n.audio_textures)
      {
        auto it = texture.samplers.find(&r);
        if (it != texture.samplers.end())
        {
          if (auto tex = it->second.second)
          {
            if (tex != r.m_emptyTexture)
              tex->deleteLater();
          }
        }
      }

      for (auto& pass : m_passes)
      {
        // TODO do we also want to remove the last pass texture here ?!
        pass.p.release();
        pass.renderTarget.release();
        pass.processUBO->deleteLater();
      }

      m_passes.clear();
    }

    for (auto sampler : m_samplers)
    {
      delete sampler.sampler;
      // texture isdeleted elsewxheree
    }
    m_samplers.clear();

    delete m_materialUBO;
    m_materialUBO = nullptr;

    m_p.release();

    m_meshBuffer = nullptr;
  }

  void release(Renderer& r) override { releaseWithoutRenderTarget(r); }

  void runPass(
      Renderer& renderer,
      QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch& res) override
  {
    // if(m_passes.empty())
    //   return RenderedNode::runPass(renderer, cb, res);

    // Update a first time everything

    // PASSINDEX must be set to the last index
    // FIXME
    n.standardUBO.passIndex = m_passes.size() - 1;

    update(renderer, res);

    auto updateBatch = &res;

    // Draw the passes
    for (const auto& pass : m_passes)
    {
      SCORE_ASSERT(pass.renderTarget.renderTarget);
      SCORE_ASSERT(pass.p.pipeline);
      SCORE_ASSERT(pass.p.srb);
      // TODO : combine all the uniforms..

      auto rt = pass.renderTarget.renderTarget;
      auto pipeline = pass.p.pipeline;
      auto srb = pass.p.srb;
      auto texture = pass.renderTarget.texture;

      // TODO need to free stuff
      cb.beginPass(rt, Qt::black, {1.0f, 0}, updateBatch);
      {
        cb.setGraphicsPipeline(pipeline);
        cb.setShaderResources(srb);

        if (texture)
        {
          cb.setViewport(QRhiViewport(
              0,
              0,
              texture->pixelSize().width(),
              texture->pixelSize().height()));
        }
        else
        {
          const auto sz = renderer.state.size;
          cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
        }

        assert(this->m_meshBuffer);
        assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
        n.mesh().setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

        cb.draw(n.mesh().vertexCount);
      }

      cb.endPass();

      if (pass.p.pipeline != m_passes.back().p.pipeline)
      {
        // Not the last pass: we have to use another resource batch
        updateBatch = renderer.state.rhi->nextResourceUpdateBatch();
      }
    }
  }
};

score::gfx::NodeRenderer* DepthNode::createRenderer(Renderer& r) const noexcept
{
  return new RenderedDepthNode{*this};
}
