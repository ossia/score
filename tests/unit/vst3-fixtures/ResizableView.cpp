// A VST3 effect whose editor can be resized, made with the SDK's AudioEffect,
// EditController and CPluginView. Its editor takes only some sizes:
// checkSizeConstraint() makes them 3:2, at least 300 x 200, the width a
// multiple of 30. It handles IPlugViewContentScaleSupport, a size of 300 x 200
// at a factor of 1. Attached, it draws nothing in an X11 window of its own,
// the size of its last onSize().
//
// Every call the host makes about its size is logged, with the sizes, and
// what the host answers to the resizeView() calls it makes:
// score_test_resizable_view_log(). score_test_resizable_view_mode() picks
// what the next editors do:
//   0  nothing more
//   1  answer onSize() with resizeView() to the size just given
//   2  answer onSize() with resizeView() to that size and 10 more pixels
//   3  say they cannot be resized
// score_test_resizable_view_resize_twice() has the open editor ask for two
// sizes in a row.

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>
#include <public.sdk/source/common/pluginview.h>
#include <public.sdk/source/main/pluginfactory.h>
#include <public.sdk/source/vst/vstaudioeffect.h>
#include <public.sdk/source/vst/vsteditcontroller.h>

#include <xcb/xcb.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace
{
const FUID processor_uid(0x6F737361, 0x52657369, 0x7A655072, 0x6F633031);
const FUID controller_uid(0x6F737361, 0x52657369, 0x7A654374, 0x726C3031);

std::string g_log;
int g_mode{};

void note(const char* fmt, auto... args)
{
  char buf[256];
  std::snprintf(buf, sizeof(buf), fmt, args...);
  g_log += buf;
  g_log += '\n';
}

//! The sizes the editor takes
ViewRect constrained(int w, int h)
{
  (void)h;
  const int cw = std::max(300, int(std::lround(w / 30.)) * 30);
  return ViewRect{0, 0, cw, cw * 2 / 3};
}

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
    if(data.numSamples <= 0 || data.numInputs < 1 || data.numOutputs < 1)
      return kResultOk;
    auto& in = data.inputs[0];
    auto& out = data.outputs[0];
    for(int32 c = 0; c < out.numChannels && c < in.numChannels; c++)
      std::copy_n(in.channelBuffers32[c], data.numSamples, out.channelBuffers32[c]);
    return kResultOk;
  }

  tresult PLUGIN_API setState(IBStream*) override { return kResultOk; }
  tresult PLUGIN_API getState(IBStream*) override { return kResultOk; }
};

class View;
View* g_view{};

class View final
    : public CPluginView
    , public IPlugViewContentScaleSupport
{
public:
  View()
      : CPluginView{nullptr}
      , mode{g_mode}
  {
    setRect(constrained(300, 200));
    g_view = this;
  }
  ~View() override
  {
    if(g_view == this)
      g_view = nullptr;
  }

  tresult PLUGIN_API isPlatformTypeSupported(FIDString type) override
  {
    return std::strcmp(type, kPlatformTypeX11EmbedWindowID) == 0 ? kResultTrue
                                                                 : kResultFalse;
  }

  tresult PLUGIN_API attached(void* parent, FIDString type) override
  {
    note("attached %dx%d", rect.getWidth(), rect.getHeight());
    if(auto r = CPluginView::attached(parent, type); r != kResultOk)
      return r;
    conn = xcb_connect(nullptr, nullptr);
    if(xcb_connection_has_error(conn))
    {
      xcb_disconnect(conn);
      conn = nullptr;
      return kResultOk;
    }
    auto* screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    window = xcb_generate_id(conn);
    const uint32_t values[] = {screen->black_pixel};
    xcb_create_window(
        conn, XCB_COPY_FROM_PARENT, window, xcb_window_t(reinterpret_cast<uintptr_t>(parent)),
        0, 0, uint16_t(rect.getWidth()), uint16_t(rect.getHeight()), 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_BACK_PIXEL, values);
    xcb_map_window(conn, window);
    xcb_flush(conn);
    return kResultOk;
  }

  tresult PLUGIN_API removed() override
  {
    note("removed");
    if(conn)
    {
      xcb_destroy_window(conn, window);
      xcb_flush(conn);
      xcb_disconnect(conn);
      conn = nullptr;
    }
    return CPluginView::removed();
  }

  tresult PLUGIN_API onSize(ViewRect* r) override
  {
    if(!r)
      return kInvalidArgument;
    note("onSize %dx%d", r->getWidth(), r->getHeight());
    CPluginView::onSize(r);
    if(conn)
    {
      const uint32_t values[]
          = {uint32_t(std::max(1, r->getWidth())), uint32_t(std::max(1, r->getHeight()))};
      xcb_configure_window(
          conn, window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
      xcb_flush(conn);
    }

    // A host that keeps answering would never return: give up long before
    // the stack does
    if(plugFrame && (mode == 1 || mode == 2) && depth < 20)
    {
      ViewRect again{0, 0, r->getWidth(), r->getHeight()};
      if(mode == 2)
        again = ViewRect{0, 0, r->getWidth() + 10, r->getHeight() + 10};
      depth++;
      ask(again);
      depth--;
    }
    return kResultOk;
  }

  tresult PLUGIN_API canResize() override { return mode == 3 ? kResultFalse : kResultTrue; }

  tresult PLUGIN_API checkSizeConstraint(ViewRect* r) override
  {
    if(!r)
      return kInvalidArgument;
    const ViewRect c = constrained(r->getWidth(), r->getHeight());
    note("check %dx%d -> %dx%d", r->getWidth(), r->getHeight(), c.getWidth(),
        c.getHeight());
    *r = c;
    return kResultTrue;
  }

  tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override
  {
    note("scale %.3f", factor);
    const ViewRect r = constrained(
        int(std::lround(300 * factor)), int(std::lround(200 * factor)));
    if(isAttached() && plugFrame)
      ask(r);
    else
      setRect(r);
    return kResultTrue;
  }

  //! What a resize of its own does
  tresult ask(ViewRect r)
  {
    const auto res = plugFrame->resizeView(this, &r);
    note("resizeView %dx%d -> %d", r.getWidth(), r.getHeight(), int(res));
    return res;
  }

  bool resizeTwice(int w1, int h1, int w2, int h2)
  {
    if(!plugFrame)
      return false;
    ask(ViewRect{0, 0, w1, h1});
    ask(ViewRect{0, 0, w2, h2});
    return true;
  }

  OBJ_METHODS(View, CPluginView)
  DEFINE_INTERFACES
    DEF_INTERFACE(IPlugViewContentScaleSupport)
  END_DEFINE_INTERFACES(CPluginView)
  REFCOUNT_METHODS(CPluginView)

private:
  const int mode{};
  int depth{};
  xcb_connection_t* conn{};
  xcb_window_t window{};
};

class Controller final : public EditController
{
public:
  IPlugView* PLUGIN_API createView(FIDString name) override
  {
    return name && std::strcmp(name, ViewType::kEditor) == 0 ? new View : nullptr;
  }

  static FUnknown* create(void*) { return static_cast<IEditController*>(new Controller); }
};
}

BEGIN_FACTORY_DEF("ossia", "https://ossia.io", "mailto:contact@ossia.io")
DEF_CLASS2(
    INLINE_UID_FROM_FUID(processor_uid), PClassInfo::kManyInstances, kVstAudioEffectClass,
    "Resizable view", Vst::kDistributable, "Fx", "1.0.0", kVstVersionString,
    Processor::create)
DEF_CLASS2(
    INLINE_UID_FROM_FUID(controller_uid), PClassInfo::kManyInstances,
    kVstComponentControllerClass, "Resizable view controller", 0, "", "1.0.0",
    kVstVersionString, Controller::create)
END_FACTORY

//! One line per call: "attached WxH", "scale F", "check WxH -> WxH",
//! "onSize WxH", "resizeView WxH -> result", "removed"
extern "C" SMTG_EXPORT_SYMBOL const char* score_test_resizable_view_log()
{
  return g_log.c_str();
}

extern "C" SMTG_EXPORT_SYMBOL void score_test_resizable_view_clear()
{
  g_log.clear();
}

extern "C" SMTG_EXPORT_SYMBOL void score_test_resizable_view_mode(int mode)
{
  g_mode = mode;
}

extern "C" SMTG_EXPORT_SYMBOL int
score_test_resizable_view_resize_twice(int w1, int h1, int w2, int h2)
{
  return g_view && g_view->resizeTwice(w1, h1, w2, h2);
}
