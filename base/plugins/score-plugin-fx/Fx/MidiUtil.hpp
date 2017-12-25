#pragma once
#include <Engine/Node/PdNode.hpp>
#undef slots
#include <frozen/unordered_map.h>
namespace Nodes::MidiUtil
{
enum scale: int8_t
{
  all,
  ionian, dorian, phyrgian, lydian, mixolydian, aeolian, locrian,

  I, II, III, IV, V, VI, VII,
  custom,


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
  if(Control::same(s, QLatin1String("all"))) return scale::all;
  else if(Control::same(s, QLatin1String("ionian"))) return scale::ionian;
  else if(Control::same(s, QLatin1String("dorian"))) return scale::dorian;
  else if(Control::same(s, QLatin1String("phyrgian"))) return scale::phyrgian;
  else if(Control::same(s, QLatin1String("lydian"))) return scale::lydian;
  else if(Control::same(s, QLatin1String("mixolydian"))) return scale::mixolydian;
  else if(Control::same(s, QLatin1String("aeolian"))) return scale::aeolian;
  else if(Control::same(s, QLatin1String("locrian"))) return scale::locrian;
  else if(Control::same(s, QLatin1String("I"))) return scale::I;
  else if(Control::same(s, QLatin1String("II"))) return scale::II;
  else if(Control::same(s, QLatin1String("III"))) return scale::III;
  else if(Control::same(s, QLatin1String("IV"))) return scale::IV;
  else if(Control::same(s, QLatin1String("V"))) return scale::V;
  else if(Control::same(s, QLatin1String("VI"))) return scale::VI;
  else if(Control::same(s, QLatin1String("VII"))) return scale::VII;
  else return scale::custom;
}
static Q_DECL_RELAXED_CONSTEXPR frozen::unordered_map<int, scales_array, scale::SCALES_MAX> scales{
  //                                C   D   E F   G   A   B
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
  , { scale::custom,     make_scale({ 1,1,1,1,1,1,1,1,1,1,1,1 })}
};

static
optional<std::size_t> find_closest_index(const scale_array& arr, std::size_t i)
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

  return ossia::none;
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

    struct State
    {
      boost::container::flat_map<uint8_t, uint8_t> map;
    };

    static const constexpr auto info =
        Control::create_node()
        .midi_ins({{"in"}})
        .midi_outs({{"out"}})
        .controls(Control::make_unvalidated_enum(
                    "Scale",
                    0U,
                    Control::array("all", "ionian", "dorian", "phyrgian", "lydian", "mixolydian", "aeolian", "locrian",
                                   "I", "II", "III", "IV", "V", "VI", "VII")),
                  Control::Widgets::OctaveSlider("Base", 0, 1),
                  Control::Widgets::OctaveSlider("Transpose", -1, 1)
                  )
        .build();

    static void exec(
        const ossia::midi_port& midi_in,
        const scale_array& scale,
        int transp,
        ossia::midi_port& midi_out,
        State& self)
    {
      for(const auto& msg : midi_in.messages)
      {
        switch(msg.getMessageType())
        {
          case mm::MessageType::NOTE_ON:
          {
            // map to scale
            if(auto index = find_closest_index(scale, msg.data[1]))
            {
              // transpose
              auto res = msg;
              res.data[1] = (uint8_t)ossia::clamp(int(*index + transp), 0, 127);
              auto it = self.map.find(msg.data[1]);
              if(it != self.map.end())
              {
                midi_out.messages.push_back(mm::MakeNoteOff(res.getChannel(), it->second, res.data[2]));
                midi_out.messages.push_back(res);
                it->second = res.data[1];
              }
              else
              {
                midi_out.messages.push_back(res);
                self.map.insert(std::make_pair((uint8_t)msg.data[1], (uint8_t)res.data[1]));
              }
            }
            break;
          }
          case mm::MessageType::NOTE_OFF:
          {
            auto it = self.map.find(msg.data[1]);
            if(it != self.map.end())
            {
              midi_out.messages.push_back(mm::MakeNoteOff(msg.getChannel(), it->second, msg.data[2]));
              self.map.erase(it);
            }
            break;
          }
          default:
            midi_out.messages.push_back(msg);
            break;
        }
      }
    }
    using control_policy = Control::DefaultTick;
    static void run(
        const ossia::midi_port& midi_in,
        const Control::timed_vec<std::string>& sc,
        const Control::timed_vec<int>& base,
        const Control::timed_vec<int>& transp,
        ossia::midi_port& midi_out,
        ossia::time_value prev_date,
        ossia::token_request tk,
        ossia::execution_state& st,
        State& self)
    {
      QLatin1String scale{sc.rbegin()->second.data(), (int)sc.rbegin()->second.size()};

      const auto cur_scale = get_scale(scale);
      if(cur_scale != scale::custom)
      {
        exec(midi_in, scales.at(cur_scale)[base.rbegin()->second], transp.rbegin()->second, midi_out, self);
      }
      else
      {
        scale_array arr{{}};
        for(int i = 0; i < std::min((int)scale.size(), (int)arr.size()); i++)
        {
          arr[i] = (scale[i] == '1');
        }
        exec(midi_in, arr, transp.rbegin()->second, midi_out, self);
      }
    }
};
}
