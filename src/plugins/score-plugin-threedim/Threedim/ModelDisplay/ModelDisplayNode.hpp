#pragma once

#include <Gfx/Graph/Node.hpp>

#include <score_plugin_threedim_export.h>

#include <QFont>
#include <QPen>

#include <array>

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
struct SCORE_PLUGIN_THREEDIM_TEST_EXPORT ModelDisplayNode : NodeModel
{
public:
  explicit ModelDisplayNode();
  virtual ~ModelDisplayNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  void process(Message&& msg) override;
  //! A value arriving on input port `port`.
  virtual void processControl(int32_t port, const ossia::value& v);
  void process(int32_t port, const ossia::transform3d& v) override;
  //! The bounds of the geometry, in its own space, when a renderer receives
  //! one it can measure. Returns whether the camera moved.
  virtual bool fitBounds(const ossia::aabb& box) { return false; }
  class Renderer;
  ModelCameraUBO ubo;

  ossia::vec3f position, center;
  //! The direction of the top of the image
  ossia::vec3f up{0.f, 1.f, 0.f};
  float fov{90.f}, near{0.001f}, far{1000.f};

  int texture_projection{};
  int draw_mode{};
  int camera_mode{};
  int blend_color_src{};
  int blend_color_dst{};
  int blend_color_op{};
  int blend_alpha_src{};
  int blend_alpha_dst{};
  int blend_alpha_op{};
  bool blend_enabled{};
  //! What an unconnected texture inlet shows: 0 white, 1 a checker for
  //! texture coordinates, 2 the edges of each triangle (barycentric)
  int fallback_pattern{};
  //! Use the colours of the vertices whenever the geometry has them
  bool prefer_vertex_colors{};
  //! Raised to have the renderers measure the geometry again
  int64_t fit_request{};

  //! Drawn with the model, for viewers
  struct Guides
  {
    //! The background, from the top of the image to its bottom, straight
    //! RGBA; transparent leaves what the target was cleared to
    std::array<float, 4> top{}, bottom{};
    //! A grid on the horizontal plane through `origin`
    bool plane{};
    //! The world axes from (0, 0, 0): X red, Y green, Z blue, dimmer on the
    //! negative side
    bool axes{};
    ossia::vec3f origin{};
    //! Size of a cell of the grid, in world units
    float spacing{1.f};

    bool operator==(const Guides&) const noexcept = default;
    bool any() const noexcept { return top[3] > 0.f || bottom[3] > 0.f || plane || axes; }
  } guides;
};

}
