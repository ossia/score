#include "PluginWindow.hpp"
namespace score
{

PluginWindow::PluginWindow(bool onTop, QWidget* parent)
    : QDialog{parent}
{
  setAttribute(Qt::WA_DeleteOnClose, true);

  auto defaultFlags = windowFlags() | Qt::Window | Qt::WindowCloseButtonHint;

  if(onTop)
    defaultFlags |= Qt::WindowStaysOnTopHint;

  setWindowFlags(defaultFlags);
}

score::PluginWindow::~PluginWindow() { }

}
