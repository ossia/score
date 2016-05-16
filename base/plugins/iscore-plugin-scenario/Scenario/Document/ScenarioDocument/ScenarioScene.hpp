#pragma once
#include <QGraphicsScene>

namespace Scenario
{
class ScenarioScene final : public QGraphicsScene
{
        Q_OBJECT
    public:
        ScenarioScene(QWidget* parent);
};
}
