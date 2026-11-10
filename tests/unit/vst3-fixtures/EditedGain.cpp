// A VST3 effect whose gain is its only parameter, made with the SDK's
// AudioEffect and EditController as most plug-ins are: its processor learns
// the gain only from process() and keeps it in its state; its controller
// takes it from that state. An editor moving the gain sets it on the
// controller and tells the host with performEdit, as the SDK's editors do.
//
// Its editor draws nothing. Attached, it has the host's run loop watch a
// file descriptor that is always readable, the same for all its editors;
// removed, it lets go of the handler without unregistering it, as the editors
// of Swanky Amp and Vitalium do:
// score_test_edited_gain_handler_calls() counts the calls it gets afterwards.

#include <base/source/fstreamer.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <public.sdk/source/common/pluginview.h>
#include <public.sdk/source/main/pluginfactory.h>
#include <public.sdk/source/vst/vstaudioeffect.h>
#include <public.sdk/source/vst/vsteditcontroller.h>

#include <atomic>
#include <cstring>

#include <unistd.h>

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

std::atomic<int> g_stale_calls{}, g_calls{}, g_refused{};

//! Lives on after its view, as a handler a plug-in forgot does
struct FdHandler final : public Linux::IEventHandler
{
  bool view_alive{true};
  void PLUGIN_API onFDIsSet(Linux::FileDescriptor) override
  {
    (view_alive ? g_calls : g_stale_calls)++;
  }
  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override
  {
    if(FUnknownPrivate::iidEqual(iid, Linux::IEventHandler::iid)
       || FUnknownPrivate::iidEqual(iid, FUnknown::iid))
    {
      *obj = this;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
};

class View final : public CPluginView
{
public:
  View()
      : CPluginView{nullptr}
  {
    ViewRect r{0, 0, 200, 100};
    setRect(r);
  }

  tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override
  {
    return std::strcmp(type, kPlatformTypeX11EmbedWindowID) == 0 ? kResultTrue
                                                                 : kResultFalse;
  }

  tresult PLUGIN_API attached(void* parent, FIDString type) override
  {
    Linux::IRunLoop* loop{};
    if(plugFrame
       && plugFrame->queryInterface(Linux::IRunLoop::iid, (void**)&loop) == kResultOk
       && loop)
    {
      // One descriptor for all its editors, as their connection to the X
      // server is
      static const int fd = [] {
        int fds[2]{-1, -1};
        if(::pipe(fds) == 0)
          (void)!::write(fds[1], "x", 1);
        return fds[0];
      }();
      if(fd >= 0)
      {
        handler = new FdHandler;
        if(loop->registerEventHandler(handler, fd) != kResultTrue)
          g_refused++;
      }
    }
    return CPluginView::attached(parent, type);
  }

  tresult PLUGIN_API removed() override
  {
    // Not unregistered: the handler is left to the host's run loop
    if(handler)
      handler->view_alive = false;
    handler = nullptr;
    return CPluginView::removed();
  }

  FdHandler* handler{};
};

class Controller final : public EditController
{
public:
  IPlugView* PLUGIN_API createView(FIDString name) override
  {
    return name && std::strcmp(name, ViewType::kEditor) == 0 ? new View : nullptr;
  }

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

//! Calls a view's event handler got after the view was removed, calls while
//! it was there, registrations the host refused
extern "C" SMTG_EXPORT_SYMBOL void
score_test_edited_gain_handler_calls(int* stale, int* live, int* refused)
{
  *stale = g_stale_calls;
  *live = g_calls;
  *refused = g_refused;
}
