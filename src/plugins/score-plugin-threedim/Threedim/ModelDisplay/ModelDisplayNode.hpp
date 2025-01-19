#pragma once

#include <Gfx/Graph/Node.hpp>

#include <QFont>
#include <QPen>

// clang-format off
#if defined(_MSC_VER)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#if !defined(UNICODE)
#define UNICODE 1
#endif
#if !defined(_UNICODE)
#define _UNICODE 1
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <inaddr.h>
#include <in6addr.h>
#include <mswsock.h>
#endif

#if defined(near)
#undef near
#undef far
#endif
// clang-format on

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

  int texture_projection{};
  int draw_mode{};
  int camera_mode{};
};

}
