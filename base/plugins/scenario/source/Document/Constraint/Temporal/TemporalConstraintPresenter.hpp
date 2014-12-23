#pragma once
#include <tools/NamedObject.hpp>

#include <vector>

#include "Document/Constraint/ConstraintData.hpp"

class TemporalConstraintViewModel;
class TemporalConstraintView;
class BoxPresenter;
class BoxModel;


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
class TemporalConstraintPresenter : public NamedObject
{
	Q_OBJECT

	public:
		TemporalConstraintPresenter(TemporalConstraintViewModel* viewModel,
						  TemporalConstraintView* view,
						  QObject* parent);
		virtual ~TemporalConstraintPresenter();

		int viewModelId() const;
		TemporalConstraintView* view();
		TemporalConstraintViewModel* viewModel();

		bool isSelected() const;
		void deselect();

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);
		void constraintReleased(ConstraintData);

		void askUpdate();

	public slots:
		void on_constraintPressed(QPointF);
		void on_boxCreated(int boxId);
		void on_boxRemoved(int boxId);

		void on_askUpdate();

	private:
		void on_boxCreated_impl(BoxModel*);

		std::vector<BoxPresenter*> m_boxes; // No content -> Phantom ?
		// Process presenters are in the deck presenters.
		TemporalConstraintViewModel* m_viewModel{};
		TemporalConstraintView* m_view{};

		long m_millisecPerPixel{1};
};

