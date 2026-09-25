// Storage images in ISF graphics shaders and CSF compute shaders take the
// lowest bindings (N93, extended from the raw raster of GfxStorageImageLowBindingN93).
//
// Qt's OpenGL backend binds a storage image with
// glBindImageTexture(<SRB binding>, ...) and the GLSL keeps the same number in
// layout(binding = N), so N has to be below GL_MAX_IMAGE_UNITS: 8 on NVIDIA,
// and 8 is also the minimum the GL spec allows. ISF numbered the storage images
// after every sampler, CSF in RESOURCES order after the Params block; each
// fixture here puts enough resources ahead of its storage images that the old
// numbering reached 8 or more, and the shader did not compile on OpenGL. Storage
// images now come first: from 3 in ISF, right after the uniform buffers in CSF.
//
// Pinned here: the emitted GLSL of each fixture (images first, samplers and
// buffers after, no binding used twice), the layout of shaders without storage
// images, and a red frame from each fixture on every backend this machine
// brings up (single-pass ISF, multi-pass ISF, CSF with a persistent image, CSF
// with a geometry storage-image auxiliary).
//
// Registration:
//   score_add_gfx_test(isf_csf_image_binding_i1 GfxIsfCsfImageBindingI1.cpp)
#include <score_test/Gfx.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>

#include <array>
#include <map>
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

struct Layout
{
  std::set<int> images;
  std::set<int> samplers;
  std::set<int> buffers;
  std::map<std::string, int> byName;
  bool duplicate = false;
};

Layout layoutOf(const std::string& glsl)
{
  static const std::regex decl{
      R"(layout\(([^)]*)\)\s*([^;{]*?)\s*(\w+)\s*[;{])"};
  static const std::regex bindingRe{R"(binding\s*=\s*(\d+))"};
  Layout l;
  std::set<int> all;
  for(auto it = std::sregex_iterator(glsl.begin(), glsl.end(), decl);
      it != std::sregex_iterator(); ++it)
  {
    const std::string q = (*it)[1];
    std::smatch bm;
    if(!std::regex_search(q, bm, bindingRe))
      continue;
    const int b = std::stoi(bm[1]);
    const std::string type = (*it)[2];
    if(!all.insert(b).second)
      l.duplicate = true;
    l.byName[(*it)[3]] = b;
    if(type.find("image") != std::string::npos)
      l.images.insert(b);
    else if(type.find("sampler") != std::string::npos)
      l.samplers.insert(b);
    else
      l.buffers.insert(b);
  }
  return l;
}

std::set<int> range(int first, int last)
{
  std::set<int> r;
  for(int i = first; i <= last; i++)
    r.insert(i);
  return r;
}

Layout isfLayout(const char* file)
{
  isf::parser p{{}, readFile(corpus(file)), 450, isf::parser::ShaderType::ISF};
  return layoutOf(p.fragment());
}

Layout csfLayout(const char* file)
{
  isf::parser p{{}, readFile(corpus(file)), 450, isf::parser::ShaderType::CSF};
  return layoutOf(p.compute_shader());
}

void checkRed(const IsfResult& r)
{
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());
  for(auto [fx, fy] : std::array<std::array<float, 2>, 3>{{{0.1f, 0.1f}, {0.5f, 0.5f}, {0.85f, 0.85f}}})
  {
    const int x = int(fx * img.width);
    const int y = int(fy * img.height);
    const auto px = img.at(x, y);
    INFO("pixel " << x << "," << y << " = " << int(px[0]) << " " << int(px[1]) << " "
                  << int(px[2]));
    CHECK(px[0] > 200);
    CHECK(px[1] < 40);
    CHECK(px[2] < 40);
  }
}
}

TEST_CASE("ISF storage images take the lowest bindings", "[gfx][isf][binding]")
{
  {
    const auto l = isfLayout("isf-i1-high-image-binding.fs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.images == std::set<int>{3, 4});
    CHECK(l.byName.at("img_a") == 3);
    CHECK(l.byName.at("img_b") == 4);
    CHECK(l.samplers == range(5, 10));
    CHECK(l.byName.at("t0") == 5);
  }
  {
    const auto l = isfLayout("isf-i1-multipass-high-image-binding.fs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.images == std::set<int>{3});
    CHECK(l.samplers == range(4, 9));
    CHECK(l.byName.at("bufferA") == 9);
  }
  {
    const auto l = isfLayout("isf-multipass-storage-rw.fs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.images.empty());
    CHECK(l.samplers == std::set<int>{3});
    CHECK(l.byName.at("timebuf_buf") == 4);
  }
}

TEST_CASE("CSF storage images take the lowest bindings", "[gfx][csf][binding]")
{
  {
    const auto l = csfLayout("csf-i1-high-image-binding.cs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.byName.at("Params") == 2);
    CHECK(l.byName.at("outputImage") == 3);
    CHECK(l.byName.at("state") == 4);
    CHECK(l.byName.at("state_prev") == 5);
    CHECK(l.images == std::set<int>{3, 4, 5});
    CHECK(l.samplers == range(6, 11));
  }
  {
    const auto l = csfLayout("csf-i1-geo-aux-image-binding.cs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.byName.at("outputImage") == 3);
    CHECK(l.byName.at("vox") == 4);
    CHECK(l.images == std::set<int>{3, 4});
    CHECK(l.samplers == range(5, 9));
    CHECK(l.byName.at("geoIn_foo_in_buf") == 10);
  }
  {
    const auto l = csfLayout("csf-texture-sampling.cs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.byName.at("outputImage") == 2);
    CHECK(l.byName.at("inputTex") == 3);
  }
  {
    const auto l = csfLayout("csf-storage-rw.cs");
    CHECK_FALSE(l.duplicate);
    CHECK(l.images.empty());
    CHECK(l.byName.at("buf_buf") == 2);
  }
}

TEST_CASE("ISF and CSF with storage images past eight bindings render", "[gfx][isf][csf][binding]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto file = GENERATE(
      "isf-i1-high-image-binding.fs", "isf-i1-multipass-high-image-binding.fs",
      "csf-i1-high-image-binding.cs", "csf-i1-geo-aux-image-binding.cs");
  CAPTURE(backend_name(api), file);

  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(api, {corpus(file)}, {32, 32}, 4);
  });
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(QString(file).endsWith(".cs"))
    if(const char* why = compute_shader_skip_reason(api))
      SKIP(why);
  checkRed(r);
}
