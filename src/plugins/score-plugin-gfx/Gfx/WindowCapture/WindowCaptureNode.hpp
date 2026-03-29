#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

namespace Gfx::WindowCapture
{

struct WindowCaptureSettings
{
  QString windowTitle;
  uint64_t windowId{};
  double fps{60.0};
};

struct WindowCaptureNode : score::gfx::ProcessNode
{
  explicit WindowCaptureNode(const WindowCaptureSettings& s);
  ~WindowCaptureNode();

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override;

  WindowCaptureSettings settings;
  std::unique_ptr<WindowCaptureBackend> backend;

  class Renderer;
};

}
