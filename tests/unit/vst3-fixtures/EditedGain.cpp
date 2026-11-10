// A VST3 effect whose gain is its only parameter, made with the SDK's
// AudioEffect and EditController as most plug-ins are: its processor learns
// the gain only from process() and keeps it in its state; its controller
// takes it from that state. An editor moving the gain sets it on the
// controller and tells the host with performEdit, as the SDK's editors do.

#include <base/source/fstreamer.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <public.sdk/source/main/pluginfactory.h>
#include <public.sdk/source/vst/vstaudioeffect.h>
#include <public.sdk/source/vst/vsteditcontroller.h>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
const FUID processor_uid(0x6F737361, 0x45646974, 0x47616950, 0x726F6331);
const FUID controller_uid(0x6F737361, 0x45646974, 0x47616943, 0x74726C31);
constexpr ParamID gain_id = 0;

class Processor final : public AudioEffect
{
public:
  Processor() { setControllerClass(controller_uid); }
  static FUnknown* create(void*) { return static_cast<IAudioProcessor*>(new Processor); }

  tresult PLUGIN_API initialize(FUnknown* context) override
  {
    if(auto r = AudioEffect::initialize(context); r != kResultOk)
      return r;
    addAudioInput(STR16("In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Out"), SpeakerArr::kStereo);
    return kResultOk;
  }

  tresult PLUGIN_API canProcessSampleSize(int32 size) override
  {
    return size == kSample32 ? kResultTrue : kResultFalse;
  }

  tresult PLUGIN_API process(ProcessData& data) override
  {
    if(auto* changes = data.inputParameterChanges)
    {
      for(int32 i = 0; i < changes->getParameterCount(); i++)
      {
        auto* queue = changes->getParameterData(i);
        int32 offset{};
        ParamValue v{};
        if(queue && queue->getParameterId() == gain_id && queue->getPointCount() > 0
           && queue->getPoint(queue->getPointCount() - 1, offset, v) == kResultOk)
          gain = v;
      }
    }
    if(data.numSamples <= 0 || data.numInputs < 1 || data.numOutputs < 1)
      return kResultOk;
    auto& in = data.inputs[0];
    auto& out = data.outputs[0];
    for(int32 c = 0; c < out.numChannels && c < in.numChannels; c++)
      for(int32 k = 0; k < data.numSamples; k++)
        out.channelBuffers32[c][k] = float(in.channelBuffers32[c][k] * gain);
    return kResultOk;
  }

  tresult PLUGIN_API setState(IBStream* state) override
  {
    IBStreamer s(state, kLittleEndian);
    double g{};
    if(!s.readDouble(g))
      return kResultFalse;
    gain = g;
    return kResultOk;
  }
  tresult PLUGIN_API getState(IBStream* state) override
  {
    IBStreamer s(state, kLittleEndian);
    s.writeDouble(gain);
    return kResultOk;
  }

  double gain{0.5};
};

class Controller final : public EditController
{
public:
  static FUnknown* create(void*) { return static_cast<IEditController*>(new Controller); }

  tresult PLUGIN_API initialize(FUnknown* context) override
  {
    if(auto r = EditController::initialize(context); r != kResultOk)
      return r;
    parameters.addParameter(
        STR16("Gain"), nullptr, 0, 0.5, ParameterInfo::kCanAutomate, gain_id);
    return kResultOk;
  }

  tresult PLUGIN_API setComponentState(IBStream* state) override
  {
    IBStreamer s(state, kLittleEndian);
    double g{};
    if(!s.readDouble(g))
      return kResultFalse;
    setParamNormalized(gain_id, g);
    return kResultOk;
  }
};
}

BEGIN_FACTORY_DEF("ossia", "https://ossia.io", "mailto:contact@ossia.io")
DEF_CLASS2(
    INLINE_UID_FROM_FUID(processor_uid), PClassInfo::kManyInstances, kVstAudioEffectClass,
    "Edited gain", Vst::kDistributable, "Fx", "1.0.0", kVstVersionString,
    Processor::create)
DEF_CLASS2(
    INLINE_UID_FROM_FUID(controller_uid), PClassInfo::kManyInstances,
    kVstComponentControllerClass, "Edited gain controller", 0, "", "1.0.0",
    kVstVersionString, Controller::create)
END_FACTORY
