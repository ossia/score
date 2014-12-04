#ifndef SCENARIOPRESENTER_H
#define SCENARIOPRESENTER_H

#include <QObject>
#include <QPointF>

class ScenarioModel;
class ScenarioView;
class TimeEventPresenter;
class TimeEventModel;

class ScenarioPresenter : public QObject
{
		Q_OBJECT
	public:
		explicit ScenarioPresenter (ScenarioModel* model, ScenarioView* view, QObject* parent);
		~ScenarioPresenter();
		
		void addTimeEventPresenter(TimeEventPresenter* p)
		{
			m_timeEvents.push_back(p);
		}

	public slots:
		void on_askTimeEvent (QPointF pos);
		void on_timeEventAdded(TimeEventModel* model);
		void on_timeEventRemoved(TimeEventModel* model);

	private:
		ScenarioModel* m_model = nullptr;
		ScenarioView* m_view = nullptr;
		
		std::vector<TimeEventPresenter*> m_timeEvents;
};

#endif // SCENARIOPRESENTER_H
