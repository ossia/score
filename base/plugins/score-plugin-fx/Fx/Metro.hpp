#pragma once
#include <Engine/Node/PdNode.hpp>
#include <numeric>
namespace Nodes
{
namespace Metro
{
struct Node
{
    struct Metadata: Control::Meta_base
    {
        static const constexpr auto prettyName = "Metro";
        static const constexpr auto objectKey = "Metro";
        static const constexpr auto category = "Control";
        static const constexpr auto tags = std::array<const char*, 0>{};
        static const constexpr auto uuid = make_uuid("50439197-521E-4ED0-A3B7-EDD8DEAEAC93");
        
        static const constexpr auto value_outs = Control::ValueOuts<1>{{"out"}};    
        
        static const constexpr auto controls = 
            std::make_tuple(Control::Widgets::MusicalDurationChooser(),
                            Control::Widgets::LFOFreqChooser(),
                            Control::ChooserToggle{"Quantify", {"Free", "Sync"}, false},
                            Control::Widgets::TempoChooser());
    };

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

    using control_policy = Control::LastTick;
    static void run(
            float quantif,
            float freq,
            bool val,
            float tempo,
            ossia::value_port& res,
            ossia::time_value prev_date,
            ossia::token_request tk,
            ossia::execution_state& st)
    {
        if(tk.date > prev_date)
        {
            const auto period = get_period(val, quantif, freq, tempo, st.sampleRate);
            const auto next = next_date(prev_date, period);
            if(next.impl < tk.date.impl)
            {
                ossia::tvalue t;
                t.value = ossia::impulse{};
                t.timestamp = next - prev_date;
                res.add_raw_value(t);
            }
        }
    }
};
}
}
