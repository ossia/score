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
class StoreyModel : public QIdentifiedObject
{
	Q_OBJECT

	public:
		friend QDataStream& operator << (QDataStream& , const StoreyModel& );

		StoreyModel(QDataStream& s, IntervalContentModel* parent);
		StoreyModel(int id, IntervalContentModel* parent);
		virtual ~StoreyModel() = default;

		int createProcessViewModel(int sharedProcessId);
		int createProcessViewModel(QDataStream& s,int sharedProcessId);
		void deleteProcessViewModel(int processViewModelId);

		/**
		 * @brief selectForEdition
		 * @param processViewId
		 *
		 * A process is selected for edition when it is
		 * the edited process when the interface is clicked.
		 */
		void selectForEdition(int processViewId);

		const std::vector<iscore::ProcessViewModelInterface*>&
		processViewModels()
		{
			return m_processViewModels;
		}


		/**
		 * @brief parentInterval
		 * @return the interval this storey is part of.
		 */
		IntervalModel* parentInterval();

	signals:
		void processViewModelCreated(int processViewModelId);
		void processViewModelDeleted(int processViewModelId);

		void processViewModelSelected(int processViewModelId);

	public slots:
		void on_deleteSharedProcessModel(int sharedProcessId);

	private:

		int m_editedProcessId{};
		std::vector<iscore::ProcessViewModelInterface*> m_processViewModels;

		int m_nextProcessViewModelId{};
};

