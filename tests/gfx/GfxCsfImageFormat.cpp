// A CSF storage image is allocated in the format its FORMAT names, spelled as
// the GLSL layout qualifier it is emitted as. The allocation compared against
// Qt's enum names: rgb10_a2 ("RGB10A2" in Qt), r32i ("R32SI"), rg16 and the
// signed integer formats fell through to RGBA8 while the shader declared the
// real format (shader board C15).
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <vector>

using namespace score::test::gfx;

namespace
{
QString csf(const std::string& format, const char* store)
{
  return QString::fromStdString(R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": ")_"
                                + format + R"_(", "WIDTH": "8", "HEIGHT": "8" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main() { imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), )_" + store + R"_(); }
)_");
}

struct Case
{
  std::string format;
  const char* store;
  QRhiTexture::Format expected;
};
}

TEST_CASE("a CSF storage image is allocated in its FORMAT", "[gfx][csf][format][C15]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  // The integer formats arrived in Qt 6.10; below it the engine falls back
  // for them (Utils.cpp), so only the float formats are asked of it.
  const auto c = GENERATE(from_range(std::vector<Case>{
      Case{"rgba16f", "vec4(1.0)", QRhiTexture::RGBA16F},
      Case{"rgb10_a2", "vec4(1.0)", QRhiTexture::RGB10A2},
      Case{"rg16", "vec4(1.0)", QRhiTexture::RG16},
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
      Case{"r32i", "ivec4(1)", QRhiTexture::R32SI},
      Case{"rg32i", "ivec4(1)", QRhiTexture::RG32SI},
      Case{"r32ui", "uvec4(1u)", QRhiTexture::R32UI},
#endif
  }));
  CAPTURE(backend_name(be), c.format);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/fmt.cs";
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(csf(c.format, c.store).toUtf8());
  }

  struct
  {
    bool skipped{};
    std::string error;
    int format{-1};
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int cs = p.addCsf(path);
    if(cs < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({8, 8});
    p.wire(p.imageOut(cs, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    auto& node = *p.isf(cs);
    if(!node.renderedNodes.empty())
      if(auto* tex
         = node.renderedNodes.begin()->second->textureForOutput(*node.output[0]))
        r.format = tex->format();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(r.error);
  REQUIRE(r.error.empty());
  CHECK(r.format == int(c.expected));
}

// Size: an axis without WIDTH / HEIGHT takes the default size's, the other
// keeps its expression. Giving only one replaced the whole size with the
// render size (shader board C17).
TEST_CASE("a CSF storage image with only WIDTH or HEIGHT keeps it", "[gfx][csf][size][C17]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto axis = GENERATE(std::string{"WIDTH"}, std::string{"HEIGHT"});
  CAPTURE(backend_name(be), axis);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/size.cs";
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QString::fromStdString(R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", ")_"
                                   + axis + R"_(": "16" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main() { imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(1.0)); }
)_").toUtf8());
  }

  struct
  {
    bool skipped{};
    std::string error;
    QSize size;
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int cs = p.addCsf(path);
    if(cs < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({64, 48});
    p.wire(p.imageOut(cs, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    auto& node = *p.isf(cs);
    if(!node.renderedNodes.empty())
      if(auto* tex
         = node.renderedNodes.begin()->second->textureForOutput(*node.output[0]))
        r.size = tex->pixelSize();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(r.error);
  REQUIRE(r.error.empty());
  INFO("size " << r.size.width() << "x" << r.size.height());
  if(axis == "WIDTH")
    CHECK(r.size.width() == 16);
  else
    CHECK(r.size.height() == 16);
  // The other axis is the render size's.
  CHECK((axis == "WIDTH" ? r.size.height() : r.size.width()) > 16);
}
