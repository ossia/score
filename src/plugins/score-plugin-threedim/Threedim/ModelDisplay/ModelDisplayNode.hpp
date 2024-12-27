#pragma once

#include <Gfx/Graph/Node.hpp>

#include <QFont>
#include <QPen>

#if defined(near)
#undef near
#undef far
#endif

namespace score::gfx
{
/**
 * @brief A node that renders a model to screen.
 */
struct ModelDisplayNode : NodeModel
{
public:
  explicit ModelDisplayNode();
  virtual ~ModelDisplayNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(Message&& msg) override;
  class Renderer;
  ModelCameraUBO ubo;

  ossia::vec3f position, center;
  float fov{90.f}, near{0.001f}, far{10000.f};

  int wantedProjection{};
  int draw_mode{};
};

}
