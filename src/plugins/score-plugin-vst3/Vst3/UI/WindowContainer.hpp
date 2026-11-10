#pragma once
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QWindow>

#include <score/widgets/PluginWindow.hpp>

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>

namespace vst3
{
class PlugFrame;

inline const char* currentPlatform()
{
#if defined(__APPLE__)
  return Steinberg::kPlatformTypeNSView;
#elif defined(_WIN32)
  return Steinberg::kPlatformTypeHWND;
#elif (!(defined(__APPLE__) || defined(_WIN32))) && __has_include(<xcb/xcb.h>)
  return Steinberg::kPlatformTypeX11EmbedWindowID;
#endif
  return "";
}

//! Tell the plug-in about the scale of the screen its view is on.
//! Returns true if the plug-in took it into account - it then has resized its
//! view by that factor, so its size has to be queried again.
inline bool applyContentScaleFactor(Steinberg::IPlugView& view, const QWidget& w)
{
#if defined(__APPLE__)
  // NSViews are scaled by the system: there is nothing for the plug-in to do
  return false;
#else
  // On Windows and X11 the plug-in has no way to find this out by itself
  Steinberg::IPlugViewContentScaleSupport* scaling{};
  if(view.queryInterface(Steinberg::IPlugViewContentScaleSupport::iid, (void**)&scaling)
         != Steinberg::kResultOk
     || !scaling)
    return false;

  const auto res = scaling->setContentScaleFactor(w.devicePixelRatioF());
  scaling->release();
  return res == Steinberg::kResultTrue;
#endif
}

struct WindowContainer
{
  WId nativeId{};
  QWindow* qwindow{};
  QWidget* container{};
  vst3::PlugFrame* frame{};

  //! Factor to go from the coordinates the plug-in gives us to the logical
  //! pixels Qt lays out with: device pixels on Windows and X11, points on
  //! macOS, both scaled by Qt's own scaling
  static double qtScaleFactor(const QWidget& w) noexcept
  {
    return 1. / score::PluginWindow::editorPixelRatio(w);
  }

  auto setSizeFromQt(
      Steinberg::IPlugView& view, const Steinberg::ViewRect& r,
      QDialog& parentWindow) const
  {
    const double scale = qtScaleFactor(parentWindow);
    int w = r.getWidth();
    int h = r.getHeight();
    int qw = r.getWidth() * scale;
    int qh = r.getHeight() * scale;

    if(w == 0 || h == 0)
    {
      return std::make_pair(w, h);
    }

    if(view.canResize() == Steinberg::kResultTrue)
    {
      parentWindow.resize(QSize{qw, qh});
    }
    else
    {
      parentWindow.setFixedSize(QSize{qw, qh});
    }
    if(qwindow)
    {
      qwindow->resize(qw, qh);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }

    return std::make_pair(w, h);
  }

  void
  setSizeFromUser(Steinberg::IPlugView& view, const QSize& sz, QDialog& parentWindow)
  {
    if(view.canResize() != Steinberg::kResultTrue)
      return;

    const double scale = qtScaleFactor(parentWindow);
    Steinberg::ViewRect r;
    r.top = 0;
    r.left = 0;
    r.right = sz.width() / scale;
    r.bottom = sz.height() / scale;
    // The plug-in may only take some sizes: the window takes the one it chose
    view.checkSizeConstraint(&r);
    if(r.getWidth() <= 0 || r.getHeight() <= 0)
      return;
    const int qw = r.getWidth() * scale;
    const int qh = r.getHeight() * scale;

    if(parentWindow.size() != QSize(qw, qh))
      parentWindow.resize(QSize(qw, qh));

    if(qwindow)
    {
      qwindow->resize(qw, qh);
    }

    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }

    view.onSize(&r);
  }

  auto setSizeFromVst(
      Steinberg::IPlugView& view, Steinberg::ViewRect& r, QDialog& parentWindow)
  {
    const double scale = qtScaleFactor(parentWindow);
    int w = r.getWidth();
    int h = r.getHeight();
    int qw = r.getWidth() * scale;
    int qh = r.getHeight() * scale;

    if(w == 0 || h == 0)
    {
      view.onSize(&r);
      return std::make_pair(qw, qh);
    }

    if(view.canResize() == Steinberg::kResultTrue)
    {
      parentWindow.resize(QSize{qw, qh});
    }
    else
    {
      parentWindow.setFixedSize(QSize{qw, qh});
    }

    if(qwindow)
    {
      qwindow->resize(qw, qh);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }

    view.onSize(&r);

    return std::make_pair(qw, qh);
  }
};

class Window;
}
