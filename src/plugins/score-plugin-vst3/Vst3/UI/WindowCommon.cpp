#include <Vst3/UI/Linux/PlugFrame.hpp>
#include <Vst3/UI/PlugFrame.hpp>
#include <Vst3/UI/Window.hpp>

namespace vst3
{
Window::Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent)
  : QDialog{parent}
  , m_model{e}
{
  setAttribute(Qt::WA_DeleteOnClose, true);

  container = createVstWindowContainer(*this, e, ctx);

  bool ontop = score::AppContext()
      .settings<Media::Settings::Model>()
      .getVstAlwaysOnTop();
  if (ontop)
  {
    setWindowFlags(
          windowFlags() | Qt::WindowStaysOnTopHint
          | Qt::WindowCloseButtonHint);
  }
  else
  {
    setWindowFlags(windowFlags() | Qt::WindowCloseButtonHint);
  }
  show();

  e.externalUIVisible(true);
}

Window::~Window()
{
  if(container.frame)
  {
    delete container.frame;
    container.frame = nullptr;
  }
}

void Window::resizeEvent(QResizeEvent* event)
{
  Steinberg::IPlugView& view = *m_model.fx.view;
  container.setSizeFromUser(view, event->size(), *this);
  /*
    QDialog::resizeEvent(event);

    Steinberg::IPlugView& view = *m_model.fx.view;

    auto& g = this->geometry();
    Steinberg::ViewRect r;
    r.top = 0;
    r.bottom = g.height();
    r.left = 0;
    r.bottom = g.width();

    container.qwindow->resize(r.getWidth(), r.getHeight());
    container.container->resize(r.getWidth(), r.getHeight());

    if(view.canResize() == Steinberg::kResultTrue)
      view.onSize(&r);
      */
}

void Window::closeEvent(QCloseEvent* event)
{
  QPointer<Window> p(this);
  if (auto view = m_model.fx.view)
    view->removed();
  delete container.frame;
  container.frame = nullptr;

  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  m_model.externalUIVisible(false);
  if (p)
    QDialog::closeEvent(event);
}
}
