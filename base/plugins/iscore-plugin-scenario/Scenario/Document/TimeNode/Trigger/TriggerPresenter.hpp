#pragma once
#include <QObject>

class QGraphicsObject;

namespace Scenario
{
class TriggerModel;
class TriggerView;

class TriggerPresenter final : public QObject
{
        Q_OBJECT
    public:
        TriggerPresenter(const TriggerModel&, QGraphicsObject*, QObject* parent = 0);

        const TriggerModel& model() const { return m_model;}

    private:
        const TriggerModel& m_model;
        TriggerView* m_view{};
};
}
