#pragma once
#include <QNamedObject>
#include <vector>

class IntervalContentModel;
class IntervalModel;

namespace iscore
{
	// Possibilité : mettre ça plutôt dans Scenario et travailler avec des ScenarioProcessSharedModel ? Bof pour autres plug-ins...
	class ProcessViewModelInterface;
}

/////////////////// !!!
/// NOTE : 
/// Pour undo-redo, dans les commandes, 
/// on doit mettre l'id de tous les modèles parents
/// récursivement (sinon ça peut pas marcher).
/// 
/// Aussi penser au cas ou quelqu'un quitte 
/// puis relance et rejoint la session réseau: 
/// les ids doivent rester les mêmes
/////////////////// !!!
class StoreyModel : public QNamedObject
{
	Q_OBJECT
	
	public:
		StoreyModel(int id, IntervalContentModel* parent);
		virtual ~StoreyModel() = default;
		
		int id() const
		{
			return m_id;
		}
		
		void createProcessViewModel(int processId);
		void deleteProcessViewModel(int processViewModelId);
		
		/**
		 * @brief selectForEdition
		 * @param processViewId
		 *
		 * A process is selected for edition when it is 
		 * the edited process when the interface is clicked.
		 */
		void selectForEdition(int processViewId);
		
	signals:
		void processViewModelCreated(int processViewModelId);
		void processViewModelDeleted(int processViewModelId);
		
		void processViewModelSelected(int processViewModelId);
		
	private:
		/**
		 * @brief parentInterval
		 * @return the interval this storey is part of.
		 */
		IntervalModel* parentInterval();
		
		int m_id{};
		
		int m_editedProcessId{};
		std::vector<iscore::ProcessViewModelInterface*> m_processViewModels;
		
		int m_nextProcessViewModelId{};
	
};

