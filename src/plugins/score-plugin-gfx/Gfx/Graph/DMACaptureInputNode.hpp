#pragma once

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

#include <Video/VideoInterface.hpp>

#include <score_plugin_gfx_export.h>

#include <memory>

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
  virtual std::unique_ptr<interop::VideoCaptureStrategy>
  pickStrategy(QRhi::Implementation backend) = 0;

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

  class Renderer;
};

} // namespace score::gfx
