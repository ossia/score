// =============================================================================
// What shaders read from premultiplied targets, and how their output is
// composited.
//
//   * IMG_THIS_PIXEL / IMG_THIS_NORM_PIXEL / IMG_PIXEL / IMG_NORM_PIXEL return
//     straight colour for an input backed by the engine's premultiplied render
//     target, so a straight ISF (or a CSF declaring ALPHA straight) forwarding
//     a translucent texel stores it unchanged instead of multiplying by alpha
//     twice; the *_PREMULTIPLIED variants return the stored value;
//   * inputs bound to a producer's texture as is (STATIC, 3D, array, cubemap,
//     audio), pass targets and samplers passed as function arguments keep the
//     raw value;
//   * COMPOSITE over / add / multiply / screen / replace, for both ALPHA values,
//     on every way an output reaches its consumer (ISF and raw raster drawn
//     directly, a persistent ISF pass copied, raw raster MRT blits, CSF image
//     copies, VSA), per OUTPUT and per storage image, below an explicit BLEND;
//   * a generic node renderer declaring premultiplied output, and an output
//     node (the offscreen sink's InvertYRenderer), draw a premultiplied texture
//     without multiplying it by alpha again.
//
// Every image is read through alpha-opaque-view.fs, which shows the stored rgb
// on the left half and the stored alpha as grey on the right half.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/TexgenNode.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
struct Shot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage image;
};

// Builds, renders and reads back one pipeline; call inside run_in_gui_app.
template <typename Build>
Shot shoot(score::gfx::GraphicsApi be, Build&& build)
{
  Shot r;
  GfxPipeline p;
  const int sink = build(p);
  if(sink < 0 || !p.error().empty())
  {
    r.error = p.error().empty() ? "pipeline build failed" : p.error();
    return r;
  }
  if(!p.create(be))
  {
    r.skipped = p.skipped();
    r.skip_reason = p.skipReason();
    r.backend = p.backend();
    r.error = p.error();
    return r;
  }
  r.backend = p.backend();
  p.render(3);
  r.image = p.readback(sink);
  if(r.error.empty())
    r.error = p.error();
  if(r.error.empty() && !r.image.valid())
    r.error = "empty readback";
  return r;
}

template <typename Build>
Shot render_pipeline(score::gfx::GraphicsApi be, Build&& build)
{
  Shot r;
  score::test::run_in_gui_app(
      [&](const score::GUIApplicationContext&) { r = shoot(be, build); });
  return r;
}

int add_shader(GfxPipeline& p, const char* file)
{
  const QString path = corpus(file);
  return path.endsWith(".cs") ? p.addCsf(path) : p.addIsf(path);
}

// source -> filter -> view -> sink
Shot through(score::gfx::GraphicsApi be, const char* source, const char* filter)
{
  return render_pipeline(be, [&](GfxPipeline& p) {
    const int src = add_shader(p, source);
    const int flt = filter ? add_shader(p, filter) : -2;
    const int view = p.addIsf(corpus("alpha-opaque-view.fs"));
    const int sink = p.addSink({64, 64});
    if(src < 0 || flt == -1 || view < 0)
      return -1;
    if(flt >= 0)
    {
      p.wire(p.imageOut(src, 0), p.imageIn(flt, 0));
      p.wire(p.imageOut(flt, 0), p.imageIn(view, 0));
    }
    else
    {
      p.wire(p.imageOut(src, 0), p.imageIn(view, 0));
    }
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    return sink;
  });
}

void require_live(const Shot& s, score::gfx::GraphicsApi backend, bool compute)
{
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  if(compute)
    if(const char* why = compute_shader_skip_reason(backend))
      SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());
  REQUIRE(s.image.valid());
}

void check_stored(const Shot& s, std::array<uint8_t, 4> rgb, uint8_t alpha)
{
  const auto c = s.image.at(16, 32);
  const auto a = s.image.at(48, 32);
  INFO("stored rgb = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  INFO("stored alpha = " << int(a[0]));
  // 3 LSB: each backend rounds the 8-bit blend and the half-alpha divide its own way.
  CHECK(near(c, rgb, 3));
  CHECK(near(a, {alpha, alpha, alpha, 255}, 3));
}

std::string generate(const std::string& vert, const std::string& frag, ::isf::parser::ShaderType t)
{
  ::isf::parser p{vert, frag, 450, t};
  return t == ::isf::parser::ShaderType::VertexShaderArt ? p.vertex() : p.fragment();
}
}

TEST_CASE(
    "a straight ISF passthrough stores a translucent texel unchanged",
    "[gfx][isf][alpha]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot ref = through(backend, "alpha-straight-red.fs", nullptr);
  require_live(ref, backend, false);
  check_stored(ref, {128, 0, 0, 255}, 128);

  const Shot s = through(backend, "alpha-straight-red.fs", "alpha-read-straight-pass.fs");
  require_live(s, backend, false);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "a premultiplied passthrough through the *_PREMULTIPLIED macros round-trips",
    "[gfx][isf][alpha]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot s = through(backend, "alpha-straight-red.fs", "alpha-read-premul-pass.fs");
  require_live(s, backend, false);
  check_stored(s, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "CSF sampling macros unpremultiply too; their *_PREMULTIPLIED forms do not",
    "[gfx][csf][alpha]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot straight
      = through(backend, "alpha-straight-red.fs", "alpha-read-csf-straight-pass.cs");
  require_live(straight, backend, true);
  check_stored(straight, {128, 0, 0, 255}, 128);

  const Shot premul
      = through(backend, "alpha-straight-red.fs", "alpha-read-csf-premul-pass.cs");
  require_live(premul, backend, true);
  check_stored(premul, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "a STATIC input bound to the producer's texture keeps the raw texel",
    "[gfx][isf][alpha]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // csf-image-translucent.cs stores (1, 0.5, 0.25, 0.5) as is; unpremultiplied it
  // would read (2, 1, 0.5, 0.5) and clamp to (255, 255, 128).
  const Shot s = through(backend, "csf-image-translucent.cs", "alpha-read-static-pass.fs");
  require_live(s, backend, true);
  check_stored(s, {255, 128, 64, 255}, 128);
}

TEST_CASE(
    "only inputs backed by a premultiplied render target are unpremultiplied",
    "[gfx][isf][alpha]")
{
  using T = ::isf::parser::ShaderType;
  const std::string isfSrc = R"_(/*{
  "ISFVSN": "2",
  "INPUTS": [
    { "NAME": "rt", "TYPE": "image" },
    { "NAME": "rtTex", "TYPE": "texture" },
    { "NAME": "vol", "TYPE": "image", "DIMENSIONS": 3 },
    { "NAME": "layers", "TYPE": "image", "IS_ARRAY": true },
    { "NAME": "lut", "TYPE": "image", "STATIC": true },
    { "NAME": "env", "TYPE": "cubemap" },
    { "NAME": "wave", "TYPE": "audio" },
    { "NAME": "fft", "TYPE": "audioFFT" }
  ],
  "PASSES": [ { "TARGET": "buf" }, {} ]
}*/
vec4 helper(sampler2D s) { return IMG_NORM_PIXEL(s, vec2(0.5)); }
void main()
{
  vec4 c = IMG_THIS_PIXEL(rt) + IMG_THIS_NORM_PIXEL( rt ) + IMG_PIXEL(rt, vec2(1.0))
         + IMG_NORM_PIXEL(rtTex, vec2(0.5)) + IMG_NORM_PIXEL(lut, vec2(0.5))
         + IMG_NORM_PIXEL(wave, vec2(0.5)) + IMG_PIXEL(fft, vec2(1.0))
         + IMG_NORM_PIXEL(buf, vec2(0.5)) + helper(rt)
         + IMG_NORM_PIXEL_PREMULTIPLIED(rt, vec2(0.5)) + texture(vol, vec3(0.5))
         + texture(layers, vec3(0.5)) + texture(env, vec3(1.0));
  gl_FragColor = c;
}
)_";
  const std::string frag = generate({}, isfSrc, T::ISF);
  CHECK(frag.find("ISF_STRAIGHT_THIS_PIXEL(rt)") != std::string::npos);
  CHECK(frag.find("ISF_STRAIGHT_THIS_NORM_PIXEL( rt )") != std::string::npos);
  CHECK(frag.find("ISF_STRAIGHT_PIXEL(rt, vec2(1.0))") != std::string::npos);
  CHECK(frag.find("ISF_STRAIGHT_NORM_PIXEL(rtTex,") != std::string::npos);
  CHECK(frag.find(" IMG_NORM_PIXEL(lut,") != std::string::npos);
  CHECK(frag.find(" IMG_NORM_PIXEL(wave,") != std::string::npos);
  CHECK(frag.find(" IMG_PIXEL(fft,") != std::string::npos);
  CHECK(frag.find(" IMG_NORM_PIXEL(buf,") != std::string::npos);
  CHECK(frag.find("return IMG_NORM_PIXEL(s,") != std::string::npos);
  CHECK(frag.find(" IMG_NORM_PIXEL_PREMULTIPLIED(rt,") != std::string::npos);

  const std::string csfSrc = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "INPUTS": [
    { "NAME": "rt", "TYPE": "image" },
    { "NAME": "vol", "TYPE": "texture", "DIMENSIONS": 3 }
  ],
  "RESOURCES": [
    { "NAME": "src", "TYPE": "image", "ACCESS": "read_only", "FORMAT": "RGBA8" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 p = ivec2(gl_GlobalInvocationID.xy);
  IMG_STORE(outputImage, p, IMG_NORM_PIXEL(rt, vec2(0.5)) + IMG_NORM_PIXEL(vol, vec3(0.5)) + IMG_LOAD(src, p));
}
)_";
  const std::string comp = generate({}, csfSrc, T::CSF);
  CHECK(comp.find("ISF_STRAIGHT_NORM_PIXEL(rt,") != std::string::npos);
  CHECK(comp.find(" IMG_NORM_PIXEL(vol,") != std::string::npos);
  CHECK(comp.find(" IMG_LOAD(src,") != std::string::npos);

  const std::string rrFrag = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [ { "NAME": "rt", "TYPE": "image" }, { "NAME": "layers", "TYPE": "image", "IS_ARRAY": true } ]
}*/
void main() { isf_FragColor = IMG_NORM_PIXEL(rt, vec2(0.5)) + texture(layers, vec3(0.5)); }
)_";
  const std::string rr = generate(
      "void main() { gl_Position = vec4(0.0); }\n", rrFrag, T::RawRasterPipeline);
  CHECK(rr.find("ISF_STRAIGHT_NORM_PIXEL(rt,") != std::string::npos);

  const std::string vsaSrc = R"_(/*{
  "DESCRIPTION": "", "ISFVSN": "2", "MODE": "VERTEX_SHADER_ART", "POINT_COUNT": 3,
  "INPUTS": [ { "NAME": "rt", "TYPE": "image" } ]
}*/
void main() { gl_Position = vec4(0.0); v_color = IMG_NORM_PIXEL(rt, vec2(0.5)); }
)_";
  const std::string vsa = generate(vsaSrc, {}, T::VertexShaderArt);
  CHECK(vsa.find("ISF_STRAIGHT_NORM_PIXEL(rt,") != std::string::npos);
}

namespace
{
// Straight (1, 0.5, 0, 0.5), i.e. premultiplied (0.5, 0.25, 0, 0.5), over the
// opaque (0.25, 0.5, 0.75, 1) of csf-composite-dst.cs.
struct Expected
{
  const char* mode;
  std::array<uint8_t, 4> rgb;
  uint8_t alpha;
};
constexpr Expected kModes[]{
    {"over", {159, 128, 96, 255}, 255},
    {"add", {191, 191, 191, 255}, 255},
    {"multiply", {64, 96, 96, 255}, 255},
    {"screen", {159, 159, 191, 255}, 255},
    {"replace", {128, 64, 0, 255}, 128},
};

const char* srcColor(bool straight)
{
  return straight ? "vec4(1.0, 0.5, 0.0, 0.5)" : "vec4(0.5, 0.25, 0.0, 0.5)";
}

QString writeShader(QTemporaryDir& dir, const QString& name, const std::string& src)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  if(f.open(QIODevice::WriteOnly))
    f.write(src.data(), (qint64)src.size());
  return path;
}

std::string isfSource(const std::string& header, const char* color)
{
  return "/*{\n  \"ISFVSN\": \"2\",\n" + header + "\n}*/\nvoid main() { gl_FragColor = "
         + color + "; }\n";
}

std::string rasterSource(const std::string& header, const std::string& body)
{
  return "/*{\n  \"ISFVSN\": \"2.0\",\n  \"MODE\": \"RAW_RASTER_PIPELINE\",\n"
         "  \"VERTEX_INPUTS\": [], \"VERTEX_OUTPUTS\": [], \"FRAGMENT_INPUTS\": [],\n"
         "  \"INPUTS\": [],\n"
         "  \"PIPELINE_STATE\": { \"DEPTH_TEST\": false, \"DEPTH_WRITE\": false, "
         "\"CULL_MODE\": \"none\", \"VERTEX_COUNT\": 3, \"TOPOLOGY\": \"triangles\" },\n"
         + header + "\n}*/\nvoid main() { " + body + " }\n";
}

std::string csfSource(const std::string& header, const std::string& resourceExtra, const char* color)
{
  return "/*{\n  \"ISFVSN\": \"2.0\",\n  \"MODE\": \"COMPUTE_SHADER\",\n" + header
         + "\n  \"RESOURCES\": [ { \"NAME\": \"outputImage\", \"TYPE\": \"image\", "
           "\"ACCESS\": \"write_only\", \"FORMAT\": \"RGBA8\", \"WIDTH\": \"64\", "
           "\"HEIGHT\": \"64\""
         + resourceExtra
         + " } ],\n  \"PASSES\": [ { \"LOCAL_SIZE\": [8, 8, 1], \"EXECUTION_MODEL\": "
           "{ \"TYPE\": \"2D_IMAGE\", \"TARGET\": \"outputImage\" } } ]\n}*/\n"
           "void main() { ivec2 p = ivec2(gl_GlobalInvocationID.xy); "
           "if(p.x >= 64 || p.y >= 64) return; IMG_STORE(outputImage, p, "
         + color + "); }\n";
}

std::string vsaSource(const std::string& header, const char* color)
{
  return "/*{\n  \"ISFVSN\": \"2\",\n  \"MODE\": \"VERTEX_SHADER_ART\",\n"
         "  \"POINT_COUNT\": 3,\n  \"PRIMITIVE_MODE\": \"TRIANGLES\",\n"
         + header
         + "\n  \"INPUTS\": []\n}*/\nvoid main() {\n"
           "  vec2 p = vertexId < 0.5 ? vec2(-3.0, -3.0) : vertexId < 1.5 ? vec2(3.0, "
           "-3.0) : vec2(0.0, 3.0);\n"
           "  gl_Position = vec4(p, 0.0, 1.0);\n  v_color = "
         + color + ";\n}\n";
}

enum class Kind
{
  Isf,
  IsfPersistent,
  Raster,
  RasterMrt,
  Csf,
  Vsa
};

// csf-composite-dst.cs, then the source, cabled into the view's input.
// Call inside run_in_gui_app.
Shot composite(
    score::gfx::GraphicsApi be, Kind kind, const std::string& header,
    const std::string& extra, bool straight, int outIndex = 0)
{
  QTemporaryDir dir;
  return shoot(be, [&](GfxPipeline& p) {
    const int dst = p.addCsf(corpus("csf-composite-dst.cs"));
    int src = -1;
    switch(kind)
    {
      case Kind::Isf:
        src = p.addIsf(writeShader(dir, "src.fs", isfSource(header, srcColor(straight))));
        break;
      case Kind::IsfPersistent:
        src = p.addIsf(writeShader(
            dir, "src.fs",
            isfSource(
                header + ",\n  \"PASSES\": [ { \"TARGET\": \"buf\", \"PERSISTENT\": true } ]",
                srcColor(straight))));
        break;
      case Kind::Raster:
        src = p.addRaster(
            corpus("alpha-rr-green.vs"),
            writeShader(
                dir, "src.fs",
                rasterSource(
                    header
                        + ",\n  \"FRAGMENT_OUTPUTS\": [ { \"TYPE\": \"vec4\", \"NAME\": "
                          "\"isf_FragColor\" } ]",
                    std::string{"isf_FragColor = "} + srcColor(straight) + ";")));
        break;
      case Kind::RasterMrt:
        src = p.addRaster(
            corpus("alpha-rr-green.vs"),
            writeShader(
                dir, "src.fs",
                rasterSource(
                    header
                        + ",\n  \"FRAGMENT_OUTPUTS\": [ { \"TYPE\": \"vec4\", \"NAME\": "
                          "\"out0\" }, { \"TYPE\": \"vec4\", \"NAME\": \"out1\" } ]"
                        + extra,
                    std::string{"out0 = "} + srcColor(straight) + "; out1 = "
                        + srcColor(straight) + ";")));
        break;
      case Kind::Csf:
        src = p.addCsf(
            writeShader(dir, "src.cs", csfSource(header, extra, srcColor(straight))));
        break;
      case Kind::Vsa:
        src = p.addVsa(writeShader(dir, "src.vs", vsaSource(header, srcColor(straight))));
        break;
    }
    const int view = p.addIsf(corpus("alpha-opaque-view.fs"));
    const int sink = p.addSink({64, 64});
    if(dst < 0 || src < 0 || view < 0)
      return -1;
    p.wire(p.imageOut(src, outIndex), p.imageIn(view, 0));
    p.wire(p.imageOut(dst, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    return sink;
  });
}

std::string alphaHeader(bool straight)
{
  return straight ? "  \"ALPHA\": \"straight\"" : "  \"ALPHA\": \"premultiplied\"";
}

std::string compositeHeader(bool straight, const char* mode)
{
  return alphaHeader(straight) + ",\n  \"COMPOSITE\": \"" + mode + "\"";
}

struct ModeShot
{
  bool straight{};
  const Expected* expected{};
  Shot shot;
};

// Every COMPOSITE mode for both ALPHA values, in one application.
template <typename Render>
std::vector<ModeShot> sweep_modes(Render&& render)
{
  std::vector<ModeShot> shots;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    for(bool straight : {true, false})
      for(const auto& e : kModes)
        shots.push_back({straight, &e, render(straight, e)});
  });
  return shots;
}

void check_modes(const std::vector<ModeShot>& shots, score::gfx::GraphicsApi backend)
{
  for(const auto& m : shots)
  {
    CAPTURE(m.straight, m.expected->mode);
    require_live(m.shot, backend, true);
    check_stored(m.shot, m.expected->rgb, m.expected->alpha);
  }
}
}

TEST_CASE("COMPOSITE on an ISF output", "[gfx][isf][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  check_modes(
      sweep_modes([&](bool straight, const Expected& e) {
        return composite(backend, Kind::Isf, compositeHeader(straight, e.mode), {}, straight);
      }),
      backend);
}

TEST_CASE(
    "COMPOSITE on a persistent ISF pass copied to the output",
    "[gfx][isf][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  check_modes(
      sweep_modes([&](bool straight, const Expected& e) {
        return composite(
            backend, Kind::IsfPersistent, compositeHeader(straight, e.mode), {}, straight);
      }),
      backend);
}

TEST_CASE("COMPOSITE on a raw raster output", "[gfx][raster][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  check_modes(
      sweep_modes([&](bool straight, const Expected& e) {
        return composite(backend, Kind::Raster, compositeHeader(straight, e.mode), {}, straight);
      }),
      backend);
}

TEST_CASE(
    "COMPOSITE on raw raster MRT outputs, top level and per OUTPUT",
    "[gfx][raster][mrt][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  check_modes(
      sweep_modes([&](bool straight, const Expected& e) {
        return composite(
            backend, Kind::RasterMrt, compositeHeader(straight, e.mode), {}, straight, 1);
      }),
      backend);

  // Per OUTPUT: out0 adds, out1 replaces; the top-level multiply is overridden.
  const std::string outputs
      = ",\n  \"OUTPUTS\": [ { \"NAME\": \"out0\", \"COMPOSITE\": \"add\" }, "
        "{ \"NAME\": \"out1\", \"COMPOSITE\": \"replace\" } ]";
  std::vector<ModeShot> perOutput;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    for(bool straight : {true, false})
    {
      const auto header = compositeHeader(straight, "multiply");
      perOutput.push_back(
          {straight, &kModes[1],
           composite(backend, Kind::RasterMrt, header, outputs, straight, 0)});
      perOutput.push_back(
          {straight, &kModes[4],
           composite(backend, Kind::RasterMrt, header, outputs, straight, 1)});
    }
  });
  check_modes(perOutput, backend);
}

TEST_CASE(
    "COMPOSITE on a CSF storage image, top level and per resource",
    "[gfx][csf][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  SECTION("top level")
  {
    check_modes(
        sweep_modes([&](bool straight, const Expected& e) {
          return composite(
              backend, Kind::Csf, compositeHeader(straight, e.mode) + ",", {}, straight);
        }),
        backend);
  }
  SECTION("per resource, over a top-level over")
  {
    check_modes(
        sweep_modes([&](bool straight, const Expected& e) {
          return composite(
              backend, Kind::Csf, compositeHeader(straight, "over") + ",",
              std::string{", \"COMPOSITE\": \""} + e.mode + "\"", straight);
        }),
        backend);
  }
}

TEST_CASE("COMPOSITE on a VSA output", "[gfx][vsa][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  check_modes(
      sweep_modes([&](bool straight, const Expected& e) {
        return composite(
            backend, Kind::Vsa, compositeHeader(straight, e.mode) + ",", {}, straight);
      }),
      backend);
}

TEST_CASE(
    "an explicit BLEND wins over COMPOSITE", "[gfx][isf][raster][alpha][composite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // BLEND One / Zero replaces with the premultiplied source; COMPOSITE add
  // would have given (191, 191, 191).
  const std::string header
      = compositeHeader(false, "add")
        + ",\n  \"PIPELINE_STATE\": { \"BLEND\": { \"ENABLE\": true, \"SRC_COLOR\": "
          "\"one\", \"DST_COLOR\": \"zero\", \"SRC_ALPHA\": \"one\", \"DST_ALPHA\": "
          "\"zero\" } }";
  Shot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    s = composite(backend, Kind::Isf, header, {}, false);
  });
  require_live(s, backend, true);
  check_stored(s, kModes[4].rgb, kModes[4].alpha);
}

TEST_CASE("COMPOSITE parsing", "[gfx][isf][alpha][composite]")
{
  using T = ::isf::parser::ShaderType;
  using ::isf::composite_mode;
  const auto parse = [](const std::string& src, T t) {
    return ::isf::parser{std::string{}, src, 450, t}.data();
  };
  const auto d = parse(
      isfSource(
          "  \"COMPOSITE\": \"Screen\",\n  \"OUTPUTS\": [ { \"NAME\": \"a\", \"COMPOSITE\": "
          "\"replace\" }, { \"NAME\": \"b\" } ]",
          "vec4(1.0)"),
      T::ISF);
  CHECK(::isf::resolve_composite(d) == composite_mode::screen);
  REQUIRE(d.outputs.size() == 2);
  CHECK(::isf::resolve_composite(d, &d.outputs[0]) == composite_mode::replace);
  CHECK(::isf::resolve_composite(d, &d.outputs[1]) == composite_mode::screen);
  CHECK(
      ::isf::resolve_composite(parse(isfSource("  \"DESCRIPTION\": \"\"", "vec4(1.0)"), T::ISF))
      == composite_mode::over);

  CHECK_THROWS(parse(isfSource("  \"COMPOSITE\": \"overlay\"", "vec4(1.0)"), T::ISF));
  CHECK_THROWS(parse(isfSource("  \"COMPOSITE\": 1", "vec4(1.0)"), T::ISF));
  CHECK_THROWS(parse(
      isfSource("  \"OUTPUTS\": [ { \"NAME\": \"a\", \"COMPOSITE\": \"lighten\" } ]", "vec4(1.0)"),
      T::ISF));
  CHECK_THROWS(::isf::parser{
      csfSource("", ", \"COMPOSITE\": \"darken\"", "vec4(1.0)"), T::CSF});
}

namespace
{
// A Texgen filling its texture with (128, 0, 0, 128), drawn as straight colour
// (the Texgen default) or as premultiplied colour.
struct HalfRedTexgen : score::gfx::TexgenNode
{
  bool premultiplied{};
  explicit HalfRedTexgen(bool p)
      : premultiplied{p}
  {
    function = +[](unsigned char* rgba, int w, int h, int) {
      for(int i = 0; i < w * h; i++)
      {
        rgba[4 * i + 0] = 128;
        rgba[4 * i + 1] = 0;
        rgba[4 * i + 2] = 0;
        rgba[4 * i + 3] = 128;
      }
    };
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override
  {
    auto r = new Rendered{*this};
    r->m_outputPremultiplied = premultiplied;
    return r;
  }
};

Shot texgen(score::gfx::GraphicsApi be, bool premultiplied)
{
  return render_pipeline(be, [&](GfxPipeline& p) {
    const int tg = p.addNode(std::make_unique<HalfRedTexgen>(premultiplied));
    const int view = p.addIsf(corpus("alpha-opaque-view.fs"));
    const int sink = p.addSink({64, 64});
    if(tg < 0 || view < 0)
      return -1;
    p.wire(p.nodeImageOut(tg, 0), p.imageIn(view, 0));
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    return sink;
  });
}
}

TEST_CASE(
    "a generic node renderer composites its texture as it declares it",
    "[gfx][alpha]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  const Shot straight = texgen(backend, false);
  require_live(straight, backend, false);
  check_stored(straight, {64, 0, 0, 255}, 128);

  const Shot premultiplied = texgen(backend, true);
  require_live(premultiplied, backend, false);
  check_stored(premultiplied, {128, 0, 0, 255}, 128);
}

TEST_CASE(
    "an output draws a translucent premultiplied input once",
    "[gfx][alpha][output]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));
  // Written premultiplied (0.5, 0.25, 0, 0.5) into the sink's target with
  // COMPOSITE replace: the output shows it over black as (128, 64, 0).
  QTemporaryDir dir;
  const Shot s = render_pipeline(backend, [&](GfxPipeline& p) {
    const int src = p.addIsf(writeShader(
        dir, "src.fs",
        isfSource(compositeHeader(false, "replace"), srcColor(false))));
    const int sink = p.addSink({64, 64});
    if(src < 0)
      return -1;
    p.wire(p.imageOut(src, 0), p.sinkInput(sink));
    return sink;
  });
  require_live(s, backend, false);
  const auto c = s.image.center();
  INFO("output = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  CHECK(near(c, {128, 64, 0, 255}, 3));
}

TEST_CASE("MULTIVIEW shaders get no multiview UBO", "[gfx][multiview]")
{
  using T = ::isf::parser::ShaderType;
  const std::string isfSrc = R"_(/*{
  "ISFVSN": "2",
  "MULTIVIEW": 2,
  "INPUTS": [ { "NAME": "img", "TYPE": "image" } ],
  "OUTPUTS": [ { "NAME": "views", "LAYERS": 2 } ]
}*/
void main() { views = IMG_THIS_PIXEL(img); }
)_";
  ::isf::parser isf{std::string{}, isfSrc, 450, T::ISF};
  CHECK(isf.fragment().find("multiview_t") == std::string::npos);
  CHECK(isf.vertex().find("multiview_t") == std::string::npos);

  const std::string rrFrag = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "MULTIVIEW": 2,
  "VERTEX_INPUTS": [], "VERTEX_OUTPUTS": [], "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [ { "NAME": "views", "TYPE": "color", "LAYERS": 2 } ],
  "INPUTS": []
}*/
void main() { isf_FragColor = vec4(1.0); }
)_";
  ::isf::parser rr{"void main() { gl_Position = vec4(0.0); }\n", rrFrag, 450, T::RawRasterPipeline};
  CHECK(rr.fragment().find("multiview_t") == std::string::npos);
  CHECK(rr.vertex().find("multiview_t") == std::string::npos);
}
