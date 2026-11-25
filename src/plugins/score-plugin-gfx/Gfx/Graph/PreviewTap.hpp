#pragma once
#include <QSize>

#include <atomic>
#include <cstdint>
#include <memory>

struct QRhiReadbackResult;

namespace score::gfx
{
//! Why a native tap (Gfx::TextureCapture) reads nothing back.
enum class CaptureProblem : uint8_t
{
  None,
  //! No node of the graph at the producer's port index
  NoProducer,
  //! Every capture input of the preview hub is taken
  NoFreeInput,
  //! The producer has published no texture of its own: no frame yet, or it
  //! only ever draws into its consumers' targets
  NoOwnTexture,
  //! The texture is not a single 2D image (an array, a cubemap, a volume)
  NotTexture2D,
  //! The backend cannot render into an RGBA32F target
  FloatUnsupported,
};

/**
 * @brief A preview of what a node outputs, read back by a render list that
 *        already renders the node.
 *
 * The tap hangs on the producer node (Node::previewTaps). A render list of an
 * output of the patch which renders that node, can sample the port and keeps
 * rendering (RenderList::previewHost) draws the port's texture,
 * fitted into `size`, into a small target of its own after the nodes have
 * rendered, and reads it back into `readback`. Nothing of the chain is
 * rendered again: the preview sees the frame the output renders.
 *
 * A node no output of the patch renders is rendered for the previews by their
 * shared output, PreviewHubNode, whose list reads the taps back the same way.
 * See GfxContext::add_preview.
 */
struct PreviewTap
{
  //! Index of the output port of the producer.
  int port{};
  //! The largest the image may be; it keeps the aspect of the texture.
  QSize size{256, 256};
  //! Read back at the texture's own size, as RGBA32F, from the texture the
  //! producer publishes (NodeRenderer::textureForOutput) only: a capture of
  //! the frames for measurement (Gfx::TextureCapture), not a thumbnail. The
  //! producer publishes one while such a tap is on the port
  //! (outputGrabbedBySink).
  bool native{};
  //! Set by the render list that renders the producer, for a native tap.
  std::atomic<CaptureProblem> problem{CaptureProblem::None};
  std::shared_ptr<QRhiReadbackResult> readback;

  //! Set when the consumer has taken the latest image: the next frame of the
  //! hosting list is read back, the ones after it are not until the consumer
  //! asks again. A readback costs a pass and, on OpenGL, a synchronous
  //! glReadPixels: the views repaint far less often than the outputs render.
  std::atomic_bool wanted{true};
  //! The render list a readback is recorded in and not completed yet, 0 when
  //! none: a list that drops it, or another that hosts the tap meanwhile, asks
  //! again.
  std::atomic<uint64_t> pendingIn{};

  //! False while the hub, hosting the tap, cannot read the port (no texture it
  //! could sample): the preview then renders it on an output of its own.
  bool readable{true};
  //! Read back by a render list: only then does it write `readback`.
  bool active{};

  //! Frames read back, for tests.
  int64_t frames{};
};
}
