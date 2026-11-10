#pragma once
#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QWindow>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

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

//! What the window and its editor agreed on, shared by every copy of the
//! container: the window's, the frame's and those of its callbacks
struct EditorSize
{
  //! The size the editor was last given or asked for, in its own pixels
  Steinberg::ViewRect current{};
  //! Sizes the window was given here whose resize the window system may still
  //! report, oldest first, in Qt's pixels, and when
  struct Asked
  {
    QSize size;
    std::chrono::steady_clock::time_point when;
  };
  std::vector<Asked> asked;
  //! attached() was called: a resize of the window that does not come from
  //! here is the user's
  bool attached{};
  //! The plug-in asked for a size before attached(): it gets it once attached
  bool pending{};
  //! The window is being resized here: the resize events it sends are ours
  bool resizingWindow{};
  //! How many onSize() sent from here the editor is in
  int onSizeDepth{};
};

struct WindowContainer
{
  WId nativeId{};
  QWindow* qwindow{};
  QWidget* container{};
  vst3::PlugFrame* frame{};
  std::shared_ptr<EditorSize> size = std::make_shared<EditorSize>();

  //! A plug-in answering onSize() with resizeView() to another size is given
  //! that one, which it may answer again: past this depth its resizeView() is
  //! refused
  static constexpr int maxOnSizeDepth = 2;

  //! Factor to go from the coordinates the plug-in gives us to the logical
  //! pixels Qt lays out with: device pixels on Windows and X11, points on
  //! macOS, both scaled by Qt's own scaling
  static double qtScaleFactor(const QWidget& w) noexcept
  {
    return 1. / score::PluginWindow::editorPixelRatio(w);
  }

  //! Rounded to the nearest pixel both ways: with a ratio of at least 1 a size
  //! that goes to the editor and comes back is the same size
  static int toQt(int editorPixels, double scale) noexcept
  {
    return std::max(1, int(std::lround(editorPixels * scale)));
  }
  static int toEditor(int qtPixels, double scale) noexcept
  {
    return std::max(1, int(std::lround(qtPixels / scale)));
  }

  static bool sameSize(const Steinberg::ViewRect& a, const Steinberg::ViewRect& b) noexcept
  {
    return a.getWidth() == b.getWidth() && a.getHeight() == b.getHeight();
  }

  //! Gives the window the size of the editor. The resize events this sends
  //! synchronously are not the user's, nor are those the window system sends
  //! later for it.
  void resizeWindow(
      Steinberg::IPlugView& view, int qw, int qh, QDialog& parentWindow) const
  {
    auto& s = *size;
    const bool was = s.resizingWindow;
    s.resizingWindow = true;
    const QSize sz{qw, qh};
    if(parentWindow.size() != sz)
    {
      s.asked.push_back({sz, std::chrono::steady_clock::now()});
      if(s.asked.size() > 8)
        s.asked.erase(s.asked.begin());
    }
    if(view.canResize() == Steinberg::kResultTrue)
    {
      if(parentWindow.size() != sz)
        parentWindow.resize(sz);
    }
    else
    {
      parentWindow.setFixedSize(sz);
    }
    if(qwindow && qwindow->size() != sz)
    {
      qwindow->resize(qw, qh);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(qw, qh);
    }
    s.resizingWindow = was;
  }

  //! The editor takes the size r
  void sendSize(Steinberg::IPlugView& view, Steinberg::ViewRect r) const
  {
    auto& s = *size;
    s.current = r;
    s.onSizeDepth++;
    view.onSize(&r);
    s.onSizeDepth--;
  }

  //! To call right before IPlugView::attached(): a resize the plug-in asks for
  //! from there on is done in the same call stack, as are those of the user
  void setAttached() const noexcept { size->attached = true; }

  //! To call once attached(): the size the plug-in asked for before
  void sendPendingSize(Steinberg::IPlugView& view) const
  {
    auto& s = *size;
    if(!s.pending)
      return;
    s.pending = false;
    if(s.current.getWidth() > 0 && s.current.getHeight() > 0)
      sendSize(view, s.current);
  }

  //! The window takes the size the editor says it has (getSize)
  auto setSizeFromQt(
      Steinberg::IPlugView& view, const Steinberg::ViewRect& r,
      QDialog& parentWindow) const
  {
    const double scale = qtScaleFactor(parentWindow);
    int w = r.getWidth();
    int h = r.getHeight();

    if(w <= 0 || h <= 0)
    {
      return std::make_pair(w, h);
    }

    size->current = r;
    resizeWindow(view, toQt(w, scale), toQt(h, scale), parentWindow);

    return std::make_pair(w, h);
  }

  //! The window was resized from outside: by the user, through the window
  //! manager. IPlugView: checkSizeConstraint() corrects the size, the window
  //! takes the corrected size, then onSize().
  void
  setSizeFromUser(Steinberg::IPlugView& view, const QSize& sz, QDialog& parentWindow)
  {
    auto& s = *size;
    // Before attached(), and while the window is resized here, the size is
    // ours and the editor already has it
    if(!s.attached || s.resizingWindow)
      return;
    if(view.canResize() != Steinberg::kResultTrue)
      return;
    if(sz.width() <= 0 || sz.height() <= 0)
      return;

    // A size given here and reported by the window system once done, as X11
    // does: the editor had it, or has a newer one if it asked for several in a
    // row. The window system reports the newer one next. One that does not
    // report them, as the resize is done by then, keeps none for long.
    const auto now = std::chrono::steady_clock::now();
    auto it = std::find_if(s.asked.begin(), s.asked.end(), [&](const auto& a) {
      return a.size == sz && now - a.when < std::chrono::seconds(1);
    });
    if(it != s.asked.end())
    {
      s.asked.erase(s.asked.begin(), it + 1);
      return;
    }
    s.asked.clear();

    const double scale = qtScaleFactor(parentWindow);

    // The size the editor has
    if(s.current.getWidth() > 0 && s.current.getHeight() > 0
       && sz.width() == toQt(s.current.getWidth(), scale)
       && sz.height() == toQt(s.current.getHeight(), scale))
      return;

    const Steinberg::ViewRect asked{
        0, 0, toEditor(sz.width(), scale), toEditor(sz.height(), scale)};
    Steinberg::ViewRect r = asked;
    // The plug-in may only take some sizes
    view.checkSizeConstraint(&r);
    if(r.getWidth() <= 0 || r.getHeight() <= 0)
      r = asked;
    r = Steinberg::ViewRect{0, 0, r.getWidth(), r.getHeight()};

    // Only a size the plug-in changed goes back to the window: one that only
    // went through the rounding stays as the user made it
    if(!sameSize(r, asked))
      resizeWindow(view, toQt(r.getWidth(), scale), toQt(r.getHeight(), scale), parentWindow);
    else if(container)
    {
      container->move(0, 0);
      container->setFixedSize(sz);
    }

    // A size the plug-in corrected back to the one it has
    if(sameSize(r, s.current))
      return;

    sendSize(view, r);
  }

  //! The plug-in asks for a size (IPlugFrame::resizeView): the window takes it,
  //! then onSize() in the same call stack. False when refused.
  bool setSizeFromVst(
      Steinberg::IPlugView& view, Steinberg::ViewRect& r, QDialog& parentWindow) const
  {
    auto& s = *size;
    const double scale = qtScaleFactor(parentWindow);
    const int w = r.getWidth();
    const int h = r.getHeight();

    if(w <= 0 || h <= 0)
      return false;

    // A plug-in answering the onSize() sent from here with the size it was
    // just given: it has it
    if(s.onSizeDepth > 0 && sameSize(r, s.current))
      return true;
    if(s.onSizeDepth >= maxOnSizeDepth)
      return false;

    s.current = Steinberg::ViewRect{0, 0, w, h};
    resizeWindow(view, toQt(w, scale), toQt(h, scale), parentWindow);

    // The view has no window yet to resize
    if(!s.attached)
    {
      s.pending = true;
      return true;
    }

    sendSize(view, s.current);
    return true;
  }
};

class Window;
}
