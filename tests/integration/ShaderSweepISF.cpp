// Renders every ISF fragment shader in the user library through the real
// graphics pipeline: ProgramCache -> ISFNode -> Graph -> RenderList -> offscreen
// output, exactly as a Gfx filter process does at runtime, then reads the frame
// back.
//
// This deliberately goes further than parsing. A shader can translate cleanly
// and still fail when a pipeline is built for it, when a texture is uploaded in
// a format the backend rejects, or by drawing nothing at all -- none of which is
// visible before a frame is drawn.
//
// Each shader is additionally baked for GLSL ES 3.00, the profile the
// WebAssembly build gets, so a shader that only fails there shows up as a
// wasm-only breakage. SCORE_SHADER_SWEEP_GLES=1 goes further and runs the whole
// sweep on an OpenGL ES context instead of desktop GL.
//
// Failures are reported per shader as one of: parse (not valid ISF), bake (the
// shader does not compile), gles300 (compiles for desktop but not for the wasm
// profile), render (the pipeline threw), warning (the backend complained),
// devicelost, blank (the frame came back one flat colour).
//
// The library is not part of the repository, so the test skips when it is
// absent. Point it somewhere explicitly with SCORE_SHADER_LIBRARY_DIR.
// Known-bad shaders are tolerated through a baseline file: the test fails on
// *new* failures only. Refresh it with SCORE_SHADER_SWEEP_WRITE_BASELINE=1.
// Every shader is named on stdout before it is rendered, so one that takes the
// process down with it can still be identified.

#include "ShaderSweep.hpp"

namespace
{
std::optional<Gfx::ProcessedProgram>
loadISF(const QString& path, QByteArray data, QString& error)
{
  const auto source = Gfx::programFromISFFragmentShaderPath(path, std::move(data));
  auto [program, err] = Gfx::ProgramCache::instance().get(source);
  error = err;
  return program;
}
}

TEST_CASE("Every ISF shader in the library renders", "[integration][gfx][shaders]")
{
  requestGlesContext();
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    sweepLibrary(ctx, {"*.fs", "*.frag"}, &loadISF, QStringLiteral(SCORE_SHADER_SWEEP_BASELINE_ISF));
  });
}
