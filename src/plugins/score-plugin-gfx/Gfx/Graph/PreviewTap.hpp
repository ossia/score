#pragma once
#include <QSize>

#include <atomic>
#include <cstdint>
#include <memory>

struct QRhiReadbackResult;

namespace score::gfx
{
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
  std::shared_ptr<QRhiReadbackResult> readback;

  //! Set when the consumer has taken the latest image: the next frame of the
  //! hosting list is read back, the ones after it are not until the consumer
  //! asks again. A readback costs a pass and, on OpenGL, a synchronous
  //! glReadPixels: the views repaint far less often than the outputs render.
  std::atomic_bool wanted{true};

  //! False while the hub, hosting the tap, cannot read the port (no texture it
  //! could sample): the preview then renders it on an output of its own.
  bool readable{true};
  //! Read back by a render list: only then does it write `readback`.
  bool active{};

  //! Frames read back, for tests.
  int64_t frames{};
};
}
