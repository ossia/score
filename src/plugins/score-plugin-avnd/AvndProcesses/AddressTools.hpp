#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/device.hpp>
#include <ossia/network/common/path.hpp>

#include <boost/algorithm/string.hpp>

#include <AvndProcesses/Alphanum.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace avnd_tools
{

// Given:
// address: /foo.*/value
//  -> /foo.1/value, /foo.2/value, etc.
// input: [1, 34, 6, 4]
// -> writes 1 on foo.1/value, 34 on foo.2/value, etc

struct PatternObject
{
  ossia::exec_state_facade ossia_state;
  std::vector<ossia::traversal::path> m_paths;
  std::vector<ossia::net::node_base*> roots;
};

// Watches the execution devices for tree changes. A Nano::Observer, so that
// its connections go away with whichever of it or the device dies first: the
// devices it stops observing (no longer executed, possibly already destroyed)
// cannot keep a slot pointing at a freed selector. A copy starts unconnected.
struct PatternDeviceObserver : Nano::Observer
{
  PatternDeviceObserver() = default;
  PatternDeviceObserver(const PatternDeviceObserver&) noexcept { }
  PatternDeviceObserver& operator=(const PatternDeviceObserver&) noexcept { return *this; }

  void mark_dirty(ossia::net::node_base&) { dirty = true; }

  bool dirty = false;
  ossia::flat_set<ossia::net::device_base*> devices;
};

struct PatternSelector : halp::lineedit<"Pattern", "">
{
  void update(PatternObject& p)
  {
    current_object = &p;
    if(!p.ossia_state.impl)
      return;
    if(value.empty())
    {
      p.m_paths.clear();
      p.roots.clear();
      return;
    }

    p.m_paths.clear();
    std::vector<std::string> results;
    boost::split(results, value, [](char c){ return c == '\n' || c == '\r'; });

    for(auto& res : results) {
      if(!res.empty())
        if(auto path = ossia::traversal::make_path(res))
        {
          p.m_paths.push_back(std::move(*path));
        }
    }

    reprocess();
  }

  void reprocess()
  {
    if(!current_object)
      return;

    auto& p = *current_object;
    ossia::execution_state& st = *p.ossia_state.impl;
    const auto& rdev = st.exec_devices();
    p.roots.clear();

    if(p.m_paths.empty())
      return;

    auto& observed = observer.devices;
    for(auto& dev : rdev)
    {
      if(observed.find(dev) == observed.end())
      {
        observed.insert(dev);
        dev->on_node_created.connect<&PatternDeviceObserver::mark_dirty>(observer);
        dev->on_node_removing.connect<&PatternDeviceObserver::mark_dirty>(observer);
      }
      p.roots.push_back(&dev->get_root_node());
    }

    // Not disconnected here: such a device may already be gone. A live one
    // stays connected until the observer dies, which is harmless.
    for(auto it = observed.begin(); it != observed.end();)
    {
      if(!ossia::contains(rdev, *it))
        it = observed.erase(it);
      else
        ++it;
    }

    const auto orig_roots = p.roots;
    std::vector<ossia::net::node_base*> result;
    std::vector<ossia::net::node_base*> total;
    for(auto& path : p.m_paths) {
      result.assign(orig_roots.begin(), orig_roots.end());

      ossia::traversal::apply(path, result);
      total.insert(total.end(), result.begin(), result.end());
    }

    ossia::remove_duplicates(total); // First remove pointer duplicates
    ossia::sort( // Then alphanum sort
        total,
        [](ossia::net::node_base* lhs, ossia::net::node_base* rhs) {
      return doj::alphanum_compare{}(lhs->osc_address(), rhs->osc_address());
    });

    std::swap(p.roots, total);

    observer.dirty = false;
  }

  PatternObject* current_object{};
  PatternDeviceObserver observer;
};

struct PatternUnfolder : PatternObject
{
  halp_meta(name, "Pattern applier")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data Processing")
  halp_meta(description, "Send a message to all nodes matching a pattern")
  halp_meta(c_name, "avnd_pattern_apply")
  halp_meta(uuid, "44a55ee1-c2c9-43d5-a655-8eaedaff394c")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pattern-applier.html#pattern-applier")

  struct
  {
    halp::val_port<"Input", ossia::value> input;
    PatternSelector pattern;
  } inputs;

  struct
  {

  } outputs;

  void operator()()
  {
    if(m_paths.empty())
      return;

    auto process = [this](const std::vector<ossia::value>& vec) {
      const auto N = std::min(roots.size(), vec.size());
      for(std::size_t i = 0; i < N; i++)
      {
        if(auto p = roots[i]->get_parameter())
        {
          p->push_value(vec[i]);
        }
      }
    };

    if(auto vvec = inputs.input.value.target<std::vector<ossia::value>>())
    {
      process(*vvec);
    }
    else
    {
      process(ossia::convert<std::vector<ossia::value>>(inputs.input.value));
    }
  }
};
}
