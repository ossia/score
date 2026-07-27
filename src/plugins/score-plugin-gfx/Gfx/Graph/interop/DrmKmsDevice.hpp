#pragma once

/**
 * @file DrmKmsDevice.hpp
 * @brief Direct DRM/KMS scanout — output with no compositor in the path.
 *
 * The lowest-latency way to put a frame on a Linux display: no X11, no Wayland,
 * no swapchain. We own the CRTC, hand the kernel a framebuffer built from a
 * DMA-BUF, and drive page flips ourselves. That buys three things a swapchain
 * cannot give us:
 *
 *   - **Flip timing we control.** The vblank event carries the kernel's own
 *     completion timestamp, so a pacing policy can schedule against a real
 *     clock instead of inferring one from when a present call happened to
 *     return. Tearing (immediate) flips are available when latency beats
 *     coherence.
 *   - **Format and modifier control.** The plane's advertised format/modifier
 *     pairs are visible, so a buffer can be allocated in a layout the display
 *     engine scans out directly. A mismatch here is exactly the kind of hidden
 *     blit this whole layer exists to avoid.
 *   - **Hardware composition.** Overlay planes composite in the display engine:
 *     a static backdrop or a second source need not cost a shader pass.
 *
 * Scope: this is the device/resource layer only — enumeration, modeset,
 * framebuffer import, flip submission and vblank events. It holds no GPU API
 * and allocates no buffers; callers bring DMA-BUFs (from GBM, from a Vulkan
 * export, or straight from a capture device).
 *
 * Enumeration works on any opened node. Everything that changes state needs
 * DRM master, which a running compositor already holds — so on a normal
 * desktop only the read-only half will succeed, by design rather than by
 * accident.
 */

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace score::gfx::drm
{

struct ModeInfo
{
  std::string name;
  std::uint32_t width{}, height{};
  std::uint32_t refreshMilliHz{}; ///< 60000 == 60.000 Hz
  std::uint32_t clockKHz{};
  bool interlaced{};
  bool preferred{};
};

struct ConnectorInfo
{
  std::uint32_t id{};
  std::string name; ///< "HDMI-A-1", "DP-2", "Writeback-1"
  bool connected{};
  /// A writeback connector is a capture sink, not a display: it reports
  /// itself connected with a full mode list once master is held, and driving
  /// it as an output is always a mistake.
  bool writeback{};
  std::uint32_t mmWidth{}, mmHeight{};
  std::vector<ModeInfo> modes;
  std::vector<std::uint32_t> possibleCrtcs;
};

/// A format the plane can scan out, with the layouts it accepts for it.
/// A buffer whose modifier is absent here cannot be scanned out directly.
struct PlaneFormat
{
  std::uint32_t fourcc{};
  std::vector<std::uint64_t> modifiers;
};

struct PlaneInfo
{
  std::uint32_t id{};
  enum Type
  {
    Overlay,
    Primary,
    Cursor
  } type{Overlay};
  std::uint32_t possibleCrtcs{}; ///< bitmask of CRTC indices
  std::vector<PlaneFormat> formats;
};

struct CrtcInfo
{
  std::uint32_t id{};
  int index{};
  bool active{};
};

struct DeviceInfo
{
  std::string path;
  std::string driver;
  bool atomic{};          ///< DRM_CLIENT_CAP_ATOMIC accepted
  bool universalPlanes{};      ///< overlay/cursor planes visible
  bool writebackConnectors{};  ///< writeback connectors visible (readback)
  bool hasMaster{};       ///< we hold master (else read-only)
  std::vector<ConnectorInfo> connectors;
  std::vector<CrtcInfo> crtcs;
  std::vector<PlaneInfo> planes;
};

/// A framebuffer the kernel can scan out, built from one or more DMA-BUFs.
struct FramebufferDesc
{
  std::uint32_t width{}, height{};
  std::uint32_t fourcc{};
  std::uint64_t modifier{};
  int fd[4]{-1, -1, -1, -1};
  std::uint32_t stride[4]{};
  std::uint32_t offset[4]{};
  std::uint32_t planeCount{1};
};

/// Result of one completed page flip, taken from the kernel's vblank event.
struct FlipEvent
{
  std::uint64_t sequence{};
  std::uint64_t timestampNs{}; ///< when the flip actually landed
  bool valid{};
};

class SCORE_PLUGIN_GFX_EXPORT KmsDevice
{
public:
  KmsDevice();
  ~KmsDevice();
  KmsDevice(const KmsDevice&) = delete;
  KmsDevice& operator=(const KmsDevice&) = delete;

  /// Opens a card node. Requests atomic + universal planes; both are recorded
  /// in DeviceInfo rather than being required, since the legacy path still
  /// works for plain flips.
  bool open(const std::string& path);
  void close();
  bool isOpen() const noexcept;

  /// Read-only; does not need DRM master.
  const DeviceInfo& info() const noexcept;

  /// Every card node on this system, enumerated read-only.
  static std::vector<DeviceInfo> enumerateDevices();

  /// Take DRM master. Fails while a compositor owns the node; that is the
  /// expected outcome on a desktop session and must degrade, not abort.
  bool acquireMaster();
  void releaseMaster();

  /// A driver-allocated, CPU-mappable scanout buffer. Not a fast path -- it
  /// exists so a display can be brought up and paced without any GPU stack,
  /// which is what makes the output testable against virtual KMS drivers.
  struct DumbBuffer
  {
    std::uint32_t handle{};
    std::uint32_t fbId{};
    std::uint32_t stride{};
    std::uint64_t size{};
    void* map{};
  };
  bool createDumbBuffer(
      std::uint32_t width, std::uint32_t height, std::uint32_t bpp,
      DumbBuffer& out);
  void destroyDumbBuffer(DumbBuffer& b);

  /// Import a DMA-BUF as a scanout framebuffer. Returns 0 on failure.
  /// The modifier must be one the target plane advertises for that fourcc,
  /// or the kernel rejects it — which is the check we want, because the
  /// alternative is a silent detiling copy somewhere else.
  std::uint32_t addFramebuffer(const FramebufferDesc& desc);
  void removeFramebuffer(std::uint32_t fbId);

  /// Light up `connectorId` with one of its modes, scanning out `fbId`.
  bool setMode(std::uint32_t connectorId, const ModeInfo& mode, std::uint32_t fbId);

  /// Queue a flip. With `async`, the kernel flips at the next scanline rather
  /// than the next vblank: lowest latency, at the cost of tearing.
  bool pageFlip(std::uint32_t fbId, bool async = false);

  /// Atomic modeset. Unlike the legacy path this changes CRTC, connector and
  /// plane state in one commit, so the display never shows an intermediate
  /// configuration. Required before any writeback capture.
  bool atomicModeset(
      std::uint32_t connectorId, const ModeInfo& mode, std::uint32_t fbId);

  /// Atomic flip of the primary plane. `async` requests an immediate
  /// (tearing) flip where the driver supports it.
  bool atomicFlip(std::uint32_t fbId, bool async = false);

  /// Attach `fbId` to an overlay plane, positioned at (x,y) with the given
  /// size. This is composition done by the display engine rather than a
  /// shader pass -- a static backdrop or a second source costs nothing on the
  /// GPU. Pass fbId == 0 to detach the plane.
  bool setOverlay(
      std::uint32_t planeId, std::uint32_t fbId, int x, int y,
      std::uint32_t width, std::uint32_t height);

  /// Capture what the display engine actually composited, through a writeback
  /// connector. This is the only way to verify scanout without pointing a
  /// camera at a screen: the kernel composites the current plane state into
  /// `fbId` exactly as it would scan it out. Only virtual and a few real
  /// drivers expose a writeback connector.
  /// `includeCrtcState` also puts the CRTC's ACTIVE/MODE_ID in the request;
  /// `allowModeset` permits the commit to modeset; `testOnly` validates
  /// without applying. The variants exist because a writeback commit that
  /// fails returns a bare EINVAL for several distinct reasons, and trying
  /// them is cheaper than a kernel debug build.
  bool writebackCapture(
      std::uint32_t writebackConnectorId, std::uint32_t fbId,
      bool includeCrtcState = true, bool allowModeset = true,
      bool testOnly = false);

  /// True when the driver accepted atomic and the required properties were
  /// all resolved.
  bool atomicReady() const noexcept;

  /// Block until the queued flip completes, returning the kernel's timestamp.
  /// `timeoutMs` < 0 waits indefinitely.
  FlipEvent waitFlip(int timeoutMs);

  const std::string& lastError() const noexcept;

  struct Impl;

private:
  std::unique_ptr<Impl> d;
};

/// Human-readable DRM format modifier ("LINEAR", "NVIDIA(...)", "INVALID"),
/// for harness output where a raw 64-bit code is unreadable.
SCORE_PLUGIN_GFX_EXPORT std::string modifierName(std::uint64_t modifier);

/// "AR24" etc.
SCORE_PLUGIN_GFX_EXPORT std::string fourccName(std::uint32_t fourcc);

} // namespace score::gfx::drm
