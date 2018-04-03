#pragma once
#include <Engine/Node/PdNode.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <numeric>
namespace Nodes
{
namespace ClassicalBeat
{
struct Node
{
    struct Metadata: Control::Meta_base
    {
        static const constexpr auto prettyName = "Classical Beat";
        static const constexpr auto objectKey = "ClassicalBeat";
        static const constexpr auto category = "Control";
        static const constexpr auto tags = std::array<const char*, 0>{};
        static const constexpr auto uuid = make_uuid("5d71816e-e081-4d85-b385-fb42472b41bf");

        static const constexpr auto value_outs = ossia::safe_nodes::value_outs<1>{{"out"}};

        static const constexpr auto controls =
            std::make_tuple(Control::Widgets::TempoChooser()
                            , Control::Widgets::TimeSigChooser()
                            );
    };

    static constexpr int64_t get_period(double quantif, double tempo, int sr)
    {
        const auto whole_dur = 240. / tempo;
        const auto whole_samples = whole_dur * sr;
        return quantif * whole_samples;
    }

    static constexpr ossia::time_value next_date(ossia::time_value cur_date, int64_t period)
    {
      return ossia::time_value{period * (1 + cur_date / period)};
    }

    using control_policy = ossia::safe_nodes::last_tick;
    static void run(
        float tempo,
        const Control::time_signature& sig,
        ossia::value_port& res,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st)
    {
      if(tk.date > prev_date)
      {
        if(prev_date == 0)
        {
          res.add_value(1, 0);
        }

        const auto period = get_period(1. / sig.second, (double)tempo, st.sampleRate);
        const auto next = next_date(prev_date, period);
        if(next.impl < tk.date.impl)
        {
          if(sig == Control::time_signature{3, 4})
          {
            ossia::tvalue t;
            if(next.impl == 0 || (next.impl % (3 * period)) == 0)
              t.value = 1;
            else
              t.value = 0;

            t.timestamp = next - prev_date;
            res.add_raw_value(std::move(t));
          }
          else if(sig == Control::time_signature{4, 4})
          {
            ossia::tvalue t;
            if(next.impl == 0 || next.impl % (2 * period) == 0)
              t.value = 1;
            else
              t.value = 0;

            t.timestamp = next - prev_date;
            res.add_raw_value(std::move(t));
          }
        }
      }
    }
};
}
}
