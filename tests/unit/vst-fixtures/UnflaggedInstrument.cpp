// A VST 2 instrument that does not set effFlagsIsSynth, as JuceOPL: no audio
// input, one output, and it can receive MIDI events. It plays a constant 0.5
// while a note is held.

#include <Vst/vst-compat.hpp>

#include <cstring>
#include <string_view>

namespace
{
struct instrument
{
  AEffect effect{};
  int held{};
  float param{0.25f};
};

instrument& self(AEffect* e)
{
  return *static_cast<instrument*>(e->object);
}

intptr_t dispatcher(AEffect* e, int32_t op, int32_t, intptr_t, void* ptr, float)
{
  switch(op)
  {
    case effClose:
      delete &self(e);
      return 0;
    case effCanDo: {
      const std::string_view s{static_cast<const char*>(ptr)};
      return s == "receiveVstMidiEvent" || s == "receiveVstEvents" ? 1 : 0;
    }
    case effGetPlugCategory:
      return kPlugCategEffect;
    case effGetParamName:
      std::strcpy(static_cast<char*>(ptr), "Level");
      return 0;
    case effProcessEvents: {
      auto* ev = static_cast<VstEvents*>(ptr);
      for(int i = 0; i < ev->numEvents; i++)
      {
        if(ev->events[i]->type != kVstMidiType)
          continue;
        auto* m = reinterpret_cast<VstMidiEvent*>(ev->events[i]);
        const auto status = uint8_t(m->midiData[0]) & 0xF0;
        if(status == 0x90 && m->midiData[2] != 0)
          self(e).held++;
        else if((status == 0x80 || status == 0x90) && self(e).held > 0)
          self(e).held--;
      }
      return 1;
    }
    default:
      return 0;
  }
}

void process(AEffect* e, float**, float** out, int32_t n)
{
  for(int i = 0; i < n; i++)
    out[0][i] = self(e).held > 0 ? 0.5f : 0.f;
}

void set_parameter(AEffect* e, int32_t, float v)
{
  self(e).param = v;
}
float get_parameter(AEffect* e, int32_t)
{
  return self(e).param;
}
}

extern "C" __attribute__((visibility("default"))) AEffect*
VSTPluginMain(audioMasterCallback)
{
  auto* p = new instrument;
  auto& e = p->effect;
  e.magic = kEffectMagic;
  e.dispatcher = dispatcher;
  e.setParameter = set_parameter;
  e.getParameter = get_parameter;
  e.processReplacing = process;
  e.numPrograms = 0;
  e.numParams = 1;
  e.numInputs = 0;
  e.numOutputs = 1;
  e.flags = effFlagsCanReplacing;
  e.uniqueID = "sUnI"_4c;
  e.version = 1;
  e.object = p;
  return &e;
}
