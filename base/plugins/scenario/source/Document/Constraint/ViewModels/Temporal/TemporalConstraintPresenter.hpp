#pragma once
#include "Document/Constraint/ViewModels/AbstractConstraintPresenter.hpp"
#include "Document/Constraint/ConstraintData.hpp"

class TemporalConstraintViewModel;
class TemporalConstraintView;


namespace iscore
{
	class SerializableCommand;
}
class ProcessPresenterInterface;

/**
 * @brief The TemporalConstraintPresenter class
 *
 * Présenteur : reçoit signaux depuis modèle et vue et présenteurs enfants.
 * Exemple : cas d'un process ajouté : le modèle reçoit la commande addprocess, émet un signal, qui est capturé par le présenteur qui va instancier le présenteur nécessaire en appelant la factory.
 */
class TemporalConstraintPresenter : public AbstractConstraintPresenter
{
	Q_OBJECT

	public:
		using view_model_type = TemporalConstraintViewModel;
		using view_type = TemporalConstraintView;

		TemporalConstraintPresenter(TemporalConstraintViewModel* viewModel,
									TemporalConstraintView* view,
									QObject* parent);
		virtual ~TemporalConstraintPresenter();

	signals:
		void constraintMoved(ConstraintData);
		void constraintReleased();

};

