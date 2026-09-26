#pragma once
#include <score_plugin_scenario_export.h>

#include <QAction>
#include <QRectF>
#include <QString>
class QGraphicsScene;
namespace Scenario
{
// Render a region of a QGraphicsScene to SVG and return the raw SVG bytes.
// If `path` is non-empty, the SVG is also written there.
// Returns an empty QByteArray on failure (e.g. QtSvg unavailable).
// Shared by the F10 SnapshotAction and the JS View.grabScene() API.
SCORE_PLUGIN_SCENARIO_EXPORT
QByteArray renderSceneToSvg(
    QGraphicsScene& scene, const QString& path = {},
    QRectF rect = QRectF(0, 0, 1920, 1080));

// Render a region of a QGraphicsScene to a file: SVG for a .svg path, else a
// raster image in the format of the extension. False if nothing was written.
SCORE_PLUGIN_SCENARIO_EXPORT
bool renderSceneToFile(QGraphicsScene& scene, const QString& path, QRectF rect);

struct SnapshotAction : public QAction
{
public:
  SnapshotAction(QGraphicsScene& scene, QWidget* parent);

private:
  void takeScreenshot(QGraphicsScene& scene);
};
}
