#ifndef SCENARIOMODEL_HPP
#define SCENARIOMODEL_HPP

#include <QObject>

class ScenarioModel : public QObject
{
    Q_OBJECT
public:
    explicit ScenarioModel(QObject *parent = 0);
    ~ScenarioModel();

signals:

public slots:

};

#endif // SCENARIOMODEL_HPP
