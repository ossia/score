#include <Vst3/Plugin.hpp>
#include <Vst3/ApplicationPlugin.hpp>

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugview.h>

#include <ossia/detail/algorithms.hpp>

#include <QTimer>
#include <QWindow>

namespace vst3
{
using namespace Steinberg;
#if defined(__linux__)
class PlugFrame
    : virtual public Steinberg::IPlugFrame
    , virtual public Steinberg::Linux::IRunLoop
{
public:
  std::vector<std::pair<Linux::ITimerHandler*, QTimer*>> timers;
  tresult queryInterface (const TUID _iid, void** obj) override
  {
    if(FUID::fromTUID(_iid)  == Linux::IRunLoop::iid)
    {
      *obj = static_cast<Steinberg::Linux::IRunLoop*>(this);
      return kResultOk;
    }
    *obj = nullptr;
    return kResultFalse;
  }

   uint32  addRef () override
   {
     return 1;

   }

   uint32  release () override
   {
     return 1;

   }
   tresult PLUGIN_API registerEventHandler (Linux::IEventHandler* handler, Linux::FileDescriptor fd) override {

     qDebug() << "registerEventHandler";
     return kResultOk;
   }
   tresult PLUGIN_API unregisterEventHandler (Linux::IEventHandler* handler)  override {

     qDebug() << "unregisterEventHandler";
     return kResultOk;
   }

   tresult PLUGIN_API registerTimer (Linux::ITimerHandler* handler,
                 Linux::TimerInterval milliseconds)  override {

     auto t = new QTimer;
     QObject::connect(t, &QTimer::timeout, [=] { handler->onTimer(); });
     t->start(milliseconds);
     timers.push_back({handler, t});
     qDebug() << "registerTimer" << milliseconds;
     return kResultOk;
   }
   tresult PLUGIN_API unregisterTimer (Linux::ITimerHandler* handler)  override {

     auto t = ossia::find_if(timers, [=] (auto& p1) { return p1.first == handler; });
     if(t != timers.end())
     {
       delete t->second;
       timers.erase(t);
     }
     qDebug() << "unregisterTimer";
     return kResultOk;
   }

  QWindow &w;
  PlugFrame(QWindow &w): w{w} { }

  tresult  resizeView (Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override
  {
    auto& r = *newSize;
    w.resize(QSize{r.getWidth(), r.getHeight()});
    return Steinberg::kResultOk;
  }
};
#elif defined(_WIN32) || defined(__APPLE__)
class PlugFrame
    : virtual public Steinberg::IPlugFrame
{
public:
  tresult queryInterface(const TUID _iid, void** obj) override
  {
    *obj = nullptr;
    return kResultFalse;
  }

  uint32 addRef() override { return 1; }
  uint32 release() override { return 1; }

  QWindow& w;
  PlugFrame(QWindow& w) : w{w} { }

  tresult resizeView(Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override
  {
    auto& r = *newSize;
    w.resize(QSize{r.getWidth(), r.getHeight()});
    return Steinberg::kResultOk;
  }
};

#endif

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

void Plugin::loadEditController(ApplicationPlugin& ctx)
{
  Steinberg::Vst::IEditController* controller{};
  auto ctl_res = component->queryInterface(Steinberg::Vst::IEditController::iid, (void **)&controller);
  qDebug( ) << " component->queryInterface " << controller;
  if (ctl_res != Steinberg::kResultOk || !controller)
  {
    qDebug( ) << " ^ not ok";
    Steinberg::TUID cid;
    if (component->getControllerClassId(cid) == Steinberg::kResultTrue)
    {
      FUID f{cid};
      mdl->getFactory().get()->createInstance(
          f, Steinberg::Vst::IEditController::iid, (void**)&controller);

      if(controller)
        controller->initialize(&ctx.m_host);
      qDebug( ) << " getControllerClassId" << controller;
    }
  }


  if (!controller)
  {
    qDebug() << "Couldn't get VST3 Controller interface : " << path.c_str();
    return;
  }

  this->controller = controller;
  qDebug() << path.c_str() << controller->getParameterCount();
  // ridiculous
  Steinberg::IPlugView* view;
  if (!(view = controller->createView(Steinberg::Vst::ViewType::kEditor))) {
    if (!(view = controller->createView(nullptr))) {
      if (controller->queryInterface (IPlugView::iid, (void**)&view) == Steinberg::kResultOk) {
        view->addRef ();
        // TODO don't forget to unref in that case *_*
      }
    }
  }

  if(view)
  {
    this->view = view;

    using namespace Steinberg;
    auto supported = view->isPlatformTypeSupported(Steinberg::kPlatformTypeX11EmbedWindowID);

    if(supported == Steinberg::kResultTrue)
    {
      auto z = new QWindow;
      Steinberg::ViewRect r;
      view->getSize(&r);
      z->resize(QSize{r.getWidth(), r.getHeight()});
      z->show();
      view->setFrame(new PlugFrame{*z});
      view->attached((void*)z->winId(), Steinberg::kPlatformTypeX11EmbedWindowID);
    }
  }
}

void Plugin::load(
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

  loadEditController(ctx);

  loadBuses();

  startPlugin(sample_rate, max_bs);
}

void Plugin::startPlugin(double_t sample_rate, int max_bs)
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

Plugin::~Plugin()
{
}

}
