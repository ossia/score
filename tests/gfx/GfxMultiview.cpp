// =============================================================================
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_multiview
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_multiview
// =============================================================================

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + file;
}
}

TEST_CASE(
    "MULTIVIEW:2 + graphics uniform_input renders without an SRB collision",
    "[gfx][l3][multiview][binding]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(
        be, {}, corpus("mv-uniform-collision.vs"), corpus("mv-uniform-collision.fs"),
        {64, 64}, 3);
  });

  if(qEnvironmentVariableIsSet("GFX_DUMP") && !r.outputs.empty())
  {
    const auto& o = r.outputs[0];
    const auto c = o.center();
    std::fprintf(
        stderr, "[multiview] be=%s skipped=%d err='%s' %dx%d center=(%d,%d,%d,%d)\n",
        r.backend.c_str(), int(r.skipped), r.error.c_str(), o.width, o.height, c[0],
        c[1], c[2], c[3]);
    std::fflush(stderr);
  }

  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);

  // No crash / no pipeline-creation failure: an SRB collision makes
  // vkCreateGraphicsPipelines fail (missing descriptor) or SIGSEGV on Vulkan.
  // This "builds + renders crash-free" guard holds on every backend.
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto c = img.center();
  INFO("center=(" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ")");

  CHECK(c[0] > 150);            // red present (geometry drawn, descriptor OK)
  CHECK(c[0] > int(c[1]) + 40); // red dominates green (VIEW_INDEX 0 colour)
}

namespace
{
// Procedural MULTIVIEW raster: view i writes R = (i + 1) * 32 from the
// fragment's VIEW_INDEX and G = (i + 1) * 32 from the vertex stage's, B = 1.
// Both stages read VIEW_INDEX as an int, as the presets do.
constexpr const char* kViewColoursVert = R"(void main()
{
  isf_vertShaderInit();
  int view = VIEW_INDEX;
  v_view = float(view);
  vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
  isf_vertShaderFinish();
}
)";

constexpr const char* kViewColoursFrag = R"(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [ { "TYPE": "float", "NAME": "v_view" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "float", "NAME": "v_view" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "views", "TYPE": "color", "FORMAT": "rgba8", "LAYERS": @N@, "WIDTH": 32, "HEIGHT": 32 }
  ],
  "MULTIVIEW": @N@,
  "PIPELINE_STATE": {
    "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none",
    "VERTEX_COUNT": 3, "TOPOLOGY": "triangles"
  }
}*/
void main()
{
  isf_FragColor = vec4(float(VIEW_INDEX + 1) * 32.0 / 255.0,
                       (v_view + 1.0) * 32.0 / 255.0, 1.0, 1.0);
}
)";

// Layer i of the array in the i-th of N vertical strips.
constexpr const char* kStripsFrag = R"(/*{
  "ISFVSN": "2.0",
  "INPUTS": [ { "NAME": "layers", "TYPE": "image", "IS_ARRAY": true } ]
}*/
void main()
{
  vec2 uv = isf_FragNormCoord;
  float n = @N@.0;
  float layer = min(floor(uv.x * n), n - 1.0);
  gl_FragColor = texture(layers, vec3(fract(uv.x * n), uv.y, layer));
}
)";

QString writeWithViews(const QTemporaryDir& dir, const char* name, const char* text, int n)
{
  QByteArray src{text};
  src.replace("@N@", QByteArray::number(n));
  const QString path = dir.filePath(QString::fromUtf8(name));
  QFile f(path);
  if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    return {};
  f.write(src);
  return path;
}
}

TEST_CASE(
    "MULTIVIEW renders each view with its own view index", "[gfx][multiview]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const int views = GENERATE(2, 6);
  CAPTURE(backend_name(be), views);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString vs = writeWithViews(dir, "mv.vs", kViewColoursVert, views);
  const QString fs = writeWithViews(dir, "mv.fs", kViewColoursFrag, views);
  const QString strips = writeWithViews(dir, "strips.fs", kStripsFrag, views);

  bool skipped = false;
  std::string backend, err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addRaster(vs, fs);
    const int view = p.addIsf(strips);
    if(prod < 0 || view < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32 * views, 32});
    p.wire(p.imageOut(prod, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      skipped = p.skipped();
      backend = p.backend();
      err = skipped ? std::string{} : p.error();
      return;
    }
    backend = p.backend();
    p.render(3);
    if(!p.error().empty())
      err = p.error();
    img = p.readback(sink);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("backend=" << backend << " error=" << err);
  REQUIRE(err.empty());
  REQUIRE(img.valid());
  for(int i = 0; i < views; ++i)
  {
    const auto c = img.at(32 * i + 16, 16);
    INFO(
        "view " << i << " = (" << int(c[0]) << "," << int(c[1]) << "," << int(c[2])
                << ")");
    CHECK(std::abs(int(c[0]) - 32 * (i + 1)) <= 2);
    CHECK(std::abs(int(c[1]) - 32 * (i + 1)) <= 2);
    CHECK(int(c[2]) > 250);
  }
}
