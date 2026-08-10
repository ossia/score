#pragma once
#include <Vst3/UI/WindowContainer.hpp>

#include <QTimer>
#include <QWindow>

#include <pluginterfaces/gui/iplugview.h>

namespace vst3
{

#if defined(_WIN32) || defined(__APPLE__) || !__has_include(<xcb/xcb.h>)
class PlugFrame final : public Steinberg::IPlugFrame
{
public:
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID _iid, void** obj) override
  {
    *obj = nullptr;
    return Steinberg::kResultFalse;
  }

  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  QDialog* w;
  WindowContainer wc;
  PlugFrame(QDialog& w, WindowContainer& wc)
      : w{&w}
      , wc{wc}
  {
  }

  Steinberg::tresult
  PLUGIN_API resizeView(Steinberg::IPlugView* view, Steinberg::ViewRect* newSize) override
  {
    wc.setSizeFromVst(*view, *newSize, *w);
    return Steinberg::kResultOk;
  }
};
#endif
}
