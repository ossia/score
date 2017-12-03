#pragma once
#include <Engine/Node/PdNode.hpp>
#include <numeric>
namespace Nodes
{
namespace Metro
{
struct Node
{
    struct Metadata
    {
        static const constexpr auto prettyName = "Metro";
        static const constexpr auto objectKey = "Metro";
        static const constexpr auto category = "Control";
        static const constexpr auto tags = std::array<const char*, 0>{};
        static const constexpr auto uuid = make_uuid("50439197-521E-4ED0-A3B7-EDD8DEAEAC93");
    };

    static const constexpr auto info =
            Process::create_node()
            .value_outs({{"out"}})
            .controls(Process::Widgets::MusicalDurationChooser(),
                      Process::Widgets::LFOFreqChooser(),
                      Process::ChooserToggle{"Quantify", {"Free", "Sync"}, false}
                      )
            .build();

    static constexpr int64_t get_period(bool use_tempo, double quantif, double freq, double tempo, int sr)
    {
      if(use_tempo)
      {
        const auto whole_dur = 240. / tempo;
        const auto whole_samples = whole_dur * sr;
        return quantif * whole_samples;
      }
      else
      {
        return sr / freq;
      }
    }

    static constexpr ossia::time_value next_date(ossia::time_value cur_date, int64_t period)
    {
        return ossia::time_value{period * (1 + cur_date / period)};
    }

    template<typename T>
    static auto get_value_or(ossia::execution_state& st, std::string_view v, T val)
    {
        if(auto node = st.find_node(v))
            return ossia::convert<T>(node->get_parameter()->value());
        return val;


    }
    static void run_last(
            float quantif,
            float freq,
            bool val,
            ossia::value_port& res,
            ossia::time_value prev_date,
            ossia::token_request tk,
            ossia::execution_state& st)
    {
        if(tk.date > prev_date)
        {
            double tempo = 120.;
            if(auto tempo_node = st.find_node("/tempo"))
                tempo = ossia::convert<float>(tempo_node->get_parameter()->value());

            const auto period = get_period(val, quantif, freq, tempo, st.sampleRate);
            const auto next = next_date(prev_date, period);
            if(next < tk.date)
            {
                ossia::tvalue t;
                t.value = ossia::impulse{};
                t.timestamp = next - prev_date;
                res.add_value(t);
            }
        }
    }
};

using Factories = Process::Factories<Node>;
}
}
