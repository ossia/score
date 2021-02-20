#include <Vst/Window.hpp>
#include <Media/Effect/Settings/Model.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(vst::Window)

namespace vst
{
ERect Window::getRect(AEffect& e)
{
  ERect* vstRect{};

  e.dispatcher(&e, effEditGetRect, 0, 0, &vstRect, 0.f);

  int16_t w{};
  int16_t h{};
  if (vstRect)
  {
    w = vstRect->right - vstRect->left;
    h = vstRect->bottom - vstRect->top;
  }

  if (w <= 1)
    w = 640;
  if (h <= 1)
    h = 480;

  if (vstRect)
    return ERect{vstRect->top, vstRect->left, vstRect->bottom, vstRect->right};
  else
    return ERect{0, 0, w, h};
}

Window::Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent)
    : Window{e, ctx}
{
  setAttribute(Qt::WA_DeleteOnClose, true);

  connect(&ctx.coarseUpdateTimer, &QTimer::timeout,
          this, &Window::refreshTimer,
          Qt::UniqueConnection);

  bool ontop = ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop();
  if (ontop)
  {
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
  }
  e.externalUIVisible(true);
}

Window::~Window() { }

void Window::refreshTimer()
{
  if (auto eff = effect.lock())
    eff->fx->dispatcher(eff->fx, effEditIdle, 0, 0, nullptr, 0);

}

void Window::closeEvent(QCloseEvent* event)
{
  QPointer<Window> p(this);
  if (auto eff = effect.lock())
    eff->fx->dispatcher(eff->fx, effEditClose, 0, 0, nullptr, 0);
  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  m_model.externalUIVisible(false);
  if (p)
    QDialog::closeEvent(event);
}

void Window::resizeEvent(QResizeEvent* event)
{
  // setup_rect(this, event->size().width(), event->size().height());
  QDialog::resizeEvent(event);
}

void Window::resize(int w, int h)
{
  setup_rect(this, w, h);
}


}
