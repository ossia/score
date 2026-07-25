// The same sweep, for CSF compute shaders (.cs).
//
// These reach score::gfx::RenderedCSFNode. Unlike ISF and VSA they are parsed
// straight through isf::parser with the CSF shader type rather than through
// ProgramCache, which only knows how to synthesize a vertex/fragment pair;
// Gfx::CSF::Model::setScript does the same thing and is the reference here.
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
  try
  {
    isf::parser p{data.toStdString(), isf::parser::ShaderType::CSF};
    if(p.mode() != isf::descriptor::CSF)
    {
      error = "Not a valid CSF shader";
      return std::nullopt;
    }

    Gfx::ProcessedProgram program;
    program.type = isf::parser::ShaderType::CSF;
    program.descriptor = p.data();
    // Gfx::CSF::Model keeps the compute source in ProcessedProgram::fragment;
    // RenderedCSFNode substitutes the ISF_LOCAL_SIZE_* placeholders per pass.
    program.fragment = QString::fromStdString(p.compute_shader());
    if(program.fragment.isEmpty())
    {
      error = "Empty compute shader";
      return std::nullopt;
    }
    return program;
  }
  catch(const std::exception& e)
  {
    error = QString("CSF error: %1").arg(e.what());
  }
  catch(...)
  {
    error = "Unknown error";
  }
  return std::nullopt;
}
}

TEST_CASE("Every CSF shader in the library renders", "[integration][gfx][shaders]")
{
  requestGlesContext();
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    sweepLibrary(ctx, {"*.cs", "*.comp", "*.csf"}, &loadCSF, QStringLiteral(SCORE_SHADER_SWEEP_BASELINE_CSF));
  });
}
