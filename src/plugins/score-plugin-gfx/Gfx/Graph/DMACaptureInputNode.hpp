#pragma once

#include <atomic>

/**
 * @file DMACaptureInputNode.hpp
 * @brief Shared base for professional capture-card INPUT nodes.
 *
 * Capture cards (AJA, Blackmagic DeckLink, Bluefish444, Deltacast, and
 * Magewell on its host-staged path) all deliver frames the same way: a vendor
 * capture thread DMAs each frame into a `VideoCaptureStrategy`'s slot, then
 * publishes the slot via a lock-free `VideoCaptureSlotRing`; the render
 * thread samples the strategy's texture through a `GPUVideoDecoder`. This is
 * fundamentally different from the camera/NDI path (CPU `AVFrame` upload, served
 * by `VideoNodeRenderer`) and the Spout/Syphon path (a shared GPU texture
 * handle) — so it gets its own base rather than overloading those.
 *
 * `DMACaptureRenderer` owns all the generic machinery (mesh/UBO setup, decoder
 * init, strategy init with CPU-staging fallback, the zero-copy texture swap,
 * `defaultPassesInit`, the per-frame slot poll + acquire/release bracket, and
 * teardown). The vendor supplies a `DMACaptureBackend`: open the device, report
 * geometry + colour metadata, build the decoder for its wire format, pick the
 * GPU-direct strategy (+ a CPU fallback), and run the capture thread that feeds
 * the slot ring.
 *
 * This is the input-side counterpart to the output-side seams
 * (`CpuStagedVideoOutput` / `VideoOutputStrategy` / `PacedFramePump`). A vendor input
 * addon shrinks to: implement `DMACaptureBackend`, subclass
 * `DMACaptureInputNode`, return the backend from `makeCaptureBackend`.
 */

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/interop/GpuCapabilities.hpp>

#include <Video/VideoInterface.hpp>

#include <score_plugin_gfx_export.h>

#include <functional>
#include <memory>
#include <vector>

namespace score::gfx
{
class GPUVideoDecoder;

namespace interop
{
struct VideoCaptureStrategy;
struct VideoCaptureSlotRing;
}

/**
 * @brief Vendor seam for a DMA capture card's input path.
 *
 * Constructed by the node (via `DMACaptureInputNode::makeCaptureBackend`) bound
 * to the renderer's `VideoCaptureSlotRing`. The backend owns the device
 * handle and the capture thread; the renderer owns the chosen strategy (passed
 * back to the backend by raw pointer via `setStrategy`).
 */
struct SCORE_PLUGIN_GFX_EXPORT DMACaptureBackend
{
  virtual ~DMACaptureBackend();

  /// Open the device + arm capture (channel/routing/AutoCirculate-equivalent).
  /// Return false on failure (the renderer aborts and the device dispatch
  /// falls back to its CPU/AVFrame path).
  virtual bool open() = 0;

  /// Negotiated geometry + total wire-format frame size, valid after open().
  virtual int width() const noexcept = 0;
  virtual int height() const noexcept = 0;
  virtual uint32_t frameByteSize() const noexcept = 0;

  /// VPID/InfoFrame-derived colour metadata for the decoder (colour space,
  /// primaries, transfer, range). The renderer slices this into the
  /// VideoMetadata it hands to `makeDecoder`.
  virtual Video::ImageFormat imageFormat() const = 0;

  /// Build the GPU decoder that unpacks the card's wire bytes into RGBA at
  /// sample time. The input texture is sized to the wire layout by the decoder.
  /// (Non-const ref: the decoder constructors take `Video::ImageFormat&`.)
  virtual std::unique_ptr<GPUVideoDecoder>
  makeDecoder(Video::VideoMetadata&) = 0;

  /// Pick the GPU-direct capture strategy for the active QRhi backend. May
  /// return nullptr (no GPU-direct path for this backend) — the renderer then
  /// uses `makeCpuStrategy`.
  ///
  /// @p caps carries the probed GPU identity so a picker can decline a
  /// vendor-specific rung the live GPU cannot run — offering it anyway costs a
  /// failed init and, worse, makes the strategy advise the user to reconfigure
  /// for a path that is unreachable on their hardware.
  virtual std::unique_ptr<interop::VideoCaptureStrategy>
  pickStrategy(QRhi::Implementation backend, const interop::GpuCapabilities& caps)
      = 0;

  /// Every GPU-direct rung this backend can offer, best first, as factories.
  ///
  /// Whether a rung works is often only knowable by initialising it — NVIDIA
  /// accepts a V4L2 EXPBUF fd and then refuses to import it; a driver's mmap
  /// may or may not be pinnable host memory. Returning the single best guess
  /// means one failure drops all the way to CPU staging past rungs that would
  /// have worked, so the node walks this list instead.
  ///
  /// Factories rather than built strategies because constructing one can be
  /// exclusive: a V4L2 session can only stream in one buffer mode, so the
  /// second candidate cannot exist until the first has released it.
  ///
  /// The default offers just `pickStrategy`, which is the whole ladder for
  /// backends that have one GPU path.
  virtual std::vector<std::function<std::unique_ptr<interop::VideoCaptureStrategy>()>>
  pickStrategies(
      QRhi::Implementation backend, const interop::GpuCapabilities& caps);

  /// The universal host-staged / CPU-staging fallback strategy. Must be
  /// non-null; works on every backend.
  virtual std::unique_ptr<interop::VideoCaptureStrategy>
  makeCpuStrategy() = 0;

  /// Bind the strategy the renderer settled on (the capture thread DMAs into
  /// its slots). Ownership stays with the renderer.
  virtual void setStrategy(interop::VideoCaptureStrategy*) noexcept = 0;

  /// Start / stop the capture thread that feeds the slot ring.
  virtual void start() = 0;
  virtual void stop() = 0;

  /// True when makeDecoder() returned a decoder only the whole-frame external
  /// image can feed. The decoder has to be chosen before the ladder runs, so
  /// this is how the node learns that the choice it already made depends on a
  /// rung that may yet decline -- in which case the decoder is remade rather
  /// than left sampling a texture nothing uploads to.
  virtual bool decoderNeedsExternalImage() const noexcept { return false; }

  /// Stop asking for the external form: the next makeDecoder() must return one
  /// the staged rungs can feed. Called only when that rung actually declined.
  virtual void dropExternalImageRequest() noexcept { }
};

/**
 * @brief ProcessNode base for DMA capture-card inputs. One Image output port;
 *        its renderer is the generic `DMACaptureRenderer`.
 *
 * Vendor subclass overrides `makeCaptureBackend` to supply the vendor backend
 * bound to the renderer's slot ring.
 */
struct SCORE_PLUGIN_GFX_EXPORT DMACaptureInputNode : ProcessNode
{
  DMACaptureInputNode();
  ~DMACaptureInputNode() override;

  NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  /// Build the vendor capture backend, bound to `ring` (the renderer owns the
  /// ring; the backend's capture thread publishes into it).
  virtual std::unique_ptr<DMACaptureBackend>
  makeCaptureBackend(interop::VideoCaptureSlotRing& ring) const = 0;

  /// Which rung of the capture ladder actually engaged, or nullptr before the
  /// renderer has resolved it.
  const char* engagedCaptureStrategy() const noexcept
  {
    return m_engagedStrategy.load(std::memory_order_acquire);
  }

  /// True when SCORE_GFX_CAPTURE_STRATEGY asked for a rung that did not engage.
  /// A harness seeing this must fail the cell rather than report its numbers.
  bool captureStrategyPinUnmet() const noexcept
  {
    return m_pinUnmet.load(std::memory_order_acquire);
  }

  /// Set by the renderer once the ladder resolves (render thread).
  void setEngagedCaptureStrategy(const char* name, bool pinUnmet) const noexcept
  {
    m_engagedStrategy.store(name, std::memory_order_release);
    m_pinUnmet.store(pinUnmet, std::memory_order_release);
  }

  /// Frames the capture thread has published into the slot ring, which is the
  /// device's own cadence — independent of how fast the render side consumes
  /// them.
  std::uint64_t capturedFrameCount() const noexcept
  {
    return m_capturedFrames.load(std::memory_order_acquire);
  }

  /// Set by the renderer each frame from the ring's latest id (render thread).
  void setCapturedFrameCount(std::uint64_t n) const noexcept
  {
    m_capturedFrames.store(n, std::memory_order_release);
  }

  class Renderer;

private:
  mutable std::atomic<const char*> m_engagedStrategy{nullptr};
  mutable std::atomic<bool> m_pinUnmet{false};
  mutable std::atomic<std::uint64_t> m_capturedFrames{0};

public:
};

} // namespace score::gfx
