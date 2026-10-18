// A sampled AUXILIARY texture with DEPTH:true is one texture binding. The
// parser used to declare a <name>_depth companion sampler for it, as it does
// for a top-level image input with DEPTH:true, but the CSF and raw raster
// renderers bind one texture per auxiliary: every later binding of the shader
// was one slot past the one the renderer filled (shader board C48).
// csf-aux-depth-binding.cs writes red into its output image after such an
// auxiliary; the GLSL side is checked on the parser's output, the runtime side
// by rendering it.
#include <score_test/Gfx.hpp>

#include <isf.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFile>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + file;
}
}

TEST_CASE("a DEPTH:true auxiliary texture declares no companion", "[gfx][csf][aux][C48]")
{
  QFile f{corpus("csf-aux-depth-binding.cs")};
  REQUIRE(f.open(QIODevice::ReadOnly));
  ::isf::parser p{{}, f.readAll().toStdString(), 450, ::isf::parser::ShaderType::CSF};
  const auto cs = p.compute_shader();
  INFO(cs);
  CHECK(cs.find("shadow_depth") == std::string::npos);
}

TEST_CASE(
    "a CSF with a DEPTH:true auxiliary texture binds what follows it",
    "[gfx][csf][aux][C48]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_csf_image(be, corpus("csf-aux-depth-binding.cs"));
  });
  if(r.skipped)
    SKIP(r.backend << ": " << r.skip_reason);
  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());
  const auto c = img.at(img.width / 2, img.height / 2);
  INFO("center=(" << int(c[0]) << "," << int(c[1]) << "," << int(c[2]) << ","
                  << int(c[3]) << ")");
  CHECK(near(c, {255, 0, 0, 255}, 2));
}
