// The Regex object in a document: its capture outlets follow the Pattern,
// keep their cables across edits, come back on undo and on load.
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/SetControllerControlValue.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/plugins/SerializableHelpers.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/command/CommandStack.hpp>
#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <memory>

namespace
{
const QString regex_uuid = "ef63f9b2-cc68-4b25-825e-e3b126875086";
const QString switch_uuid = "51083c8f-aea0-4617-b026-34fd2793c818";
constexpr int fixed_outlets = 5; // Match, Groups, Matched, Unmatched, Error

Process::ControlInlet& pattern(Process::ProcessModel& p)
{
  auto inlet = qobject_cast<Process::ControlInlet*>(p.inlets()[1]);
  REQUIRE(inlet);
  REQUIRE(inlet->name() == "Pattern");
  return *inlet;
}
void setPattern(score::Document& doc, Process::ProcessModel& p, std::string text)
{
  CommandDispatcher<>{doc.context().commandStack}
      .submit<Scenario::SetControllerControlValue>(
          pattern(p), ossia::value{std::move(text)}, doc.context());
}
QStringList outletNames(Process::ProcessModel& p)
{
  QStringList names;
  for(auto* o : p.outlets())
    names.push_back(o->name());
  return names;
}
}

TEST_CASE("Regex capture outlets follow the pattern and keep their cables", "[avnd][regex][model]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto re = score::test::add_process(*doc, regex_uuid, {});
    auto sink = score::test::add_process(*doc, switch_uuid, {});
    if(!re)
      SKIP("Regex not built");
    REQUIRE(sink);
    REQUIRE(re->outlets().size() == fixed_outlets);

    setPattern(*doc, *re, R"((?P<temp>\d+);(\d+))");
    REQUIRE(re->outlets().size() == fixed_outlets + 2);
    CHECK(re->outlets()[fixed_outlets]->name() == "temp");
    auto temp = re->outlets()[fixed_outlets];

    auto& scenario = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc);
    CommandDispatcher<>{doc->context().commandStack}.submit<Dataflow::CreateCable>(
        scenario, Id<Process::Cable>{1}, Process::CableType::ImmediateGlutton, *temp,
        *sink->inlets()[0]);

    // "temp" moves and a group is added: same outlet, same cable
    setPattern(*doc, *re, R"((\d+);(?P<temp>\d+);(\d+))");
    REQUIRE(re->outlets().size() == fixed_outlets + 3);
    CHECK(re->outlets()[fixed_outlets + 1] == temp);
    REQUIRE(scenario.cables.size() == 1);
    CHECK(scenario.cables.begin()->source().try_find(doc->context()) == temp);

    // A typo: the groups are read from the text, nothing moves
    setPattern(*doc, *re, R"re((\d+);(?P<temp>\d+);(\d+)re");
    CHECK(re->outlets().size() == fixed_outlets + 3);
    CHECK(scenario.cables.size() == 1);

    // "temp" gone: its cable too; undo brings both back
    setPattern(*doc, *re, R"((\d+))");
    CHECK(re->outlets().size() == fixed_outlets + 1);
    CHECK(scenario.cables.empty());
    doc->commandStack().undo();
    REQUIRE(scenario.cables.size() == 1);
    CHECK(outletNames(*re).contains("temp"));

    // Saved and loaded: the outlets are found again with their ids
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    JSONReader writer;
    writer.readFrom(*re);
    auto json = readJson(writer.toByteArray());
    JSONObject::Deserializer reader{json};
    std::unique_ptr<Process::ProcessModel> restored{
        deserialize_interface(factories, reader, doc->context(), re->parent())};
    REQUIRE(restored);
    REQUIRE(restored->outlets().size() == re->outlets().size());
    for(std::size_t i = 0; i < re->outlets().size(); i++)
    {
      CHECK(restored->outlets()[i]->id() == re->outlets()[i]->id());
      CHECK(restored->outlets()[i]->name() == re->outlets()[i]->name());
    }
  });
}
