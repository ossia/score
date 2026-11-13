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
// - index 4: gives the number of blocks processed so far.

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
  std::atomic<intptr_t> set_calls{};
  std::atomic<intptr_t> blocks{};
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
      }
      return 0;
    default:
      return 0;
  }
}

void process(AEffect* e, float** in, float** out, int32_t n)
{
  self(e).blocks++;
  std::copy_n(in[0], n, out[0]);
}

void set_parameter(AEffect* e, int32_t, float v)
{
  self(e).set_calls++;
  self(e).level = std::clamp(v, 0.f, 1.f);
}

float get_parameter(AEffect* e, int32_t)
{
  return self(e).level;
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
