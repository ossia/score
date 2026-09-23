// What a state pushes to an address reaches the event inlets bound to that
// address in the tick the state runs in, as a cable would carry it.
//
// A state pushes its messages to the parameters; the parameters' callbacks
// queue the values for the event inlets reading them, and those queues are
// read at the start of a tick. The static graphs run the nodes without ports,
// the states, first, then read the queues again before running the others;
// the parallel graphs run them first too, on the calling thread. Every value is
// received once, whoever pushed it.

#include <ossia/dataflow/bench_state.hpp>
#include <ossia/dataflow/connection.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/nodes/state.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/logger.hpp>
#include <ossia/editor/state/message.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/generic/generic_parameter.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <ossia-config.hpp>

template <>
struct Catch::StringMaker<ossia::value>
{
  static std::string convert(const ossia::value& v)
  {
    return ossia::value_to_pretty_string(v);
  }
};

namespace
{
using values = std::vector<ossia::value>;

//! A node reading an address through an event inlet, as score binds a control
//! to the address that publishes it.
struct reader final : ossia::graph_node
{
  ossia::value_inlet* in{};
  values received;

  explicit reader(ossia::net::parameter_base& p)
  {
    in = new ossia::value_inlet;
    (*in)->is_event = true;
    in->address = &p;
    m_inlets.push_back(in);
  }

  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    for(const auto& v : (*in)->get_data())
      received.push_back(v.value);
  }
  [[nodiscard]] std::string label() const noexcept override { return "reader"; }
};

//! A node with ports that pushes to a parameter while it runs.
struct pusher final : ossia::graph_node
{
  ossia::net::parameter_base& param;
  ossia::value value;

  pusher(ossia::net::parameter_base& p, ossia::value v)
      : param{p}
      , value{std::move(v)}
  {
    m_outlets.push_back(new ossia::value_outlet);
  }

  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    param.push_value(value);
  }
  [[nodiscard]] std::string label() const noexcept override { return "pusher"; }
};

//! A node writing a value on its outlet each time it runs.
struct writer final : ossia::graph_node
{
  ossia::value_outlet* out{};
  explicit writer()
  {
    out = new ossia::value_outlet;
    m_outlets.push_back(out);
  }
  void run(const ossia::token_request&, ossia::exec_state_facade) noexcept override
  {
    (*out)->write_value(9.f, 0);
  }
  [[nodiscard]] std::string label() const noexcept override { return "writer"; }
};

std::shared_ptr<ossia::nodes::state_writer>
make_state(ossia::net::parameter_base& p, const values& vals)
{
  auto st = std::make_shared<ossia::nodes::state_writer>();
  for(const auto& v : vals)
    st->data.add(ossia::message{ossia::destination{p}, v});
  return st;
}

struct config
{
  const char* name;
  ossia::graph_setup_options opt;
  //! How many ticks after the state's the inlets get what it pushed.
  int delay;
};

std::vector<config> configs()
{
  std::vector<config> res;
  using opts = ossia::graph_setup_options;
#if defined(OSSIA_PARALLEL)
  const bool parallel[] = {false, true};
#else
  const bool parallel[] = {false};
#endif
  for(bool par : parallel)
  {
    for(auto sched : {opts::StaticFixed, opts::StaticBFS, opts::StaticTC})
    {
      for(int variant = 0; variant < 4; variant++)
      {
        ossia::graph_setup_options opt;
        opt.scheduling = sched;
        opt.parallel = par;
        opt.parallel_threads = 2;
        if(variant & 1)
          opt.log = ossia::logger_ptr();
        if(variant & 2)
          opt.bench = std::make_shared<ossia::bench_state>();
        res.push_back({par ? "parallel" : "static", opt, 0});
      }
    }
  }

  // Runs the states alongside the other nodes: what they push comes a tick
  // later. score never uses it: its execution always asks for StaticFixed.
  {
    ossia::graph_setup_options opt;
    opt.scheduling = opts::Dynamic;
    res.push_back({"dynamic", opt, 1});
  }
  return res;
}

struct fixture
{
  ossia::net::generic_device device{"test"};
  ossia::net::parameter_base* x{};
  ossia::net::parameter_base* y{};
  ossia::execution_state st;
  std::shared_ptr<ossia::graph_interface> graph;

  explicit fixture(const ossia::graph_setup_options& opt)
  {
    x = device.create_child("x")->create_parameter(ossia::val_type::FLOAT);
    y = device.create_child("y")->create_parameter(ossia::val_type::FLOAT);
    st.register_device(&device);
    st.apply_device_changes();
    graph = ossia::make_graph(opt);
  }

  ~fixture()
  {
    for(auto n : graph->get_nodes())
      for(auto in : n->root_inputs())
        st.unregister_port(*in);
    graph->clear();
    st.unregister_device(&device);
    st.apply_device_changes();
  }

  std::shared_ptr<reader> add_reader(ossia::net::parameter_base& p)
  {
    auto r = std::make_shared<reader>(p);
    graph->add_node(r);
    st.register_port(*r->in);
    return r;
  }

  //! One tick in which the given nodes run.
  void tick(std::initializer_list<ossia::graph_node*> running)
  {
    st.begin_tick();
    for(auto n : running)
      n->request(ossia::token_request{});
    graph->state(st);
    st.commit();
  }
};
}

TEST_CASE(
    "A value a state pushes reaches the event inlets reading it in the tick the "
    "state runs, once",
    "[dataflow][state][execution_state]")
{
  for(const auto& c : configs())
  {
    DYNAMIC_SECTION(
        c.name << " graph, scheduling " << int(c.opt.scheduling) << ", log "
               << bool(c.opt.log) << ", bench " << bool(c.opt.bench))
    {
      fixture f{c.opt};
      auto r = f.add_reader(*f.x);
      auto s = make_state(*f.x, {5.f});
      f.graph->add_node(s);

      std::vector<values> seen;
      for(int t = 0; t < 3; t++)
      {
        r->received.clear();
        if(t == 0)
          f.tick({s.get(), r.get()});
        else
          f.tick({r.get()});
        seen.push_back(r->received);
      }

      std::vector<values> expected(3);
      expected[c.delay] = {5.f};
      CHECK(seen == expected);
    }
  }
}

TEST_CASE(
    "What reaches an address from elsewhere is received with what the states push, "
    "once",
    "[dataflow][state][execution_state]")
{
  for(const auto& c : configs())
  {
    if(c.delay != 0)
      continue;
    DYNAMIC_SECTION(
        c.name << " graph, scheduling " << int(c.opt.scheduling) << ", log "
               << bool(c.opt.log) << ", bench " << bool(c.opt.bench))
    {
      fixture f{c.opt};
      auto r = f.add_reader(*f.x);

      SECTION("Pushed before the tick: received first")
      {
        auto s = make_state(*f.x, {5.f});
        f.graph->add_node(s);
        f.x->push_value(1.f);
        f.tick({s.get(), r.get()});
        CHECK(r->received == values{1.f, 5.f});

        r->received.clear();
        f.tick({r.get()});
        CHECK(r->received.empty());
      }

      SECTION("Pushed from another thread")
      {
        auto s = make_state(*f.x, {5.f});
        f.graph->add_node(s);
        std::thread{[&] { f.x->push_value(7.f); }}.join();
        f.tick({s.get(), r.get()});
        CHECK(r->received == values{7.f, 5.f});

        r->received.clear();
        f.tick({r.get()});
        CHECK(r->received.empty());
      }

      SECTION("Pushed twice by a state: both, in order")
      {
        auto s = make_state(*f.x, {5.f, 6.f});
        f.graph->add_node(s);
        f.tick({s.get(), r.get()});
        CHECK(r->received == values{5.f, 6.f});
      }

      SECTION("Pushed by a state to another address: not received here")
      {
        auto s = make_state(*f.y, {5.f});
        f.graph->add_node(s);
        f.tick({s.get(), r.get()});
        CHECK(r->received.empty());
        r->received.clear();
        f.tick({r.get()});
        CHECK(r->received.empty());
      }

      SECTION("A state that does not run pushes nothing")
      {
        auto s = make_state(*f.x, {5.f});
        f.graph->add_node(s);
        f.tick({r.get()});
        CHECK(r->received.empty());
      }

      SECTION("Pushed by a node with ports while it runs: received in the next tick")
      {
        // Its value is only known once it ran, which may be after the inlet's.
        auto p = std::make_shared<pusher>(*f.x, ossia::value{3.f});
        f.graph->add_node(p);
        f.tick({p.get(), r.get()});
        CHECK(r->received.empty());

        r->received.clear();
        f.tick({r.get()});
        CHECK(r->received == values{3.f});

        r->received.clear();
        f.tick({r.get()});
        CHECK(r->received.empty());
      }

      SECTION("A state added or removed while playing")
      {
        f.tick({r.get()});
        auto s = make_state(*f.x, {5.f});
        f.graph->add_node(s);
        r->received.clear();
        f.tick({s.get(), r.get()});
        CHECK(r->received == values{5.f});

        auto s2 = make_state(*f.x, {6.f});
        f.graph->add_node(s2);
        f.graph->remove_node(s);
        r->received.clear();
        f.tick({s2.get(), r.get()});
        CHECK(r->received == values{6.f});

        f.graph->remove_node(s2);
        r->received.clear();
        f.tick({r.get()});
        CHECK(r->received.empty());
      }

      SECTION("A cable still orders the nodes it joins, and wins over the address")
      {
        auto w = std::make_shared<writer>();
        f.graph->add_node(w);
        f.graph->connect(f.graph->allocate_edge(
            ossia::immediate_strict_connection{}, w->out, r->in, w, r));
        auto s = make_state(*f.x, {5.f});
        f.graph->add_node(s);
        for(int t = 0; t < 3; t++)
        {
          r->received.clear();
          f.tick({w.get(), s.get(), r.get()});
          CHECK(r->received == values{9.f});
        }
      }

      SECTION("Several inlets on the address each receive the value once")
      {
        auto r2 = f.add_reader(*f.x);
        auto s = make_state(*f.x, {5.f});
        f.graph->add_node(s);
        f.tick({s.get(), r.get(), r2.get()});
        CHECK(r->received == values{5.f});
        CHECK(r2->received == values{5.f});
      }
    }
  }
}

TEST_CASE(
    "Receiving the new values adds them to the tick's without clearing them",
    "[dataflow][execution_state]")
{
  ossia::net::generic_device device{"test"};
  auto x = device.create_child("x")->create_parameter(ossia::val_type::FLOAT);
  ossia::execution_state st;
  st.register_device(&device);
  st.apply_device_changes();

  ossia::value_inlet in;
  in->is_event = true;
  in.address = x;
  st.register_port(in);

  auto pull = [&] {
    in->clear();
    st.copy_from_global(*x, in);
    values res;
    for(const auto& v : in->get_data())
      res.push_back(v.value);
    return res;
  };

  x->push_value(1.f);
  st.begin_tick();
  CHECK(pull() == values{1.f});

  // Nothing queued: what the tick received stays.
  st.receive_new_values();
  CHECK(pull() == values{1.f});

  x->push_value(2.f);
  st.receive_new_values();
  CHECK(pull() == values{1.f, 2.f});

  st.receive_new_values();
  CHECK(pull() == values{1.f, 2.f});

  // The next tick does not give them again.
  st.begin_tick();
  CHECK(pull().empty());

  st.unregister_port(in);
  st.unregister_device(&device);
  st.apply_device_changes();
}
