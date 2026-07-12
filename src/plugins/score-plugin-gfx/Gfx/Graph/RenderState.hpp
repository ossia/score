#pragma once
#include <QOffscreenSurface>
#include <QtGui/private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

#include <functional>

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
using QRhiBufferReadbackResult = QRhiReadbackResult;
#endif

class QOffscreenSurface;
class QWindow;
namespace score::gfx
{
class RenderList;
/**
 * @brief Available graphics APIs to use
 */
enum GraphicsApi
{
  Null,
  OpenGL,
  Vulkan,
  D3D11,
  Metal,
  D3D12
};

class Window;

/**
 * @brief Global state associated to a rendering context.
 */
struct RenderState
{
  RenderState() = default;
  RenderState(const RenderState&) = delete;
  RenderState(RenderState&&) = delete;
  RenderState& operator=(const RenderState&) = delete;
  RenderState& operator=(RenderState&&) = delete;

  QRhi* rhi{};
  QRhiRenderPassDescriptor* renderPassDescriptor{};
  std::weak_ptr<RenderList> renderer{};

  QOffscreenSurface* surface{};
  std::weak_ptr<score::gfx::Window>
      window{}; // Not always set, only used to get mouse events & such.
  QSize renderSize{};
  QSize outputSize{};
  int samples{1};
  QRhiTexture::Format renderFormat{QRhiTexture::RGBA8};
  GraphicsApi api{};
  QShaderVersion version{};

  struct Caps
  {
    // Indirect draw — Qt 6.12+; populated only on compatible builds.
    bool drawIndirect{false};
    bool drawIndirectMulti{false};

    // Always queryable.
    bool multiview{false};
    bool resolveDepthStencil{false};
    bool tessellation{false};
    bool geometryShader{false};

    // Extended capability set. Drives shader feature gating +
    // observability.
    //
    // baseInstance:
    //   Lets indirect draws use `firstInstance` as the draw ID via
    //   `gl_BaseInstance` (ARB_shader_draw_parameters). MDI's per-draw
    //   lookup table reads this way.
    //
    // instanceIndexIncludesBaseInstance:
    //   Disambiguates whether `gl_InstanceIndex` already contains the
    //   `firstInstance` offset (Vulkan-like) or not. Shader prepass
    //   injects a `#define SCORE_INSTANCE_INDEX_INCLUDES_BASE_INSTANCE`
    //   based on this flag so presets work on both paths.
    //
    // variableRateShading:
    //   Per-tile shading-rate maps (VK_EXT_fragment_shading_rate,
    //   D3D12 VRS). Feeds the VRS-opt-in path on fullscreen presets.
    //
    // timestamps:
    //   Whether `QRhiCommandBuffer::lastCompletedGpuTime()` returns
    //   meaningful values. Prereq for the per-pass timing panel.
    //
    // pipelineCacheDataLoadSave:
    //   Backend supports pipeline binary cache round-trip. Used by
    //   tryLoadPipelineCache / tryStorePipelineCache; surfaced so
    //   upper layers can skip PSO prewarm when unsupported.
    //
    // textureViewFormat:
    //   R32UI ↔ R32F aliasing. Needed by the visibility buffer preset
    //   and surfaced early so consumers can feature-detect uniformly.
    //
    // depthClamp:
    //   For reverse-Z shadow passes to avoid near-plane clipping;
    //   shadow_cascades / point_shadow presets opt in when available.
    bool baseInstance{false};
    bool instanceIndexIncludesBaseInstance{false};
    bool variableRateShading{false};
    bool timestamps{false};
    bool pipelineCacheDataLoadSave{false};
    bool textureViewFormat{false};
    bool depthClamp{false};

    void populate(QRhi& rhi);
  } caps;

  // Called after QRhi is destroyed to clean up an imported VkDevice
  std::function<void()> customDeviceCleanup;

  // Called right before the QRhi is destroyed, while its pipeline cache is
  // still accessible. Used to persist QRhi::pipelineCacheData() to disk.
  std::function<void()> preRhiDestroy;

  // Mid-session pipeline-cache flush. Same storage path
  // as preRhiDestroy but callable during normal operation — invoked
  // from RenderList::render after a PSO-compile burst so the cache
  // survives crashes / force-quits without a clean shutdown. Null
  // when the backend doesn't support PipelineCacheDataLoadSave.
  std::function<void()> savePipelineCache;

  void destroy()
  {
    window.reset();

    if(preRhiDestroy)
    {
      preRhiDestroy();
      preRhiDestroy = nullptr;
    }

    delete rhi;
    rhi = nullptr;

    // Destroy imported VkDevice AFTER QRhi (which still references it during shutdown)
    if(customDeviceCleanup)
    {
      customDeviceCleanup();
      customDeviceCleanup = nullptr;
    }

    delete surface;
    surface = nullptr;
  }
};

SCORE_PLUGIN_GFX_EXPORT
std::shared_ptr<RenderState>
createRenderState(GraphicsApi graphicsApi, QSize sz, QWindow* window);

static const constexpr int32_t invalid_node_index = -1;
}
