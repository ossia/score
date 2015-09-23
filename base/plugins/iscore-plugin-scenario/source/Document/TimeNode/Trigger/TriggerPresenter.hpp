#pragma once
#include <QObject>
class TriggerModel;
class TriggerView;
class QGraphicsObject;

class TriggerPresenter : public QObject
{
        Q_OBJECT
    public:
        TriggerPresenter(TriggerModel*, QGraphicsObject*, QObject* parent = 0);

    private:
        TriggerModel* m_model;
        TriggerView* m_view;
};
