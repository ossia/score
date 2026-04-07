#include "CubemapLoader.hpp"

#include <Gfx/Graph/ShaderCache.hpp>

#include <QFile>
#include <QFileInfo>

#include <cmath>

namespace Threedim
{

// Fullscreen triangle vertex shader
static const constexpr auto equirect_vs = R"_(#version 450

layout(location = 0) out vec2 v_texcoord;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  // Fullscreen triangle
  vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  v_texcoord = pos;
  gl_Position = vec4(pos * 2.0 - 1.0, 0.0, 1.0);
}
)_";

// Fragment shader: sample equirectangular map for a specific cubemap face
// The face index is passed via UBO
static const constexpr auto equirect_fs = R"_(#version 450

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform FaceInfo {
  int faceIndex;
} face;

layout(binding = 1) uniform sampler2D equirectMap;

const float PI = 3.14159265358979323846;

vec3 faceDirection(int faceIdx, vec2 uv)
{
  // Map UV from [0,1] to [-1,1]
  float u = uv.x * 2.0 - 1.0;
  float v = uv.y * 2.0 - 1.0;

  // QRhi cubemap face order: +X, -X, +Y, -Y, +Z, -Z
  switch(faceIdx)
  {
    case 0: return vec3( 1.0,   -v,   -u); // +X
    case 1: return vec3(-1.0,   -v,    u); // -X
    case 2: return vec3(   u,  1.0,    v); // +Y
    case 3: return vec3(   u, -1.0,   -v); // -Y
    case 4: return vec3(   u,   -v,  1.0); // +Z
    case 5: return vec3(  -u,   -v, -1.0); // -Z
    default: return vec3(0.0);
  }
}

void main()
{
  vec3 dir = normalize(faceDirection(face.faceIndex, v_texcoord));

  // Convert direction to equirectangular UV
  float theta = atan(dir.z, dir.x);       // [-PI, PI]
  float phi   = asin(clamp(dir.y, -1.0, 1.0)); // [-PI/2, PI/2]

  vec2 equirectUV;
  equirectUV.x = theta / (2.0 * PI) + 0.5;
  equirectUV.y = phi / PI + 0.5;

  fragColor = texture(equirectMap, equirectUV);
}
)_";

void CubemapLoader::loadImage()
{
  const auto& path = inputs.image.value;
  if(path.empty())
  {
    m_loadedImage = QImage{};
    return;
  }

  QString qpath = QString::fromStdString(path);
  if(!QFileInfo::exists(qpath))
  {
    m_loadedImage = QImage{};
    return;
  }

  QImage img(qpath);
  if(img.isNull())
  {
    m_loadedImage = QImage{};
    return;
  }

  m_loadedImage = img.convertToFormat(QImage::Format_RGBA8888);
}

QImage CubemapLoader::extractFace(int faceIndex) const
{
  if(m_loadedImage.isNull())
    return {};

  const int w = m_loadedImage.width();
  const int h = m_loadedImage.height();
  const CubemapLayout layout = inputs.layout.value;

  // Face order: +X, -X, +Y, -Y, +Z, -Z
  int faceW = 0, faceH = 0;
  int fx = 0, fy = 0;

  switch(layout)
  {
    case CubemapLayout::HorizontalCross: {
      // Layout (4 cols x 3 rows):
      //       [+Y]
      // [-X]  [+Z]  [+X]  [-Z]
      //       [-Y]
      faceW = w / 4;
      faceH = h / 3;
      static const int cross_x[] = {2, 0, 1, 1, 1, 3};
      static const int cross_y[] = {1, 1, 0, 2, 1, 1};
      fx = cross_x[faceIndex] * faceW;
      fy = cross_y[faceIndex] * faceH;
      break;
    }
    case CubemapLayout::VerticalCross: {
      // Layout (3 cols x 4 rows):
      //    [+Y]
      // [-X][+Z][+X]
      //    [-Y]
      //    [-Z]
      faceW = w / 3;
      faceH = h / 4;
      static const int cross_x[] = {2, 0, 1, 1, 1, 1};
      static const int cross_y[] = {1, 1, 0, 2, 1, 3};
      fx = cross_x[faceIndex] * faceW;
      fy = cross_y[faceIndex] * faceH;
      break;
    }
    case CubemapLayout::HorizontalStrip: {
      // 6 faces side by side: +X, -X, +Y, -Y, +Z, -Z
      faceW = w / 6;
      faceH = h;
      fx = faceIndex * faceW;
      fy = 0;
      break;
    }
    case CubemapLayout::VerticalStrip: {
      // 6 faces stacked: +X, -X, +Y, -Y, +Z, -Z
      faceW = w;
      faceH = h / 6;
      fx = 0;
      fy = faceIndex * faceH;
      break;
    }
    default:
      return {};
  }

  return m_loadedImage.copy(fx, fy, faceW, faceH);
}

void CubemapLoader::createCubemapTexture(QRhi& rhi, int faceSize)
{
  releaseCubemapTexture();

  m_faceSize = faceSize;
  m_cubemapTex = rhi.newTexture(
      QRhiTexture::RGBA8, QSize{faceSize, faceSize}, 1,
      QRhiTexture::CubeMap | QRhiTexture::RenderTarget | QRhiTexture::MipMapped
          | QRhiTexture::UsedWithGenerateMips);
  m_cubemapTex->create();

  outputs.cubemap.texture.handle = m_cubemapTex;
}

void CubemapLoader::releaseCubemapTexture()
{
  for(auto& frt : m_faceRTs)
  {
    if(frt.renderTarget)
    {
      frt.renderTarget->deleteLater();
      frt.renderTarget = nullptr;
    }
    if(frt.renderPass)
    {
      frt.renderPass->deleteLater();
      frt.renderPass = nullptr;
    }
  }

  if(m_cubemapTex)
  {
    m_cubemapTex->deleteLater();
    m_cubemapTex = nullptr;
  }
  m_faceSize = 0;
  outputs.cubemap.texture.handle = nullptr;
}

void CubemapLoader::releaseEquirectResources()
{
  if(m_equirectPipeline)
  {
    m_equirectPipeline->deleteLater();
    m_equirectPipeline = nullptr;
  }
  if(m_equirectSrb)
  {
    m_equirectSrb->deleteLater();
    m_equirectSrb = nullptr;
  }
  if(m_equirectUbo)
  {
    m_equirectUbo->deleteLater();
    m_equirectUbo = nullptr;
  }
  if(m_equirectSampler)
  {
    m_equirectSampler->deleteLater();
    m_equirectSampler = nullptr;
  }
  if(m_equirectTex)
  {
    m_equirectTex->deleteLater();
    m_equirectTex = nullptr;
  }
  if(m_quadVbuf)
  {
    m_quadVbuf->deleteLater();
    m_quadVbuf = nullptr;
  }
}

void CubemapLoader::setupEquirectPipeline(score::gfx::RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;

  // UBO for face index
  m_equirectUbo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(int32_t) * 4);
  m_equirectUbo->create();

  // Sampler for equirectangular source
  m_equirectSampler = rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::Repeat, QRhiSampler::ClampToEdge);
  m_equirectSampler->create();

  // SRB
  m_equirectSrb = rhi.newShaderResourceBindings();
  m_equirectSrb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           0,
           QRhiShaderResourceBinding::FragmentStage,
           m_equirectUbo),
       QRhiShaderResourceBinding::sampledTexture(
           1,
           QRhiShaderResourceBinding::FragmentStage,
           m_equirectTex,
           m_equirectSampler)});
  m_equirectSrb->create();

  // Create per-face render targets
  for(int face = 0; face < 6; face++)
  {
    QRhiColorAttachment colorAtt(m_cubemapTex);
    colorAtt.setLayer(face);
    QRhiTextureRenderTargetDescription rtDesc(colorAtt);

    m_faceRTs[face].renderTarget = rhi.newTextureRenderTarget(rtDesc);
    m_faceRTs[face].renderPass
        = m_faceRTs[face].renderTarget->newCompatibleRenderPassDescriptor();
    m_faceRTs[face].renderTarget->setRenderPassDescriptor(
        m_faceRTs[face].renderPass);
    m_faceRTs[face].renderTarget->create();
  }

  // Compile shaders
  auto [vs, vsErr]
      = score::gfx::ShaderCache::get(renderer.state, equirect_vs, QShader::VertexStage);
  auto [fs, fsErr]
      = score::gfx::ShaderCache::get(renderer.state, equirect_fs, QShader::FragmentStage);

  // Pipeline
  m_equirectPipeline = rhi.newGraphicsPipeline();
  m_equirectPipeline->setShaderStages(
      {{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});

  // No vertex input - using gl_VertexIndex for fullscreen triangle
  m_equirectPipeline->setVertexInputLayout({});
  m_equirectPipeline->setShaderResourceBindings(m_equirectSrb);
  m_equirectPipeline->setRenderPassDescriptor(m_faceRTs[0].renderPass);
  m_equirectPipeline->create();
}

void CubemapLoader::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  m_imageChanged = true;
}

void CubemapLoader::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
    score::gfx::Edge* e)
{
  if(!m_imageChanged)
    return;

  loadImage();
}

void CubemapLoader::release(score::gfx::RenderList& r)
{
  releaseEquirectResources();
  releaseCubemapTexture();
}

void CubemapLoader::uploadCrossOrStrip(QRhiResourceUpdateBatch* res)
{
  QVector<QRhiTextureUploadEntry> entries;

  for(int face = 0; face < 6; face++)
  {
    QImage faceImg = extractFace(face);
    if(faceImg.isNull())
      continue;

    if(faceImg.width() != m_faceSize || faceImg.height() != m_faceSize)
      faceImg = faceImg.scaled(
          m_faceSize, m_faceSize, Qt::IgnoreAspectRatio,
          Qt::SmoothTransformation);

    faceImg = faceImg.convertToFormat(QImage::Format_RGBA8888);

    QRhiTextureSubresourceUploadDescription subresDesc(faceImg);
    entries.append(QRhiTextureUploadEntry(face, 0, subresDesc));
  }

  if(!entries.isEmpty())
  {
    QRhiTextureUploadDescription desc;
    desc.setEntries(entries.begin(), entries.end());
    res->uploadTexture(m_cubemapTex, desc);
    res->generateMips(m_cubemapTex);
  }
}

void CubemapLoader::renderEquirectangular(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res)
{
  auto& rhi = *renderer.state.rhi;

  // Upload equirectangular source texture
  if(!m_equirectTex)
  {
    m_equirectTex = rhi.newTexture(
        QRhiTexture::RGBA8,
        QSize{m_loadedImage.width(), m_loadedImage.height()}, 1,
        QRhiTexture::Flag{});
    m_equirectTex->create();
  }
  else if(
      m_equirectTex->pixelSize()
      != QSize{m_loadedImage.width(), m_loadedImage.height()})
  {
    m_equirectTex->deleteLater();
    m_equirectTex = rhi.newTexture(
        QRhiTexture::RGBA8,
        QSize{m_loadedImage.width(), m_loadedImage.height()}, 1,
        QRhiTexture::Flag{});
    m_equirectTex->create();
  }

  res->uploadTexture(m_equirectTex, m_loadedImage);

  // Setup pipeline if needed
  if(!m_equirectPipeline)
  {
    setupEquirectPipeline(renderer);
  }
  else
  {
    // Update SRB if equirect texture changed
    m_equirectSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(
             0,
             QRhiShaderResourceBinding::FragmentStage,
             m_equirectUbo),
         QRhiShaderResourceBinding::sampledTexture(
             1,
             QRhiShaderResourceBinding::FragmentStage,
             m_equirectTex,
             m_equirectSampler)});
    m_equirectSrb->create();
  }

  // Commit the source texture upload before rendering
  commands.resourceUpdate(res);
  res = rhi.nextResourceUpdateBatch();

  // Render each face
  for(int face = 0; face < 6; face++)
  {
    // Update face index UBO
    int32_t faceIdx = face;
    auto* faceRes = rhi.nextResourceUpdateBatch();
    faceRes->updateDynamicBuffer(m_equirectUbo, 0, sizeof(int32_t), &faceIdx);

    commands.beginPass(
        m_faceRTs[face].renderTarget, Qt::black, {1.0f, 0}, faceRes);
    commands.setGraphicsPipeline(m_equirectPipeline);
    commands.setViewport(QRhiViewport(0, 0, m_faceSize, m_faceSize));
    commands.setShaderResources();
    commands.draw(3); // Fullscreen triangle
    commands.endPass();
  }

  // Generate mipmaps
  res->generateMips(m_cubemapTex);
}

void CubemapLoader::runInitialPasses(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
    QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge)
{
  if(!m_imageChanged)
    return;
  m_imageChanged = false;

  if(m_loadedImage.isNull())
    return;

  auto& rhi = *renderer.state.rhi;

  const int faceSize = inputs.resolution.value;
  if(faceSize != m_faceSize || !m_cubemapTex)
  {
    releaseEquirectResources();
    createCubemapTexture(rhi, faceSize);
  }

  const CubemapLayout layout = inputs.layout.value;
  if(layout == CubemapLayout::Equirectangular)
  {
    renderEquirectangular(renderer, commands, res);
  }
  else
  {
    uploadCrossOrStrip(res);
  }
}

}
