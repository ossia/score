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

    // Extended capability set, driving shader feature gating and observability.
    //
    // baseInstance: indirect draws can use firstInstance as the draw ID through
    //   gl_BaseInstance (ARB_shader_draw_parameters), which MDI's per-draw lookup
    //   reads.
    // instanceIndexIncludesBaseInstance: whether gl_InstanceIndex already contains
    //   the firstInstance offset. The shader prepass injects
    //   SCORE_INSTANCE_INDEX_INCLUDES_BASE_INSTANCE from this so presets work on
    //   both paths.
    // variableRateShading: per-tile shading-rate maps
    //   (VK_EXT_fragment_shading_rate, D3D12 VRS).
    // timestamps: whether lastCompletedGpuTime() returns meaningful values.
    // pipelineCacheDataLoadSave: pipeline binary cache round-trip, used by
    //   tryLoadPipelineCache / tryStorePipelineCache.
    // textureViewFormat: R32UI <-> R32F aliasing, needed by the visibility buffer
    //   preset.
    // depthClamp: reverse-Z shadow passes avoiding near-plane clipping.
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

/**
 * @brief Who owns the imported Vulkan device behind a RenderState.
 *
 * Owned is the historical behaviour: one vkCreateDevice per RenderState, one
 * vkDestroyDevice when it goes away. Cached takes a reference on the
 * process-wide SharedVulkanDeviceCache instead, so a create/destroy pair is
 * not paid every time the state is rebuilt. Only the shader previews opt in —
 * they are rebuilt on every selection, whereas outputs and encoders are
 * long-lived and must keep their current behaviour.
 */
enum class SharedDeviceMode
{
  Owned,
  Cached
};

SCORE_PLUGIN_GFX_EXPORT
std::shared_ptr<RenderState> createRenderState(
    GraphicsApi graphicsApi, QSize sz, QWindow* window,
    SharedDeviceMode deviceMode = SharedDeviceMode::Owned);

static const constexpr int32_t invalid_node_index = -1;

/**
 * @brief Drop a StorageBuffer usage the backend cannot actually honour.
 *
 * QGles2Buffer::create() picks the target it will run every glBufferData /
 * glBufferSubData through from the usage flags, and StorageBuffer outranks both
 * VertexBuffer and IndirectBuffer (qrhigles2.cpp):
 *
 *     targetForDataOps = GL_ARRAY_BUFFER;
 *     if (usage & IndexBuffer)         targetForDataOps = GL_ELEMENT_ARRAY_BUFFER;
 *     else if (usage & StorageBuffer)  targetForDataOps = GL_SHADER_STORAGE_BUFFER;
 *     else if (usage & IndirectBuffer) targetForDataOps = GL_DRAW_INDIRECT_BUFFER;
 *
 * GL_SHADER_STORAGE_BUFFER arrived with GL 4.3 / GLES 3.1. macOS caps desktop
 * OpenGL at 4.1, so on Apple's driver the bind and every upload against that
 * target raise GL_INVALID_ENUM and do nothing: glGenBuffers still hands out a
 * name, create() still returns true, and the object never receives a data store
 * or a single byte of content. Nothing in score can see that, because the only
 * thing it can check -- create() -- succeeded.
 *
 * The damage lands at draw time and looks like two unrelated bugs:
 *   - bound as a VERTEX buffer, Apple's GL walks the storeless object and
 *     faults inside gleRunVertexSubmitImmediate, EXC_BAD_ACCESS at the
 *     attribute's own byte offset (0x0, 0x40, ...);
 *   - bound as an INDIRECT buffer, the draw reads zeros and rasterises nothing,
 *     silently and with an empty error string.
 * Mesa and the NVIDIA driver expose 4.6, so the same code is correct on
 * Linux/OpenGL, which is what makes this look platform-specific rather than
 * backend-specific.
 *
 * A buffer a shader genuinely reads as an SSBO cannot be rescued here: below
 * 4.3 there are no SSBOs to read. So a StorageBuffer-ONLY usage is returned
 * untouched and fails honestly. This only demotes buffers whose storage role is
 * an ADDITIONAL one alongside a vertex / index / indirect role the backend can
 * still serve -- the mesh arena, the geometry VBOs, the MDI command buffer.
 */
inline QRhiBuffer::UsageFlags
compatibleBufferUsage(QRhi& rhi, QRhiBuffer::UsageFlags usage) noexcept
{
  if(!usage.testFlag(QRhiBuffer::StorageBuffer))
    return usage;
  // Storage-only: there is no other role to fall back to, keep it as asked.
  if(int(usage) == int(QRhiBuffer::StorageBuffer))
    return usage;
  // QRhi::Compute is exactly the GL backend's own SSBO line: caps.compute is
  // set for GL >= 4.3 / GLES >= 3.1 (qrhigles2.cpp), the same versions that
  // introduce GL_SHADER_STORAGE_BUFFER. Ask the capability, do not name a
  // backend: it stays right if a backend gains or loses the ability.
  if(rhi.isFeatureSupported(QRhi::Compute))
    return usage;
  return usage & ~QRhiBuffer::UsageFlags(QRhiBuffer::StorageBuffer);
}

/**
 * @brief Whether MULTIVIEW must be emulated with one pass per view.
 *
 * gl_ViewIndex becomes HLSL SV_ViewID, which requires shader model 6.1. D3D11
 * is pinned to SM 5.0 for good, and D3D12 drops to 5.0 whenever dxcompiler.dll
 * is absent, so on those targets a multiview shader cannot be COMPILED at all:
 *
 *     Vertex shader error: View Index input is only supported in VS and PS
 *     6.1 or higher.
 *
 * ShaderCache answers this by rewriting gl_ViewIndex to the PASSINDEX uniform,
 * which the N-pass path stamps per invocation.
 *
 * That rewrite and the choice of render path MUST be driven by the same
 * predicate. When they disagree the failure is silent and looks like a
 * rasterizer bug: on D3D12 the shader was lowered to read PASSINDEX while
 * QRhi still reported MultiView, so the runtime issued ONE amplified draw in
 * which passIndex never advanced past 0 -- and all six cube faces came back
 * carrying face 0's colour. Ask this function in both places.
 */
inline bool viewIndexNeedsPassIndexFallback(
    GraphicsApi api, const QShaderVersion& version, int multiViewCount) noexcept
{
  // SCORE_GFX_DISABLE_MULTIVIEW means "pretend this backend has no multiview".
  // It also makes this path -- otherwise reachable only on a D3D target --
  // testable everywhere.
  if(qEnvironmentVariableIsSet("SCORE_GFX_DISABLE_MULTIVIEW"))
    return true;

  if(api != GraphicsApi::D3D11 && api != GraphicsApi::D3D12)
    return false;

  // D3D11 is pinned to SM 5.0 for good, so it can never have SV_ViewID.
  if(api == GraphicsApi::D3D11)
    return true;

  // D3D12 without dxcompiler.dll drops to SM 5.0, which has no SV_ViewID
  // either. ossia/sdk 86207a70 ships that runtime, so shipping builds DO
  // reach 6.1 -- but a source build without it still lands here.
  if(version.version() < 61)
    return true;

  // D3D12 ViewInstancing is capped at D3D12_MAX_VIEW_INSTANCE_COUNT == 4.
  // Above that, CreatePipelineState rejects the PSO outright:
  //
  //   Failed to create graphics pipeline state: COM error 0x80070057:
  //   The parameter is incorrect.
  //   Warning! MRT Pipeline not created
  //
  // and with no pipeline nothing draws, so every cube face reads back
  // (0,0,0,255) -- which looks like a rasterizer or copy fault and is neither.
  // Qt encodes the same limit in its QVarLengthArray<D3D12_VIEW_INSTANCE_LOCATION, 4>.
  //
  // This is exactly the split measured on an RTX 3090 with the SDK's DXC
  // installed: test_gfx_multiview declares MULTIVIEW:2 and PASSES natively,
  // while cubemap_six_faces and camera_array_faces declare MULTIVIEW:6 and
  // failed every face. A cubemap is inherently 6 views, so CUBEMAP+MULTIVIEW
  // can never use D3D12 ViewInstancing -- it is not a Qt bug and not a shim
  // bug, it is the API's limit.
  //
  // Keep the fast path where it is legal: 2- and 4-view shaders still get real
  // ViewInstancing. Only what D3D12 cannot express falls back to N passes.
  if(multiViewCount > 4)
    return true;

  return false;
}

}
