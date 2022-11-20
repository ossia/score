#pragma once
#include <AvndProcesses/AddressTools.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/network/common/path.hpp>

#include <QCoreApplication>
#include <QTimer>

#include <halp/audio.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

namespace avnd_tools
{

// Given:
// address: /foo.*/value
//  -> /foo.1/value, /foo.2/value, etc.
// input: [1, 34, 6, 4]
// -> writes 1 on foo.1/value, 34 on foo.2/value, etc

struct PatternSweeper : PatternObject
{
  halp_meta(name, "Sweeper")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Sweep a message to all nodes matching a pattern")
  halp_meta(c_name, "avnd_pattern_sweep")
  halp_meta(uuid, "55f4fe13-f71b-4c20-ac32-74668e82664c")

  struct
  {
    halp::val_port<"Input", ossia::value> input;
    halp::time_chooser<"Interval"> time;
    PatternSelector pattern;
  } inputs;

  struct
  {

  } outputs;


  std::size_t current_parameter = 0;
  double last_message_sent_pos = std::numeric_limits<double>::lowest();

  using tick = halp::tick_musical;
  void operator()(const halp::tick_musical& tk)
  {
    if(!m_path || this->roots.empty())
      return;

    auto elapsed_ns = (tk.position_in_nanoseconds - last_message_sent_pos);
    if(elapsed_ns < inputs.time.value * 1e9)
      return;
    last_message_sent_pos = tk.position_in_nanoseconds;

    auto& v = inputs.input.value;
    if(current_parameter >= 0 && current_parameter < this->roots.size())
    {
      if(auto p = roots[current_parameter]->get_parameter())
      {
        QTimer::singleShot(1, qApp, [p, v] { p->push_value(v); });
      }
      current_parameter++;
      current_parameter = current_parameter % this->roots.size();
    }
  }
};
}
