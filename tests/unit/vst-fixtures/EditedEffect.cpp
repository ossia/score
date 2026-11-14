// A VST 2 effect with one parameter, "Dry Level", from 0 to 100 %, whose
// editor is simulated by effVendorSpecific. As the DPF plug-ins whose host
// callback takes an int32_t in place of the float (dpf-plugins 1.7, Dragonfly
// Reverb 3.2), getParameter gives the normalized value but audioMasterAutomate
// carries the value in percent.
//
// effVendorSpecific, with opt the normalized value the editor moves the
// parameter to:
// - index 1: the editor moves it and calls audioMasterAutomate in percent;
// - index 2: the editor moves it and calls audioMasterAutomate normalized, as
//   a plug-in that follows the specification;
// - index 3: gives the number of setParameter calls so far;
// - index 4: gives the number of blocks processed so far;
// - index 5: the plug-in gets a second parameter, "Wet Level", and its editor
//   moves it to opt and calls audioMasterAutomate, as a plug-in whose
//   parameters depend on its state;
// - index 6: from then on, each process call asks the host for the transport
//   (audioMasterGetTime) and says its parameter changed (audioMasterAutomate),
//   as plug-ins that follow the host's tempo or automate themselves do;
// - index 7: gives what audioMasterGetTime answered in the last process call.

#include <Vst/vst-compat.hpp>

#include <algorithm>
#include <atomic>
#include <cstring>

namespace
{
struct effect
{
  AEffect effect{};
  audioMasterCallback master{};
  std::atomic<float> level{0.8f};
  std::atomic<float> wet{0.f};
  std::atomic<intptr_t> set_calls{};
  std::atomic<intptr_t> blocks{};
  std::atomic_bool host_calls_in_process{};
  std::atomic<intptr_t> time_answer{-1};
};

effect& self(AEffect* e)
{
  return *static_cast<effect*>(e->object);
}

intptr_t dispatcher(AEffect* e, int32_t op, int32_t idx, intptr_t, void* ptr, float opt)
{
  switch(op)
  {
    case effClose:
      delete &self(e);
      return 0;
    case effGetPlugCategory:
      return kPlugCategEffect;
    case effGetParamName:
      std::strcpy(static_cast<char*>(ptr), "Dry Level");
      return 0;
    case effVendorSpecific:
      switch(idx)
      {
        case 1:
          self(e).level = opt;
          self(e).master(e, audioMasterAutomate, 0, 0, nullptr, opt * 100.f);
          return 1;
        case 2:
          self(e).level = opt;
          self(e).master(e, audioMasterAutomate, 0, 0, nullptr, opt);
          return 1;
        case 3:
          return self(e).set_calls;
        case 4:
          return self(e).blocks;
        case 5:
          self(e).wet = opt;
          e->numParams = 2;
          self(e).master(e, audioMasterAutomate, 1, 0, nullptr, opt);
          return 1;
        case 6:
          self(e).host_calls_in_process = true;
          return 1;
        case 7:
          return self(e).time_answer;
      }
      return 0;
    default:
      return 0;
  }
}

void process(AEffect* e, float** in, float** out, int32_t n)
{
  auto& s = self(e);
  s.blocks++;
  if(s.host_calls_in_process)
  {
    s.time_answer = s.master(e, audioMasterGetTime, 0, kVstTempoValid, nullptr, 0.f);
    s.master(e, audioMasterAutomate, 0, 0, nullptr, s.level);
  }
  std::copy_n(in[0], n, out[0]);
}

void set_parameter(AEffect* e, int32_t i, float v)
{
  self(e).set_calls++;
  (i == 1 ? self(e).wet : self(e).level) = std::clamp(v, 0.f, 1.f);
}

float get_parameter(AEffect* e, int32_t i)
{
  return i == 1 ? self(e).wet : self(e).level;
}
}

extern "C" __attribute__((visibility("default"))) AEffect*
VSTPluginMain(audioMasterCallback master)
{
  auto* p = new effect;
  p->master = master;
  auto& e = p->effect;
  e.magic = kEffectMagic;
  e.dispatcher = dispatcher;
  e.setParameter = set_parameter;
  e.getParameter = get_parameter;
  e.processReplacing = process;
  e.numPrograms = 0;
  e.numParams = 1;
  e.numInputs = 1;
  e.numOutputs = 1;
  e.flags = effFlagsCanReplacing;
  e.uniqueID = "sEdE"_4c;
  e.version = 1;
  e.object = p;
  return &e;
}
