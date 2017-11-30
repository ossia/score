#include <Engine/Node/PdNode.hpp>
#include <array>
#include <tuple>
#include <utility>
#include <algorithm>
#include <map>
#include <tuple>
#include <bitset>
#include <random>
namespace Nodes
{


namespace LFO
{
struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "LFO";
    static const constexpr auto objectKey = "LFO";
    static const constexpr auto uuid = make_uuid("0697b807-f588-49b5-926c-f97701edd0d8");
  };

  // Idea: save internal state for rewind... ? -> require Copyable
  struct State
  {
      int64_t phase{};
  };

  enum Waveform
  {
    Sin, Triangle, Saw, Square, Noise1, Noise2, Noise3
  };

  static const constexpr auto info =
      Process::create_node()
      .value_outs({{"out"}})
      .controls(Process::FloatSlider{"Freq.", 0., 50., 1.}
              , Process::FloatSlider{"Coarse intens.", 0., 1000., 1.}
              , Process::FloatSlider{"Fine intens.", 0., 1., 1.}
              , Process::FloatSlider{"Offset.", -1000., 1000., 0.}
              , Process::FloatSlider{"Jitter", 0., 1., 0.}
              , Process::FloatSlider{"Phase", -1., 1., 0.}
              , Process::Enum{"Function", 0U,
                  Process::array("Sin", "Triangle", "Saw", "Square", "Noise 1", "Noise 2", "Noise 3")
                }
                )
      .state<State>()
      .build();

  static void run_precise(
      float freq, float coarse, float fine, float offset, float jitter, float phase, const std::string& type,
      ossia::value_port& out,
      State& s,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state&)
  {
    static const ossia::string_view_map<Waveform> map{{"Sin", Sin}, {"Triangle", Triangle}, {"Saw", Saw},
                                                   {"Square", Square},
                                                   {"Noise 1", Noise1}, {"Noise 2", Noise2}, {"Noise 3", Noise3} };

    static std::mt19937 rd;

    if(auto it = map.find(type); it != map.end())
    {
      float new_val{};
      auto ph = s.phase;
      if(jitter > 0)
      {
        ph += std::normal_distribution<float>(0., 5000.)(rd) * jitter;
      }

      const auto phi = phase + (2.f * float(M_PI) * freq * ph)/ 44100.f;

      switch(it->second)
      {
        case Sin:
          new_val = (coarse + fine) * std::sin(phi);
          break;
        case Triangle:
          new_val = (coarse + fine) * std::asin(std::sin(phi));
          break;
        case Saw:
          new_val = (coarse + fine) * std::atan(std::tan(phi));
          break;
        case Square:
          new_val = (coarse + fine) * ((std::sin(phi) > 0.f) ? 1.f : -1.f);
          break;
        case Noise1:
          new_val = std::uniform_real_distribution<float>(-(coarse + fine), coarse + fine)(rd);
          break;
        case Noise2:
          new_val = std::normal_distribution<float>(0, coarse + fine)(rd);
          break;
        case Noise3:
          new_val = std::cauchy_distribution<float>(0, coarse + fine)(rd);
          break;
      }
      out.add_value(new_val + offset);
  //    qDebug() << freq  ;
    }

    s.phase += (tk.date - prev_date);
  }
};

using Factories = Process::Factories<Node>;
}
}
