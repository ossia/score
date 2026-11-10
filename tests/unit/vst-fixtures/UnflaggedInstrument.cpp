// A VST 2 instrument that does not set effFlagsIsSynth, as JuceOPL: no audio
// input, one output, and it can receive MIDI events. It plays a constant 0.5
// while a note is held, and sends back each note it gets, from its process
// call, one frame later. Its eight programs set its level: 0.75 for "Loud",
// 0.5 for "Medium", 0.05 times its index for the others. It opens on the
// second, as Permut8 opens on another than its first.
//
// Built with SCORE_TEST_VST_NO_INITIAL_PROGRAM, it opens on no program, as the
// plug-ins of the VST SDK that only set themselves up in setProgram: it says
// it is on its first, but its level is 0 until a program is selected.
//
// Built with SCORE_TEST_VST_CHUNKS, its state is a chunk: its level.
//
// It counts the programs it is told to select between effBeginSetProgram and
// effEndSetProgram, and outside them: effVendorSpecific with index 1 and 0
// gives them.

#include <Vst/vst-compat.hpp>

#include <cstdio>
#include <cstring>
#include <string_view>
#include <vector>

#if defined(SCORE_TEST_VST_NO_INITIAL_PROGRAM)
#define SCORE_TEST_VST_OPENING_PROGRAM 0
#define SCORE_TEST_VST_OPENING_LEVEL 0.f
#define SCORE_TEST_VST_ID "sUnP"_4c
#elif defined(SCORE_TEST_VST_CHUNKS)
#define SCORE_TEST_VST_OPENING_PROGRAM 1
#define SCORE_TEST_VST_OPENING_LEVEL 0.5f
#define SCORE_TEST_VST_ID "sUnC"_4c
#else
#define SCORE_TEST_VST_OPENING_PROGRAM 1
#define SCORE_TEST_VST_OPENING_LEVEL 0.5f
#define SCORE_TEST_VST_ID "sUnI"_4c
#endif

namespace
{
struct instrument
{
  AEffect effect{};
  audioMasterCallback master{};
  int held{};
  int program{SCORE_TEST_VST_OPENING_PROGRAM};
  bool changing_program{};
  intptr_t wrapped_selections{};
  intptr_t unwrapped_selections{};
  float param{SCORE_TEST_VST_OPENING_LEVEL};
  float chunk{};
  std::vector<VstMidiEvent> echo;
};

instrument& self(AEffect* e)
{
  return *static_cast<instrument*>(e->object);
}

intptr_t dispatcher(AEffect* e, int32_t op, int32_t idx, intptr_t v, void* ptr, float)
{
  switch(op)
  {
    case effClose:
      delete &self(e);
      return 0;
    case effCanDo: {
      const std::string_view s{static_cast<const char*>(ptr)};
      return s == "receiveVstMidiEvent" || s == "receiveVstEvents"
                     || s == "sendVstMidiEvent" || s == "sendVstEvents"
                 ? 1
                 : 0;
    }
    case effBeginSetProgram:
      self(e).changing_program = true;
      return 1;
    case effEndSetProgram:
      self(e).changing_program = false;
      return 1;
    case effVendorSpecific:
      return idx == 1 ? self(e).wrapped_selections : self(e).unwrapped_selections;
    case effSetProgram:
      (self(e).changing_program ? self(e).wrapped_selections
                                : self(e).unwrapped_selections)++;
      self(e).program = int(v);
      self(e).param = v == 0 ? 0.75f : v == 1 ? 0.5f : 0.05f * float(v);
      return 0;
    case effGetChunk:
      self(e).chunk = self(e).param;
      *static_cast<void**>(ptr) = &self(e).chunk;
      return sizeof(float);
    case effSetChunk:
      if(v == sizeof(float))
        std::memcpy(&self(e).param, ptr, sizeof(float));
      return 0;
    case effGetProgram:
      return self(e).program;
    case effGetProgramNameIndexed:
      if(idx == 0)
        std::strcpy(static_cast<char*>(ptr), "Loud");
      else if(idx == 1)
        std::strcpy(static_cast<char*>(ptr), "Medium");
      else
        std::snprintf(static_cast<char*>(ptr), 24, "Program %d", idx + 1);
      return 1;
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
        self(e).echo.push_back(*m);
        self(e).echo.back().deltaFrames++;
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
  auto& s = self(e);
  if(!s.echo.empty())
  {
    std::vector<char> buf(sizeof(VstEvents) + sizeof(VstEvent*) * s.echo.size());
    auto* ev = reinterpret_cast<VstEvents*>(buf.data());
    ev->numEvents = int32_t(s.echo.size());
    for(std::size_t i = 0; i < s.echo.size(); i++)
      ev->events[i] = reinterpret_cast<VstEvent*>(&s.echo[i]);
    s.master(e, audioMasterProcessEvents, 0, 0, ev, 0.f);
    s.echo.clear();
  }
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
VSTPluginMain(audioMasterCallback master)
{
  auto* p = new instrument;
  p->master = master;
  auto& e = p->effect;
  e.magic = kEffectMagic;
  e.dispatcher = dispatcher;
  e.setParameter = set_parameter;
  e.getParameter = get_parameter;
  e.processReplacing = process;
  e.numPrograms = 8;
  e.numParams = 1;
  e.numInputs = 0;
  e.numOutputs = 1;
  e.flags = effFlagsCanReplacing;
#if defined(SCORE_TEST_VST_CHUNKS)
  e.flags |= effFlagsProgramChunks;
#endif
  e.uniqueID = SCORE_TEST_VST_ID;
  e.version = 1;
  e.object = p;
  return &e;
}
