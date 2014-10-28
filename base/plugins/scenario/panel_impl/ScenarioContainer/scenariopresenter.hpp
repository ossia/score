#ifndef SCENARIOPRESENTER_H
#define SCENARIOPRESENTER_H

#include <QObject>
#include <QPointF>

class ScenarioModel;
class ScenarioView;

class ScenarioPresenter : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioPresenter(ScenarioModel *model, ScenarioView *view, QObject *parent);
    ~ScenarioPresenter();

signals:
    void addTimeEvent(QPointF pos);

public slots:
    void addTimeEventInModel(QPointF pos);
    void addTimeEventInView(QPointF pos);

private:
    ScenarioModel *_pModel = nullptr;
    ScenarioView *_pView = nullptr;
};

#endif // SCENARIOPRESENTER_H
