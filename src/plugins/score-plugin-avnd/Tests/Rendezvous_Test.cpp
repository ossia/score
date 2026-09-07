#include <AvndProcesses/Rendezvous.hpp>
#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <catch2/catch_all.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace
{
// Exercise native dynamic-port binding so selection observes every arrival,
// including multiple values at an identical timestamp within one tick.
struct RendezvousHost
{
  avnd_tools::Rendezvous object;
  std::vector<std::unique_ptr<ossia::value_inlet>> storage;
  std::vector<ossia::value_inlet*> inlets;
  std::vector<std::vector<ossia::value>> emitted;

  explicit RendezvousHost(int count = 2)
  {
    object.outputs.out.call.context = this;
    object.outputs.out.call.function
        = [](void* context, const std::vector<ossia::value>& values) {
      static_cast<RendezvousHost*>(context)->emitted.push_back(values);
    };
    object.inputs.in_i.request_port_resize = [this](int count) { resize(count); };
    resize(count);
  }

  template <typename Field, typename Value, std::size_t Index>
  bool from_ossia_value(
      Field& field, const ossia::value& source, Value& destination,
      avnd::field_index<Index>)
  {
    oscr::from_ossia_value(field, source, destination);
    return true;
  }

  void resize(int count)
  {
    object.inputs.controller.value = count;
    object.inputs.controller.update(object);
    storage.clear();
    inlets.clear();
    for(int i = 0; i < count; ++i)
    {
      storage.push_back(std::make_unique<ossia::value_inlet>());
      inlets.push_back(storage.back().get());
    }
    // Executor resizes the processor ports before graph processing resumes.
    object.inputs.in_i.ports.resize(count);
  }

  void event(std::size_t inlet, ossia::value value)
  {
    inlets.at(inlet)->data.write_value(std::move(value), 0);
  }

  void tick()
  {
    oscr::process_before_run<RendezvousHost, avnd_tools::Rendezvous> preprocess{
        *this, object};
    preprocess(object.inputs.in_i, inlets, avnd::field_index<2>{});
    object();
    for(auto* inlet : inlets)
      inlet->data.get_data().clear();
  }
};
}

TEST_CASE("Rendezvous waits for fresh events on every input", "[avnd][rendezvous]")
{
  RendezvousHost host{3};
  host.tick();
  REQUIRE(host.emitted.empty());
  REQUIRE(host.object.outputs.waiting.value == 3);

  host.event(2, "third");
  host.tick();
  REQUIRE(host.emitted.empty());
  REQUIRE(host.object.outputs.waiting.value == 2);

  // Repeated configuration of the unchanged count must not erase partial work.
  host.object.inputs.controller.update(host.object);
  host.tick();
  host.event(0, 1);
  host.tick();
  REQUIRE(host.object.outputs.waiting.value == 1);
  host.event(1, 2.f);
  host.tick();
  REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{1, 2.f, "third"}});
  REQUIRE(host.object.outputs.waiting.value == 3);

  for(int i = 0; i < 4; ++i)
    host.tick();
  REQUIRE(host.emitted.size() == 1);
  host.event(1, 2.f);
  host.tick();
  REQUIRE(host.emitted.size() == 1);
  REQUIRE(host.object.outputs.waiting.value == 2);
}

TEST_CASE("Rendezvous accepts equal-valued complete cycles", "[avnd][rendezvous]")
{
  RendezvousHost host;
  for(int cycle = 0; cycle < 2; ++cycle)
  {
    host.event(0, 123);
    host.event(1, "same");
    host.tick();
    REQUIRE(host.emitted.size() == cycle + 1);
    REQUIRE(host.emitted.back() == std::vector<ossia::value>{123, "same"});
    host.tick();
    REQUIRE(host.emitted.size() == cycle + 1);
  }
}

TEST_CASE(
    "Rendezvous replaces pending values without adding readiness", "[avnd][rendezvous]")
{
  RendezvousHost host;
  host.event(0, 1);
  host.tick();
  host.event(0, 2);
  host.event(0, 3);
  host.tick();
  REQUIRE(host.emitted.empty());
  REQUIRE(host.object.outputs.waiting.value == 1);
  host.event(1, "last");
  host.tick();
  REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{3, "last"}});
}

TEST_CASE("Rendezvous keeps the first fresh value when selected", "[avnd][rendezvous]")
{
  RendezvousHost host;
  host.object.inputs.keep_first.value = true;
  host.event(0, "first");
  host.event(0, "same tick");
  host.tick();
  host.event(0, "later tick");
  host.tick();
  host.event(1, 1);
  host.event(1, 2);
  host.tick();
  REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{"first", 1}});

  // Completing a cycle releases the retained values for the next cycle.
  host.event(0, "next");
  host.event(1, 3);
  host.tick();
  REQUIRE(host.emitted.back() == std::vector<ossia::value>{"next", 3});
}

TEST_CASE("Rendezvous retention changes affect future arrivals", "[avnd][rendezvous]")
{
  RendezvousHost host;
  host.event(0, 1);
  host.tick();
  host.object.inputs.keep_first.value = true;
  host.event(0, 2);
  host.tick();
  host.object.inputs.keep_first.value = false;
  host.event(0, 3);
  host.event(1, "complete");
  host.tick();
  REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{3, "complete"}});
}

TEST_CASE("Rendezvous distinguishes impulse and null from absence", "[avnd][rendezvous]")
{
  RendezvousHost host;
  host.event(0, ossia::impulse{});
  host.tick();
  REQUIRE(host.emitted.empty());
  host.tick();
  REQUIRE(host.emitted.empty());
  host.event(1, ossia::value{});
  host.tick();
  REQUIRE(
      host.emitted
      == std::vector<std::vector<ossia::value>>{{ossia::impulse{}, ossia::value{}}});
  host.tick();
  REQUIRE(host.emitted.size() == 1);
}

TEST_CASE(
    "Rendezvous Clear discards the partial cycle and same-tick arrivals",
    "[avnd][rendezvous]")
{
  RendezvousHost host;
  host.event(0, "old");
  host.tick();
  host.object.inputs.clear.value.emplace();
  host.event(1, "discarded");
  host.tick();
  REQUIRE(host.emitted.empty());
  REQUIRE(host.object.outputs.waiting.value == 2);

  host.event(1, "new second");
  host.tick();
  REQUIRE(host.emitted.empty());
  // The impulse was consumed; idle ticks must not clear subsequent work.
  host.tick();
  host.event(0, "new first");
  host.tick();
  REQUIRE(
      host.emitted
      == std::vector<std::vector<ossia::value>>{{"new first", "new second"}});
}

TEST_CASE("Rendezvous resizing discards partial cycles", "[avnd][rendezvous]")
{
  RendezvousHost host;
  host.event(0, "old");
  host.tick();

  SECTION("growing")
  {
    decltype(host.object.inputs.controller)::on_controller_interaction()(host.object, 3);
    host.event(1, "second");
    host.event(2, "third");
    host.tick();
    REQUIRE(host.emitted.empty());
    host.event(0, "first");
    host.tick();
    REQUIRE(
        host.emitted
        == std::vector<std::vector<ossia::value>>{{"first", "second", "third"}});
  }

  SECTION("shrinking")
  {
    host.resize(1);
    host.tick();
    REQUIRE(host.emitted.empty());
    host.event(0, "fresh");
    host.tick();
    REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{"fresh"}});
  }

  SECTION("resizing away and back before the next tick")
  {
    host.resize(3);
    host.resize(2);
    host.event(1, "second");
    host.tick();
    REQUIRE(host.emitted.empty());
    host.event(0, "fresh");
    host.tick();
    REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{"fresh", "second"}});
  }

  SECTION("host-driven port vector changes without a controller update")
  {
    host.inlets.pop_back();
    host.tick();
    REQUIRE(host.emitted.empty());
    host.event(0, "fresh");
    host.tick();
    REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{"fresh"}});
  }
}

TEST_CASE("Rendezvous with zero inputs never auto-emits", "[avnd][rendezvous]")
{
  RendezvousHost host{0};
  host.tick();
  host.tick();
  REQUIRE(host.emitted.empty());
  host.resize(1);
  host.event(0, 42);
  host.tick();
  REQUIRE(host.emitted == std::vector<std::vector<ossia::value>>{{42}});
  host.resize(0);
  host.tick();
  host.object.inputs.clear.value.emplace();
  host.tick();
  REQUIRE(host.emitted.size() == 1);
}
