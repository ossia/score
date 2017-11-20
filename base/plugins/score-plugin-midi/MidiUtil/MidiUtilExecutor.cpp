// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiUtilExecutor.hpp"
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/network/midi/detail/midi_impl.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Engine/Protocols/OSSIADevice.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <MidiUtil/MidiUtilProcess.hpp>
#include <ossia/detail/math.hpp>
#include <ossia/dataflow/node_process.hpp>
#undef slots
#include <frozen/unordered_map.h>

namespace MidiUtil
{
namespace Executor
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
  for(int octave = 0; octave < 11; octave++)
  {
    int pos = 0;
    for(bool note : notes)
    {
      if(octave * 12 + pos < 128)
      {
        r[0][octave * 12 + pos] = note;
        pos++;
      }
    }
  }

  for(int octave = 1; octave < 12; octave++)
  {
    r[octave] = r[0];
    constexpr_rotate(r[octave].rbegin(), r[octave].rbegin() + octave, r[octave].rend());
  }
  return r;
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
      int r = 0;
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

class midi_node
    : public ossia::graph_node
{
  public:
    midi_node()
    {
      // input
      m_inlets.push_back(ossia::make_inlet<ossia::midi_port>());

      // scale
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

      // mode
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());

      // transpose
      m_inlets.push_back(ossia::make_inlet<ossia::value_port>());


      // output
      m_outlets.push_back(ossia::make_outlet<ossia::midi_port>());
    }

    ~midi_node() override
    {

    }

    void set_scale(scale c)
    {
      m_scale = c;
    }

    void set_base(uint8_t base)
    {
      m_base = base;
    }

    void set_transpose(uint8_t x)
    {
      m_transpose = x;
    }

  private:
    template<typename T>
    ossia::value get_value(const ossia::inlet& in)
    {
      auto& vp = *in.data.target<ossia::value_port>();
      if(!vp.get_data().empty())
        return ossia::convert<T>(vp.get_data().back().value);
      return {};
    }
    void run(ossia::token_request t, ossia::execution_state& e) override
    {
      auto& midi_in = *m_inlets[0]->data.target<ossia::midi_port>();
      auto& midi_out = *m_outlets[0]->data.target<ossia::midi_port>();

      {
        //auto scale_in = m_inlets[1]->data.target<ossia::value_port>()->data;
        //if(!m_inlets[1]->data.target<ossia::value_port>()->data.empty())
      }

      for(const auto& msg : midi_in.messages)
      {
        if(msg.isNoteOnOrOff())
        {
          // map to scale
          auto scale = scales.at(m_scale)[m_base];
          int index = (int)find_closest_index(scale, msg.data[1]);

          // transpose
          auto res = msg;
          res.data[1] = (uint8_t)ossia::clamp(index + m_transpose, 0, 127);
          midi_out.messages.push_back(res);
        }
      }
    }

    scale m_scale{};
    uint8_t m_base{};
    uint8_t m_transpose{};
};

Component::Component(
    MidiUtil::ProcessModel& element,
    const Engine::Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
  : ::Engine::Execution::
      ProcessComponent_T<MidiUtil::ProcessModel, ossia::node_process>{
        element, ctx, id, "MidiComponent", parent}
{
  m_node = std::make_shared<midi_node>();
  m_ossia_process = std::make_shared<ossia::node_process>(m_node);
  ctx.plugin.register_node(element, m_node);
}
Component::~Component()
{
  system().plugin.unregister_node(process(), m_node);
}
}
}
