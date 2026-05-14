#include "CubemapLoader.hpp"

#include <Gfx/Graph/ShaderCache.hpp>

#include <QDebug>

#include <cmath>

namespace Threedim
{

// Fullscreen triangle vertex shader. Applies clipSpaceCorrMatrix + the
// non-GL conditional Y-flip — matches the engine-wide ossia convention
// (see isf.cpp's vertexInitFunc). Guarantees v_texcoord.y=1 is the top
// of the rendered face across GL / Vulkan / Metal / D3D.
static const constexpr auto equirect_vs = R"_(#version 450

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 RENDERSIZE;
} renderer;

layout(location = 0) out vec2 v_texcoord;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  // Fullscreen triangle
  vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  v_texcoord = pos;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(pos * 2.0 - 1.0, 0.0, 1.0);
#if defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = -gl_Position.y;
#endif
}
)_";

// Fragment shader: sample equirectangular map for a specific cubemap face.
// renderer_t (binding 0) matches the engine convention; FaceInfo moves to
// binding 2 so it doesn't collide.
static const constexpr auto equirect_fs = R"_(#version 450

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 RENDERSIZE;
} renderer;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 2) uniform FaceInfo {
  int faceIndex;
} face;

layout(binding = 3) uniform sampler2D equirectMap;

const float PI = 3.14159265358979323846;

// Face direction — v_texcoord.y=1 is the TOP of the rendered face
// (after the vertex stage's clipSpaceCorrMatrix + non-GL flip). This
// maps to sampled UV.y=0 in QRhi's top-left-origin UV, which per cube
// spec corresponds to cube-spec t=-1 → direction biased toward +Y.
// Hence the signs on `v` (flipped vs. the legacy raw-NDC form).
vec3 faceDirection(int faceIdx, vec2 uv)
{
  // Map UV from [0,1] to [-1,1]
  float u = uv.x * 2.0 - 1.0;
  float v = uv.y * 2.0 - 1.0;

  // QRhi cubemap face order: +X, -X, +Y, -Y, +Z, -Z
  switch(faceIdx)
  {
    case 0: return vec3( 1.0,    v,   -u); // +X
    case 1: return vec3(-1.0,    v,    u); // -X
    case 2: return vec3(   u,  1.0,   -v); // +Y
    case 3: return vec3(   u, -1.0,    v); // -Y
    case 4: return vec3(   u,    v,  1.0); // +Z
    case 5: return vec3(  -u,    v, -1.0); // -Z
    default: return vec3(0.0);
  }
}

void main()
{
  vec3 dir = normalize(faceDirection(face.faceIndex, v_texcoord));

  // Convert direction to equirectangular UV.
  // Longitude: atan2(z, x) ∈ [-π, π] → u ∈ [0, 1].
  // Latitude:  asin(y)    ∈ [-π/2, π/2].
  //
  // Y flip: QRhi normalizes texture sampling to top-left-origin UV
  // (UV.y = 0 at the top of the stored image — uniform across
  // backends, see qrhi.cpp + QRhi::isYUpInFramebuffer). QImage
  // uploads via uploadTexture(QImage) land scanline 0 at the
  // texture's UV.y = 0, so sky (image top) is at UV.y = 0 and
  // ground (image bottom) at UV.y = 1. The raw formula
  // `v = phi/π + 0.5` would put sky at UV.y = 1 — wrong. Flip.
  //
  // LearnOpenGL uses the unflipped formula and works because GL's
  // bottom-left-origin UV cancels the inversion — QRhi's top-left
  // convention doesn't cancel it, so we flip explicitly.
  //
  // (Cube-face rendering side: this shader, like the rest of the
  // IBL / test-cube shader family, writes raw NDC without
  // clipSpaceCorrMatrix. That choice is backend-specific — the
  // face-direction convention in `faceDirection()` above matches
  // what Vulkan / Metal / D3D store after rasterization. Under
  // OpenGL the whole cube content ends up vertically flipped —
  // normalising that would require either applying
  // clipSpaceCorrMatrix across every shader in the family OR
  // conditionally flipping v_texcoord by isYUpInFramebuffer.
  // Out of scope for this edit.)
  float theta = atan(dir.z, dir.x);
  float phi   = asin(clamp(dir.y, -1.0, 1.0));

  vec2 equirectUV;
  equirectUV.x = theta / (2.0 * PI) + 0.5;
  equirectUV.y = 0.5 - phi / PI;

  fragColor = texture(equirectMap, equirectUV);
}
)_";

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

  // Publish the cube on the Scene outlet too: one shared_ptr-stable
  // scene_state whose environment.skybox_texture.native_handle points at
  // our QRhiTexture. Version bumps only when the handle actually changes
  // so merge_scenes / ScenePreprocessor short-circuit unchanged frames.
  if(!m_sceneState)
    m_sceneState = std::make_shared<ossia::scene_state>();
  if(m_lastPublishedHandle != m_cubemapTex)
  {
    m_sceneState->environment = {};  // only skybox_texture is ours to touch
    m_sceneState->environment.skybox_texture.native_handle = m_cubemapTex;
    m_lastPublishedHandle = m_cubemapTex;
    m_sceneVersion++;
    m_sceneState->version = m_sceneVersion;
    outputs.scene_out.scene.state = m_sceneState;
    outputs.scene_out.dirty = ossia::scene_port::dirty_environment;
  }
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

  // Clear the scene outlet too: downstream merge_scenes will stop
  // contributing a skybox_texture from us once the handle goes null.
  if(m_sceneState)
  {
    m_sceneState->environment = {};
    m_lastPublishedHandle = nullptr;
    m_sceneVersion++;
    m_sceneState->version = m_sceneVersion;
    outputs.scene_out.dirty = ossia::scene_port::dirty_environment;
  }
}

void CubemapLoader::releaseEquirectResources(score::gfx::RenderList* renderer)
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
    if(renderer)
      renderer->releaseBuffer(m_equirectUbo);
    else
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
    if(renderer)
      renderer->releaseBuffer(m_quadVbuf);
    else
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
  m_equirectUbo->setName("CubemapLoader::equirect_ubo");
  m_equirectUbo->create();

  // Sampler for equirectangular source
  m_equirectSampler = rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::Repeat, QRhiSampler::ClampToEdge);
  m_equirectSampler->create();

  // SRB — matches the new shader layout:
  //   binding 0: renderer_t (shared engine UBO with clipSpaceCorrMatrix)
  //   binding 2: FaceInfo (our per-face index)
  //   binding 3: equirectangular source sampler
  // Binding 1 is reserved for the engine's process_t UBO convention
  // (not used here, but skipped to avoid future collisions).
  m_equirectSrb = rhi.newShaderResourceBindings();
  m_equirectSrb->setBindings(
      {QRhiShaderResourceBinding::uniformBuffer(
           0,
           QRhiShaderResourceBinding::VertexStage
               | QRhiShaderResourceBinding::FragmentStage,
           &renderer.outputUBO()),
       QRhiShaderResourceBinding::uniformBuffer(
           2,
           QRhiShaderResourceBinding::FragmentStage,
           m_equirectUbo),
       QRhiShaderResourceBinding::sampledTexture(
           3,
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
  // No-op on the render thread. The decode runs on the halp file-port
  // worker (see image_t::process in CubemapLoader.hpp) which delivers
  // the decoded QImage to m_loadedImage and sets m_imageChanged.
  // runInitialPasses() picks that up and uploads + transcodes the cube.
}

void CubemapLoader::release(score::gfx::RenderList& r)
{
  releaseEquirectResources(&r);
  releaseCubemapTexture();
}

CubemapLoader::~CubemapLoader()
{
  // Safety net — idempotent. releaseEquirectResources() and
  // releaseCubemapTexture() null each pointer after deleteLater(), so
  // calling them again is a no-op if the framework already ran
  // release(RenderList&).
  if(m_cubemapTex || m_equirectTex)
  {
    qDebug() << "[BUFTRACE] ~CubemapLoader FALLBACK this=" << (void*)this
             << " m_cubemapTex=" << (void*)m_cubemapTex
             << " m_equirectTex=" << (void*)m_equirectTex
             << " (release(RenderList&) was never called — leaked textures)";
  }
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
    // Update SRB if equirect texture changed. Mirror the slot layout
    // from setupEquirectPipeline: binding 0 = engine renderer_t,
    // binding 2 = FaceInfo, binding 3 = equirect sampler.
    m_equirectSrb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(
             0,
             QRhiShaderResourceBinding::VertexStage
                 | QRhiShaderResourceBinding::FragmentStage,
             &renderer.outputUBO()),
         QRhiShaderResourceBinding::uniformBuffer(
             2,
             QRhiShaderResourceBinding::FragmentStage,
             m_equirectUbo),
         QRhiShaderResourceBinding::sampledTexture(
             3,
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
