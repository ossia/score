#ifndef SCENARIOMODEL_HPP
#define SCENARIOMODEL_HPP

#include <QObject>
#include <QPointF>
#include <QVector>

class ScenarioModel : public QObject
{
		Q_OBJECT
	public:
		explicit ScenarioModel(int id, QObject *parent);
		~ScenarioModel();
		
		int id() const;
		
	signals:
		void TimeEventAddedInModel(QPointF pos);
		void TimeEventRemovedInModel(QPointF pos);
		
	public slots:
		void addTimeEvent(QPointF pos);
		// @todo plutôt par id que par point.
		void removeTimeEvent(QPointF pos);
		
	private:
		int m_id;
};

#endif // SCENARIOMODEL_HPP
