#ifndef SCENARIOCONTAINER_HPP
#define SCENARIOCONTAINER_HPP

#include <QObject>

class ScenarioModel;
class ScenarioView;
class ScenarioPresenter;
class QGraphicsObject;

class ScenarioContainer : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioContainer(QObject *parent, QGraphicsObject *parentView);
    ~ScenarioContainer();

    ScenarioView* view() const {return _pView;}
    ScenarioModel* model() const {return _pModel;}
signals:

public slots:

private:
    ScenarioModel* _pModel = nullptr;
    ScenarioView* _pView = nullptr;
    ScenarioPresenter* _pPresenter = nullptr;

};

#endif // SCENARIOCONTAINER_HPP
