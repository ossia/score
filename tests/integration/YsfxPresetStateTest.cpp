// A JSFX preset carries what the script keeps in @serialize, not only its
// sliders: loading one restores it, and undoing the load puts back the state
// the script had before, as a bank preset (which loads it too) needs.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/PresetHelpers.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QFile>
#include <QTemporaryDir>

#include <YSFX/ProcessModel.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
const QString ysfx_uuid = QStringLiteral("cb6b50d3-82c0-4356-a282-317da8b8022f");

// "kept" is only ever set by @serialize, never by a slider
constexpr auto script = R"(desc:serialize test
slider1:0<0,10,1>Level
@init
kept = 0;
@serialize
file_var(0, kept);
)";

std::unique_ptr<score::Command> loadCommand(
    const Process::ProcessModel& proc, const Process::Preset& p,
    const score::DocumentContext& ctx)
{
  auto& factories = ctx.app.interfaces<Process::LoadPresetCommandFactoryList>();
  return std::unique_ptr<score::Command>{
      factories.make(&Process::LoadPresetCommandFactory::make, proc, p, ctx)};
}

double kept(YSFX::ProcessModel& p)
{
  return ysfx_read_var(p.fx.get(), "kept");
}
void setKept(YSFX::ProcessModel& p, double v)
{
  *ysfx_find_var(p.fx.get(), "kept") = v;
}
// See YSFX::ProcessModel::recreatePorts: slider i is inlet 4 + i
Process::ControlInlet* slider(YSFX::ProcessModel& p)
{
  return static_cast<Process::ControlInlet*>(p.inlet(Id<Process::Port>(4)));
}
float level(YSFX::ProcessModel& p)
{
  return ossia::convert<float>(slider(p)->value());
}
void setLevel(YSFX::ProcessModel& p, float v)
{
  slider(p)->setValue(v);
}
}

TEST_CASE(
    "a JSFX preset restores the script's serialized state, and undoing it too",
    "[integration][presets][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("serialize.jsfx");
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(script);
    }

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, path));
    if(!proc || !proc->fx)
      SKIP("YSFX not built");
    REQUIRE(ysfx_find_var(proc->fx.get(), "kept"));
    REQUIRE(slider(*proc));
    auto& dc = doc->context();

    setKept(*proc, 42.);
    setLevel(*proc, 3.f);
    const Process::ProcessModel& base = *proc;
    const auto preset = base.savePreset();
    CHECK(!base.presetChangesPorts(preset));

    setKept(*proc, 5.);
    setLevel(*proc, 7.f);

    auto cmd = loadCommand(*proc, preset, dc);
    REQUIRE(cmd);
    cmd->redo(dc);
    CHECK(kept(*proc) == 42.);
    CHECK(level(*proc) == 3.f);

    cmd->undo(dc);
    CHECK(kept(*proc) == 5.);
    CHECK(level(*proc) == 7.f);

    cmd->redo(dc);
    CHECK(kept(*proc) == 42.);
  });
}

TEST_CASE(
    "a JSFX preset saved with its controls only still loads them",
    "[integration][presets][ysfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString path = dir.filePath("serialize.jsfx");
    {
      QFile f{path};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write(script);
    }

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = static_cast<YSFX::ProcessModel*>(
        score::test::add_process(*doc, ysfx_uuid, path));
    if(!proc || !proc->fx)
      SKIP("YSFX not built");
    REQUIRE(slider(*proc));

    // What savePreset wrote before it kept the state
    setLevel(*proc, 6.f);
    const auto preset = Process::saveScriptProcessPreset(*proc, proc->text());
    setLevel(*proc, 1.f);

    Process::ProcessModel& base = *proc;
    setKept(*proc, 5.);
    base.loadPreset(preset);
    CHECK(level(*proc) == 6.f);
    CHECK(kept(*proc) == 5.);
  });
}
