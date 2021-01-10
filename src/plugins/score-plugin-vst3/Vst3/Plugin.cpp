#include <Vst3/Plugin.hpp>
#include <Vst3/ApplicationPlugin.hpp>

namespace vst3
{

static Steinberg::IPtr<Steinberg::Vst::IComponent> createComponent(
    VST3::Hosting::Module& module,
    const std::string& name)
{
  const auto& factory = module.getFactory();
  for (auto &class_info : factory.classInfos())
    if (class_info.category() == kVstAudioEffectClass)
    {
      std::cerr << class_info.name() << std::endl;
      if(name.empty() || name == class_info.name())
      {
        return factory.createInstance<Steinberg::Vst::IComponent>(class_info.ID());
      }
    }

  throw vst_error("Couldn't create VST3 component ({})", module.getPath());
}

void Plugin::load(
    ApplicationPlugin& ctx,
    const std::string& path, const std::string& name,
    double_t sample_rate, int max_bs)
{
  auto module = ctx.getModule(path);

  component = createComponent(*module, name);

  if(component->initialize(&ctx.m_host) != Steinberg::kResultOk)
    throw vst_error("Couldn't initialize VST3 component ({})", path);



  Steinberg::Vst::IAudioProcessor *processor_ptr = nullptr;
  auto audio_iface_res = component->queryInterface(Steinberg::Vst::IAudioProcessor::iid, (void **)&processor_ptr);
  if (audio_iface_res != Steinberg::kResultOk || !processor_ptr)
    throw vst_error("Couldn't get VST3 AudioProcessor interface ({})", path);

  processor = Steinberg::shared(processor_ptr);

  // Some level of introspection
  auto sampleSize = Steinberg::Vst::kSample32;
  if (processor->canProcessSampleSize(Steinberg::Vst::kSample64))
  {
    sampleSize = Steinberg::Vst::kSample64;
    supportsDouble = true;
  }

  audio_ins =  component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
  event_ins =  component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
  audio_outs = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
  event_outs = component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);


  Steinberg::Vst::ProcessSetup setup{
        Steinberg::Vst::kRealtime,
        sampleSize,
        max_bs,
        sample_rate
  };

  if (processor->setupProcessing(setup) != Steinberg::kResultOk)
    throw vst_error("Couldn't setup VST3 processing ({})", path);

  if (component->setActive(true) != Steinberg::kResultOk)
    throw vst_error("Couldn't set VST3 active ({})", path);
}

Plugin::~Plugin()
{
}

}
