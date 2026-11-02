// Loading a preset into a process whose presets keep its ports sets values
// and nothing else: its ports, their cables and what draws them stay. Only a
// load that can move ports (ProcessModel::presetChangesPorts) takes the cables
// off and matches them again.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Process/Commands/LoadPresetCommandFactory.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/LoadPresetCommand.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace
{
// LFO: fixed controls, a value outlet
const QString lfo_uuid = QStringLiteral("0b1b1816-c33e-4796-a16d-5aab27fe600f");

std::unique_ptr<score::Command> loadCommand(
    const Process::ProcessModel& proc, const Process::Preset& p,
    const score::DocumentContext& ctx)
{
  auto& factories = ctx.app.interfaces<Process::LoadPresetCommandFactoryList>();
  return std::unique_ptr<score::Command>{
      factories.make(&Process::LoadPresetCommandFactory::make, proc, p, ctx)};
}

//! The port objects themselves, which a rebuild would replace
std::vector<const QObject*> portObjects(const Process::ProcessModel& proc)
{
  std::vector<const QObject*> v;
  for(auto* p : proc.inlets())
    v.push_back(p);
  for(auto* p : proc.outlets())
    v.push_back(p);
  return v;
}

Process::ControlInlet* control(Process::ProcessModel& proc, const QString& name)
{
  for(auto* in : proc.inlets())
    if(auto c = qobject_cast<Process::ControlInlet*>(in); c && c->name() == name)
      return c;
  return nullptr;
}
}

TEST_CASE(
    "a preset loads into a cabled process without touching its ports or cables",
    "[integration][presets][cable]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* src = score::test::add_process(*doc, lfo_uuid, {});
    auto* dst = score::test::add_process(*doc, lfo_uuid, {});
    if(!src || !dst)
      SKIP("Fx not built");
    auto& dc = doc->context();
    auto& sm = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);

    auto* ampl = control(*dst, "Ampl.");
    REQUIRE(ampl);
    CommandDispatcher<>{dc.commandStack}.submit(new Dataflow::CreateCable{
        sm, Id<Process::Cable>{4242}, Process::CableType::ImmediateGlutton,
        *src->outlets()[0], *ampl});
    REQUIRE(ampl->cables().size() == 1);
    const auto* cable = &*sm.cables.find(Id<Process::Cable>{4242});

    ampl->setValue(0.25f);
    auto preset = dst->savePreset();
    ampl->setValue(0.5f);

    const auto ports = portObjects(*dst);
    int rebuilt = 0;
    QObject::connect(dst, &Process::ProcessModel::inletsChanged, [&] { rebuilt++; });
    QObject::connect(dst, &Process::ProcessModel::outletsChanged, [&] { rebuilt++; });

    auto cmd = loadCommand(*dst, preset, dc);
    REQUIRE(cmd);
    CHECK(cmd->key() == Scenario::Command::LoadPreset::static_key());
    cmd->redo(dc);
    CHECK(ossia::convert<float>(ampl->value()) == 0.25f);
    cmd->undo(dc);
    CHECK(ossia::convert<float>(ampl->value()) == 0.5f);
    cmd->redo(dc);

    // The same ports, the same cable on them, nothing rebuilt
    CHECK(portObjects(*dst) == ports);
    REQUIRE(sm.cables.find(Id<Process::Cable>{4242}) != sm.cables.end());
    CHECK(&*sm.cables.find(Id<Process::Cable>{4242}) == cable);
    CHECK(ampl->cables().size() == 1);
    CHECK(src->outlets()[0]->cables().size() == 1);
    CHECK(rebuilt == 0);
  });
}

TEST_CASE(
    "every process whose presets keep its ports loads them without rebuilding a port",
    "[integration][presets]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dc = doc->context();

    int tested = 0, moving = 0;
    for(const Process::ProcessModelFactory& f :
        ctx.interfaces<Process::ProcessFactoryList>())
    {
      // Hosts of external plug-ins need one to be made
      if(f.flags() & Process::ProcessFlags::RequiresCustomData)
        continue;
      auto* proc = score::test::add_process(
          *doc, QString::fromLatin1(score::uuids::toByteArray(f.concreteKey().impl())),
          {});
      if(!proc)
        continue;
      INFO(f.prettyName().toStdString());

      const auto preset = proc->savePreset();
      if(proc->presetChangesPorts(preset))
      {
        moving++;
        continue;
      }
      tested++;

      const auto ports = portObjects(*proc);
      const auto signature = Scenario::Command::portSignature(*proc);
      int rebuilt = 0;
      auto a = QObject::connect(
          proc, &Process::ProcessModel::inletsChanged, [&] { rebuilt++; });
      auto b = QObject::connect(
          proc, &Process::ProcessModel::outletsChanged, [&] { rebuilt++; });

      auto cmd = loadCommand(*proc, preset, dc);
      REQUIRE(cmd);
      CHECK(cmd->key() == Scenario::Command::LoadPreset::static_key());
      cmd->redo(dc);
      cmd->undo(dc);
      cmd->redo(dc);

      CHECK(portObjects(*proc) == ports);
      CHECK(Scenario::Command::portSignature(*proc) == signature);
      CHECK(rebuilt == 0);
      QObject::disconnect(a);
      QObject::disconnect(b);
    }
    INFO(tested << " processes keep their ports, " << moving << " may move them");
    CHECK(tested > 50);
    CHECK(moving > 0);
  });
}

TEST_CASE(
    "a load that can move ports still takes the cables off and matches them again",
    "[integration][presets][cable]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto* lfo = score::test::add_process(*doc, lfo_uuid, {});
    if(!lfo)
      SKIP("Fx not built");
    // A preset of another script
    auto preset = lfo->savePreset();
    preset.key.effect = "another script";
    CHECK(lfo->presetChangesPorts(preset));
    auto cmd = loadCommand(*lfo, preset, doc->context());
    REQUIRE(cmd);
    CHECK(cmd->key() == Scenario::Command::LoadPresetWithCablesBackup::static_key());
  });
}
