#ifndef SCENARIOMODEL_HPP
#define SCENARIOMODEL_HPP

#include <QObject>
#include <QPointF>
#include <QVector>
class TimeBoxModel;
class TimeEventModel;
class ScenarioModel : public QObject
{
		Q_OBJECT
	public:
		explicit ScenarioModel (int id, QObject* parent);
		~ScenarioModel();

		int id() const;
		
		int addTimeEvent(int id, QPointF p);
		void removeTimeEvent(int modelIndex);
		
		// Boîte : on la crée toujours entre deux évènements.
		// Dans un cas cela revient à créer un nouvel évènement
		// Actions de base sur le modèle : 
		//	* ajouter un event : 
		//		- Crée un event au temps T
		//		- Obtention d'une relation fantôme avec l'event de début du scénario depuis l'API, 
		//	* créer une relation entre deux events.
		//
		// Commandes dans l'interface : 
		//	* créer un event
		//	* créer une relation entre deux events
		//	* créer une relation dans le vide
		//	* créer une relation depuis un event existant dans le vide
		//	* matérialiser l'action fantôme du début ?
		int addTimeBox(int id, int startEventId, int endEventId);
		void removeTimeBox(int modelIndex);
		
		const std::vector<TimeEventModel*>& timeEvents() const
		{
			return m_timeEvents;
		}
		
	signals:
		void timeEventAdded(TimeEventModel*);
		void timeEventRemoved(TimeEventModel* model);
		
		void timeBoxAdded(TimeBoxModel*);
		void timeBoxRemoved(TimeBoxModel* model);
		
	private:
		int m_id;

		std::vector<TimeBoxModel*> m_timeBoxes;
		std::vector<TimeEventModel*> m_timeEvents;
};

#endif // SCENARIOMODEL_HPP
