#pragma once
#include <QNamedObject>
#include <vector>


class IntervalModel;
class IntervalContentPresenter;
namespace iscore
{
	class ProcessPresenterInterface;
}
/**
 * @brief The IntervalPresenter class
 *
 * Présenteur : reçoit signaux depuis modèle et vue et présenteurs enfants.
 * Exemple : cas d'un process ajouté : le modèle reçoit la commande addprocess, émet un signal, qui est capturé par le présenteur qui va instancier le présenteur nécessaire en appelant la factory.
 */
class IntervalPresenter : public QNamedObject
{
	Q_OBJECT

	public:
		IntervalPresenter(IntervalModel* model, QObject* parent);
		virtual ~IntervalPresenter() = default;

	private:

		std::vector<IntervalContentPresenter*> m_contentModels; // No content -> Phantom ?
		std::vector<iscore::ProcessPresenterInterface*> m_processes;

		IntervalModel* m_model;

};

