#pragma once
#include <ossia/detail/algorithms.hpp>

#include <pluginterfaces/gui/iplugview.h>

#include <QWindow>
#include <QTimer>
#include <QDebug>

#include <vector>
#include <utility>

namespace vst3
{
#if defined(__linux__)
namespace Linux = Steinberg::Linux;

class PlugFrame
    : virtual public Steinberg::IPlugFrame
    , virtual public Steinberg::Linux::IRunLoop
{
public:
  using TUID = Steinberg::TUID;
  using FUID = Steinberg::FUID;
  using uint32 = Steinberg::uint32;
  using tresult = Steinberg::tresult;

  std::vector<std::pair<Linux::ITimerHandler*, QTimer*>> timers;

  tresult queryInterface (const TUID _iid, void** obj) override
  {
    using namespace Steinberg;
    if(FUID::fromTUID(_iid)  == Linux::IRunLoop::iid)
    {
      *obj = static_cast<Linux::IRunLoop*>(this);
      return kResultOk;
    }
    *obj = nullptr;
    return kResultFalse;
  }

   uint32 addRef () override
   {
     return 1;
   }

   uint32 release () override
   {
     return 1;
   }

   tresult PLUGIN_API registerEventHandler (Linux::IEventHandler* handler, Linux::FileDescriptor fd) override {

     qDebug() << "registerEventHandler";
     return Steinberg::kResultOk;
   }
   tresult PLUGIN_API unregisterEventHandler (Linux::IEventHandler* handler)  override {

     qDebug() << "unregisterEventHandler";
     return Steinberg::kResultOk;
   }

   tresult PLUGIN_API registerTimer (Linux::ITimerHandler* handler,
                 Linux::TimerInterval milliseconds)  override {

     auto t = new QTimer;
     QObject::connect(t, &QTimer::timeout, [=] { handler->onTimer(); });
     t->start(milliseconds);
     timers.push_back({handler, t});
     qDebug() << "registerTimer" << milliseconds;
     return Steinberg::kResultOk;
   }
   tresult PLUGIN_API unregisterTimer (Linux::ITimerHandler* handler)  override {

     auto t = ossia::find_if(timers, [=] (auto& p1) { return p1.first == handler; });
     if(t != timers.end())
     {
       delete t->second;
       timers.erase(t);
     }
     qDebug() << "unregisterTimer";
     return Steinberg::kResultOk;
   }

  QWindow &w;
  PlugFrame(QWindow &w): w{w} { }

  tresult resizeView (Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override
  {
    auto& r = *newSize;
    w.resize(QSize{r.getWidth(), r.getHeight()});

    if(view->canResize())
    {
      view->onSize(&r);
    }

    return Steinberg::kResultOk;
  }
};

#endif
}
