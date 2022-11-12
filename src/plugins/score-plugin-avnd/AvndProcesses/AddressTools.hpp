#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/network/common/path.hpp>

#include <halp/controls.hpp>

namespace avnd_tools
{

// Given:
// address: /foo.*/value
//  -> /foo.1/value, /foo.2/value, etc.
// input: [1, 34, 6, 4]
// -> writes 1 on foo.1/value, 34 on foo.2/value, etc

struct PatternUnfolder
{
  halp_meta(name, "Pattern applier")
  halp_meta(c_name, "avnd_pattern_apply")
  halp_meta(uuid, "44a55ee1-c2c9-43d5-a655-8eaedaff394c")

  ossia::exec_state_facade ossia_state;

  struct
  {
    halp::val_port<"Input", ossia::value> input;
    struct : halp::lineedit<"Pattern", "">
    {
      void update(PatternUnfolder& p)
      {
        if(!p.ossia_state.impl)
          return;
        p.m_path = ossia::traversal::make_path(value);

        ossia::execution_state& st = *p.ossia_state.impl;
        const auto& rdev = st.exec_devices();
        p.roots.clear();
        for(auto& dev : rdev)
        {
          p.roots.push_back(&dev->get_root_node());
        }
        ossia::traversal::apply(*p.m_path, p.roots);
      }
    } pattern;
  } inputs;

  struct
  {

  } outputs;

  std::optional<ossia::traversal::path> m_path;

  std::vector<ossia::net::node_base*> roots;
  void operator()()
  {
    if(!m_path)
      return;

    auto process = [this](const std::vector<ossia::value>& vec) {
      QTimer::singleShot(1, qApp, [roots = this->roots, vec] {
        const auto N = std::min(roots.size(), vec.size());
        for(std::size_t i = 0; i < N; i++)
        {
          if(auto p = roots[i]->get_parameter())
          {
            // Needs to be done in main thread because of the QJSEngine in serial_protocols
            p->push_value(vec[i]);
          }
        }
      });
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
