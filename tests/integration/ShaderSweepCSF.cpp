// The same sweep, for CSF compute shaders (.cs).
//
// These reach score::gfx::RenderedCSFNode. Unlike ISF and VSA they are parsed
// straight through isf::parser with the CSF shader type rather than through a
// programFrom* helper, because a compute shader has no vertex/fragment pair to
// synthesize.
//
// Some of the corpus is known to produce geometry for a downstream raster stage
// and so renders nothing on its own; those land in the "blank" category and are
// expected to sit in the baseline rather than be treated as failures.

#include "ShaderSweep.hpp"

namespace
{
std::optional<Gfx::ProcessedProgram>
loadCSF(const QString& path, QByteArray data, QString& error)
{
  Gfx::ShaderSource source{
      isf::parser::ShaderType::CSF, QString{}, QString::fromUtf8(data)};
  auto [program, err] = Gfx::ProgramCache::instance().get(source);
  error = err;
  return program;
}
}

TEST_CASE("Every CSF shader in the library renders", "[integration][gfx][shaders]")
{
  requestGlesContext();
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    sweepLibrary(ctx, {"*.cs", "*.comp", "*.csf"}, &loadCSF, QStringLiteral(SCORE_SHADER_SWEEP_BASELINE_CSF));
  });
}
