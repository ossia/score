#pragma once
#include <Scenario/Document/Constraint/ViewModels/ConstraintPresenter.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>

#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>

class ObjectPath;
class QGraphicsItem;
class QObject;

namespace iscore
{
}

/**
 * @brief The FullViewConstraintPresenter class
 *
 * Présenteur : reçoit signaux depuis modèle et vue et présenteurs enfants.
 * Exemple : cas d'un process ajouté : le modèle reçoit la commande addprocess, émet un signal, qui est capturé par le présenteur qui va instancier le présenteur nécessaire en appelant la factory.
 */
class FullViewConstraintPresenter final : public ConstraintPresenter
{
        Q_OBJECT

    public:
        using viewmodel_type = FullViewConstraintViewModel;
        using view_type = FullViewConstraintView;

        FullViewConstraintPresenter(const FullViewConstraintViewModel& viewModel,
                                    QGraphicsItem* parentobject,
                                    QObject* parent);

        virtual ~FullViewConstraintPresenter();

    signals:
        void objectSelected(ObjectPath);

    private:
        iscore::SelectionDispatcher m_selectionDispatcher;

};

