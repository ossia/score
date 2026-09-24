// Storage images in a raw raster take the lowest bindings.
//
// Qt's OpenGL backend binds a storage image with
// glBindImageTexture(<SRB binding>, ...) and the GLSL keeps the same number in
// layout(binding = N), so N has to be below GL_MAX_IMAGE_UNITS, whose GL-spec
// minimum is 8. Storage images therefore come first, from binding 3, on every
// backend, ahead of the samplers.
//
// Checks that the emitted GLSL puts every storage image below 8, and that the
// fixture renders red on every available backend.
#include <score_test/Gfx.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>

#include <array>
#include <regex>
#include <set>
#include <string>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

std::string readFile(const QString& path)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return {};
  return f.readAll().toStdString();
}
}

TEST_CASE("raw raster storage images take the lowest bindings", "[gfx][raster][binding]")
{
  isf::parser p{
      readFile(corpus("rr-storage-images-after-samplers.vs")),
      readFile(corpus("rr-storage-images-after-samplers.fs")), 450,
      isf::parser::ShaderType::RawRasterPipeline};
  REQUIRE(p.mode() == isf::descriptor::RawRaster);
  const std::string frag = p.fragment();

  static const std::regex decl{
      R"(layout\((std140, )?binding = (\d+)[^)]*\) (readonly |writeonly |restrict )?uniform (readonly |writeonly |restrict )?(\w+))"};
  std::set<int> bindings;
  std::set<int> images;
  int samplers = 0;
  for(auto it = std::sregex_iterator(frag.begin(), frag.end(), decl);
      it != std::sregex_iterator(); ++it)
  {
    const int b = std::stoi((*it)[2]);
    const std::string type = (*it)[5];
    INFO((*it)[0].str());
    CHECK(bindings.insert(b).second);
    if(type.find("image") != std::string::npos)
      images.insert(b);
    else if(type.find("sampler") != std::string::npos)
      samplers++;
  }
  CHECK(samplers == 6);
  CHECK(images == std::set<int>{3, 4});
}

TEST_CASE("a raw raster with storage images past eight bindings renders", "[gfx][raster][binding]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int raster = p.addRaster(
        corpus("rr-storage-images-after-samplers.vs"), corpus("rr-storage-images-after-samplers.fs"));
    if(raster < 0)
    {
      err = p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << err);
  REQUIRE(err.empty());
  for(auto [x, y] : std::array<std::array<int, 2>, 3>{{{4, 4}, {16, 16}, {27, 27}}})
  {
    const auto px = img.at(x, y);
    INFO("pixel " << x << "," << y << " = " << int(px[0]) << " " << int(px[1]) << " "
                  << int(px[2]));
    CHECK(px[0] > 200);
    CHECK(px[1] < 40);
    CHECK(px[2] < 40);
  }
}
