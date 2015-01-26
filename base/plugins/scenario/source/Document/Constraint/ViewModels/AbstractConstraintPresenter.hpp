#pragma once
#include <tools/SettableIdentifier.hpp>
#include <tools/NamedObject.hpp>

#include <vector>

#include "Document/Constraint/ConstraintData.hpp"

class AbstractConstraintViewModel;
class AbstractConstraintView;
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
class AbstractConstraintPresenter : public NamedObject
{
	Q_OBJECT

	public:
		AbstractConstraintPresenter(QString name,
									AbstractConstraintViewModel *model,
									AbstractConstraintView *view,
									QObject *parent);
		virtual ~AbstractConstraintPresenter() = default;

		virtual void recomputeViewport() = 0;

		bool isSelected() const;
		void deselect();

		BoxPresenter* box() const
		{ return m_box; }

		AbstractConstraintViewModel* abstractConstraintViewModel() const
		{ return m_viewModel; }

		AbstractConstraintView* abstractConstraintView() const
		{ return m_view; }

	signals:
		void submitCommand(iscore::SerializableCommand*);
		void elementSelected(QObject*);

		void defaultWidthChanged();

		void askUpdate();

	public slots:
		void on_boxShown(id_type<BoxModel> boxId);
		void on_boxHidden();
		void on_boxRemoved();

		virtual void updateView() = 0;
		virtual void on_constraintPressed(QPointF) { }

	private:
		void createBoxPresenter(BoxModel*);
		void clearBoxPresenter();

		BoxPresenter* m_box{};

		// Process presenters are in the deck presenters.
		AbstractConstraintViewModel* m_viewModel{};
		AbstractConstraintView* m_view{};
};

// TODO concept: constraint view model.
template<typename T>
typename T::view_model_type* viewModel(T* obj)
{
	return static_cast<typename T::view_model_type*>(obj->abstractConstraintViewModel());
}

template<typename T>
typename T::view_type* view(T* obj)
{
	return static_cast<typename T::view_type*>(obj->abstractConstraintView());
}
