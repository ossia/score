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

		virtual void recomputeViewport() override {}

		// public slot override
		virtual void on_constraintPressed(QPointF) override;

	signals:
		void constraintReleased(ConstraintData);

		void minDurationChanged();
		void maxDurationChanged();
		void defaultDurationChanged();

	public slots:
		void on_minDurationChanged(int);
		void on_maxDurationChanged(int);

		void updateView();
};

