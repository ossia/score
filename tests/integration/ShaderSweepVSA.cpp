// The same sweep, for VertexShaderArt shaders (.vs). They are the largest group
// in the library -- more numerous than the ISF set -- and were previously
// unswept entirely.
//
// A VSA shader drives vertices rather than shading a full-screen quad, so it
// reaches score::gfx::SimpleRenderedVSANode instead of the ISF renderers; the
// dispatch happens inside ISFNode::createRenderer, keyed on the parsed
// descriptor's mode. Everything else -- the categories, the baseline diff, the
// environment variables -- matches ShaderSweepISF.cpp.

#include "ShaderSweep.hpp"

namespace
{
std::optional<Gfx::ProcessedProgram>
loadVSA(const QString& path, QByteArray data, QString& error)
{
  const auto source = Gfx::programFromVSAVertexShaderPath(path, std::move(data));
  auto [program, err] = Gfx::ProgramCache::instance().get(source);
  error = err;
  return program;
}
}

TEST_CASE("Every VSA shader in the library renders", "[integration][gfx][shaders]")
{
  requestGlesContext();
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    sweepLibrary(
        ctx, {"*.vs", "*.vert"}, &loadVSA,
        QStringLiteral(SCORE_SHADER_SWEEP_BASELINE_VSA),
        QStringLiteral("VERTEX_SHADER_ART"));
  });
}
