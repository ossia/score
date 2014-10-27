#ifndef SCENARIOPRESENTER_H
#define SCENARIOPRESENTER_H

#include <QObject>

class ScenarioModel;
class ScenarioView;

class ScenarioPresenter : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioPresenter(ScenarioModel *model, ScenarioView *view, QObject *parent = 0);
    ~ScenarioPresenter();

signals:

public slots:

private:
    ScenarioModel *_pModel = nullptr;
    ScenarioView *_pView = nullptr;
};

#endif // SCENARIOPRESENTER_H
