#pragma once
#include <QAction>
class QGraphicsScene;
namespace Scenario
{
struct SnapshotAction : public QAction
{
    public:
        SnapshotAction(QGraphicsScene& scene, QWidget* parent);

    private:
        void takeScreenshot(QGraphicsScene& scene);
};

}
