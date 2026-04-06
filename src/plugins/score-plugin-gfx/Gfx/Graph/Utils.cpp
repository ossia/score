#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{
TextureRenderTarget
createRenderTarget(const RenderState& state, QRhiTexture* tex, int samples, bool depth, bool samplableDepth)
{
  TextureRenderTarget ret;
  SCORE_ASSERT(tex);
  ret.texture = tex;

  // The color "tex" is the resolve target — it is always single-sampled.
  //
  // When samplable depth is requested alongside MSAA we need depth resolve:
  // render into a multisample depth attachment, resolve into a single-sample
  // depth texture that downstream shaders can sample. This requires the
  // QRhi::ResolveDepthStencil feature, which is supported on Vulkan 1.2+ and
  // Metal but NOT on D3D11/12. On unsupported backends we degrade the RT to
  // samples=1 — Vulkan/Metal otherwise reject the render pass for mixed
  // sample counts across attachments.
  int effectiveSamples = samples;
  bool useDepthResolve = false;
  if(samplableDepth && samples > 1)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    useDepthResolve = state.rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
    if(!useDepthResolve)
    {
      qWarning() << "createRenderTarget: samplable depth + samples=" << samples
                 << "but QRhi::ResolveDepthStencil is unsupported on this backend"
                 << "— degrading this RT to samples=1.";
      effectiveSamples = 1;
    }
  }

  QRhiTextureRenderTargetDescription desc;
  if(effectiveSamples == 1)
  {
    QRhiColorAttachment color0(tex);
    desc.setColorAttachments({color0});
  }
  else
  {
    ret.colorRenderBuffer = state.rhi->newRenderBuffer(
        QRhiRenderBuffer::Color, tex->pixelSize(), effectiveSamples, {}, tex->format());
    ret.colorRenderBuffer->setName("createRenderTarget::ret.colorRenderBuffer");
    SCORE_ASSERT(ret.colorRenderBuffer->create());

    QRhiColorAttachment color0(ret.colorRenderBuffer);
    color0.setResolveTexture(tex);
    desc.setColorAttachments({color0});
  }
  if(samplableDepth)
  {
    // The single-sample depth texture is what downstream shaders sample.
    ret.depthTexture = state.rhi->newTexture(
        QRhiTexture::D32F, tex->pixelSize(), 1,
        QRhiTexture::RenderTarget);
    ret.depthTexture->setName("createRenderTarget::depthTexture");
    SCORE_ASSERT(ret.depthTexture->create());

    if(useDepthResolve)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
      // Multisample depth attachment used during rendering; resolves into
      // ret.depthTexture at endPass(). Owned via ret.msDepthTexture so it
      // is released alongside the rest of the RT.
      ret.msDepthTexture = state.rhi->newTexture(
          QRhiTexture::D32F, tex->pixelSize(), effectiveSamples,
          QRhiTexture::RenderTarget);
      ret.msDepthTexture->setName("createRenderTarget::msDepthTexture");
      SCORE_ASSERT(ret.msDepthTexture->create());

      desc.setDepthTexture(ret.msDepthTexture);
      desc.setDepthResolveTexture(ret.depthTexture);
#endif
    }
    else
    {
      desc.setDepthTexture(ret.depthTexture);
    }
  }
  else if(depth)
  {
    ret.depthRenderBuffer = state.rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil, tex->pixelSize(), effectiveSamples);
    ret.depthRenderBuffer->setName("createRenderTarget::ret.depthRenderBuffer");
    SCORE_ASSERT(ret.depthRenderBuffer->create());

    desc.setDepthStencilBuffer(ret.depthRenderBuffer);
  }

  auto renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createRenderTarget::renderTarget");
  SCORE_ASSERT(renderTarget);

  auto renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createRenderTarget::renderPass");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

TextureRenderTarget createRenderTarget(
    const RenderState& state, QRhiTexture::Format fmt, QSize sz, int samples, bool depth,
    bool samplableDepth, QRhiTexture::Flags flags)
{
  // FIXME not every RT needs mipmap / generatemips
  auto texture = state.rhi->newTexture(
      fmt, sz, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore | QRhiTexture::MipMapped
          | QRhiTexture::UsedWithGenerateMips | flags);
  texture->setName("createRenderTarget::texture");
  SCORE_ASSERT(texture->create());
  return createRenderTarget(state, texture, samples, depth, samplableDepth);
}

TextureRenderTarget createRenderTarget(
    const RenderState& state,
    std::span<QRhiTexture* const> colorTextures,
    QRhiTexture* depthTex,
    int samples)
{
  TextureRenderTarget ret;
  SCORE_ASSERT(!colorTextures.empty());

  ret.texture = colorTextures[0];
  for(std::size_t i = 1; i < colorTextures.size(); i++)
    ret.additionalColorTextures.push_back(colorTextures[i]);

  // depthTex is the single-sample resolve target supplied by the caller; if
  // MSAA is requested we need depth-resolve support to keep both. Without it
  // (e.g. D3D11/12) all attachments must share a sample count, so degrade
  // this RT to samples=1 — see the matching comment in the non-MRT overload.
  int effectiveSamples = samples;
  bool useDepthResolve = false;
  if(depthTex && samples > 1)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    useDepthResolve = state.rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
    if(!useDepthResolve)
    {
      qWarning() << "createRenderTarget(MRT): samplable depth + samples=" << samples
                 << "but QRhi::ResolveDepthStencil is unsupported on this backend"
                 << "— degrading this RT to samples=1.";
      effectiveSamples = 1;
    }
  }

  QList<QRhiColorAttachment> attachments;
  for(auto* tex : colorTextures)
  {
    if(effectiveSamples == 1)
    {
      attachments.append(QRhiColorAttachment(tex));
    }
    else
    {
      auto* rb = state.rhi->newRenderBuffer(
          QRhiRenderBuffer::Color, tex->pixelSize(), effectiveSamples, {}, tex->format());
      rb->setName("createRenderTarget::MRT::colorRB");
      SCORE_ASSERT(rb->create());

      QRhiColorAttachment att(rb);
      att.setResolveTexture(tex);
      attachments.append(att);
    }
  }

  QRhiTextureRenderTargetDescription desc;
  desc.setColorAttachments(attachments.begin(), attachments.end());

  if(depthTex)
  {
    ret.depthTexture = depthTex;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if(useDepthResolve)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
      // Multisample depth attachment used during rendering, resolves into
      // the caller-supplied depthTex on endPass(). We own msDepthTexture.
      ret.msDepthTexture = state.rhi->newTexture(
          QRhiTexture::D32F, depthTex->pixelSize(), effectiveSamples,
          QRhiTexture::RenderTarget);
      ret.msDepthTexture->setName("createRenderTarget::MRT::msDepthTexture");
      SCORE_ASSERT(ret.msDepthTexture->create());

      desc.setDepthTexture(ret.msDepthTexture);
      desc.setDepthResolveTexture(depthTex);
#endif
    }
    else
    {
      desc.setDepthTexture(depthTex);
    }
#else
    // Qt < 6.6 doesn't support sampleable depth textures in render targets;
    // fall back to a depth renderbuffer (depth won't be sampleable)
    ret.depthRenderBuffer = state.rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil, colorTextures[0]->pixelSize(), effectiveSamples, {},
        QRhiTexture::D32F);
    ret.depthRenderBuffer->setName("createRenderTarget::MRT::depthRB_fallback");
    SCORE_ASSERT(ret.depthRenderBuffer->create());
    desc.setDepthStencilBuffer(ret.depthRenderBuffer);
#endif
  }

  auto renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createRenderTarget::MRT::renderTarget");
  SCORE_ASSERT(renderTarget);

  auto renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createRenderTarget::MRT::renderPass");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

void replaceBuffer(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiBuffer* newBuffer)
{
  // const auto bufType = newBuffer->resourceType();
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->binding == binding)
    {
      switch(d->type)
      {
        case QRhiShaderResourceBinding::Type::UniformBuffer:
          d->u.ubuf.buf = newBuffer;
          break;
        case QRhiShaderResourceBinding::Type::BufferLoad:
        case QRhiShaderResourceBinding::Type::BufferStore:
        case QRhiShaderResourceBinding::Type::BufferLoadStore:
          d->u.sbuf.buf = newBuffer;
          break;
        default:
          break;
      }
    }
  }
}

void replaceSampler(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiSampler* newSampler)
{
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->binding == binding)
    {
      if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
      {
        d->u.stex.texSamplers[0].sampler = newSampler;
      }
    }
  }
}

void replaceTexture(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiTexture* newTexture)
{
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->binding == binding)
    {
      switch(d->type)
      {
        case QRhiShaderResourceBinding::Type::SampledTexture:
          d->u.stex.texSamplers[0].tex = newTexture;
          break;
        case QRhiShaderResourceBinding::Type::ImageLoad:
        case QRhiShaderResourceBinding::Type::ImageStore:
        case QRhiShaderResourceBinding::Type::ImageLoadStore:
          d->u.simage.tex = newTexture;
          break;
        default:
          break;
      }
    }
  }
}

void replaceBuffer(QRhiShaderResourceBindings& srb, int binding, QRhiBuffer* newBuffer)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());

  replaceBuffer(tmp, binding, newBuffer);

  srb.destroy();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.create();
}

void replaceSampler(
    QRhiShaderResourceBindings& srb, int binding, QRhiSampler* newSampler)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());

  replaceSampler(tmp, binding, newSampler);

  srb.destroy();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.create();
}

void replaceTexture(
    QRhiShaderResourceBindings& srb, int binding, QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());

  replaceTexture(tmp, binding, newTexture);

  srb.destroy();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.create();
}

void replaceSampler(
    QRhiShaderResourceBindings& srb, QRhiSampler* oldSampler, QRhiSampler* newSampler)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
      SCORE_ASSERT(d->u.stex.count >= 1);
      if(d->u.stex.texSamplers[0].sampler == oldSampler)
      {
        d->u.stex.texSamplers[0].sampler = newSampler;
      }
    }
  }

  srb.destroy();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.create();
}

void replaceSamplerAndTexture(
    QRhiShaderResourceBindings& srb, QRhiSampler* oldSampler, QRhiSampler* newSampler,
    QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
      SCORE_ASSERT(d->u.stex.count >= 1);
      if(d->u.stex.texSamplers[0].sampler == oldSampler)
      {
        d->u.stex.texSamplers[0].sampler = newSampler;
        d->u.stex.texSamplers[0].tex = newTexture;
      }
    }
  }

  srb.destroy();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.create();
}

void replaceTexture(
    QRhiShaderResourceBindings& srb, QRhiSampler* sampler, QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
      SCORE_ASSERT(d->u.stex.count >= 1);
      if(d->u.stex.texSamplers[0].sampler == sampler)
      {
        d->u.stex.texSamplers[0].tex = newTexture;
      }
    }
  }

  srb.destroy();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.create();
}

void replaceTexture(
    QRhiShaderResourceBindings& srb, QRhiTexture* old_tex, QRhiTexture* new_tex)
{
  QVarLengthArray<QRhiShaderResourceBinding> bindings;
  for(auto it = srb.cbeginBindings(); it != srb.cendBindings(); ++it)
  {
    bindings.push_back(*it);

    auto& binding = bindings.back();

    auto& d = *reinterpret_cast<QRhiShaderResourceBinding::Data*>(&binding);
    if(d.type == QRhiShaderResourceBinding::SampledTexture)
    {
      if(d.u.stex.texSamplers[0].tex == old_tex)
      {
        d.u.stex.texSamplers[0].tex = new_tex;
      }
    }
  }
  srb.destroy();
  srb.setBindings(bindings.begin(), bindings.end());
  srb.create();
}

bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom)
{
  const auto& shader_inputs = vertexShader.description().inputVariables();
  if(shader_inputs.empty())
    return true;

  QVarLengthArray<QRhiVertexInputAttribute> remappedAttrs;

  for(const auto& shader_var : shader_inputs)
  {
    // Resolve shader variable name to semantic
    const std::string_view var_name(shader_var.name.constData(), shader_var.name.size());
    auto sem = ossia::name_to_semantic(var_name);

    // Find matching geometry attribute: by semantic, then custom name, then display name
    const ossia::geometry::attribute* match = nullptr;
    if(sem != ossia::attribute_semantic::custom)
      match = geom.find(sem);
    if(!match)
      match = geom.find(var_name);
    if(!match)
    {
      // Fallback: match shader variable name against attribute display names
      for(const auto& a : geom.attributes)
      {
        if(ossia::geometry::display_name(a) == var_name)
        {
          match = &a;
          break;
        }
      }
    }

    if(!match)
      return false;

    // binding/format/offset from GEOMETRY, location from SHADER
    remappedAttrs.append(QRhiVertexInputAttribute(
        match->binding, shader_var.location,
        static_cast<QRhiVertexInputAttribute::Format>(match->format),
        match->byte_offset));
  }

  // Override vertex input layout, keeping the bindings (stride/classification)
  QRhiVertexInputLayout inputLayout;
  const auto& prevLayout = pip.vertexInputLayout();
  inputLayout.setBindings(prevLayout.cbeginBindings(), prevLayout.cendBindings());
  inputLayout.setAttributes(remappedAttrs.begin(), remappedAttrs.end());
  pip.setVertexInputLayout(inputLayout);
  return true;
}

Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt,
    QRhiShaderResourceBindings* srb)
{
  auto& rhi = *renderer.state.rhi;
  auto ps = rhi.newGraphicsPipeline();
  ps->setName("buildPipeline::ps");
  SCORE_ASSERT(ps);

  QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
  premulAlphaBlend.enable = true;
  premulAlphaBlend.srcColor = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
  premulAlphaBlend.dstColor = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;
  premulAlphaBlend.srcAlpha = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
  premulAlphaBlend.dstAlpha = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;

  // MRT: one blend state per color attachment
  int numColorAttachments = rt.colorAttachmentCount();
  QList<QRhiGraphicsPipeline::TargetBlend> blends;
  for(int i = 0; i < std::max(1, numColorAttachments); i++)
    blends.append(premulAlphaBlend);
  ps->setTargetBlends(blends.begin(), blends.end());

  // Use the render target's actual sample count whenever it can be queried,
  // NOT renderer.samples(). The two can differ when an RT was degraded
  // (e.g. samplable-depth + MSAA without depth-resolve support) — in that
  // case the pipeline must agree with the RT or Vulkan will reject the
  // render pass. When only renderPass is set (e.g. MultiWindowNode passes
  // a placeholder rt that targets a swap chain), sampleCount() returns -1
  // and we have to trust the renderlist value.
  const int rtSamplesQueried = rt.sampleCount();
  const int pipelineSamples = (rtSamplesQueried > 0) ? rtSamplesQueried : renderer.samples();
  if(rtSamplesQueried > 0 && rtSamplesQueried != renderer.samples())
  {
    qWarning() << "buildPipeline: RT sampleCount=" << rtSamplesQueried
               << "differs from renderer.samples()=" << renderer.samples()
               << "— pipeline will use" << pipelineSamples;
  }
  ps->setSampleCount(pipelineSamples);

  mesh.preparePipeline(*ps);

  // Remap vertex inputs by semantic if the mesh provides semantic geometry.
  // This matches shader input variable names to geometry attribute semantics,
  // so that locations are determined by the shader, not by the geometry producer.
  if(auto* geom = mesh.semanticGeometry())
  {
    if(!remapPipelineVertexInputs(*ps, vertexS, *geom))
    {
      qDebug() << "Warning! Shader requires attributes not present in mesh";
      delete ps;
      return {nullptr, srb};
    }
  }

  // FIXME does that check make sense?
  if(!renderer.anyNodeRequiresDepth())
  {
    ps->setDepthTest(false);
    ps->setDepthWrite(false);
  }

  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vertexS}, {QRhiShaderStage::Fragment, fragmentS}});

  ps->setShaderResourceBindings(srb);

  SCORE_ASSERT(rt.renderPass);
  ps->setRenderPassDescriptor(rt.renderPass);

  if(!ps->create())
  {
    qDebug() << "Warning! Pipeline not created";
    delete ps;
    ps = nullptr;
  }
  return {ps, srb};
}

QRhiShaderResourceBindings* createDefaultBindings(
    const RenderList& renderer, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  auto& rhi = *renderer.state.rhi;
  // Shader resource bindings
  auto srb = rhi.newShaderResourceBindings();
  SCORE_ASSERT(srb);

  QVarLengthArray<QRhiShaderResourceBinding, 8> bindings;

  const auto bindingStages = QRhiShaderResourceBinding::VertexStage
                             | QRhiShaderResourceBinding::FragmentStage;

  {
    const auto rendererBinding = QRhiShaderResourceBinding::uniformBuffer(
        0, bindingStages, &renderer.outputUBO());
    bindings.push_back(rendererBinding);
  }

  if(processUBO)
  {
    const auto standardUniformBinding
        = QRhiShaderResourceBinding::uniformBuffer(1, bindingStages, processUBO);
    bindings.push_back(standardUniformBinding);
  }

  // Bind materials
  if(materialUBO)
  {
    const auto materialBinding
        = QRhiShaderResourceBinding::uniformBuffer(2, bindingStages, materialUBO);
    bindings.push_back(materialBinding);
  }

  // Bind samplers
  int binding = 3;
  for(auto sampler : samplers)
  {
    assert(sampler.texture);
    auto actual_texture = sampler.texture;

    // For cases where we do multi-pass rendering, set "this pass"'s input texture
    // to an empty texture instead as we can't output to an input texture
    if(actual_texture == rt.texture)
      actual_texture = &renderer.emptyTexture();

    bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
        binding,
        QRhiShaderResourceBinding::VertexStage
            | QRhiShaderResourceBinding::FragmentStage,
        actual_texture, sampler.sampler));
    binding++;
  }

  for(auto& other : additionalBindings)
  {
    bindings.push_back(other);
  }

  srb->setBindings(bindings.begin(), bindings.end());
  SCORE_ASSERT(srb->create());
  return srb;
}

Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  auto bindings = createDefaultBindings(
      renderer, rt, processUBO, materialUBO, samplers, additionalBindings);
  return buildPipeline(renderer, mesh, vertexS, fragmentS, rt, bindings);
}

std::pair<QShader, QShader> makeShaders(const RenderState& v, QString vert, QString frag)
{
  auto [vertexS, vertexError] = ShaderCache::get(v, vert.toUtf8(), QShader::VertexStage);
  if(!vertexError.isEmpty())
  {
    qDebug() << vertexError;
    qDebug() << vert.toStdString().data();
  }

  auto [fragmentS, fragmentError]
      = ShaderCache::get(v, frag.toUtf8(), QShader::FragmentStage);
  if(!fragmentError.isEmpty())
  {
    qDebug() << fragmentError;
    qDebug() << frag.toStdString().data();
  }

  // qDebug().noquote() << vert.toUtf8().constData();
  if(!vertexS.isValid())
    throw std::runtime_error("invalid vertex shader");
  if(!fragmentS.isValid())
    throw std::runtime_error("invalid fragment shader");

  return {vertexS, fragmentS};
}

// TODO move to ShaderCache
QShader makeCompute(const RenderState& v, QString compute)
{
  auto [computeS, computeError]
      = ShaderCache::get(v, compute.toUtf8(), QShader::ComputeStage);
  if(!computeError.isEmpty())
    qDebug() << computeError;

  if(!computeS.isValid())
    throw std::runtime_error("invalid compute shader");
  return computeS;
}

void DefaultShaderMaterial::init(
    RenderList& renderer, const std::vector<Port*>& input,
    ossia::small_vector<Sampler, 8>& samplers)
{
  auto& rhi = *renderer.state.rhi;

  // Set up shader inputs
  {
    size = 0;
    for(auto in : input)
    {
      switch(in->type)
      {
        case Types::Empty:
          break;
        case Types::Int:
        case Types::Float:
          size += 4;
          break;
        case Types::Vec2:
          size += 8;
          if(size % 8 != 0)
            size += 4;
          break;
        case Types::Vec3:
          while(size % 16 != 0)
          {
            size += 4;
          }
          size += 12;
          break;
        case Types::Vec4:
          while(size % 16 != 0)
          {
            size += 4;
          }
          size += 16;
          break;
        case Types::Image: {
          SCORE_TODO;
          /*
          auto sampler = rhi.newSampler(
              QRhiSampler::Linear,
              QRhiSampler::Linear,
              QRhiSampler::None,
              QRhiSampler::ClampToEdge,
              QRhiSampler::ClampToEdge);
          sampler->setName("DefaultShaderMaterial::sampler");
          SCORE_ASSERT(sampler->create());

          samplers.push_back(
              {sampler, renderer.textureTargetForInputPort(*in)});
*/
          break;
        }
        case Types::Audio:
          break;
        case Types::Geometry:
          break;
        case Types::Camera:
          size += sizeof(ModelCameraUBO);
          break;
      }
    }

    if(size > 0)
    {
      buffer = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
      buffer->setName("DefaultShaderMaterial::buffer");
      SCORE_ASSERT(buffer->create());
    }
  }
}

QSize resizeTextureSize(QSize sz, int min, int max) noexcept
{
  if(sz.width() >= min && sz.height() >= min && sz.width() <= max && sz.height() <= max)
  {
    return sz;
  }
  else
  {
    // To prevent division by zero
    if(sz.width() < 1)
    {
      sz.rwidth() = 1;
    }
    if(sz.height() < 1)
    {
      sz.rheight() = 1;
    }

    // Rescale to max dimension by maintaining aspect ratio
    if(sz.width() > max && sz.height() > max)
    {
      qreal factor = max / qreal(std::max(sz.width(), sz.height()));
      sz.rwidth() *= factor;
      sz.rheight() *= factor;
    }
    else if(sz.width() > max)
    {
      qreal factor = (qreal)max / sz.width();
      sz.rwidth() *= factor;
      sz.rheight() *= factor;
    }
    else if(sz.height() > max)
    {
      qreal factor = (qreal)max / sz.height();
      sz.rwidth() *= factor;
      sz.rheight() *= factor;
    }

    // In case we rescaled below min
    if(sz.width() < min)
    {
      sz.rwidth() = min;
    }
    if(sz.height() < min)
    {
      sz.rheight() = min;
    }
  }
  return sz;
}

QImage resizeTexture(const QImage& img, int min, int max) noexcept
{
  QSize sz = img.size();
  QSize rescaled = resizeTextureSize(sz, min, max);
  if(rescaled == sz || sz.width() == 0 || sz.height() == 0)
    return img;

  return img.scaled(rescaled, Qt::KeepAspectRatio);
}

QSizeF computeScaleForMeshSizing(ScaleMode mode, QSizeF viewport, QSizeF texture)
{
  if(viewport.isEmpty() || texture.isEmpty())
    return QSizeF{1., 1.};
  switch(mode)
  {
    case score::gfx::ScaleMode::BlackBars: {
      const auto new_tex_size
          = viewport.scaled(texture, Qt::AspectRatioMode::KeepAspectRatioByExpanding);
      return {
          texture.width() / new_tex_size.width(),
          texture.height() / new_tex_size.height()};
    }
    case score::gfx::ScaleMode::Fill: {
      double correct_ratio_w = 2. * texture.width() / viewport.width();
      double correct_ratio_h = 2. * texture.height() / viewport.height();
      if(texture.width() >= viewport.width() && texture.height() >= viewport.height())
      {
        double rw = viewport.width() / texture.width();
        double rh = viewport.height() / texture.height();
        double min = std::max(rw, rh) / 2.;

        return {correct_ratio_w * min, correct_ratio_h * min};
      }
      const auto new_tex_size1
          = viewport.scaled(texture, Qt::AspectRatioMode::KeepAspectRatio);
      return {
          texture.width() / new_tex_size1.width(),
          texture.height() / new_tex_size1.height()};
    }
    case score::gfx::ScaleMode::Original: {
      return {texture.width() / viewport.width(), texture.height() / viewport.height()};
    }
    case score::gfx::ScaleMode::Stretch:
    default:
      return {1., 1.};
  }
}

QSizeF
computeScaleForTexcoordSizing(ScaleMode mode, QSizeF renderSize, QSizeF textureSize)
{
  if(renderSize.isEmpty() || textureSize.isEmpty())
    return QSizeF{1., 1.};
  switch(mode)
  {
    // Fits the viewport at the original texture aspect ratio, with black bars around
    case score::gfx::ScaleMode::BlackBars: {
      const auto textureAspect = textureSize.width() / textureSize.height();
      const auto renderAspect = renderSize.width() / renderSize.height();

      if(textureAspect > renderAspect)
        return {1.0, textureAspect / renderAspect};
      else
        return {renderAspect / textureAspect, 1.0};
    }

    // Fits the viewport by stretching
    case score::gfx::ScaleMode::Stretch:
      return {1., 1.};

    // Fits the viewport by filling, maintaining aspect ratio and cropping if necessary
    case score::gfx::ScaleMode::Fill: {
      const auto textureAspect = textureSize.width() / textureSize.height();
      const auto renderAspect = renderSize.width() / renderSize.height();

      if(textureAspect > renderAspect)
        return {renderAspect / textureAspect, 1.0};
      else
        return {1.0, textureAspect / renderAspect};
    }

    case score::gfx::ScaleMode::Original: {
      return {
          renderSize.width() / textureSize.width(),
          renderSize.height() / textureSize.height()};
    }
    default:
      return {};
  }
}

std::vector<Sampler> initInputSamplers(
    const score::gfx::Node& node, RenderList& renderer, const std::vector<Port*>& ports)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;

  int cur_port = 0;
  for(Port* in : ports)
  {
    switch(in->type)
    {
      case Types::Image: {
        if((in->flags & Flag::GrabsFromSource) == Flag::GrabsFromSource)
        {
          // GrabsFromSource: the upstream node owns the texture (e.g. cubemap).
          // We don't create a render target — just grab the texture pointer
          // from the source renderer and create a sampler for it.
          QRhiTexture* srcTex = nullptr;

          for(auto* edge : in->edges)
          {
            if(auto* src_node = edge->source->node)
            {
              if(auto src_it = src_node->renderedNodes.find(&renderer);
                 src_it != src_node->renderedNodes.end())
              {
                if(auto* src_renderer = src_it->second)
                {
                  srcTex = src_renderer->textureForOutput(*edge->source);
                  break;
                }
              }
            }
          }

          if(!srcTex)
            srcTex = &renderer.emptyTexture();

          auto sampler = rhi.newSampler(
              QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
              QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
          sampler->setName("initInputSamplers::cubemap_sampler");
          SCORE_ASSERT(sampler->create());

          samplers.push_back({sampler, srcTex});
        }
        else
        {
          // Look up the pre-created render target from the RenderList
          auto rt = renderer.renderTargetForInputPort(*in);
          auto* texture = rt.texture ? rt.texture : &renderer.emptyTexture();

          auto spec = node.resolveRenderTargetSpecs(cur_port, renderer);
          auto sampler = rhi.newSampler(
              spec.mag_filter, spec.min_filter, spec.mipmap_mode, spec.address_u,
              spec.address_v, spec.address_w);
          sampler->setName("initInputSamplers::sampler");
          SCORE_ASSERT(sampler->create());

          samplers.push_back({sampler, texture});

          // If this port has sampleable depth, add depth sampler
          if((in->flags & Flag::SamplableDepth) == Flag::SamplableDepth)
          {
            auto depthSampler = rhi.newSampler(
                QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            depthSampler->setName("initInputSamplers::depth_sampler");
            SCORE_ASSERT(depthSampler->create());

            auto* depthTex = rt.depthTexture ? rt.depthTexture : &renderer.emptyTexture();
            samplers.push_back({depthSampler, depthTex});
          }
        }
        break;
      }

      default:
        break;
    }
    cur_port++;
  }
  return samplers;
}
}
