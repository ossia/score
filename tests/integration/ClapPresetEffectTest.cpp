// A CLAP process names its plug-in in effect(), as "<path>:::<id>": what the
// factory makes it from, what its presets are stamped with, and so what a
// preset dropped from the library recreates. Its presets are only listed for
// the plug-in that saved them, but a preset that names no plug-in still loads.
//
// Made from the validation plug-ins MaFreeverb and Kars (tests/nodes/
// ValidationPlugins.hpp), skipped when they are not installed.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Dataflow/Port.hpp>
#include <Process/Preset.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QFileInfo>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");

//! What the library calls the plug-in in `file`, or nothing when not installed
QString plugin_effect(const QString& file, const QString& id)
{
  const QString env = qEnvironmentVariable("SCORE_VALIDATION_PLUGINS");
  const QString root = env.isEmpty() ? QStringLiteral("/usr/lib") : env;
  const QString path = root + "/clap/" + file;
  if(!QFileInfo::exists(path))
    return {};
  return path + ":::" + id;
}

Process::ControlInlet* first_control(Process::ProcessModel& proc)
{
  for(auto* in : proc.inlets())
    if(auto* c = qobject_cast<Process::ControlInlet*>(in))
      return c;
  return nullptr;
}

float value_of(const Process::ControlInlet& c)
{
  return ossia::convert<float>(c.value());
}

//! Its first control set to another value than it has, which is returned
float change_first_control(Process::ProcessModel& proc)
{
  auto* c = first_control(proc);
  REQUIRE(c);
  const auto dom = c->domain().get();
  const float lo = ossia::convert<float>(dom.get_min());
  const float hi = ossia::convert<float>(dom.get_max());
  REQUIRE(hi > lo);
  const float cur = value_of(*c);
  const float v = std::abs(cur - (lo + 0.25f * (hi - lo))) > 0.1f * (hi - lo)
                      ? lo + 0.25f * (hi - lo)
                      : lo + 0.75f * (hi - lo);
  c->setValue(v);
  return v;
}

struct Plugins
{
  QString freeverb
      = plugin_effect(QStringLiteral("MaFreeverb.clap"), "studio.kx.distrho.MaFreeverb");
  QString kars = plugin_effect(QStringLiteral("Kars.clap"), "studio.kx.distrho.Kars");
};

//! A process of the plug-in, or null when it did not load
Process::ProcessModel* make(score::Document& doc, const QString& effect)
{
  auto* proc = score::test::add_process(doc, clap_uuid, effect);
  if(!proc || !first_control(*proc))
    return nullptr;
  return proc;
}
}

TEST_CASE(
    "CLAP: a process names its plug-in, and a process made from that name is it",
    "[integration][clap][presets]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const Plugins plugs;
    if(plugs.freeverb.isEmpty())
      SKIP("MaFreeverb.clap is not installed (dpf-plugins-clap)");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = make(*doc, plugs.freeverb);
    if(!proc)
      SKIP("MaFreeverb.clap does not load");
    CHECK(proc->effect() == plugs.freeverb);

    auto* again = make(*doc, proc->effect());
    REQUIRE(again);
    CHECK(again->effect() == plugs.freeverb);
    CHECK(again->inlets().size() == proc->inlets().size());

    // An empty process has no plug-in to name
    auto* empty = score::test::add_process(*doc, clap_uuid, QString{});
    REQUIRE(empty);
    CHECK(empty->effect().isEmpty());
  });
}

TEST_CASE(
    "CLAP: a preset dropped from the library makes a process of its plug-in with it",
    "[integration][clap][presets]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const Plugins plugs;
    if(plugs.freeverb.isEmpty())
      SKIP("MaFreeverb.clap is not installed (dpf-plugins-clap)");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = make(*doc, plugs.freeverb);
    if(!proc)
      SKIP("MaFreeverb.clap does not load");

    // The plug-in's own presets name it too
    for(const auto& p : proc->builtinPresets())
      CHECK(p.key.effect == plugs.freeverb);

    const float v = change_first_control(*proc);
    const auto preset = proc->savePreset();
    CHECK(preset.key.effect == plugs.freeverb);

    auto& interval = score::test::base_interval(*doc);
    Process::ProcessModel* dropped{};
    {
      Scenario::Command::Macro m{
          new Scenario::Command::DropProcessInIntervalMacro, doc->context()};
      dropped = m.loadProcessFromPreset(interval, preset, QPointF{});
      m.commit();
    }
    REQUIRE(dropped);
    CHECK(dropped != proc);
    CHECK(dropped->effect() == plugs.freeverb);
    auto* c = first_control(*dropped);
    REQUIRE(c);
    CHECK(value_of(*c) == Catch::Approx(v).margin(1e-4));
  });
}

TEST_CASE(
    "CLAP: the presets of one plug-in are not listed for another",
    "[integration][clap][presets]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const Plugins plugs;
    if(plugs.freeverb.isEmpty() || plugs.kars.isEmpty())
      SKIP("MaFreeverb.clap or Kars.clap is not installed (dpf-plugins-clap)");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* freeverb = make(*doc, plugs.freeverb);
    auto* kars = make(*doc, plugs.kars);
    if(!freeverb || !kars)
      SKIP("MaFreeverb.clap or Kars.clap does not load");

    const auto fp = freeverb->savePreset();
    const auto kp = kars->savePreset();
    CHECK(freeverb->presetMatches(fp));
    CHECK(kars->presetMatches(kp));
    CHECK(!kars->presetMatches(fp));
    CHECK(!freeverb->presetMatches(kp));
  });
}

TEST_CASE(
    "CLAP: a preset that does not name its plug-in still loads",
    "[integration][clap][presets]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const Plugins plugs;
    if(plugs.freeverb.isEmpty())
      SKIP("MaFreeverb.clap is not installed (dpf-plugins-clap)");

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = make(*doc, plugs.freeverb);
    if(!proc)
      SKIP("MaFreeverb.clap does not load");

    auto* c = first_control(*proc);
    REQUIRE(c);
    const float before = value_of(*c);
    const float v = change_first_control(*proc);
    auto old = proc->savePreset();
    old.key.effect.clear();
    c->setValue(before);
    REQUIRE(value_of(*c) == Catch::Approx(before).margin(1e-4));

    // Listed for any CLAP plug-in, as it cannot tell which one wrote it
    CHECK(proc->presetMatches(old));
    if(!plugs.kars.isEmpty())
      if(auto* kars = make(*doc, plugs.kars))
        CHECK(kars->presetMatches(old));

    proc->loadPreset(old);
    CHECK(value_of(*c) == Catch::Approx(v).margin(1e-4));
  });
}
