// The depth an ISF writes reaches the next shader's `_depth` companion, read
// back as a value, not as the presence of an attachment:
//
//   * a single pass declaring DEPTH_TEST / DEPTH_WRITE / DEPTH_COMPARE;
//   * a chain A -> B (reads A's depth, writes 1 - d) -> C;
//   * a multipass last pass drawn into the destination (FLOAT, a four-channel
//     FORMAT, FILTER);
//   * a last pass with its own target, copied into the destination with its
//     depth: its own WIDTH / HEIGHT, a single-channel FORMAT, PERSISTENT, as
//     one pass and as the last of two;
//   * OUTPUTS colour + depth, the colour output cabled to the reader;
//   * a raw raster through 0, 1 or 2 depth-forwarding ISF filters;
//   * several depth writers into one port, which depth-test against each other
//     whatever their order, drawn directly, copied, or rasterised.
//
// A shader that writes gl_FragDepth declares its depth state: the ISF quad's
// pipeline has the depth test off otherwise, and Vulkan and OpenGL write no
// depth with the test off.
//
// The hidden "[.perf]" case prints the frame time and the GPU allocations of a
// plain single-pass and a plain multipass ISF, which no depth path may change.
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/RenderedISFNode.hpp>
#include <Gfx/Graph/SimpleRenderedISFNode.hpp>

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstdio>

using namespace score::test::gfx;

namespace
{
constexpr const char* kFullState
    = R"("PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_COMPARE": "always", "DEPTH_WRITE": true },)";
constexpr const char* kLessState
    = R"("PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_COMPARE": "less", "DEPTH_WRITE": true },)";

// r = the input's depth, g / b = the input's green / blue.
constexpr const char* kReader = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image", "IS_DEPTH": true } ]
}*/
void main()
{
  vec2 uv = isf_FragNormCoord;
  float d = IMG_DEPTH_NORM_PIXEL(inputImage, uv);
  vec4 c = IMG_NORM_PIXEL(inputImage, uv);
  gl_FragColor = vec4(d, c.g, c.b, 1.0);
}
)";

// Colour (0.25, 0.5, 0.75), depth = x.
constexpr const char* kWriterBody = R"(
void main()
{
  gl_FragColor = vec4(0.25, 0.5, 0.75, 1.0);
  gl_FragDepth = isf_FragNormCoord.x;
}
)";

QString writeFile(const QTemporaryDir& dir, const char* name, const std::string& text)
{
  const QString path = dir.filePath(name);
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly))
    return {};
  f.write(QByteArray::fromStdString(text));
  return path;
}

// An ISF with no inputs: `header` is inserted in its JSON (ending with a comma).
std::string isfSource(const std::string& header, const std::string& body)
{
  return R"(/*{
  "ISFVSN": "2.0",
  )" + header + R"(
  "INPUTS": []
}*/
)" + body;
}

float u(int x, int w = 64) { return (x + 0.5f) / w; }
int byte(float v) { return int(v * 255.f + 0.5f); }

struct Shot
{
  bool skipped = false;
  std::string error, backend;
  ReadbackImage img;
  bool simple = false;
  int blitted = -1;
};

#define DEPTH_REQUIRE_LIVE(s)          \
  if((s).skipped)                      \
    SKIP("backend unavailable");       \
  CAPTURE((s).backend);                \
  INFO((s).error);                     \
  REQUIRE((s).error.empty());          \
  REQUIRE((s).img.valid())

// The ISF chain `paths` (each image output into the next one's first image
// input), then the sink. Records the renderer kind of the first node.
Shot renderChain(score::gfx::GraphicsApi api, const std::vector<QString>& paths)
{
  Shot out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    std::vector<int> nodes;
    for(const auto& path : paths)
      nodes.push_back(p.addIsf(path));
    const int sink = p.addSink({64, 64});
    for(int n : nodes)
      if(n < 0)
      {
        out.error = "build failed: " + p.error();
        return;
      }
    for(std::size_t i = 0; i + 1 < nodes.size(); i++)
      p.wire(p.imageOut(nodes[i], 0), p.imageIn(nodes[i + 1], 0));
    p.wire(p.imageOut(nodes.back(), 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    out.backend = p.backend();
    p.render(3);
    out.img = p.readback(sink);
    auto* n = p.isf(nodes.front());
    if(n->renderedNodes.empty())
    {
      out.error = "no renderer";
      return;
    }
    auto* r = n->renderedNodes.begin()->second;
    out.simple = dynamic_cast<score::gfx::SimpleRenderedISFNode*>(r) != nullptr;
    if(auto* multi = dynamic_cast<score::gfx::RenderedISFNode*>(r))
      out.blitted = multi->blittedEdges();
  });
  return out;
}

// The reader's red at x = 8, 32, 56 of row 32 is the depth x (+- tol / 255),
// its green the writer's 0.5 unless `colour` is false.
void checkRamp(const ReadbackImage& img, int tol, bool colour = true)
{
  for(int x : {8, 32, 56})
  {
    const auto px = img.at(x, 32);
    CAPTURE(x, int(px[0]), int(px[1]), int(px[2]));
    CHECK(std::abs(int(px[0]) - byte(u(x))) <= tol);
    if(colour)
      CHECK(std::abs(int(px[1]) - 128) <= 2);
  }
}
}

TEST_CASE("a declared depth write reaches the reader", "[gfx][isf][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto shot = renderChain(
      api, {writeFile(dir, "w.fs", isfSource(kFullState, kWriterBody)),
            writeFile(dir, "r.fs", kReader)});
  DEPTH_REQUIRE_LIVE(shot);
  CHECK(shot.simple);
  checkRamp(shot.img, 2);
  CHECK(std::abs(int(shot.img.at(32, 32)[2]) - 191) <= 2);
}

TEST_CASE(
    "a filter reads the depth it is given and writes its own for the next one",
    "[gfx][isf][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  // B: red = A's depth, writes 1 - d. C: red = B's depth, green = B's red.
  const std::string b = std::string(R"(/*{
  "ISFVSN": "2.0",
  )") + kFullState + R"(
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image", "IS_DEPTH": true } ]
}*/
void main()
{
  float d = IMG_DEPTH_NORM_PIXEL(inputImage, isf_FragNormCoord);
  gl_FragColor = vec4(d, 0.0, 0.0, 1.0);
  gl_FragDepth = 1.0 - d;
}
)";
  const std::string c = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image", "IS_DEPTH": true } ]
}*/
void main()
{
  vec2 uv = isf_FragNormCoord;
  gl_FragColor = vec4(
      IMG_DEPTH_NORM_PIXEL(inputImage, uv), IMG_NORM_PIXEL(inputImage, uv).r, 0.0, 1.0);
}
)";
  const auto shot = renderChain(
      api, {writeFile(dir, "a.fs", isfSource(kFullState, kWriterBody)),
            writeFile(dir, "b.fs", b), writeFile(dir, "c.fs", c)});
  DEPTH_REQUIRE_LIVE(shot);
  for(int x : {8, 32, 56})
  {
    const auto px = shot.img.at(x, 32);
    CAPTURE(x, int(px[0]), int(px[1]));
    CHECK(std::abs(int(px[0]) - byte(1.f - u(x))) <= 2);
    CHECK(std::abs(int(px[1]) - byte(u(x))) <= 2);
  }
}

namespace
{
struct LastPassCase
{
  const char* name;
  std::string setting; // the last pass's PASSES entry, without braces
  bool blitted;        // the last pass has its own target
  int tolerance;       // own size: nearest resampling of a 32-wide target
  bool keepsGreen;     // a single-channel target keeps only red
};

std::string lastPassShader(const LastPassCase& c, bool single)
{
  const std::string passes
      = single ? "[ { " + c.setting + " } ]"
               : R"([ { "TARGET": "first" }, { )" + c.setting + " } ]";
  // Pass 0 of the two: white at depth 0.9, which must not be what arrives.
  return isfSource(
      std::string(kFullState) + "\n  \"PASSES\": " + passes + ",",
      single ? std::string{kWriterBody} : std::string{R"(
void main()
{
  if(PASSINDEX == 0)
  {
    gl_FragColor = vec4(1.0);
    gl_FragDepth = 0.9;
  }
  else
  {
    gl_FragColor = vec4(0.25, 0.5, 0.75, 1.0);
    gl_FragDepth = isf_FragNormCoord.x;
  }
}
)"});
}
}

TEST_CASE(
    "the depth of an ISF's last pass reaches the reader, drawn into the "
    "destination or copied from its own target",
    "[gfx][isf][passes][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const LastPassCase c = GENERATE(
      LastPassCase{"FLOAT", R"("FLOAT": true)", false, 2, true},
      LastPassCase{"rgba16f", R"("FORMAT": "rgba16f")", false, 2, true},
      LastPassCase{"NEAREST", R"("FILTER": "NEAREST")", false, 2, true},
      LastPassCase{"own size", R"("WIDTH": "$WIDTH/2", "HEIGHT": "$HEIGHT/2")", true, 5, true},
      LastPassCase{"r16f", R"("FORMAT": "r16f")", true, 2, false},
      LastPassCase{"PERSISTENT", R"("TARGET": "buf", "PERSISTENT": true)", true, 2, true});
  const bool single = GENERATE(true, false);
  CAPTURE(backend_name(api), c.name, single);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto shot = renderChain(
      api, {writeFile(dir, "w.fs", lastPassShader(c, single)),
            writeFile(dir, "r.fs", kReader)});
  DEPTH_REQUIRE_LIVE(shot);
  if(c.blitted)
    CHECK(shot.blitted == 1);
  else if(single)
    CHECK(shot.simple);
  else
    CHECK(shot.blitted == 0);
  checkRamp(shot.img, c.tolerance, c.keepsGreen);
}

TEST_CASE(
    "the depth OUTPUT of an ISF reaches the reader of its colour OUTPUT",
    "[gfx][isf][mrt][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto mrt = isfSource(
      std::string(kFullState) + R"(
  "OUTPUTS": [ { "NAME": "colorOut", "TYPE": "color" }, { "NAME": "depthOut", "TYPE": "depth" } ],)",
      R"(
void main()
{
  colorOut = vec4(0.25, 0.5, 0.75, 1.0);
  depthOut = isf_FragNormCoord.x;
}
)");
  const auto shot = renderChain(
      api, {writeFile(dir, "mrt.fs", mrt), writeFile(dir, "r.fs", kReader)});
  DEPTH_REQUIRE_LIVE(shot);
  checkRamp(shot.img, 2);
}

namespace
{
// A procedural raw raster: a full-screen quad at window depth 0.2 + 0.6 x,
// coloured `colour`, depth-tested `less`.
std::pair<std::string, std::string> rasterQuad(const char* colour)
{
  std::string vs = R"(
void main()
{
  isf_vertShaderInit();
  const vec2 corners[6] = vec2[6](
      vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
      vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
  vec2 c = corners[gl_VertexIndex % 6];
  float d = 0.2 + 0.6 * (c.x * 0.5 + 0.5);
#if !defined(QSHADER_SPIRV) && !defined(QSHADER_HLSL) && !defined(QSHADER_MSL)
  d = d * 2.0 - 1.0;
#endif
  gl_Position = vec4(c, d, 1.0);
  isf_vertShaderFinish();
}
)";
  std::string fs = std::string(R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_WRITE": true, "DEPTH_COMPARE": "less",
                      "CULL_MODE": "none", "VERTEX_COUNT": 6, "TOPOLOGY": "triangles" }
}*/
void main() { isf_FragColor = )") + colour + "; }\n";
  return {vs, fs};
}

// Passes its input's colour and depth on.
constexpr const char* kForwarder = R"(/*{
  "ISFVSN": "2.0",
  "PIPELINE_STATE": { "DEPTH_TEST": true, "DEPTH_COMPARE": "always", "DEPTH_WRITE": true },
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image", "IS_DEPTH": true } ]
}*/
void main()
{
  vec2 uv = isf_FragNormCoord;
  gl_FragColor = IMG_NORM_PIXEL(inputImage, uv);
  gl_FragDepth = IMG_DEPTH_NORM_PIXEL(inputImage, uv);
}
)";
}

TEST_CASE(
    "a raw raster's depth goes through any number of depth-forwarding filters",
    "[gfx][isf][raster][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const int forwarders = GENERATE(0, 1, 2);
  CAPTURE(backend_name(api), forwarders);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto [vs, fs] = rasterQuad("vec4(0.0, 0.5, 1.0, 1.0)");
  const QString rvs = writeFile(dir, "quad.vs", vs);
  const QString rfs = writeFile(dir, "quad.fs", fs);
  const QString fwd = writeFile(dir, "fwd.fs", kForwarder);
  const QString reader = writeFile(dir, "r.fs", kReader);
  Shot shot;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    std::vector<int> chain{p.addRaster(rvs, rfs)};
    for(int i = 0; i < forwarders; i++)
      chain.push_back(p.addIsf(fwd));
    chain.push_back(p.addIsf(reader));
    const int sink = p.addSink({64, 64});
    for(int n : chain)
      if(n < 0)
      {
        shot.error = "build failed: " + p.error();
        return;
      }
    for(std::size_t i = 0; i + 1 < chain.size(); i++)
      p.wire(p.imageOut(chain[i], 0), p.imageIn(chain[i + 1], 0));
    p.wire(p.imageOut(chain.back(), 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      shot.skipped = p.skipped();
      shot.error = shot.skipped ? std::string{} : p.error();
      return;
    }
    shot.backend = p.backend();
    p.render(3);
    shot.img = p.readback(sink);
  });
  DEPTH_REQUIRE_LIVE(shot);
  for(int x : {8, 32, 56})
  {
    const auto px = shot.img.at(x, 32);
    CAPTURE(x, int(px[0]), int(px[1]), int(px[2]));
    CHECK(std::abs(int(px[0]) - byte(0.2f + 0.6f * u(x))) <= 2);
    CHECK(std::abs(int(px[1]) - 128) <= 2);
    CHECK(int(px[2]) >= 253);
  }
}

namespace
{
enum class Producer
{
  Direct,     // a single-pass ISF, drawn into the port's target
  Persistent, // a PERSISTENT pass, copied from its own target
  Raster      // a raw raster
};

const char* producerName(Producer p)
{
  switch(p)
  {
    case Producer::Direct:
      return "direct";
    case Producer::Persistent:
      return "persistent";
    default:
      return "raster";
  }
}
}

// Two producers into one port, compare `less`: the near one wins every pixel.
// "near" is depth 0.2 + 0.6 x, blue when rasterised and red otherwise; "far"
// is depth 0.8 - 0.6 x and green. They cross at x = 0.5.
TEST_CASE(
    "several depth writers into one port depth-test against each other",
    "[gfx][isf][depth][composite]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const Producer nearKind
      = GENERATE(Producer::Direct, Producer::Persistent, Producer::Raster);
  const bool nearFirst = GENERATE(true, false);
  CAPTURE(backend_name(api), producerName(nearKind), nearFirst);
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const std::string nearBody = R"(
void main()
{
  gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
  gl_FragDepth = 0.2 + 0.6 * isf_FragNormCoord.x;
}
)";
  const QString nearIsf = writeFile(
      dir, "near.fs",
      isfSource(
          std::string(kLessState)
              + (nearKind == Producer::Persistent
                     ? R"( "PASSES": [ { "TARGET": "buf", "PERSISTENT": true } ],)"
                     : ""),
          nearBody));
  const auto [rvs, rfs] = rasterQuad("vec4(0.0, 0.0, 1.0, 1.0)");
  // Not near.vs: an ISF takes the .vs of the same name next to it as its
  // vertex shader.
  const QString nearVs = writeFile(dir, "raster.vs", rvs);
  const QString nearRfs = writeFile(dir, "raster.fs", rfs);
  const QString farIsf = writeFile(dir, "far.fs", isfSource(kLessState, R"(
void main()
{
  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
  gl_FragDepth = 0.8 - 0.6 * isf_FragNormCoord.x;
}
)"));
  const QString reader = writeFile(dir, "r.fs", kReader);

  Shot shot;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int nearNode
        = nearKind == Producer::Raster ? p.addRaster(nearVs, nearRfs) : p.addIsf(nearIsf);
    const int farNode = p.addIsf(farIsf);
    const int view = p.addIsf(reader);
    const int sink = p.addSink({64, 64});
    if(nearNode < 0 || farNode < 0 || view < 0)
    {
      shot.error = "build failed: " + p.error();
      return;
    }
    if(nearFirst)
    {
      p.wire(p.imageOut(nearNode, 0), p.imageIn(view, 0));
      p.wire(p.imageOut(farNode, 0), p.imageIn(view, 0));
    }
    else
    {
      p.wire(p.imageOut(farNode, 0), p.imageIn(view, 0));
      p.wire(p.imageOut(nearNode, 0), p.imageIn(view, 0));
    }
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      shot.skipped = p.skipped();
      shot.error = shot.skipped ? std::string{} : p.error();
      return;
    }
    shot.backend = p.backend();
    p.render(3);
    shot.img = p.readback(sink);
  });
  DEPTH_REQUIRE_LIVE(shot);
  for(int x : {8, 20, 44, 56})
  {
    const auto px = shot.img.at(x, 32);
    CAPTURE(x, int(px[0]), int(px[1]), int(px[2]));
    const bool nearWins = u(x) < 0.5f;
    const float d = nearWins ? 0.2f + 0.6f * u(x) : 0.8f - 0.6f * u(x);
    CHECK(std::abs(int(px[0]) - byte(d)) <= 2);
    // The reader's green is the input's green, its blue the input's blue.
    if(nearWins)
    {
      CHECK(int(px[1]) <= 2);
      CHECK(int(px[2]) == (nearKind == Producer::Raster ? 255 : 0));
    }
    else
    {
      CHECK(int(px[1]) >= 253);
      CHECK(int(px[2]) <= 2);
    }
  }
}

// Not a pass / fail case: run with "[.perf]" before and after a change of the
// depth paths. A shader that writes no depth must keep the same allocations
// and the same frame time.
TEST_CASE("frame time and allocations of shaders that write no depth", "[.perf][depth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  struct Case
  {
    const char* name;
    std::string source;
  };
  const Case c = GENERATE(
      Case{"one pass", isfSource("", R"(
void main() { gl_FragColor = vec4(isf_FragNormCoord, 0.5, 1.0); }
)")},
      Case{"two passes", isfSource(R"("PASSES": [ { "TARGET": "first" }, {} ],)", R"(
void main()
{
  if(PASSINDEX == 0)
    gl_FragColor = vec4(isf_FragNormCoord, 0.5, 1.0);
  else
    gl_FragColor = IMG_NORM_PIXEL(first, isf_FragNormCoord).bgra;
}
)")},
      Case{"persistent", isfSource(R"("PASSES": [ { "TARGET": "buf", "PERSISTENT": true } ],)", R"(
void main() { gl_FragColor = mix(IMG_NORM_PIXEL(buf, isf_FragNormCoord), vec4(isf_FragNormCoord, 0.5, 1.0), 0.1); }
)")},
      Case{"depth writer, one pass", isfSource(kFullState, kWriterBody)},
      Case{"depth writer, persistent", isfSource(
          std::string(kFullState) + R"( "PASSES": [ { "TARGET": "buf", "PERSISTENT": true } ],)",
          kWriterBody)});
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString fs = writeFile(dir, "perf.fs", c.source);
  const QString passthrough = writeFile(dir, "view.fs", R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "inputImage", "TYPE": "image" } ]
}*/
void main() { gl_FragColor = IMG_THIS_NORM_PIXEL(inputImage); }
)");
  constexpr int frames = 300;
  bool skipped = false;
  std::string err, backend;
  double msPerFrame = 0.;
  QRhiStats after{};
  bool depth = false;
  int blitted = -1;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int node = p.addIsf(fs);
    const int view = p.addIsf(passthrough);
    const int sink = p.addSink({1280, 720});
    if(node < 0 || view < 0)
    {
      err = p.error();
      return;
    }
    p.wire(p.imageOut(node, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    backend = p.backend();
    auto& rhi = *p.sink(sink)->renderState()->rhi;
    p.render(20);
    after = rhi.statistics();
    QElapsedTimer t;
    t.start();
    p.render(frames);
    msPerFrame = t.nsecsElapsed() / 1e6 / frames;
    auto rt = p.sink(sink)->renderer()->renderTargetForInputPort(*p.imageIn(view, 0));
    depth = rt.depthTexture || rt.depthRenderBuffer || rt.msDepthTexture;
    auto* n = p.isf(node);
    if(!n->renderedNodes.empty())
      if(auto* multi
         = dynamic_cast<score::gfx::RenderedISFNode*>(n->renderedNodes.begin()->second))
        blitted = multi->blittedEdges();
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO(err);
  REQUIRE(err.empty());
  std::printf(
      "PERF %s | %-26s | %.3f ms/frame | allocs %u | used %.2f MiB | depth "
      "attachment on the reader's port %d | blitted %d\n",
      backend.c_str(), c.name, msPerFrame, unsigned(after.allocCount),
      after.usedBytes / (1024. * 1024.), int(depth), blitted);
  std::fflush(stdout);
  CHECK(true);
}
