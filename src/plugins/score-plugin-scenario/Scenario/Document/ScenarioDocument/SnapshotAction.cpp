// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SnapshotAction.hpp"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QFile>
#include <QGraphicsScene>
#include <QMimeData>
#include <QPainter>
#if __has_include(<QSvgGenerator>)
#include <QSvgGenerator>
#endif
namespace Scenario
{

SnapshotAction::SnapshotAction(QGraphicsScene& scene, QWidget* parent)
    : QAction{tr("Scenario screenshot"), parent}
{
  setShortcutContext(Qt::WidgetWithChildrenShortcut);
  setShortcut(QKeySequence(Qt::Key_F10));

  connect(this, &QAction::triggered, this, [&] { takeScreenshot(scene); });
}

void SnapshotAction::takeScreenshot(QGraphicsScene& scene)
{
#if __has_include(<QSvgGenerator>)
  // Create a SVG from the scene
  QBuffer b;
  QSvgGenerator p;
  p.setOutputDevice(&b);
  QPainter painter;
  painter.begin(&p);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  scene.render(&painter, QRectF(0, 0, 1920, 1080), QRectF(0, 0, 1920, 1080));
  painter.end();

  // Set the clipboard
  auto d = new QMimeData;
  d->setData("image/svg+xml", b.buffer());
  // TODO investigate : the doc says that setMimeData takes ownership.
  QApplication::clipboard()->setMimeData(d, QClipboard::Clipboard);
  QApplication::clipboard()->setMimeData(d, QClipboard::Selection);

// Also save a file for convenience
#if defined(__APPLE__) || defined(__linux__)
  auto path = "/tmp/screenshot.svg";
#else
  auto path = "screenshot.svg";
#endif
  QFile screenshot(path);
  screenshot.open(QFile::WriteOnly);
  screenshot.write(b.buffer());
  screenshot.close();
#endif
}
}
