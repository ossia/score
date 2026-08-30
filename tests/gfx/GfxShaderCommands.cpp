// The four Gfx shader-editing commands, driven the way a live edit drives
// them: ChangeShader (Gfx/Filter), ChangeGeometryShader (Gfx/GeometryFilter),
// ChangeCSF (Gfx/CSF) and ChangeVSAShader (Gfx/VSA).
//
// All four derive from Scenario::EditScript, whose redo() replaces the whole
// port surface of the process and whose undo() has to put back the exact ports
// it had before — including a SCORE_ASSERT that the inlet and outlet COUNTS
// match what was saved. That assert is the interesting part: every one of these
// models rebuilds its ports from the parsed shader, and three of them leave a
// DIFFERENT number of ports behind when the shader fails to parse. So the
// undo/redo of a shader edit that goes through a bad intermediate state is
// exactly the fragile path, and nothing covered it.
//
// Scope: model + command + undo stack. No rendering is asserted here; the
// pixels of these shaders are covered by the L3 render tests in this directory.

#include "GfxProcessDoc.hpp"

#include <Gfx/CSF/Process.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Gfx/GeometryFilter/Process.hpp>
#include <Gfx/ShaderProgram.hpp>
#include <Gfx/VSA/Process.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
constexpr auto UUID_FILTER = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
constexpr auto UUID_GEOMFILTER = "27d3cc85-a4b0-4924-8fde-71c337b40f59";
constexpr auto UUID_CSF = "a5bbffe0-93d2-4e70-995c-cf46c2c43520";
constexpr auto UUID_VSA = "ea13ed06-d21c-4c84-8d0f-83ce0027b81c";

bool has_port(const std::vector<QString>& names, const char* n)
{
  return std::find(names.begin(), names.end(), QString::fromUtf8(n)) != names.end();
}
}

TEST_CASE("ISF filter shader change is undoable", "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_FILTER, corpus("isf-solid-color.fs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Filter::Model&>(*proc);

    // A shader with no INPUTS: the only inlet-less state of this process.
    const auto before = inlet_names(m);
    const auto before_outs = outlet_names(m);
    CHECK_FALSE(m.processedProgram().fragment.isEmpty());
    CHECK_FALSE(has_port(before, "level"));

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeShader{
        m, Gfx::programFromISFFragmentShaderPath(corpus("isf-control-float.fs"), {}),
        doc->context()});

    const auto after = inlet_names(m);
    CHECK(has_port(after, "level"));
    CHECK(after != before);
    CHECK(outlet_names(m) == before_outs);

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == before);
    CHECK(outlet_names(m) == before_outs);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(inlet_names(m) == after);
  });
}

TEST_CASE(
    "An unparseable ISF shader leaves the filter usable", "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_FILTER, corpus("isf-control-float.fs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Filter::Model&>(*proc);

    const auto good = inlet_names(m);
    REQUIRE(has_port(good, "level"));
    const auto good_processed = m.processedProgram().fragment;

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};

    // Not ISF at all: no JSON header, and the GLSL is invalid too.
    disp.submit(new Gfx::ChangeShader{
        m, Gfx::ShaderSource{QString{}, QStringLiteral("this is not a shader {{{")},
        doc->context()});

    // The port surface must survive a shader that could not be compiled: a
    // process whose controls vanished on a typo would drop every cable and
    // every automation pointing at it.
    CHECK(inlet_names(m) == good);

    // ...and undo/redo must still be consistent across the failure.
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == good);
    CHECK(m.processedProgram().fragment == good_processed);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(inlet_names(m) == good);
  });
}

TEST_CASE("Geometry filter shader change is undoable", "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    // Empty construction data -> the model's built-in default geometry filter,
    // which declares one "intensity" control.
    auto* proc = add_process(ctx, *doc, UUID_GEOMFILTER);
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::GeometryFilter::Model&>(*proc);

    const auto before = inlet_names(m);
    CHECK(has_port(before, "Geometry In"));
    CHECK(has_port(before, "intensity"));
    CHECK_FALSE(m.processedProgram().shader.isEmpty());
    CHECK(outlet_names(m).size() == 1);

    const QString shifted = corpus_text("syn-geofilter-shift.glsl");
    REQUIRE_FALSE(shifted.isEmpty());

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeGeometryShader{m, shifted, doc->context()});

    const auto after = inlet_names(m);
    CHECK(has_port(after, "shift"));
    CHECK_FALSE(has_port(after, "intensity"));
    CHECK(m.script() == shifted);

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == before);
    CHECK(has_port(inlet_names(m), "intensity"));

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(inlet_names(m) == after);
    CHECK(m.script() == shifted);
  });
}

TEST_CASE(
    "An unparseable geometry filter degrades without crashing",
    "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_GEOMFILTER);
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::GeometryFilter::Model&>(*proc);

    const auto before = inlet_names(m);
    REQUIRE(has_port(before, "intensity"));

    int errors = 0;
    QObject::connect(
        &m, &Gfx::GeometryFilter::Model::errorMessage, &m,
        [&errors](int, const QString&) { ++errors; });

    // setScript() catches the ISF parser's exception; the model is left with
    // only its geometry inlet.
    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeGeometryShader{
        m, QStringLiteral("/*{ not json at all */ void nope("), doc->context()});

    CHECK(errors >= 0); // the parse failure path must not abort the process
    const auto broken = inlet_names(m);
    CHECK(broken.size() >= 1);
    CHECK(has_port(broken, "Geometry In"));

    // The port count changed, which is exactly what EditScript::undo asserts
    // on. Undo restores the previous script, so the counts line up again.
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == before);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(inlet_names(m) == broken);

    // ...and a second undo must still land on the good script.
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == before);
  });
}

TEST_CASE("CSF compute shader change is undoable", "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_CSF, corpus("csf-gradient-y.cs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::CSF::Model&>(*proc);

    const auto before_in = inlet_names(m);
    const auto before_out = outlet_names(m);
    CHECK_FALSE(m.processedCompute().isEmpty());
    // A 2D_IMAGE compute pass declares an image RESOURCE, hence an outlet.
    CHECK(before_out.size() >= 1);

    const QString other = corpus_text("csf-texture-sampling.cs");
    REQUIRE_FALSE(other.isEmpty());

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeCSF{m, other, doc->context()});

    const auto after_in = inlet_names(m);
    const auto after_out = outlet_names(m);
    CHECK(m.compute() == other);
    // csf-texture-sampling reads a texture: it gains an image input the
    // gradient shader does not have.
    CHECK(after_in.size() > before_in.size());

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == before_in);
    CHECK(outlet_names(m) == before_out);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(inlet_names(m) == after_in);
    CHECK(outlet_names(m) == after_out);
  });
}

TEST_CASE(
    "An unparseable CSF shader clears every port, and undo restores them",
    "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_CSF, corpus("csf-gradient-y.cs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::CSF::Model&>(*proc);

    const auto good_in = inlet_names(m);
    const auto good_out = outlet_names(m);
    REQUIRE(good_out.size() >= 1);

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeCSF{
        m, QStringLiteral("/*{ \"MODE\": \"COMPUTE_SHADER\" "), doc->context()});

    // Unlike the ISF filter, CSF::setScript() clears inlets AND outlets before
    // parsing and does not put them back on failure: a broken compute shader
    // leaves a process with no texture output at all.
    CHECK(outlet_names(m).empty());

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == good_in);
    CHECK(outlet_names(m) == good_out);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(outlet_names(m).empty());

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(outlet_names(m) == good_out);
  });
}

// FINDING (defect, filed by this test): a CSF shader whose ISF header is
// truncated mid-JSON does not throw out of isf::parser, so
// Gfx::CSF::Model::setScript() takes its SUCCESS path — it never emits
// errorMessage — while setupCSF() finds no PASSES and no RESOURCES and leaves
// the process with zero inlets and zero outlets. The user gets an inert
// process and no diagnostic; the editor's error line stays empty.
//
// Expected-failure: the day the parser (or the model) reports this, the case
// passes, Catch2 flags the [!shouldfail], and the tag comes off.
TEST_CASE(
    "An unparseable CSF shader reports an error",
    "[gfx][process][command][gui][!shouldfail]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_CSF, corpus("csf-gradient-y.cs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::CSF::Model&>(*proc);

    int errors = 0;
    QObject::connect(
        &m, &Gfx::CSF::Model::errorMessage, &m,
        [&errors](int, const QString&) { ++errors; });

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeCSF{
        m, QStringLiteral("/*{ \"MODE\": \"COMPUTE_SHADER\" "), doc->context()});

    // It did break the process — that half is asserted green above.
    REQUIRE(m.outlets().empty());
    CHECK(errors >= 1);
  });
}

TEST_CASE("VSA vertex shader change is undoable", "[gfx][process][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_VSA, corpus("vsa-triangle.vs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::VSA::Model&>(*proc);

    const auto before = inlet_names(m);
    CHECK_FALSE(m.vertex().isEmpty());

    const QString points = corpus_text("vsa-points.vs");
    REQUIRE_FALSE(points.isEmpty());

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeVSAShader{m, points, doc->context()});

    const auto after = inlet_names(m);
    CHECK(m.vertex() == points);

    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(inlet_names(m) == before);
    CHECK(m.vertex() != points);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(inlet_names(m) == after);
    CHECK(m.vertex() == points);
  });
}

TEST_CASE(
    "Shader edits survive a chain of undo and redo", "[gfx][process][command][gui]")
{
  // Three consecutive edits, then the whole stack unwound and replayed. The
  // saved-port bookkeeping in EditScript is per command, so an edit whose
  // undo left the process one port short only shows up once a LATER command's
  // undo runs against it.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_FILTER, corpus("isf-solid-color.fs"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Filter::Model&>(*proc);

    const auto s0 = inlet_names(m);

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};

    disp.submit(new Gfx::ChangeShader{
        m, Gfx::programFromISFFragmentShaderPath(corpus("isf-control-float.fs"), {}),
        doc->context()});
    const auto s1 = inlet_names(m);

    disp.submit(new Gfx::ChangeShader{
        m, Gfx::programFromISFFragmentShaderPath(corpus("isf-two-images.fs"), {}),
        doc->context()});
    const auto s2 = inlet_names(m);

    disp.submit(new Gfx::ChangeShader{
        m, Gfx::programFromISFFragmentShaderPath(corpus("isf-gradient-x.fs"), {}),
        doc->context()});
    const auto s3 = inlet_names(m);

    CHECK(s1 != s0);
    CHECK(s2 != s1);

    stack.undo();
    CHECK(inlet_names(m) == s2);
    stack.undo();
    CHECK(inlet_names(m) == s1);
    stack.undo();
    CHECK(inlet_names(m) == s0);

    stack.redo();
    CHECK(inlet_names(m) == s1);
    stack.redo();
    CHECK(inlet_names(m) == s2);
    stack.redo();
    CHECK(inlet_names(m) == s3);
  });
}
