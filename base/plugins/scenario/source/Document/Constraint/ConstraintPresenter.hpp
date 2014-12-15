#pragma once
#include <tools/NamedObject.hpp>

#include <vector>

#include "Document/Constraint/ConstraintData.hpp"

class ConstraintModel;
class ConstraintView;
class BoxPresenter;
class BoxModel;


namespace iscore
{
	class SerializableCommand;
}
class ProcessPresenterInterface;

/**
 * @brief The ConstraintPresenter class
 *
 * Présenteur : reçoit signaux depuis modèle et vue et présenteurs enfants.
 * Exemple : cas d'un process ajouté : le modèle reçoit la commande addprocess, émet un signal, qui est capturé par le présenteur qui va instancier le présenteur nécessaire en appelant la factory.
 */
class ConstraintPresenter : public NamedObject
{
	Q_OBJECT

	public:
		ConstraintPresenter(ConstraintModel* model,
						  ConstraintView* view,
						  QObject* parent);
		virtual ~ConstraintPresenter();

		int id() const;
		ConstraintView* view();
		ConstraintModel* model();

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);
		void constraintReleased(ConstraintData);

		void askUpdate();

	public slots:
		void on_constraintPressed(QPointF);
		void on_boxCreated(int boxId);

		void on_askUpdate();

	private:
		void on_boxCreated_impl(BoxModel*);

		std::vector<BoxPresenter*> m_contentPresenters; // No content -> Phantom ?
		// Process presenters are in the storey presenters.
		ConstraintModel* m_model{};
		ConstraintView* m_view{};

		long m_millisecPerPixel{1};

		QPointF clickedPoint{-1,-1};
};

