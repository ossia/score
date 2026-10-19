// $COUNT_<name> in a CSF size expression is the element count of the buffer's
// array member: a flexible array's allocated capacity, a fixed array's extent.
// A buffer of fixed arrays reported 1, as did a uniform block (shader board
// C25); a layout without an array is one element.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>
#include <QTemporaryDir>

using namespace score::test::gfx;

TEST_CASE("$COUNT_ is the element count of the buffer's array", "[gfx][csf][C25]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  struct Case
  {
    std::string resource;
    int count;
  };
  const auto c = GENERATE(
      Case{R"_({ "NAME": "buf", "TYPE": "storage", "ACCESS": "read_write", "LAYOUT": [ { "NAME": "n", "TYPE": "uint" }, { "NAME": "v", "TYPE": "vec4[16]" } ] })_", 16},
      Case{R"_({ "NAME": "buf", "TYPE": "storage", "ACCESS": "read_write", "LAYOUT": [ { "NAME": "v", "TYPE": "vec4[2][3]" } ] })_", 6},
      Case{R"_({ "NAME": "buf", "TYPE": "storage", "ACCESS": "read_write", "LAYOUT": [ { "NAME": "a", "TYPE": "float" }, { "NAME": "b", "TYPE": "vec4" } ] })_", 1},
      Case{R"_({ "NAME": "buf", "TYPE": "uniform", "LAYOUT": [ { "NAME": "w", "TYPE": "vec4[8]" } ] })_", 8});
  CAPTURE(backend_name(be), c.resource);

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString path = dir.path() + "/count.cs";
  {
    QFile f{path};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QString::fromStdString(R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    )_" + c.resource + R"_(,
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "$COUNT_buf", "HEIGHT": "4" }
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
         = node.renderedNodes.begin()->second->textureForOutput(*p.imageOut(cs, 0)))
        r.size = tex->pixelSize();
  });
  if(r.skipped)
    SKIP(backend_name(be) << ": no usable backend");
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);
  INFO(r.error);
  REQUIRE(r.error.empty());
  CHECK(r.size.width() == c.count);
  CHECK(r.size.height() == 4);
}
