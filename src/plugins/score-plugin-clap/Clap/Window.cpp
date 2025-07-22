#include <Clap/Window.hpp>
#include <Media/Effect/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QTimer>
#include <QWindow>
#include <QResizeEvent>

#include <wobjectimpl.h>

namespace Clap
{

Window::Window(const Model& e, const score::DocumentContext& ctx, QWidget* parent)
    : PluginWindow{ctx.app.settings<Media::Settings::Model>().getVstAlwaysOnTop(), parent}
    , m_model{e}
{
  const_cast<Model&>(e).window = this;
  m_handle = e.handle();
  if(!m_handle || !m_handle->plugin)
    throw std::runtime_error("Cannot create UI - no plugin loaded");

  if(!queryExtensions())
    throw std::runtime_error("Plugin does not support GUI extension");

  initializeGui();
}

Window::~Window()
{
  const_cast<Model&>(m_model).window = nullptr;
  destroyClapWindow();
}

bool Window::queryExtensions()
{
  if(!m_handle->plugin)
    return false;
    
  m_gui_ext = static_cast<const clap_plugin_gui_t*>(
      m_handle->plugin->get_extension(m_handle->plugin, CLAP_EXT_GUI));
      
  return m_gui_ext != nullptr;
}

void Window::initializeGui()
{
  if(!m_gui_ext)
    return;
    
  // Query preferred API
  const char* api = nullptr;
  bool is_floating = false;
  
  if(m_gui_ext->get_preferred_api)
  {
    m_gui_ext->get_preferred_api(m_handle->plugin, &api, &is_floating);
  }

  // Try to use the preferred API, or fallback to platform default
  if(!api || !m_gui_ext->is_api_supported(m_handle->plugin, api, false))
  {
#if defined(__APPLE__)
    api = CLAP_WINDOW_API_COCOA;
#elif defined(_WIN32)
    api = CLAP_WINDOW_API_WIN32;
#else
    api = CLAP_WINDOW_API_X11;
#endif
  }

  // Check if the selected API is supported for embedded mode
  if(!m_gui_ext->is_api_supported(m_handle->plugin, api, false))
  {
    // Try floating mode
    if(m_gui_ext->is_api_supported(m_handle->plugin, api, true))
    {
      m_is_floating = true;
    }
    else
    {
      throw std::runtime_error("No supported GUI API found");
    }
  }

  m_gui_api = api;

  if(!createClapWindow())
    throw std::runtime_error("Failed to create CLAP window");
}

bool Window::createClapWindow()
{
  if(!m_gui_ext || m_gui_created)
    return false;
    
  // Create the GUI
  if(!m_gui_ext->create(m_handle->plugin, m_gui_api.c_str(), m_is_floating))
    return false;
    
  m_gui_created = true;
  
  // Get initial size
  uint32_t width = 0, height = 0;
  if(m_gui_ext->get_size && m_gui_ext->get_size(m_handle->plugin, &width, &height))
  {
    if(width > 0 && height > 0)
    {
      setup_rect(this, width, height);
    }
    else
    {
      width = 640;
      height = 480;

      if(m_gui_ext->set_size && m_gui_ext->set_size(m_handle->plugin, width, height))
      {
      }
    }
  }
  else
  {
    width = 640;
    height = 480;

    if(m_gui_ext->set_size && m_gui_ext->set_size(m_handle->plugin, width, height))
    {
    }
  }

  if(!m_is_floating)
  {
    // Set up the parent window
    m_clap_window.api = m_gui_api.c_str();
    
    // Platform-specific window handle will be set in show event
    
    // Set scale if supported
    if(m_gui_ext->set_scale)
    {
      m_gui_ext->set_scale(m_handle->plugin, devicePixelRatio());
    }
  }
  
  return true;
}

void Window::destroyClapWindow()
{
  if(m_gui_ext && m_gui_created)
  {
    if(m_gui_visible)
    {
      m_gui_ext->hide(m_handle->plugin);
      m_gui_visible = false;
    }
    
    m_gui_ext->destroy(m_handle->plugin);
    m_gui_created = false;
  }
}

void Window::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);

  if(!m_gui_ext || !m_gui_created || m_gui_visible)
    return;
    
  if(!m_is_floating)
  {
    // Set the platform-specific window handle
#if defined(__APPLE__)
    // Will be handled in platform-specific code
#elif defined(_WIN32)
    m_clap_window.win32 = reinterpret_cast<clap_hwnd>(windowHandle()->winId());
#else
    m_clap_window.x11 = windowHandle()->winId();
#endif

    // Set parent window
    if(!m_gui_ext->set_parent(m_handle->plugin, &m_clap_window))
    {
      qWarning() << "Failed to set parent window for CLAP plugin";
      return;
    }
  }
  else
  {
    // For floating windows, set transient parent
    if(m_gui_ext->set_transient)
    {
      // FIXME should be the main window WId instead?
      clap_window_t parent_window;
      parent_window.api = m_gui_api.c_str();
#if defined(__APPLE__)
      // Will be handled in platform-specific code
#elif defined(_WIN32)
      parent_window.win32 = reinterpret_cast<clap_hwnd>(windowHandle()->winId());
#else
      parent_window.x11 = windowHandle()->winId();
#endif
      m_gui_ext->set_transient(m_handle->plugin, &parent_window);
    }
    
    // Suggest window title
    if(m_gui_ext->suggest_title)
      m_gui_ext->suggest_title(
          m_handle->plugin, m_model.prettyName().toStdString().c_str());
  }
  
  // Show the GUI
  if(m_gui_ext->show(m_handle->plugin))
  {
    m_gui_visible = true;
  }
}

void Window::hideEvent(QHideEvent* event)
{
  if(m_gui_ext && m_gui_visible)
  {
    m_gui_ext->hide(m_handle->plugin);
    m_gui_visible = false;
  }
  
  QDialog::hideEvent(event);
}

void Window::closeEvent(QCloseEvent* event)
{
  QPointer<Window> p(this);
  
  destroyClapWindow();
  
  // Clean up model references
  const_cast<QWidget*&>(m_model.externalUI) = nullptr;
  m_model.externalUIVisible(false);
  
  if(p)
    QDialog::closeEvent(event);
}

void Window::resizeEvent(QResizeEvent* event)
{
  QDialog::resizeEvent(event);
  
  if(m_gui_ext && m_gui_created && !m_is_floating)
  {
    // For embedded windows, we might need to resize the plugin GUI
    uint32_t width = event->size().width();
    uint32_t height = event->size().height();
    
    if(m_gui_ext->can_resize && m_gui_ext->can_resize(m_handle->plugin))
    {
      if(m_gui_ext->adjust_size)
      {
        m_gui_ext->adjust_size(m_handle->plugin, &width, &height);
      }
      
      if(m_gui_ext->set_size)
      {
        m_gui_ext->set_size(m_handle->plugin, width, height);
      }
    }
  }
}

void Window::resize(int w, int h)
{
  setup_rect(this, w, h);
}

void Window::setup_rect(QWidget* container, int width, int height)
{
  width = width / container->devicePixelRatio();
  height = height / container->devicePixelRatio();
  container->setFixedSize(width, height);
}
}
