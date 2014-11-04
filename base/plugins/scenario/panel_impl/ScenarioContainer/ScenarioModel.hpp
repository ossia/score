#ifndef SCENARIOMODEL_HPP
#define SCENARIOMODEL_HPP

#include <QObject>
#include <QPointF>

class ScenarioModel : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioModel(QObject *parent = 0);
    ~ScenarioModel();

signals:
    void TimeEventAddedInModel(QPointF pos);

public slots:
    void addTimeEvent(QPointF pos);
};

#endif // SCENARIOMODEL_HPP
