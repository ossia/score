#pragma once
#include <Vst3/UI/Linux/PlugFrame.hpp>
#include <Vst3/UI/PlugFrame.hpp>
#include <Vst3/EffectModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <Media/Effect/Settings/Model.hpp>

#include <pluginterfaces/gui/iplugview.h>

#include <QDialog>

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

class Window : public QDialog
{
  const Model& m_model;
public:
  Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent)
    : QDialog{parent}
    , m_model{e}
  {
    Steinberg::IPlugView& view = *e.fx.view;

    setAttribute(Qt::WA_DeleteOnClose, true);

    Steinberg::ViewRect r;
    view.getSize(&r);

    if(view.canResize())
    {
      resize(QSize{r.getWidth(), r.getHeight()});
    }
    else
    {
      setFixedSize(QSize{r.getWidth(), r.getHeight()});
    }
    show();

    view.setFrame(new PlugFrame{*this->windowHandle()});
    view.attached((void*)winId(), currentPlatform());

    bool ontop = score::AppContext().settings<Media::Settings::Model>().getVstAlwaysOnTop();
    if (ontop)
    {
      setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint | Qt::WindowCloseButtonHint);
    }
    else
    {
      setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
    }

    e.externalUIVisible(true);
  }

  void resizeEvent(QResizeEvent* event)
  {
    QDialog::resizeEvent(event);
    Steinberg::IPlugView& view = *m_model.fx.view;

    auto& g = this->geometry();
    Steinberg::ViewRect r;
    r.top = g.top();
    r.bottom = g.bottom();
    r.left = g.left();
    r.bottom = g.right();
    view.onSize(&r);
  }


  void closeEvent(QCloseEvent* event)
  {
    QPointer<Window> p(this);
    if(auto view = m_model.fx.view)
      view->removed();

    const_cast<QWidget*&>(m_model.externalUI) = nullptr;
    m_model.externalUIVisible(false);
    if (p)
      QDialog::closeEvent(event);
  }

};
}
