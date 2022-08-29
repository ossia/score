#include <Vst3/UI/Linux/PlugFrame.hpp>
#include <Vst3/UI/PlugFrame.hpp>
#include <Vst3/UI/Window.hpp>

namespace vst3
{
Window::Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent)
    : PluginWindow{ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop(), parent}
    , m_model{e}
{
  if(!e.fx)
    throw std::runtime_error("Cannot create UI");

  setAttribute(Qt::WA_DeleteOnClose, true);

  container = createVstWindowContainer(*this, e, ctx);
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
  event->accept();
}

void Window::closeEvent(QCloseEvent* event)
{
  QPointer<Window> p(this);
  if(auto view = m_model.fx.view)
    view->removed();
  delete container.frame;
  container.frame = nullptr;

  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  m_model.externalUIVisible(false);
  if(p)
    QDialog::closeEvent(event);
}
}
