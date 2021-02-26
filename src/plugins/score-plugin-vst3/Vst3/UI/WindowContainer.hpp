#pragma once
#include <pluginterfaces/gui/iplugview.h>

#include <QDialog>
#include <QWindow>

namespace vst3
{

inline const char* currentPlatform()
{
#if defined (__APPLE__)
  return Steinberg::kPlatformTypeNSView;
#elif defined(__linux__)
  return Steinberg::kPlatformTypeX11EmbedWindowID;
#elif defined(_WIN32)
  return Steinberg::kPlatformTypeHWND;
#endif
  return "";
}

struct WindowContainer
{
  WId nativeId;
  QWindow* qwindow{};
  QWidget* container{};

  auto setSizeFromQt(Steinberg::IPlugView& view, const Steinberg::ViewRect& r, QDialog& parentWindow)
  {
    int w = r.getWidth();
    int h = r.getHeight();

    if(w < 5) w = 640;
    if(h < 5) h = 480;

    if(view.canResize() == Steinberg::kResultTrue)
    {
      parentWindow.resize(QSize{w, h});
    }
    else
    {
      parentWindow.setFixedSize(QSize{w, h});
    }
    if(qwindow)
    {
      qwindow->resize(w, h);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(w, h);
    }

    return std::make_pair(w, h);
  }

  void setSizeFromUser(Steinberg::IPlugView& view, const QSize& sz, QDialog& parentWindow)
  {

    if(view.canResize() != Steinberg::kResultTrue)
    {
      return;
    }
    Steinberg::ViewRect r;
    r.top = 0;
    r.left = 0;
    r.right = sz.width();
    r.bottom = sz.height();
    view.checkSizeConstraint(&r);

    int w = r.getWidth();
    int h = r.getHeight();
    parentWindow.resize(QSize(w, h));

    if(qwindow)
    {
      qwindow->resize(w, h);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(w, h);
    }

    view.onSize(&r);
  }

  auto setSizeFromVst(Steinberg::IPlugView& view, Steinberg::ViewRect& r, QDialog& parentWindow)
  {
    int w = r.getWidth();
    int h = r.getHeight();

    if(w < 5) w = 640;
    if(h < 5) h = 480;

    if(view.canResize() == Steinberg::kResultTrue)
    {
      parentWindow.resize(QSize{w, h});
    }
    else
    {
      parentWindow.setFixedSize(QSize{w, h});
    }

    if(qwindow)
    {
      qwindow->resize(w, h);
    }
    if(container)
    {
      container->move(0, 0);
      container->setFixedSize(w, h);
    }

    view.onSize(&r);

    return std::make_pair(w, h);
  }

};

class Window;
}
