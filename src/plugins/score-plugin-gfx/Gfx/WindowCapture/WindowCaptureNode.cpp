#include "WindowCaptureNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <QDebug>

#if defined(__linux__)
#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN
#if __has_include(<Gfx/Graph/decoders/DMABufImport.hpp>)
#include <Gfx/Graph/decoders/DMABufImport.hpp>
#define HAS_DMABUF_IMPORT 1
#endif
#endif
#endif

#if defined(__APPLE__)
#include <IOSurface/IOSurface.h>
#endif

namespace Gfx::WindowCapture
{

WindowCaptureNode::WindowCaptureNode(const WindowCaptureSettings& s)
    : settings{s}
    , backend{createWindowCaptureBackend()}
{
  output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

WindowCaptureNode::~WindowCaptureNode() { }

class WindowCaptureNode::Renderer : public score::gfx::NodeRenderer
{
public:
  explicit Renderer(const WindowCaptureNode& n)
      : score::gfx::NodeRenderer{n}
      , node{n}
  {
  }

  ~Renderer() { }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;

    const auto& mesh = renderer.defaultTriangle();
    if(m_meshBuffer.buffers.empty())
      m_meshBuffer = renderer.initMeshBuffer(mesh, res);

    m_processUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(score::gfx::ProcessUBO));
    m_processUBO->create();

    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
        sizeof(score::gfx::VideoMaterialUBO));
    m_materialUBO->create();

    // Initial texture — will be resized on first frame
    m_width = 640;
    m_height = 480;

    // Use BGRA8 — native format for all capture backends
    m_texture = rhi.newTexture(
        QRhiTexture::BGRA8, QSize{m_width, m_height}, 1, QRhiTexture::Flag{});
    m_texture->create();

    m_sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

#if HAS_DMABUF_IMPORT
    // Initialize DMA-BUF importer if Vulkan is the backend
    if(score::gfx::DMABufPlaneImporter::isAvailable(rhi))
    {
      m_dmaBufImporter.emplace();
      m_dmaBufImporter->init(rhi);
    }
#endif

    // Fragment shader: BGRA8 texture is already decoded to RGBA by the GPU.
    // Flip Y in texcoord to account for top-down image from capture APIs.
    static constexpr const char* frag = R"_(#version 450

)_" SCORE_GFX_VIDEO_UNIFORMS R"_(

    layout(binding=3) uniform sampler2D y_tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    void main ()
    {
      vec2 flipped = vec2(v_texcoord.x, 1.0 - v_texcoord.y);
      fragColor = texture(y_tex, flipped);
    })_";

    {
      auto [vertS, fragS] = score::gfx::makeShaders(
          renderer.state, score::gfx::GPUVideoDecoder::vertexShader(), frag);

      const score::gfx::Sampler samplers[] = {{m_sampler, m_texture}};
      score::gfx::defaultPassesInit(
          m_p, this->node.output[0]->edges, renderer, mesh, vertS, fragS,
          m_processUBO, m_materialUBO, samplers);
    }

    // Start capturing
    if(node.backend && node.backend->available())
    {
      CaptureTarget target;
      target.mode = node.settings.mode;
      target.windowId = node.settings.windowId;
      target.screenId = node.settings.screenId;
      target.regionX = node.settings.regionX;
      target.regionY = node.settings.regionY;
      target.regionW = node.settings.regionW;
      target.regionH = node.settings.regionH;
      const_cast<WindowCaptureNode&>(node).backend->start(target);
    }
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    if(!node.backend)
      return;

    auto frame = const_cast<WindowCaptureNode&>(node).backend->grab();
    if(frame.type == CapturedFrame::None || frame.width <= 0 || frame.height <= 0)
      return;

    // Handle resize
    if(frame.width != m_width || frame.height != m_height)
    {
      m_width = frame.width;
      m_height = frame.height;

      // Only resize for CPU upload path — GPU paths recreate from native handle
      if(frame.type == CapturedFrame::CPU_BGRA || frame.type == CapturedFrame::CPU_RGBA)
      {
        m_texture->setPixelSize(QSize{m_width, m_height});
        m_texture->create();
      }
    }

    // Upload/import frame data to GPU texture
    switch(frame.type)
    {
      case CapturedFrame::CPU_BGRA:
      case CapturedFrame::CPU_RGBA:
      {
        if(!frame.data)
          break;
        QRhiTextureSubresourceUploadDescription sub(
            frame.data, frame.stride * frame.height);
        sub.setDataStride(frame.stride);
        sub.setSourceSize(QSize{frame.width, frame.height});
        res.uploadTexture(m_texture, QRhiTextureUploadDescription{{0, 0, sub}});
        break;
      }
      case CapturedFrame::D3D11_Texture:
      {
        // Zero-copy D3D11: import native texture from same device
        // The Windows backend copies to a staging texture on its own device,
        // so this only works if QRhi and the backend share the same ID3D11Device.
        // Otherwise the backend returns CPU_BGRA and we use the upload path above.
        if(frame.nativeHandle)
        {
          m_texture->setPixelSize(QSize{frame.width, frame.height});
          m_texture->createFrom(
              QRhiTexture::NativeTexture{quint64(frame.nativeHandle), 0});
        }
        break;
      }
      case CapturedFrame::IOSurface_Ref:
      {
        // Metal zero-copy: create MTLTexture from IOSurface
        // The macOS backend stores the IOSurfaceRef in nativeHandle.
        // For now, the macOS backend returns CPU_BGRA (IOSurfaceLock path).
        // When Metal zero-copy is implemented, this path will be used.
        break;
      }
      case CapturedFrame::DMA_BUF_FD:
      {
#if HAS_DMABUF_IMPORT
        // Vulkan DMA-BUF import for PipeWire frames
        if(m_dmaBufImporter && frame.dmabufFd >= 0)
        {
          // Clean up previous import
          m_dmaBufImporter->cleanupPlane(m_dmaBufPlane);

          // DRM_FORMAT_XRGB8888 / ARGB8888 → VK_FORMAT_B8G8R8A8_UNORM
          if(m_dmaBufImporter->importPlane(
                 m_dmaBufPlane, frame.dmabufFd, frame.drmModifier, frame.dmabufOffset,
                 frame.dmabufStride, VK_FORMAT_B8G8R8A8_UNORM, frame.width,
                 frame.height))
          {
            m_texture->setPixelSize(QSize{frame.width, frame.height});
            m_texture->createFrom(QRhiTexture::NativeTexture{
                quint64(m_dmaBufPlane.image), VK_IMAGE_LAYOUT_UNDEFINED});
          }
        }
#endif
        break;
      }
      default:
        break;
    }

    // Update UBOs
    m_material.textureSize[0] = m_width;
    m_material.textureSize[1] = m_height;
    m_material.scale[0] = 1.f;
    m_material.scale[1] = 1.f;
    res.updateDynamicBuffer(
        m_materialUBO, 0, sizeof(score::gfx::VideoMaterialUBO), &m_material);
  }

  void release(score::gfx::RenderList& r) override
  {
    if(node.backend)
      const_cast<WindowCaptureNode&>(node).backend->stop();

#if HAS_DMABUF_IMPORT
    if(m_dmaBufImporter)
      m_dmaBufImporter->cleanupPlane(m_dmaBufPlane);
#endif

    for(auto& [edge, pass] : m_p)
      pass.release();
    m_p.clear();

    delete m_texture;
    m_texture = nullptr;
    delete m_sampler;
    m_sampler = nullptr;
    delete m_processUBO;
    m_processUBO = nullptr;
    delete m_materialUBO;
    m_materialUBO = nullptr;
    m_meshBuffer = {};
  }

  void runRenderPass(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override
  {
    score::gfx::defaultRenderPass(
        renderer, renderer.defaultTriangle(), m_meshBuffer, cb, edge, m_p);
  }

private:
  const WindowCaptureNode& node;

  score::gfx::PassMap m_p;
  score::gfx::MeshBuffers m_meshBuffer{};
  QRhiBuffer* m_processUBO{};
  QRhiBuffer* m_materialUBO{};
  QRhiTexture* m_texture{};
  QRhiSampler* m_sampler{};
  score::gfx::VideoMaterialUBO m_material;

  int m_width{};
  int m_height{};

#if HAS_DMABUF_IMPORT
  std::optional<score::gfx::DMABufPlaneImporter> m_dmaBufImporter;
  score::gfx::DMABufPlaneImporter::PlaneImport m_dmaBufPlane{};
#endif
};

score::gfx::NodeRenderer*
WindowCaptureNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new Renderer{*this};
}

}
