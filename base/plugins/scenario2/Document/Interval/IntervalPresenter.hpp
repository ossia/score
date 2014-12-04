#pragma once
#include <QNamedObject>
#include <vector>


class IntervalModel;
class IntervalView;
class IntervalContentPresenter;
namespace iscore
{
	class SerializableCommand;
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
		IntervalPresenter(IntervalModel* model,
						  IntervalView* view,
						  QObject* parent);
		virtual ~IntervalPresenter();

		int id() const;

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

	public slots:
		void on_intervalPressed();

	private:
		std::vector<IntervalContentPresenter*> m_contentPresenters; // No content -> Phantom ?
		// Process presenters are in the storey presenters.
		IntervalModel* m_model{};
		IntervalView* m_view{};

};

