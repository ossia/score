#include "WindowCaptureNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QDebug>

#if defined(__linux__)
#include <unistd.h>
#endif

#if defined(__linux__)
#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN
#include <vulkan/vulkan.h>
#if defined(VK_EXT_image_drm_format_modifier) && defined(VK_KHR_external_memory_fd) \
    && QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <Gfx/Graph/interop/DMABufImport.hpp>
#include <Gfx/Graph/interop/EglDmaBufImport.hpp>

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
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

  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
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

    // BGRA8 covers Windows / macOS / X11 backends. PipeWire on Wayland may
    // negotiate SPA_VIDEO_FORMAT_RGBA / RGBx (mapped to CapturedFrame::CPU_RGBA)
    // — we recreate the texture in QRhiTexture::RGBA8 the first time a CPU_RGBA
    // frame arrives. Without that branch, RGBA bytes were uploaded as BGRA and
    // displayed with R/B swapped.
    m_textureFormat = QRhiTexture::BGRA8;
    m_texture = rhi.newTexture(
        m_textureFormat, QSize{m_width, m_height}, 1, QRhiTexture::Flag{});
    m_texture->create();

    m_sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_sampler->create();

#if HAS_DMABUF_IMPORT
    // Initialize DMA-BUF importer for whichever backend matches the
    // live QRhi. Vulkan is preferred (broader format support); EGL
    // is the OpenGL fallback on Wayland and EGL-X11 setups.
    if(score::gfx::DMABufPlaneImporter::isAvailable(rhi))
    {
      m_dmaBufImporter.emplace();
      m_dmaBufImporter->init(rhi);
    }
    else if(score::gfx::EglDmaBufImporter::isAvailable(rhi))
    {
      m_eglDmaBufImporter.emplace();
      if(m_eglDmaBufImporter->init(rhi))
      {
        // Pre-create a persistent GL texture id; the EGL importer
        // re-targets its storage via glEGLImageTargetTexture2DOES
        // each frame, so the GL id is stable and we createFrom once.
        if(auto* ctx = QOpenGLContext::currentContext())
        {
          if(auto* funcs = ctx->extraFunctions())
          {
            funcs->glGenTextures(1, &m_eglGlTexture);
            constexpr unsigned int GL_TEXTURE_2D_v = 0x0DE1;
            constexpr unsigned int GL_LINEAR_v = 0x2601;
            constexpr unsigned int GL_TEXTURE_MIN_FILTER_v = 0x2801;
            constexpr unsigned int GL_TEXTURE_MAG_FILTER_v = 0x2800;
            constexpr unsigned int GL_TEXTURE_WRAP_S_v = 0x2802;
            constexpr unsigned int GL_TEXTURE_WRAP_T_v = 0x2803;
            constexpr unsigned int GL_CLAMP_TO_EDGE_v = 0x812F;
            funcs->glBindTexture(GL_TEXTURE_2D_v, m_eglGlTexture);
            funcs->glTexParameteri(
                GL_TEXTURE_2D_v, GL_TEXTURE_MIN_FILTER_v, GL_LINEAR_v);
            funcs->glTexParameteri(
                GL_TEXTURE_2D_v, GL_TEXTURE_MAG_FILTER_v, GL_LINEAR_v);
            funcs->glTexParameteri(
                GL_TEXTURE_2D_v, GL_TEXTURE_WRAP_S_v, GL_CLAMP_TO_EDGE_v);
            funcs->glTexParameteri(
                GL_TEXTURE_2D_v, GL_TEXTURE_WRAP_T_v, GL_CLAMP_TO_EDGE_v);
          }
        }
      }
      else
      {
        m_eglDmaBufImporter.reset();
      }
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
      m_vertexS = vertS;
      m_fragmentS = fragS;
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

    m_initialized = true;
  }

  void addOutputPass(
      score::gfx::RenderList& renderer, score::gfx::Edge& edge,
      QRhiResourceUpdateBatch& res) override
  {
    if(!m_vertexS.isValid() || !m_fragmentS.isValid())
      return;

    auto rt = renderer.renderTargetForOutput(edge);
    if(rt.renderTarget)
    {
      const score::gfx::Sampler samplers[] = {{m_sampler, m_texture}};
      auto pip = score::gfx::buildPipeline(
          renderer, renderer.defaultTriangle(), m_vertexS, m_fragmentS, rt,
          m_processUBO, m_materialUBO, samplers);
      if(pip.pipeline)
        m_p.emplace_back(&edge, score::gfx::Pass{rt, pip, nullptr});
    }
  }

  void removeOutputPass(score::gfx::RenderList& renderer, score::gfx::Edge& edge) override
  {
    auto it = ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; });
    if(it != m_p.end())
    {
      it->second.release();
      m_p.erase(it);
    }
  }

  bool hasOutputPassForEdge(score::gfx::Edge& edge) const override
  {
    return ossia::find_if(m_p, [&](const auto& p) { return p.first == &edge; })
           != m_p.end();
  }

  void releaseState(score::gfx::RenderList& r) override
  {
    if(!m_initialized)
      return;

    if(node.backend)
      const_cast<WindowCaptureNode&>(node).backend->stop();

#if HAS_DMABUF_IMPORT
    if(m_dmaBufImporter)
      m_dmaBufImporter->cleanupPlane(m_dmaBufPlane);
    if(m_eglDmaBufImporter)
    {
      m_eglDmaBufImporter->cleanupPlane(m_eglDmaBufPlane);
      if(m_eglGlTexture != 0)
      {
        if(auto* ctx = QOpenGLContext::currentContext())
        {
          if(auto* funcs = ctx->extraFunctions())
            funcs->glDeleteTextures(1, &m_eglGlTexture);
        }
        m_eglGlTexture = 0;
      }
    }
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
    m_vertexS = {};
    m_fragmentS = {};

    m_initialized = false;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);

    for(auto* edge : this->node.output[0]->edges)
      addOutputPass(renderer, *edge, res);
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

    // Detect format mismatch and recreate the texture in the matching format.
    // PipeWire negotiates RGBA/RGBx on some compositors (yields CPU_RGBA);
    // X11 / Windows / macOS yield CPU_BGRA. The two formats can both arrive
    // in a single session if the user changes Wayland compositors mid-session
    // or if the backend renegotiates. Done before the resize check so a
    // simultaneous resize+format change is handled in a single create.
    QRhiTexture::Format wanted = m_textureFormat;
    if(frame.type == CapturedFrame::CPU_RGBA)
      wanted = QRhiTexture::RGBA8;
    else if(frame.type == CapturedFrame::CPU_BGRA)
      wanted = QRhiTexture::BGRA8;
    // Other branches (D3D11_Texture / IOSurface_Ref / DMABUF) recreate the
    // texture below via createFrom(...) on the native handle and don't go
    // through this CPU upload path.

    const bool formatChanged = (wanted != m_textureFormat);
    const bool sizeChanged = (frame.width != m_width || frame.height != m_height);

    if(formatChanged || sizeChanged)
    {
      m_width = frame.width;
      m_height = frame.height;

      // Only the CPU upload paths participate in setPixelSize/setFormat
      // recreation. GPU import paths replace the texture wholesale via
      // createFrom() further down.
      if(frame.type == CapturedFrame::CPU_BGRA || frame.type == CapturedFrame::CPU_RGBA)
      {
        if(formatChanged)
        {
          m_texture->setFormat(wanted);
          m_textureFormat = wanted;
        }
        if(sizeChanged)
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
        if(frame.dmabufFd < 0)
          break;

        // Vulkan path (preferred)
        if(m_dmaBufImporter)
        {
          m_dmaBufImporter->cleanupPlane(m_dmaBufPlane);
          // DRM_FORMAT_XRGB8888 / ARGB8888 → VK_FORMAT_B8G8R8A8_UNORM
          if(m_dmaBufImporter->importPlane(
                 m_dmaBufPlane, frame.dmabufFd, frame.drmModifier,
                 frame.dmabufOffset, frame.dmabufStride,
                 VK_FORMAT_B8G8R8A8_UNORM, frame.width, frame.height))
          {
            m_texture->setPixelSize(QSize{frame.width, frame.height});
            m_texture->createFrom(QRhiTexture::NativeTexture{
                quint64(m_dmaBufPlane.image), VK_IMAGE_LAYOUT_UNDEFINED});
          }
        }
        // EGL path (fallback for QRhi::OpenGLES2 backend)
        else if(m_eglDmaBufImporter && m_eglGlTexture != 0)
        {
          m_eglDmaBufImporter->cleanupPlane(m_eglDmaBufPlane);
          if(m_eglDmaBufImporter->importPlane(
                 m_eglDmaBufPlane, m_eglGlTexture, frame.dmabufFd,
                 frame.drmModifier, frame.dmabufOffset, frame.dmabufStride,
                 frame.drmFormat, frame.width, frame.height))
          {
            m_texture->setPixelSize(QSize{frame.width, frame.height});
            // The GL texture id is stable; createFrom re-binds the
            // QRhiTexture's native pointer to that id. Doing this
            // every frame is cheap because the underlying GL object
            // hasn't changed — only its storage (via the EGLImage
            // re-target inside importPlane).
            m_texture->createFrom(
                QRhiTexture::NativeTexture{quint64(m_eglGlTexture), 0});
          }
        }
#endif
        // The importer dup'd the fd into the VkImage; close the consumer's
        // copy handed over by grab(). DMA_BUF_FD is Linux-only.
#if defined(__linux__)
        if(frame.ownsDmabufFd && frame.dmabufFd >= 0)
          ::close(frame.dmabufFd);
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
    releaseState(r);
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
  QRhiTexture::Format m_textureFormat{QRhiTexture::BGRA8};
  QRhiSampler* m_sampler{};
  QShader m_vertexS;
  QShader m_fragmentS;
  score::gfx::VideoMaterialUBO m_material;

  int m_width{};
  int m_height{};

#if HAS_DMABUF_IMPORT
  std::optional<score::gfx::DMABufPlaneImporter> m_dmaBufImporter;
  score::gfx::DMABufPlaneImporter::PlaneImport m_dmaBufPlane{};
  // GL fallback
  std::optional<score::gfx::EglDmaBufImporter> m_eglDmaBufImporter;
  score::gfx::EglDmaBufImporter::PlaneImport m_eglDmaBufPlane{};
  unsigned int m_eglGlTexture{0};
#endif
};

score::gfx::NodeRenderer*
WindowCaptureNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  return new Renderer{*this};
}

}
