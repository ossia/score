#include <Vst3/Plugin.hpp>
#include <Vst3/DataStream.hpp>
#include <Vst3/EditHandler.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QTimer>
#include <QWindow>

#include <Vst3/UI/Window.hpp>


namespace vst3
{
using namespace Steinberg;



static Steinberg::Vst::IComponent* createComponent(
    VST3::Hosting::Module& mdl,
    const std::string& name)
{
  const auto& factory = mdl.getFactory();
  for (auto &class_info : factory.classInfos())
    if (class_info.category() == kVstAudioEffectClass)
    {
      if(name.empty() || name == class_info.name())
      {
        Steinberg::Vst::IComponent* obj{};
        factory.get()->createInstance(class_info.ID().data (), Steinberg::Vst::IComponent::iid, reinterpret_cast<void**> (&obj));
        return obj;
      }
    }

  throw vst_error("Couldn't create VST3 component ({})", mdl.getPath());
}

void Plugin::loadAudioProcessor(ApplicationPlugin& ctx)
{
  Steinberg::Vst::IAudioProcessor *processor_ptr = nullptr;
  auto audio_iface_res = component->queryInterface(Steinberg::Vst::IAudioProcessor::iid, (void **)&processor_ptr);
  if (audio_iface_res != Steinberg::kResultOk || !processor_ptr)
    throw vst_error("Couldn't get VST3 AudioProcessor interface ({})", path);

  processor = processor_ptr;
}

void Plugin::loadBuses()
{
  audio_ins =  component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kInput);
  event_ins =  component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kInput);
  audio_outs = component->getBusCount(Steinberg::Vst::kAudio, Steinberg::Vst::kOutput);
  event_outs = component->getBusCount(Steinberg::Vst::kEvent, Steinberg::Vst::kOutput);
}

void Plugin::loadPluginState()
{
  // Copy the state from the processor component to the controller

  QByteArray arr;
  QDataStream str{&arr, QIODevice::ReadWrite};
  Vst3DataStream stream{str};
  // thanks steinberg doc not even up to date.... stream.setByteOrder (kLittleEndian);
  if (this->component->getState(&stream) == kResultTrue)
  {
    Steinberg::int64 res{};
    stream.seek(0, Steinberg::IBStream::kIBSeekSet, &res);
    controller->setComponentState (&stream);
  }
}

void Plugin::loadEditController(
    Model& model,
    ApplicationPlugin& ctx)
{
  Steinberg::Vst::IEditController* controller{};
  auto ctl_res = component->queryInterface(Steinberg::Vst::IEditController::iid, (void **)&controller);
  if (ctl_res != Steinberg::kResultOk || !controller)
  {
    Steinberg::TUID cid;
    if (component->getControllerClassId(cid) == Steinberg::kResultTrue)
    {
      FUID f{cid};
      mdl->getFactory().get()->createInstance(
          f, Steinberg::Vst::IEditController::iid, (void**)&controller);

      if(controller)
        controller->initialize(&ctx.m_host);
    }
  }

  if (!controller)
  {
    qDebug() << "Couldn't get VST3 Controller interface : " << path.c_str();
    return;
  }

  this->controller = controller;

  // Connect the controller to the component... for... reasons
  {
    controller->setComponentHandler(new Handler{model});
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    // TODO need disconnection

    IConnectionPoint* compICP{};
    IConnectionPoint* contrICP{};
    if (component->queryInterface (IConnectionPoint::iid, (void**)&compICP) != kResultOk)
      compICP = nullptr;

    if (controller->queryInterface (IConnectionPoint::iid, (void**)&contrICP) != kResultOk)
      contrICP = nullptr;

    if(compICP && contrICP)
    {
      compICP->connect(contrICP);
      contrICP->connect(compICP);
    }
  }

  // Try to instantiate a view
  // r i d i c u l o u s
  Steinberg::IPlugView* view{};
  if (!(view = controller->createView(Steinberg::Vst::ViewType::kEditor))) {
    if (!(view = controller->createView(nullptr))) {
      if (controller->queryInterface (IPlugView::iid, (void**)&view) == Steinberg::kResultOk) {
        view->addRef();
        // TODO don't forget to unref in that case *_*
      }
    }
  }

  // Create a widget to put the view inside
  if(view)
  {
    this->view = view;
    this->hasUI = view->isPlatformTypeSupported(currentPlatform()) == Steinberg::kResultTrue;
  }
}

void Plugin::load(
    Model& model,
    ApplicationPlugin& ctx,
    const std::string& path, const std::string& name,
    double sample_rate, int max_bs)
{
  this->path = path;
  mdl = ctx.getModule(path);
  component = createComponent(*mdl, name);

  if(component->initialize(&ctx.m_host) != Steinberg::kResultOk)
    throw vst_error("Couldn't initialize VST3 component ({})", path);

  // Reload: component->getState();
  loadAudioProcessor(ctx);

  loadEditController(model, ctx);

  loadBuses();

  start(sample_rate, max_bs);
}

void Plugin::start(double_t sample_rate, int max_bs)
{
  // Some level of introspection
  auto sampleSize = Steinberg::Vst::kSample32;
  if (processor->canProcessSampleSize(Steinberg::Vst::kSample64) == Steinberg::kResultTrue)
  {
    sampleSize = Steinberg::Vst::kSample64;
    supportsDouble = true;
  }

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

void Plugin::stop()
{
  if(!component)
    return;

  component->setActive(false);


  if(view)
  {
    qDebug() << view->release();
    view = nullptr;
  }

  if(controller)
  {
    qDebug() << controller->release();
    controller = nullptr;
  }

  if(processor)
  {
    qDebug() << processor->release();
    processor = nullptr;
  }

  qDebug() << component->release();
  component = nullptr;

}

Plugin::~Plugin()
{
}

}
