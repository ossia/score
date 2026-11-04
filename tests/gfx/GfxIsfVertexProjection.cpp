// An ISF vertex shader works in OpenGL's clip space, as in VVISF:
// isf_vertShaderInit sets gl_Position to the quad in [-1, 1] with z = 0 and
// w = 1, and the shader's own rotation and perspective projection (VIDVOX
// 3d Rotate) act on that. The backend's clip-space correction comes after the
// shader's main, so the projected quad lands on the same pixels on every
// backend, whether or not the shader ends with isf_vertShaderFinish.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>

using namespace score::test::gfx;

namespace
{
using vec4 = std::array<double, 4>;
// Column-major, as GLSL's mat4(col0, col1, col2, col3).
using mat4 = std::array<vec4, 4>;

mat4 operator*(const mat4& a, const mat4& b)
{
  mat4 r{};
  for(int c = 0; c < 4; c++)
    for(int row = 0; row < 4; row++)
      for(int k = 0; k < 4; k++)
        r[c][row] += a[k][row] * b[c][k];
  return r;
}

vec4 operator*(const mat4& a, const vec4& v)
{
  vec4 r{};
  for(int row = 0; row < 4; row++)
    for(int k = 0; k < 4; k++)
      r[row] += a[k][row] * v[k];
  return r;
}

constexpr double pi = 3.14159265359;

// The matrices of corpus/isf-vertex-projection.vs, same parameters.
mat4 projection(double aspect)
{
  const double t = std::tan(45.0 * pi / 180.0), n = 0.5, f = 10.0;
  const mat4 frustum{
      vec4{1.0 / t, 0, 0, 0}, vec4{0, aspect / t, 0, 0},
      vec4{0, 0, (f + n) / (f - n), 1}, vec4{0, 0, -2 * f * n / (f - n), 0}};
  const mat4 translate{
      vec4{1, 0, 0, 0}, vec4{0, 1, 0, 0}, vec4{0, 0, 1, 0}, vec4{0.3, -0.2, 2.0, 1}};
  const auto rx = [](double a) {
    const double c = std::cos(a * pi), s = std::sin(a * pi);
    return mat4{vec4{1, 0, 0, 0}, vec4{0, c, s, 0}, vec4{0, -s, c, 0}, vec4{0, 0, 0, 1}};
  };
  const auto ry = [](double a) {
    const double c = std::cos(a * pi), s = std::sin(a * pi);
    return mat4{vec4{c, 0, s, 0}, vec4{0, 1, 0, 0}, vec4{-s, 0, c, 0}, vec4{0, 0, 0, 1}};
  };
  const auto rz = [](double a) {
    const double c = std::cos(a * pi), s = std::sin(a * pi);
    return mat4{vec4{c, -s, 0, 0}, vec4{s, c, 0, 0}, vec4{0, 0, 1, 0}, vec4{0, 0, 0, 1}};
  };
  const mat4 scale{
      vec4{0.6 * aspect, 0, 0, 0}, vec4{0, 0.6, 0, 0}, vec4{0, 0, 0.6, 0},
      vec4{0, 0, 0, 1}};
  return frustum * translate * rx(-0.15) * ry(0.1) * rz(-0.1) * scale;
}

struct Box
{
  double x0{1e9}, y0{1e9}, x1{-1e9}, y1{-1e9};
  bool empty() const noexcept { return x1 < x0; }
};

// The quad's corners projected by the VVISF math, in readback pixels (origin
// top-left, OpenGL's y = +1 at the top row).
Box expected_box(int w, int h)
{
  const mat4 m = projection(double(w) / h);
  Box b;
  for(auto [x, y] : {std::pair{-1., -1.}, {1., -1.}, {1., 1.}, {-1., 1.}})
  {
    const vec4 c = m * vec4{x, y, 0, 1};
    const double px = (c[0] / c[3] + 1) / 2 * w;
    const double py = (1 - c[1] / c[3]) / 2 * h;
    b.x0 = std::min(b.x0, px);
    b.x1 = std::max(b.x1, px);
    b.y0 = std::min(b.y0, py);
    b.y1 = std::max(b.y1, py);
  }
  return b;
}

// Edges of the covered pixels, in the same units as expected_box.
Box drawn_box(const ReadbackImage& img)
{
  Box b;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
      if(img.at(x, y)[0] > 128)
      {
        b.x0 = std::min(b.x0, double(x));
        b.x1 = std::max(b.x1, double(x + 1));
        b.y0 = std::min(b.y0, double(y));
        b.y1 = std::max(b.y1, double(y + 1));
      }
  return b;
}

std::string describe(const Box& b)
{
  return "[" + std::to_string(b.x0) + ", " + std::to_string(b.x1) + "] x ["
         + std::to_string(b.y0) + ", " + std::to_string(b.y1) + "]";
}

bool within(const Box& a, const Box& b, double tol)
{
  return std::abs(a.x0 - b.x0) <= tol && std::abs(a.x1 - b.x1) <= tol
         && std::abs(a.y0 - b.y0) <= tol && std::abs(a.y1 - b.y1) <= tol;
}
}

TEST_CASE(
    "ISF vertex shader projection lands on the same pixels on every backend",
    "[gfx][isf]")
{
  const auto shader = GENERATE(
      std::string("isf-vertex-projection"), std::string("isf-vertex-projection-vvisf"));
  constexpr int w = 80, h = 60;
  const Box expected = expected_box(w, h);

  std::map<std::string, Box> drawn;
  for(auto be : platform_backends())
  {
    IsfResult r;
    score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
      r = render_isf_chain(
          be, {QString::fromStdString(GFX_TEST_CORPUS_DIR "/" + shader + ".fs")}, {w, h});
    });
    if(r.skipped)
    {
      UNSCOPED_INFO(r.backend << " skipped: " << r.skip_reason);
      continue;
    }
    INFO("shader=" << shader << " backend=" << r.backend << " error=" << r.error);
    REQUIRE(r.error.empty());
    REQUIRE(!r.outputs.empty());
    const auto& img = r.outputs.back();
    REQUIRE(img.valid());
    REQUIRE(img.width == w);
    REQUIRE(img.height == h);

    const Box b = drawn_box(img);
    INFO("drawn " << describe(b) << ", expected " << describe(expected));
    REQUIRE_FALSE(b.empty());
    // Coverage is decided at pixel centres, so a covered edge is within one
    // pixel of the exact projected one.
    CHECK(within(b, expected, 1.0));
    drawn[r.backend] = b;
  }
  if(drawn.empty())
    SKIP("no backend could render");

  const auto& [first_name, first] = *drawn.begin();
  for(const auto& [name, b] : drawn)
  {
    INFO(name << " " << describe(b) << " vs " << first_name << " " << describe(first));
    CHECK(within(b, first, 1.0));
  }
}

