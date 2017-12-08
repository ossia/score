#pragma once
#include <Engine/Node/PdNode.hpp>
#undef slots
#include <frozen/unordered_map.h>
namespace Nodes
{

namespace MidiUtil
{

enum scale: int8_t
{
  all,
  ionian, dorian, phyrgian, lydian, mixolydian, aeolian, locrian,

  I, II, III, IV, V, VI, VII,


  SCALES_MAX // always at end, used for counting
};


template<typename T>
constexpr void constexpr_swap(T& a, T& b)
{
  T tmp = a;
  a = b;
  b = tmp;
}

template <typename T>
constexpr void constexpr_rotate(T first, T middle, T last)
{
  using namespace std;
  T next = middle;
  while (first != next)
  {
    constexpr_swap(*first++,*next++);
    if (next == last)
      next = middle;
    else if (first == middle) 
      middle = next;
  }
}

using scale_array = std::array<bool, 128>;
using scales_array = std::array<scale_array, 12>;
constexpr scales_array make_scale(std::initializer_list<bool> notes)
{
  std::array<scale_array, 12> r{};
  for(std::size_t octave = 0; octave < 11; octave++)
  {
    std::size_t pos = 0;
    for(bool note : notes)
    {
      if(octave * 12 + pos < 128)
      {
        r[0][octave * 12 + pos] = note;
        pos++;
      }
    }
  }

  for(std::size_t octave = 1; octave < 12; octave++)
  {
    r[octave] = r[0];
    constexpr_rotate(r[octave].rbegin(), r[octave].rbegin() + octave, r[octave].rend());
  }
  return r;
}

constexpr int get_scale(QLatin1String s)
{
  using namespace std::literals;
  if(Process::same(s, QLatin1String("all"))) return scale::all;
  else if(Process::same(s, QLatin1String("ionian"))) return scale::ionian;
  else if(Process::same(s, QLatin1String("dorian"))) return scale::dorian;
  else if(Process::same(s, QLatin1String("phyrgian"))) return scale::phyrgian;
  else if(Process::same(s, QLatin1String("lydian"))) return scale::lydian;
  else if(Process::same(s, QLatin1String("mixolydian"))) return scale::mixolydian;
  else if(Process::same(s, QLatin1String("aeolian"))) return scale::aeolian;
  else if(Process::same(s, QLatin1String("locrian"))) return scale::locrian;
  else if(Process::same(s, QLatin1String("I"))) return scale::I;
  else if(Process::same(s, QLatin1String("II"))) return scale::II;
  else if(Process::same(s, QLatin1String("III"))) return scale::III;
  else if(Process::same(s, QLatin1String("IV"))) return scale::IV;
  else if(Process::same(s, QLatin1String("V"))) return scale::V;
  else if(Process::same(s, QLatin1String("VI"))) return scale::VI;
  else if(Process::same(s, QLatin1String("VII"))) return scale::VII;
  else throw;
}
static Q_DECL_RELAXED_CONSTEXPR frozen::unordered_map<int, scales_array, scale::SCALES_MAX> scales{
    //                                     C   D   E F   G   A   B
    { scale::all,        make_scale({ 1,1,1,1,1,1,1,1,1,1,1,1 })}
  , { scale::ionian,     make_scale({ 1,0,1,0,1,1,0,1,0,1,0,1 })}
  , { scale::dorian,     make_scale({ 1,0,1,1,0,1,0,1,0,1,1,0 })}
  , { scale::phyrgian,   make_scale({ 1,1,0,1,0,1,0,1,1,0,1,0 })}
  , { scale::lydian,     make_scale({ 1,0,1,0,1,0,1,1,0,1,0,1 })}
  , { scale::mixolydian, make_scale({ 1,0,1,0,1,1,0,1,0,1,1,0 })}
  , { scale::aeolian,    make_scale({ 1,0,1,1,0,1,0,1,1,0,1,0 })}
  , { scale::locrian,    make_scale({ 1,1,0,1,0,1,1,0,1,0,1,0 })}
  , { scale::I,          make_scale({ 1,0,0,0,1,0,0,1,0,0,0,0 })}
  , { scale::II,         make_scale({ 0,0,1,0,0,1,0,0,0,1,0,0 })}
  , { scale::III,        make_scale({ 0,0,0,0,1,0,0,1,0,0,0,1 })}
  , { scale::IV,         make_scale({ 1,0,0,0,0,1,0,0,0,1,0,0 })}
  , { scale::V,          make_scale({ 0,0,1,0,0,0,0,1,0,0,0,1 })}
  , { scale::VI,         make_scale({ 1,0,0,0,1,0,0,0,0,1,0,0 })}
  , { scale::VII,        make_scale({ 0,0,1,0,0,1,0,0,0,0,0,1 })}
};

static
std::size_t find_closest_index(const scale_array& arr, std::size_t i)
{
  if(arr[i] == 1)
    return i;

  switch(i)
  {
    case 0:
      while(i != 12)
      {
        i++;
        if(arr[i] == 1)
          return i;
      }
      break;

    case 12:
      while(i != 0)
      {
        i--;
        if(arr[i] == 1)
          return i;
      }
      break;

    default:
    {
      std::size_t r = 0;
      while((i-r) != 0 && (i+r) != 12)
      {
        if(arr[i + r] == 1)
          return i + r;
        else if(arr[i - r] == 1)
          return i - r;
        r++;
      }

      break;
    }
  }

  SCORE_ABORT;
}

struct Node
{
  struct Metadata
  {
    static const constexpr auto prettyName = "Midi scale";
    static const constexpr auto objectKey = "MidiScale";
    static const constexpr auto category = "Midi";
    static const constexpr auto tags = std::array<const char*, 0>{};
    static const constexpr auto uuid = make_uuid("06b33b83-bb67-4f7a-9980-f5d66e4266c5");
  };

  static const constexpr auto info =
    Process::create_node()
  .midi_ins({{"in"}})
  .midi_outs({{"out"}})
  .controls(Process::make_enum(
               "scale",
               0U,
               Process::array("all", "ionian", "dorian", "phyrgian", "lydian", "mixolydian", "aeolian", "locrian",
                "I", "II", "III", "IV", "V", "VI", "VII")),
            Process::Widgets::OctaveSlider("base", 0, 1),
            Process::Widgets::OctaveSlider("transpose", -1, 1)
  )
  .build();

  using control_policy = Process::DefaultTick;
  static void run(
      const ossia::midi_port& midi_in,
      const Process::timed_vec<std::string>& sc,
      const Process::timed_vec<int>& base,
      const Process::timed_vec<int>& transp,
      ossia::midi_port& midi_out,
      ossia::time_value prev_date,
      ossia::token_request tk,
      ossia::execution_state& st)
  {
    auto cur_scale = get_scale(QLatin1String{sc.begin()->second.data(), (int)sc.begin()->second.size()});
    auto scale = scales.at(cur_scale)[base.begin()->second];
    for(const auto& msg : midi_in.messages)
    {
      if(msg.isNoteOnOrOff())
      {
        // map to scale
        auto index = find_closest_index(scale, msg.data[1]);

        // transpose
        auto res = msg;
        res.data[1] = (uint8_t)ossia::clamp(int(index + transp.begin()->second), 0, 127);
        midi_out.messages.push_back(res);
      }
    }
  }
};
using Factories = Process::Factories<Node>;
}



}
