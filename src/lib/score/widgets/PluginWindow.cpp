#include "PluginWindow.hpp"

#include <QKeyEvent>
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

void PluginWindow::keyPressEvent(QKeyEvent* event)
{
  if(event->matches(QKeySequence::Cancel))
  {
    event->accept();
    close();
    return;
  }
  QDialog::keyPressEvent(event);
}

}
