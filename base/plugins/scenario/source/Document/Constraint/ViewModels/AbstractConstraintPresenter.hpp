#pragma once
#include <tools/SettableIdentifier.hpp>
#include <tools/NamedObject.hpp>
#include <core/interface/selection/Selection.hpp>
#include <vector>

#include "Document/Constraint/ConstraintData.hpp"
#include "ProcessInterface/TimeValue.hpp"
#include "ProcessInterface/ZoomHelper.hpp"

class AbstractConstraintViewModel;
class AbstractConstraintView;
class BoxPresenter;
class BoxModel;
class ProcessSharedModelInterface;

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
                                    AbstractConstraintViewModel* model,
                                    AbstractConstraintView* view,
                                    QObject* parent);
        virtual ~AbstractConstraintPresenter() = default;
        virtual void updateScaling(double msPerPixels);

        bool isSelected() const;

        BoxPresenter* box() const
        { return m_box; }

        ConstraintModel* model() const;

        AbstractConstraintViewModel* abstractConstraintViewModel() const
        { return m_viewModel; }

        AbstractConstraintView* abstractConstraintView() const
        { return m_view; }

        void on_zoomRatioChanged(ZoomRatio val);

    signals:
        void pressed();
        void askUpdate();

    public slots:
        void on_defaultDurationChanged(TimeValue val);
        void on_minDurationChanged(TimeValue min);
        void on_maxDurationChanged(TimeValue max);

        void on_boxShown(id_type<BoxModel> boxId);
        void on_boxHidden();
        void on_boxRemoved();

        void updateHeight();

    private:
        void createBoxPresenter(BoxModel*);
        void clearBoxPresenter();

        ZoomRatio m_zoomRatio {};
        BoxPresenter* m_box {};

        // Process presenters are in the deck presenters.
        AbstractConstraintViewModel* m_viewModel {};
        AbstractConstraintView* m_view {};
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
