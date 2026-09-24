// An execution graph whose immediate cables form a cycle (fixM, N63, execution
// side). graph_static::sort_all_nodes used to swallow the topological-sort
// failure silently, leaving no node to execute. It must now log an error that
// names the nodes of the cycle; a delayed cable closing the same loop is legal
// and must execute without the error.
#include <ossia/dataflow/connection.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_static.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/logger.hpp>

#include <spdlog/sinks/ostream_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <sstream>

namespace
{
struct labelled_node final : ossia::graph_node
{
  std::string name;
  int runs = 0;

  explicit labelled_node(std::string n)
      : name{std::move(n)}
  {
    m_inlets.push_back(new ossia::value_inlet);
    m_outlets.push_back(new ossia::value_outlet);
  }
  std::string label() const noexcept override { return name; }

  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    ++runs;
  }
};

struct log_capture
{
  std::ostringstream stream;
  std::shared_ptr<spdlog::sinks::ostream_sink_mt> sink
      = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
  log_capture() { ossia::logger().sinks().push_back(sink); }
  ~log_capture()
  {
    auto& sinks = ossia::logger().sinks();
    sinks.erase(std::remove(sinks.begin(), sinks.end(), sink), sinks.end());
  }
};

struct cycle_result
{
  std::string log;
  int runs_a = 0, runs_b = 0;
};

cycle_result run_cycle(ossia::connection back)
{
  cycle_result r;
  log_capture capture;
  {
    auto g = std::make_unique<ossia::tc_graph>();
    auto e = std::make_unique<ossia::execution_state>();
    auto a = std::make_shared<labelled_node>("fixm-node-alpha");
    auto b = std::make_shared<labelled_node>("fixm-node-beta");
    g->add_node(a);
    g->add_node(b);
    g->connect(g->allocate_edge(
        ossia::immediate_glutton_connection{}, a->root_outputs()[0],
        b->root_inputs()[0], a, b));
    g->connect(g->allocate_edge(
        std::move(back), b->root_outputs()[0], a->root_inputs()[0], b, a));

    for(int i = 0; i < 3; i++)
    {
      a->request(ossia::token_request{});
      b->request(ossia::token_request{});
      e->begin_tick();
      g->state(*e);
      e->commit();
    }
    r.runs_a = a->runs;
    r.runs_b = b->runs;
    g->clear();
  }
  ossia::logger().flush();
  r.log = capture.stream.str();
  return r;
}
}

TEST_CASE("an immediate cycle in the execution graph is reported", "[dataflow][graph][fixm]")
{
  const auto r = run_cycle(ossia::immediate_glutton_connection{});
  INFO(r.log);
  CHECK(r.log.find("not a DAG") != std::string::npos);
  CHECK(r.log.find("fixm-node-alpha") != std::string::npos);
  CHECK(r.log.find("fixm-node-beta") != std::string::npos);
  CHECK(r.runs_a == 0);
  CHECK(r.runs_b == 0);
}

TEST_CASE("a delayed cable closing the loop executes silently", "[dataflow][graph][fixm]")
{
  const auto r = run_cycle(ossia::delayed_glutton_connection{});
  INFO(r.log);
  CHECK(r.log.find("not a DAG") == std::string::npos);
  CHECK(r.runs_a == 3);
  CHECK(r.runs_b == 3);
}
