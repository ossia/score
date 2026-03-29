#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/WindowCapture/WindowCaptureBackend.hpp>

namespace Gfx::WindowCapture
{

struct WindowCaptureSettings
{
  CaptureMode mode{CaptureMode::Window};
  QString windowTitle;
  uint64_t windowId{};
  uint64_t screenId{};
  QString screenName;
  int regionX{}, regionY{}, regionW{}, regionH{};
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
