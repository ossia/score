#pragma once
#include <QAction>
class QQuickItem;
namespace Scenario
{
struct SnapshotAction : public QAction
{
public:
  SnapshotAction(QQuickItem& scene, QWidget* parent);

private:
  void takeScreenshot(QQuickItem& scene);
};
}
