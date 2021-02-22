#pragma once
#include <pluginterfaces/gui/iplugview.h>

#include <QWindow>
#include <QTimer>

namespace vst3
{
#if defined(_WIN32) || defined(__APPLE__)
class PlugFrame final
    : public Steinberg::IPlugFrame
{
public:
  Steinberg::tresult queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    *obj = nullptr;
    return Steinberg::kResultFalse;
  }

  Steinberg::uint32 addRef() override { return 1; }
  Steinberg::uint32 release() override { return 1; }

  QWindow& w;
  PlugFrame(QWindow& w) : w{w} { }

  Steinberg::tresult resizeView(Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override
  {
    auto& r = *newSize;
    w.resize(QSize{r.getWidth(), r.getHeight()});
    if(view->canResize() == Steinberg::kResultTrue)
    {
      view->onSize(&r);
    }
    return Steinberg::kResultOk;
  }
};

#endif
}
