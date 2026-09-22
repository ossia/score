// An LFO cabled into a Threedim primitive's Rotation must sweep the control's
// whole range, not the LFO's own [0;1]. The primitive runs on the graphics
// graph, whose control ports are made by hand rather than by the avnd binding.

#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Dataflow/Commands/EditConnection.hpp>
#include <Execution/BaseScenarioComponent.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/thread.hpp>
#include <ossia/network/domain/domain.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

namespace
{
const QString lfo_uuid = QStringLiteral("1e17e479-3513-44c8-a8a7-017be9f6ac8a");
const QString plane_uuid = QStringLiteral("1e923d52-3494-49e8-8698-b001405000da");

void makeCable(
    score::Document& doc, int id, const Process::Port& source, const Process::Port& sink)
{
  auto& model = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc);
  CommandDispatcher<>{doc.context().commandStack}.submit<Dataflow::CreateCable>(
      model, Id<Process::Cable>{id}, Process::CableType::ImmediateGlutton, source, sink);
  REQUIRE(model.cables.find(Id<Process::Cable>{id}) != model.cables.end());
}

Execution::DocumentPlugin& load_execution(score::Document& doc)
{
  auto& plug = doc.context().plugin<Execution::DocumentPlugin>();
  plug.reload(true, score::test::base_interval(doc));
  ossia::set_thread_pinned(ossia::thread_type::Audio, 0);
  plug.runAllCommands();
  ossia::set_thread_pinned(ossia::thread_type::Ui, 0);
  return plug;
}

std::shared_ptr<ossia::graph_node>
node_of(Execution::DocumentPlugin& plug, const Process::ProcessModel& proc)
{
  REQUIRE(plug.baseScenario());
  auto& procs = plug.baseScenario()->baseInterval().processes();
  auto it = procs.find(proc.id());
  REQUIRE(it != procs.end());
  REQUIRE(it->second);
  REQUIRE(it->second->node);
  return it->second->node;
}
}

TEST_CASE("An LFO sweeps a rotation control's whole range", "[avnd][execution][domain]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto* lfo = score::test::add_process(*doc, lfo_uuid, {});
    auto* plane = score::test::add_process(*doc, plane_uuid, {});
    if(!lfo || !plane)
      SKIP("LFO / 3d Plane are not built");

    // Position, Rotation, Scale, H divs., V divs.
    REQUIRE(plane->inlets().size() >= 3);
    auto* rotation = plane->inlets()[1];
    REQUIRE(rotation->name() == QStringLiteral("Rotation"));

    makeCable(*doc, 1, *lfo->outlets()[0], *rotation);

    auto& plug = load_execution(*doc);
    auto& src = **safe_cast<ossia::value_outlet*>(node_of(plug, *lfo)->root_outputs()[0]);
    auto& sink
        = **safe_cast<ossia::value_inlet*>(node_of(plug, *plane)->root_inputs()[1]);

    SECTION("the control port says what it holds and in what range")
    {
      auto* t = sink.type.target<ossia::val_type>();
      REQUIRE(t);
      CHECK(*t == ossia::val_type::VEC3F);

      auto [min, max] = ossia::get_float_minmax(sink.domain);
      REQUIRE(min);
      REQUIRE(max);
      CHECK(*min == 0.f);
      CHECK(*max == 360.f);
    }

    SECTION("the top of the LFO's output reaches the top of the rotation")
    {
      src.write_value(1.0f, 0);
      sink.add_port_values(src);

      REQUIRE(sink.get_data().size() == 1);
      const auto* v = sink.get_data()[0].value.target<ossia::vec3f>();
      REQUIRE(v);
      CHECK((*v)[0] == 360.f);
      CHECK((*v)[1] == 360.f);
      CHECK((*v)[2] == 360.f);
    }
  });
}
