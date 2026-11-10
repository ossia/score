// The window of a VST3 editor that can be resized, resized from outside (by
// the user, the window manager), resizes the editor to the size closest to it
// that the editor takes, and takes that size itself. One that cannot be
// resized is not told.

#include <Vst3/UI/WindowContainer.hpp>

#include <QApplication>
#include <QDialog>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{
//! Takes widths that are a multiple of 10, and remembers the sizes it is given
struct snapping_view final : Steinberg::IPlugView
{
  bool resizable{true};
  std::vector<std::pair<int, int>> sizes;

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID, void** obj) override
  {
    *obj = nullptr;
    return Steinberg::kNoInterface;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString) override
  {
    return Steinberg::kResultTrue;
  }
  Steinberg::tresult PLUGIN_API attached(void*, Steinberg::FIDString) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API removed() override { return Steinberg::kResultOk; }
  Steinberg::tresult PLUGIN_API onWheel(float) override { return Steinberg::kResultFalse; }
  Steinberg::tresult PLUGIN_API
  onKeyDown(Steinberg::char16, Steinberg::int16, Steinberg::int16) override
  {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API
  onKeyUp(Steinberg::char16, Steinberg::int16, Steinberg::int16) override
  {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* r) override
  {
    *r = Steinberg::ViewRect{0, 0, 400, 300};
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* r) override
  {
    sizes.emplace_back(r->getWidth(), r->getHeight());
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame*) override
  {
    return Steinberg::kResultOk;
  }
  Steinberg::tresult PLUGIN_API canResize() override
  {
    return resizable ? Steinberg::kResultTrue : Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* r) override
  {
    r->right = r->left + r->getWidth() / 10 * 10;
    return Steinberg::kResultTrue;
  }
};
}

TEST_CASE("VST3: a resizable editor follows its window", "[vst3][editor][resize]")
{
  int argc = 1;
  char arg0[] = "test";
  char* argv[] = {arg0, nullptr};
  QApplication app{argc, argv};

  snapping_view view;
  QDialog window;
  window.resize(400, 300);
  vst3::WindowContainer wc;
  // Before attached(), resizes of the window are score's own
  wc.setSizeFromUser(view, QSize{537, 421}, window);
  CHECK(view.sizes.empty());
  wc.setAttached();
  const double scale = vst3::WindowContainer::qtScaleFactor(window);
  REQUIRE(scale == 1.);

  wc.setSizeFromUser(view, QSize{537, 421}, window);
  REQUIRE(view.sizes.size() == 1);
  CHECK(view.sizes.back() == std::pair{530, 421});
  CHECK(window.size() == QSize(530, 421));

  view.resizable = false;
  wc.setSizeFromUser(view, QSize{600, 500}, window);
  CHECK(view.sizes.size() == 1);
}
