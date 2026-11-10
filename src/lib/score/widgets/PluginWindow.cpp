#include "PluginWindow.hpp"

#include <QKeyEvent>
#include <QScreen>
#include <QWindow>

#if __has_include(<QtGui/private/qhighdpiscaling_p.h>)
#include <QtGui/private/qhighdpiscaling_p.h>
#define SCORE_HAS_QHIGHDPISCALING 1
#else
#define SCORE_HAS_QHIGHDPISCALING 0
#endif

namespace score
{
double PluginWindow::editorPixelRatio(const QWidget& w)
{
  double qt_factor = 1.;
#if SCORE_HAS_QHIGHDPISCALING && QT_CONFIG(highdpiscaling)
  if(auto* win = w.window()->windowHandle())
    qt_factor = QHighDpiScaling::factor(win);
  else if(auto* screen = w.screen())
    qt_factor = QHighDpiScaling::factor(screen);
#else
  if(const double r = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(); r > 0.)
    qt_factor = r;
#endif
  return editorPixelRatio(w.devicePixelRatioF(), qt_factor);
}

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
