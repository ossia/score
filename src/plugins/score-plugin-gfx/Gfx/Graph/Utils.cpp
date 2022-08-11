#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
TextureRenderTarget createRenderTarget(const RenderState& state, QRhiTexture* tex)
{
  TextureRenderTarget ret;
  ret.texture = tex;

  ret.renderBuffer
      = state.rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, tex->pixelSize(), 1);
  ret.renderBuffer->create();

  QRhiColorAttachment color0{ret.texture};

  auto renderTarget = state.rhi->newTextureRenderTarget({color0, ret.renderBuffer});
  SCORE_ASSERT(renderTarget);

  auto renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

TextureRenderTarget createRenderTarget(
    const RenderState& state, QRhiTexture::Format fmt, QSize sz,
    QRhiTexture::Flags flags)
{
  auto texture = state.rhi->newTexture(fmt, sz, 1, QRhiTexture::RenderTarget | flags);
  SCORE_ASSERT(texture->create());
  return createRenderTarget(state, texture);
}

void replaceBuffer(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiBuffer* newBuffer)
{
  // const auto bufType = newBuffer->resourceType();
  for(QRhiShaderResourceBinding& b : tmp)
  {
    if(b.data()->binding == binding)
    {
      switch(b.data()->type)
      {
        case QRhiShaderResourceBinding::Type::UniformBuffer:
          b.data()->u.ubuf.buf = newBuffer;
          break;
        case QRhiShaderResourceBinding::Type::BufferLoad:
        case QRhiShaderResourceBinding::Type::BufferStore:
        case QRhiShaderResourceBinding::Type::BufferLoadStore:
          b.data()->u.sbuf.buf = newBuffer;
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
    if(b.data()->binding == binding)
    {
      if(b.data()->type == QRhiShaderResourceBinding::Type::SampledTexture)
      {
        b.data()->u.stex.texSamplers[0].sampler = newSampler;
      }
    }
  }
}

void replaceTexture(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiTexture* newTexture)
{
  for(QRhiShaderResourceBinding& b : tmp)
  {
    if(b.data()->binding == binding)
    {
      switch(b.data()->type)
      {
        case QRhiShaderResourceBinding::Type::SampledTexture:
          b.data()->u.stex.texSamplers[0].tex = newTexture;
          break;
        case QRhiShaderResourceBinding::Type::ImageLoad:
        case QRhiShaderResourceBinding::Type::ImageStore:
        case QRhiShaderResourceBinding::Type::ImageLoadStore:
          b.data()->u.simage.tex = newTexture;
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
    if(b.data()->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      SCORE_ASSERT(b.data()->u.stex.count >= 1);
      if(b.data()->u.stex.texSamplers[0].sampler == oldSampler)
      {
        b.data()->u.stex.texSamplers[0].sampler = newSampler;
      }
#else
      if(b.data()->u.stex.sampler == oldSampler)
      {
        b.data()->u.stex.sampler = newSampler;
      }
#endif
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
    if(b.data()->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      SCORE_ASSERT(b.data()->u.stex.count >= 1);
      if(b.data()->u.stex.texSamplers[0].sampler == sampler)
      {
        b.data()->u.stex.texSamplers[0].tex = newTexture;
      }
#else
      if(b.data()->u.stex.sampler == sampler)
      {
        b.data()->u.stex.tex = newTexture;
      }
#endif
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

    auto& binding = *bindings.back().data();
    if(binding.type == QRhiShaderResourceBinding::SampledTexture)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      if(binding.u.stex.texSamplers[0].tex == old_tex)
      {
        binding.u.stex.texSamplers[0].tex = new_tex;
      }
#else
      if(binding.u.stex.tex == old_tex)
      {
        binding.u.stex.tex = new_tex;
      }
#endif
    }
  }
  srb.setBindings(bindings.begin(), bindings.end());
  srb.create();
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
  ps->setTargetBlends({premulAlphaBlend});

  ps->setSampleCount(1);

  if(mesh.cullMode == QRhiGraphicsPipeline::None)
  {
    ps->setDepthTest(false);
    ps->setDepthWrite(false);
  }
  else
  {
    ps->setDepthTest(true);
    ps->setDepthWrite(true);
  }

  ps->setTopology(mesh.topology);
  ps->setCullMode(mesh.cullMode);
  ps->setFrontFace(mesh.frontFace);

  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vertexS}, {QRhiShaderStage::Fragment, fragmentS}});

  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings(mesh.vertexBindings.begin(), mesh.vertexBindings.end());
  inputLayout.setAttributes(mesh.vertexAttributes.begin(), mesh.vertexAttributes.end());
  ps->setVertexInputLayout(inputLayout);

  ps->setShaderResourceBindings(srb);

  SCORE_ASSERT(rt.renderPass);
  ps->setRenderPassDescriptor(rt.renderPass);

  SCORE_ASSERT(ps->create());
  return {ps, srb};
}

QRhiShaderResourceBindings* createDefaultBindings(
    const RenderList& renderer, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, const std::vector<Sampler>& samplers)
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

  srb->setBindings(bindings.begin(), bindings.end());
  SCORE_ASSERT(srb->create());
  return srb;
}

Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, const std::vector<Sampler>& samplers)
{
  auto bindings = createDefaultBindings(renderer, rt, processUBO, materialUBO, samplers);
  return buildPipeline(renderer, mesh, vertexS, fragmentS, rt, bindings);
}

std::pair<QShader, QShader> makeShaders(const RenderState& v, QString vert, QString frag)
{
  auto [vertexS, vertexError] = ShaderCache::get(v, vert.toUtf8(), QShader::VertexStage);
  if(!vertexError.isEmpty())
    qDebug() << vertexError;

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
    std::vector<Sampler>& samplers)
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

#include <Gfx/Qt5CompatPop> // clang-format: keep

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

QSizeF computeScale(ScaleMode mode, QSizeF viewport, QSizeF texture)
{
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
      return {
          2. * texture.width() / viewport.width(),
          2. * texture.height() / viewport.height()};
    }
    case score::gfx::ScaleMode::Stretch:
    default:
      return {1., 1.};
  }
}

}
