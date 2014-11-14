#ifndef SCENARIOCONTAINER_HPP
#define SCENARIOCONTAINER_HPP

#include <QNamedObject>

class ScenarioModel;
class ScenarioView;
class ScenarioPresenter;
class QGraphicsObject;
class TimeEvent;

/*
 * TODO @Nico
 * En fait ça il faut (suffit) d'en faire le ScenarioProcess en gros.
 */
class ScenarioContainer : public QNamedObject
{
		Q_OBJECT
	public:
		explicit ScenarioContainer (QObject* parent, QGraphicsObject* parentView);
		~ScenarioContainer();

		ScenarioModel* model() const
		{
			return _pModel;
		}
		ScenarioView* view() const
		{
			return _pView;
		}
		
		ScenarioPresenter* presenter() const
		{
			return _pPresenter;
		}

	signals:

	public slots:

	private:
		ScenarioModel* _pModel = nullptr;
		ScenarioView* _pView = nullptr;
		ScenarioPresenter* _pPresenter = nullptr;

		static int m_modelId;

};

#endif // SCENARIOCONTAINER_HPP
