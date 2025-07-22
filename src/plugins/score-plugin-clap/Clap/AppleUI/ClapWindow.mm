#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

#include <Clap/Window.hpp>
#include <Clap/EffectModel.hpp>

#include <QWindow>
#include <QWidget>
#include <QApplication>
#include <QDebug>

namespace Clap
{

void Window::setup_rect(QWidget* container, int width, int height)
{
  width = width / container->devicePixelRatio();
  height = height / container->devicePixelRatio();
  container->setFixedSize(width, height);

  auto c = container->findChild<QWidget*>("ClapWindow");
  if(c)
  {
    c->setFixedSize(width, height);
  }
}

// Override showEvent for macOS-specific handling
void Window::showEvent(QShowEvent* event)
{
  QDialog::showEvent(event);
  
  if(!m_gui_ext || !m_gui_created || m_gui_visible)
    return;
    
  if(!m_is_floating)
  {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Get initial size
    uint32_t width = 640, height = 480;
    if(m_gui_ext->get_size)
    {
      m_gui_ext->get_size(m_handle->plugin, &width, &height);
    }
    
    // Create NSView for the plugin
    id superview = [[NSView alloc] initWithFrame: NSMakeRect(0, 0, width, height)];
    
    // Set up the CLAP window structure
    m_clap_window.api = CLAP_WINDOW_API_COCOA;
    m_clap_window.cocoa = superview;
    
    // Set parent window
    if(!m_gui_ext->set_parent(m_handle->plugin, &m_clap_window))
    {
      [superview release];
      [pool release];
      qWarning() << "Failed to set parent window for CLAP plugin";
      return;
    }
    
    // Create Qt container for the NSView
    auto superview_window = QWindow::fromWinId(reinterpret_cast<WId>(superview));
    auto container = QWidget::createWindowContainer(superview_window, this);
    container->setObjectName("ClapWindow");
    container->resize(width, height);
    
    // Set up frame change notification
    NSArray* subviews = [superview subviews];
    if([subviews count] > 0)
    {
      id plugin_view = [[subviews objectAtIndex:0] retain];
      
      [[NSNotificationCenter defaultCenter] addObserverForName:@"NSViewFrameDidChangeNotification" 
        object:plugin_view 
        queue:nullptr 
        usingBlock:^(NSNotification* notification) {
          Q_UNUSED(notification);
          
          // Adjust superview frame to match plugin view frame
          [superview setFrame:[plugin_view frame]];
          
          NSRect frame = [plugin_view frame];
          QSize newSize(frame.size.width, frame.size.height);
          setFixedSize(newSize);
          
          // Process events to update the UI
          QApplication::processEvents();
          adjustSize();
        }];
    }
    
    [pool release];
  }
  else
  {
    // For floating windows on macOS
    clap_window_t parent_window;
    parent_window.api = CLAP_WINDOW_API_COCOA;
    
    // Get the NSView from the Qt window
    NSView* view = reinterpret_cast<NSView*>(winId());
    parent_window.cocoa = view;
    
    if(m_gui_ext->set_transient)
    {
      m_gui_ext->set_transient(m_handle->plugin, &parent_window);
    }
    
    // Suggest window title
    if(m_gui_ext->suggest_title)
    {
      m_gui_ext->suggest_title(m_handle->plugin, m_model.prettyName().toStdString().c_str());
    }
  }
  
  // Show the GUI
  if(m_gui_ext->show(m_handle->plugin))
  {
    m_gui_visible = true;
  }
}

}