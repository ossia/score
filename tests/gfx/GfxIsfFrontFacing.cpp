// Culling and gl_FrontFacing of an ISF-mode shader follow OpenGL on every
// backend.
//
// An ISF vertex stage works in OpenGL's clip space and the generated entry
// point maps it to the backend's: clipSpaceCorrMatrix everywhere, plus a Y
// negation on D3D and Metal. That negation reverses window-space winding, so
// the pipeline swaps its FrontFace there; the swap also decides gl_FrontFacing,
// which is why it applies under CULL_MODE none as well.
//
// The ISF quad is wound counter-clockwise in OpenGL's clip space and an ISF
// pipeline's FrontFace is CW unless PIPELINE_STATE FRONT_FACE says otherwise,
// so OpenGL reports the quad as back, or as front under FRONT_FACE ccw. Each
// probe paints green where gl_FrontFacing holds and red elsewhere over the
// black clear, with the default vertex stage, a
// vertex shader that scales the quad (the user main wrapped by the entry
// point), and one that mirrors it in X (reversing its winding, with and
// without the trailing isf_vertShaderFinish call). CULL_MODE back must then
// keep exactly the quads OpenGL calls front and CULL_MODE front the others.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <string>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct Vertex
{
  const char* name;
  const char* source; // nullptr: the default vertex stage
  bool mirrored;
};

constexpr Vertex kVertices[] = {
    {"default", nullptr, false},
    {"scaled", R"(void main()
{
  isf_vertShaderInit();
  gl_Position.xy *= 0.5;
  isf_vertShaderFinish();
}
)",
     false},
    {"mirrored", R"(void main()
{
  isf_vertShaderInit();
  gl_Position.x = -gl_Position.x;
  isf_vertShaderFinish();
}
)",
     true},
    {"mirrored-no-finish", R"(void main()
{
  isf_vertShaderInit();
  gl_Position.x = -gl_Position.x;
}
)",
     true},
};

QByteArray fragment(const std::string& cull, const std::string& frontFace)
{
  QByteArray state = "\"CULL_MODE\": \"" + QByteArray::fromStdString(cull) + "\"";
  if(!frontFace.empty())
    state += ", \"FRONT_FACE\": \"" + QByteArray::fromStdString(frontFace) + "\"";
  return QByteArray(R"(/*{ "ISFVSN": "2.0", "INPUTS": [],
  "PIPELINE_STATE": { )")
         + state + R"( } }*/
void main()
{
  gl_FragColor = gl_FrontFacing ? vec4(0.0, 1.0, 0.0, 1.0) : vec4(1.0, 0.0, 0.0, 1.0);
}
)";
}

bool writeText(const QString& path, const QByteArray& text)
{
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return false;
  return f.write(text) == text.size();
}

enum class Seen
{
  Front,
  Back,
  Culled,
  Other
};

const char* name(Seen s)
{
  switch(s)
  {
    case Seen::Front:
      return "front";
    case Seen::Back:
      return "back";
    case Seen::Culled:
      return "culled";
    default:
      return "other";
  }
}

Seen classify(std::array<uint8_t, 4> px)
{
  if(near(px, {0, 255, 0, 255}, 8))
    return Seen::Front;
  if(near(px, {255, 0, 0, 255}, 8))
    return Seen::Back;
  if(near(px, {0, 0, 0, px[3]}, 8))
    return Seen::Culled;
  return Seen::Other;
}
}

TEST_CASE(
    "ISF-mode culling and gl_FrontFacing agree with OpenGL on every backend",
    "[gfx][isf][cull]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto& vertex = GENERATE(from_range(kVertices));
  const std::string cull = GENERATE(std::string("none"), "back", "front");
  const std::string frontFace = GENERATE(std::string(), "ccw");
  CAPTURE(backend_name(api), vertex.name, cull, frontFace);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = dir.filePath("facing.fs");
  REQUIRE(writeText(fs, fragment(cull, frontFace)));
  if(vertex.source)
    REQUIRE(writeText(dir.filePath("facing.vs"), vertex.source));

  IsfResult r;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(api, {fs}, {32, 32});
  });
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs.back();
  REQUIRE(img.valid());

  const bool front = vertex.mirrored != (frontFace == "ccw");
  Seen expected{};
  if(cull == "none")
    expected = front ? Seen::Front : Seen::Back;
  else if(cull == "back")
    expected = front ? Seen::Front : Seen::Culled;
  else
    expected = front ? Seen::Culled : Seen::Back;

  // The centre is inside the quad for every vertex variant.
  const auto px = img.at(img.width / 2, img.height / 2);
  const Seen seen = classify(px);
  INFO(
      "centre=(" << int(px[0]) << "," << int(px[1]) << "," << int(px[2]) << ","
                 << int(px[3]) << ") seen=" << name(seen)
                 << " expected=" << name(expected));
  CHECK(seen == expected);
}
