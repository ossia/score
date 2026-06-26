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
#include <QWidget>
#if __has_include(<QSvgGenerator>)
#include <QSvgGenerator>
#endif
namespace Scenario
{

QByteArray renderSceneToSvg(QGraphicsScene& scene, const QString& path, QRectF rect)
{
#if __has_include(<QSvgGenerator>)
  // Create a SVG from the scene
  QBuffer b;
  QSvgGenerator p;
  p.setOutputDevice(&b);
  QPainter painter;
  painter.begin(&p);
  painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  scene.render(&painter, rect, rect);
  painter.end();

  if(!path.isEmpty())
  {
    QFile screenshot(path);
    if(screenshot.open(QFile::WriteOnly))
    {
      screenshot.write(b.buffer());
      screenshot.close();
    }
  }

  return b.buffer();
#else
  return {};
#endif
}

SnapshotAction::SnapshotAction(QGraphicsScene& scene, QWidget* parent)
    : QAction{tr("Scenario screenshot"), parent}
{
  setShortcutContext(Qt::WidgetWithChildrenShortcut);
  setShortcut(QKeySequence(Qt::Key_F10));

  connect(this, &QAction::triggered, this, [&] { takeScreenshot(scene); });
}

void SnapshotAction::takeScreenshot(QGraphicsScene& scene)
{
// Render the scene and save a file for convenience
#if defined(__APPLE__) || defined(__linux__)
  auto path = QStringLiteral("/tmp/screenshot.svg");
#else
  auto path = QStringLiteral("screenshot.svg");
#endif

  QByteArray svg = renderSceneToSvg(scene, path);
  if(svg.isEmpty())
    return;

  // Set the clipboard
  auto d = new QMimeData;
  d->setData("image/svg+xml", svg);
  // TODO investigate : the doc says that setMimeData takes ownership.
  QApplication::clipboard()->setMimeData(d, QClipboard::Clipboard);
  QApplication::clipboard()->setMimeData(d, QClipboard::Selection);
}
}
