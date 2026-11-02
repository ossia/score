// A preset of the script a process already runs keeps its ports: it loads in
// place, like a fixed process's, and can be auditioned. A preset of another
// script is a load that can move them.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Port.hpp>

#include <Scenario/Commands/LoadPresetCommand.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <memory>

namespace
{
struct script_case
{
  const char* name;
  QString uuid;
  //! The preset made to be of another script
  std::function<void(Process::Preset&)> other_script;
};

std::unique_ptr<score::Command> loadCommand(
    const Process::ProcessModel& proc, const Process::Preset& p,
    const score::DocumentContext& ctx)
{
  auto& factories = ctx.app.interfaces<Process::LoadPresetCommandFactoryList>();
  return std::unique_ptr<score::Command>{
      factories.make(&Process::LoadPresetCommandFactory::make, proc, p, ctx)};
}

std::vector<const QObject*> portObjects(const Process::ProcessModel& proc)
{
  std::vector<const QObject*> v;
  for(auto* p : proc.inlets())
    v.push_back(p);
  for(auto* p : proc.outlets())
    v.push_back(p);
  return v;
}
}

TEST_CASE(
    "a preset of the same script keeps a script process's ports",
    "[integration][presets][script]")
{
  const script_case cases[] = {
      {"JS", "846a5de5-47f9-46c5-a898-013cb20951d0",
       [](Process::Preset& p) { p.key.effect += "\n// another"; }},
      {"Faust", "5354c61a-1649-4f59-b952-5c2f1b79c1bd",
       [](Process::Preset& p) { p.key.effect += "\n// another"; }},
      {"Shader", "74ca45ff-92c9-44a0-8f1a-754dea05ee1b",
       [](Process::Preset& p) { p.data.replace("void main", "void  main"); }},
  };

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dc = doc->context();
    int tested = 0;
    for(const auto& c : cases)
    {
      INFO(c.name);
      auto* proc = score::test::add_process(*doc, c.uuid, {});
      if(!proc)
        continue;
      tested++;
      const Process::ProcessModel& base = *proc;
      const auto preset = base.savePreset();
      CHECK(!base.presetChangesPorts(preset));

      const auto ports = portObjects(*proc);
      auto cmd = loadCommand(*proc, preset, dc);
      REQUIRE(cmd);
      CHECK(cmd->key() == Scenario::Command::LoadPreset::static_key());
      cmd->redo(dc);
      cmd->undo(dc);
      cmd->redo(dc);
      CHECK(portObjects(*proc) == ports);

      auto other = preset;
      c.other_script(other);
      REQUIRE(other.data + other.key.effect.toUtf8() != preset.data + preset.key.effect.toUtf8());
      CHECK(base.presetChangesPorts(other));
      auto other_cmd = loadCommand(*proc, other, dc);
      REQUIRE(other_cmd);
      CHECK(other_cmd->key() == Scenario::Command::LoadPresetWithCablesBackup::static_key());
    }
    if(tested == 0)
      SKIP("no script process built");
  });
}

TEST_CASE("a shader preset keeps the kind of shader it is", "[integration][presets][gfx]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* proc = score::test::add_process(
        *doc, QStringLiteral("74ca45ff-92c9-44a0-8f1a-754dea05ee1b"), {});
    if(!proc)
      SKIP("Gfx not built");
    const Process::ProcessModel& base = *proc;
    const auto preset = base.savePreset();
    CHECK(preset.data.contains("\"Type\""));
  });
}

TEST_CASE(
    "a malformed shader preset is not loaded, and does not crash",
    "[integration][presets][gfx]")
{
  // Filter, compute shader, vertex shader art, geometry filter
  const char* uuids[] = {
      "74ca45ff-92c9-44a0-8f1a-754dea05ee1b", "a5bbffe0-93d2-4e70-995c-cf46c2c43520",
      "ea13ed06-d21c-4c84-8d0f-83ce0027b81c", "27d3cc85-a4b0-4924-8fde-71c337b40f59"};
  const QByteArray malformed[] = {
      R"([[1]])",
      R"({"Fragment":5,"Vertex":[],"Compute":{},"Shader":null,"Controls":[]})",
      R"({"Fragment":"","Vertex":"","Compute":"","Shader":"","Controls":"x"})",
      R"({"Controls":[["a",1],5,[],[1]]})",
      R"(not json)",
  };

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    int tested = 0;
    for(const char* uuid : uuids)
    {
      INFO(uuid);
      auto* proc = score::test::add_process(*doc, QString::fromLatin1(uuid), {});
      if(!proc)
        continue;
      tested++;
      Process::ProcessModel& base = *proc;
      for(const auto& data : malformed)
      {
        INFO(data.toStdString());
        auto preset = base.savePreset();
        preset.data = data;
        const bool moves = base.presetChangesPorts(preset);
        const auto ports = portObjects(*proc);
        base.loadPreset(preset);
        // What a shader preset holds is all strings: none here is a whole
        // shader but the empty one, which moves the ports if anything does
        if(!moves)
          CHECK(portObjects(*proc) == ports);
      }
    }
    if(tested == 0)
      SKIP("Gfx not built");
  });
}
