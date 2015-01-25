#pragma once
#include <tools/SettableIdentifier.hpp>
#include <tools/NamedObject.hpp>

#include <vector>

#include "Document/Constraint/ConstraintData.hpp"

class FullViewConstraintViewModel;
class BaseConstraintView;
class BaseElementPresenter;
class BoxPresenter;
class BoxModel;


namespace iscore
{
	class SerializableCommand;
}
class ProcessPresenterInterface;

/**
 * @brief The BaseConstraintPresenter class
 *
 * Présenteur : reçoit signaux depuis modèle et vue et présenteurs enfants.
 * Exemple : cas d'un process ajouté : le modèle reçoit la commande addprocess, émet un signal, qui est capturé par le présenteur qui va instancier le présenteur nécessaire en appelant la factory.
 */
class BaseConstraintPresenter : public NamedObject
{
	Q_OBJECT

	public:
		BaseConstraintPresenter(FullViewConstraintViewModel *viewModel,
								BaseConstraintView* view,
								BaseElementPresenter *parent);
		virtual ~BaseConstraintPresenter();

		BaseConstraintView* view();
		FullViewConstraintViewModel* viewModel();

		bool isSelected() const;
		void deselect();

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);
		void constraintReleased(ConstraintData);

		void viewportChanged();

		void askUpdate();

	public slots:
		void on_constraintPressed(QPointF);
		void on_boxShown(id_type<BoxModel> boxId);
		void on_boxHidden();
		void on_boxRemoved();

		void on_horizontalZoomChanged(int);

		void recomputeViewport();

		void updateView();


	private:
		void createBoxPresenter(BoxModel*);
		void clearBoxPresenter();

		BoxPresenter* m_box{};
		// Process presenters are in the deck presenters.
		FullViewConstraintViewModel* m_viewModel{};
		BaseConstraintView* m_view{};

		int m_viewportStartTime{};
		int m_viewportEndTime{};

		int m_horizontalZoomSliderVal{};


};

